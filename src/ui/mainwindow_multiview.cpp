#include "mainwindow.h"
#include "mpvwidget.h"
#include "multiviewwidget.h"
#include "localization.h"

#include <QMenuBar>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>

void MainWindow::onOpenMultiView()
{
    if (!m_mpv) return;

    if (!m_multiViewMode) {
        // ── Enter MultiView ──
        m_multiViewMode = true;

        if (!m_multiViewWidget) {
            m_multiViewWidget = new MultiViewWidget(this);
            m_playerStack->addWidget(m_multiViewWidget); // index 1

            // Apply config settings to all 4 players
            for (int i = 0; i < 4; ++i) {
                MpvWidget *p = m_multiViewWidget->player(i);
                if (!m_config.mpvHwDecode.isEmpty())
                    p->setMpvProperty(QStringLiteral("hwdec"), m_config.mpvHwDecode);
                if (!m_config.mpvExtraArgs.isEmpty()) {
                    const QStringList parts = m_config.mpvExtraArgs.split(
                        QLatin1Char(' '), Qt::SkipEmptyParts);
                    for (const QString &part : parts) {
                        const int eq = part.indexOf(QLatin1Char('='));
                        if (eq > 0) p->setMpvProperty(part.left(eq), part.mid(eq + 1));
                    }
                }
                // Reduce per-player buffer for multiview: 4 streams × 30s cache
                // overwhelms RAM and network. 5s is enough for live IPTV.
                p->setMpvProperty(QStringLiteral("cache-secs"),       QStringLiteral("5"));
                p->setMpvProperty(QStringLiteral("demuxer-max-bytes"), QStringLiteral("10MiB"));

                // Auto-retry with exponential backoff when a stream fails to load.
                // IPTV servers often reject concurrent connections — retry gives
                // the server time to accept after an initial refusal.
                connect(p, &MpvWidget::errorOccurred, this, [this, i](const QString &err) {
                    if (!m_multiViewMode || !m_multiViewWidget) return;
                    const QString url = m_multiViewChannels[i].streamUrl;
                    if (url.isEmpty()) return;

                    const int delay = m_multiViewRetryDelay[i];
                    qWarning() << "[MultiView] player" << i << "error:" << err
                               << "— retrying in" << delay << "s";
                    // Double the delay for next failure, cap at 30s
                    m_multiViewRetryDelay[i] = qMin(delay * 2, 30);

                    QTimer::singleShot(delay * 1000, this, [this, i, url]() {
                        if (m_multiViewMode && m_multiViewWidget
                                && m_multiViewChannels[i].streamUrl == url) {
                            m_multiViewWidget->player(i)->play(url);
                        }
                    });
                });

                // Reset retry delay on successful playback start
                connect(p, &MpvWidget::playbackStarted, this, [this, i]() {
                    m_multiViewRetryDelay[i] = 2;
                });
            }

            connect(m_multiViewWidget, &MultiViewWidget::activeChanged,
                    this, &MainWindow::onMultiViewActiveChanged);
        }

        // Disconnect m_mpv UI signals
        disconnectMpvSignals(m_mpv);

        // Switch to multiview stack page
        m_playerStack->setCurrentWidget(m_multiViewWidget);

        // Transfer current channel to cell 0
        if (!m_currentChannel.streamUrl.isEmpty()) {
            m_multiViewRetryDelay[0] = 2;
            m_multiViewWidget->player(0)->play(m_currentChannel.streamUrl);
            m_multiViewChannels[0] = m_currentChannel;
        }

        // Connect signals from active multiview player
        connectActiveMpvSignals();
        m_multiViewWidget->activePlayer()->setVolume(m_volumeSlider->value());

        m_multiViewBtn->setChecked(true);

    } else {
        // ── Exit MultiView ──
        const int activeIdx = m_multiViewWidget->activeIndex();
        const Channel activeChannel = m_multiViewChannels[activeIdx];

        // Disconnect active multiview player signals
        disconnectMpvSignals(m_multiViewWidget->activePlayer());

        // Stop all multiview players
        for (int i = 0; i < 4; ++i)
            m_multiViewWidget->player(i)->stop();

        // Switch back to single player
        m_playerStack->setCurrentWidget(m_mpv);
        m_multiViewMode = false;

        // Play active cell's channel in m_mpv
        if (!activeChannel.streamUrl.isEmpty()) {
            m_currentChannel = activeChannel;
            m_mpv->play(activeChannel.streamUrl);
            setWindowTitle(activeChannel.name + QStringLiteral(" — Vibestreamer"));
            statusBar()->showMessage(activeChannel.name);
        }

        // Reconnect m_mpv signals
        connectActiveMpvSignals();
        m_mpv->setVolume(m_volumeSlider->value());

        m_multiViewBtn->setChecked(false);
    }
}

void MainWindow::onMultiViewActiveChanged(int newIndex, int oldIndex)
{
    disconnectMpvSignals(m_multiViewWidget->player(oldIndex));
    m_multiViewWidget->player(oldIndex)->setVolume(0);

    connectActiveMpvSignals();
    m_multiViewWidget->activePlayer()->setVolume(m_volumeSlider->value());

    // Update play/pause button state
    onPauseChanged(m_multiViewWidget->activePlayer()->isPaused());
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        m_sidebar->setVisible(true);
        menuBar()->setVisible(true);
        statusBar()->setVisible(true);
        if (!m_savedSplitterSizes.isEmpty())
            m_mainSplitter->setSizes(m_savedSplitterSizes);
    } else {
        m_savedSplitterSizes = m_mainSplitter->sizes();
        m_sidebar->setVisible(false);
        menuBar()->setVisible(false);
        statusBar()->setVisible(false);
        showFullScreen();
    }
}
