#include "mainwindow.h"
#include "mpvwidget.h"
#include "icons.h"
#include "localization.h"

#include <QMenu>
#include <QMessageBox>
#include <QStatusBar>

void MainWindow::onPlayPause()
{
    if (auto *mpv = activeMpv()) mpv->setPause(!mpv->isPaused());
}

void MainWindow::onPauseChanged(bool paused)
{
    m_playPauseBtn->setIcon(paused ? Icons::play(m_theme.iconAccent) : Icons::pause(m_theme.iconAccent));
}

void MainWindow::onVolumeChanged(int value)
{
    if (auto *mpv = activeMpv()) mpv->setVolume(value);
    m_config.volume = value;
    statusBar()->showMessage(t(QStringLiteral("volume_status")).arg(value), 1500);
}

void MainWindow::onSpeedChanged(int index)
{
    if (!m_speedCombo) return;
    MpvWidget *mpv = activeMpv();
    if (!mpv) return;
    constexpr double speeds[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    if (index >= 0 && index < 6)
        mpv->setSpeed(speeds[index]);
}

void MainWindow::onPositionChanged(double secs)
{
    if (!m_seeking) {
        MpvWidget *mpv = activeMpv();
        const double dur = mpv ? mpv->duration() : 0.0;
        if (dur > 0)
            m_seekSlider->setValue(static_cast<int>(secs / dur * 1000));
        m_timeLabel->setText(formatTime(secs) + QStringLiteral(" / ")
                             + formatTime(dur));
    }
}

void MainWindow::onDurationChanged(double secs)
{
    const bool isLive = (m_currentChannel.streamType == StreamType::Live);

    // Seek slider stays visible for all streams (live allows rewind)
    m_seekSlider->setEnabled(secs > 0);
    m_liveLabel->setVisible(isLive);

    MpvWidget *mpv = activeMpv();
    m_timeLabel->setText(formatTime(mpv ? mpv->position() : 0.0) + QStringLiteral(" / ")
                         + formatTime(secs));
}

void MainWindow::onSkipBack()
{
    if (auto *mpv = activeMpv()) mpv->seek(qMax(0.0, mpv->position() - 10));
}

void MainWindow::onSkipForward()
{
    if (auto *mpv = activeMpv()) mpv->seek(mpv->position() + 10);
}

void MainWindow::onPrevChannel()
{
    const int row = m_channelList->currentIndex().row();
    if (row > 0) {
        auto idx = m_proxyModel->index(row - 1, 0);
        m_channelList->setCurrentIndex(idx);
        onChannelDoubleClicked(idx);
    }
}

void MainWindow::onNextChannel()
{
    const int row = m_channelList->currentIndex().row();
    if (row >= 0 && row < m_proxyModel->rowCount() - 1) {
        auto idx = m_proxyModel->index(row + 1, 0);
        m_channelList->setCurrentIndex(idx);
        onChannelDoubleClicked(idx);
    }
}

void MainWindow::onShowAudioMenu()
{
    MpvWidget *mpv = activeMpv();
    if (!mpv) return;
    QMenu menu(this);
    const auto tracks = mpv->trackList();
    bool hasAudio = false;
    for (const TrackInfo &t : tracks) {
        if (t.type != QLatin1String("audio")) continue;
        hasAudio = true;
        QString label = this->t(QStringLiteral("track_n")).arg(t.id);
        if (!t.lang.isEmpty()) label += QStringLiteral(" [%1]").arg(t.lang);
        if (!t.title.isEmpty()) label += QStringLiteral(" - %1").arg(t.title);
        if (!t.codec.isEmpty()) label += QStringLiteral(" (%1)").arg(t.codec);
        QAction *act = menu.addAction(label);
        act->setCheckable(true);
        act->setChecked(t.selected);
        const int64_t id = t.id;
        connect(act, &QAction::triggered, this, [this, id]{ if (auto *m = activeMpv()) m->setAudioTrack(id); });
    }
    if (!hasAudio)
        menu.addAction(t(QStringLiteral("no_audio_tracks")))->setEnabled(false);
    menu.exec(m_audioBtn->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void MainWindow::onShowSubMenu()
{
    MpvWidget *mpv = activeMpv();
    if (!mpv) return;
    QMenu menu(this);
    const auto tracks = mpv->trackList();
    bool hasSub = false;

    // "Off" option
    QAction *offAct = menu.addAction(t(QStringLiteral("subtitle_off")));
    offAct->setCheckable(true);
    bool anySel = false;

    for (const TrackInfo &t : tracks) {
        if (t.type != QLatin1String("sub")) continue;
        hasSub = true;
        if (t.selected) anySel = true;
        QString label = this->t(QStringLiteral("subtitle_n")).arg(t.id);
        if (!t.lang.isEmpty()) label += QStringLiteral(" [%1]").arg(t.lang);
        if (!t.title.isEmpty()) label += QStringLiteral(" - %1").arg(t.title);
        QAction *act = menu.addAction(label);
        act->setCheckable(true);
        act->setChecked(t.selected);
        const int64_t id = t.id;
        connect(act, &QAction::triggered, this, [this, id]{ if (auto *m = activeMpv()) m->setSubtitleTrack(id); });
    }
    offAct->setChecked(!anySel);
    connect(offAct, &QAction::triggered, this, [this]{
        if (auto *m = activeMpv()) m->setSubtitleTrack(0);  // sid=0 disables subtitles
    });

    if (!hasSub && menu.actions().size() == 1)
        menu.addAction(t(QStringLiteral("no_subtitles")))->setEnabled(false);
    menu.exec(m_subBtn->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void MainWindow::onShowMediaInfo()
{
    MpvWidget *mpv = activeMpv();
    if (!mpv) return;
    const auto tracks = mpv->trackList();
    QString info;

    // General
    info += t(QStringLiteral("channel_info")).arg(m_currentChannel.name);
    if (!m_currentChannel.streamUrl.isEmpty())
        info += QStringLiteral("URL: %1\n").arg(m_currentChannel.streamUrl);
    info += QLatin1Char('\n');

    // Video
    const int vw = mpv->videoWidth();
    const int vh = mpv->videoHeight();
    if (vw > 0 && vh > 0)
        info += QStringLiteral("Video: %1×%2").arg(vw).arg(vh);
    const QString vc = mpv->videoCodec();
    if (!vc.isEmpty())
        info += QStringLiteral("  Codec: %1").arg(vc);
    info += QLatin1Char('\n');

    // Audio
    const QString ac = mpv->audioCodec();
    if (!ac.isEmpty())
        info += QStringLiteral("Audio Codec: %1\n").arg(ac);
    info += QLatin1Char('\n');

    // All tracks
    info += t(QStringLiteral("tracks_header"));
    for (const TrackInfo &t : tracks) {
        QString line = QStringLiteral("#%1 %2").arg(t.id).arg(t.type);
        if (!t.lang.isEmpty()) line += QStringLiteral(" [%1]").arg(t.lang);
        if (!t.title.isEmpty()) line += QStringLiteral(" %1").arg(t.title);
        if (!t.codec.isEmpty()) line += QStringLiteral(" (%1)").arg(t.codec);
        if (t.selected) line += QStringLiteral(" ✓");
        info += line + QLatin1Char('\n');
    }

    QMessageBox::information(this, t(QStringLiteral("media_info_title")), info);
}
