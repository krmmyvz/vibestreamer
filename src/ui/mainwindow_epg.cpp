#include "mainwindow.h"
#include "xtreamclient.h"
#include "icons.h"
#include "localization.h"

#include <QDateTime>
#include <QStandardItemModel>
#include <QStatusBar>

void MainWindow::updateInfoPanel(const Channel &ch)
{
    m_epgProgress->setVisible(false);
    QString header = ch.name;
    if (!ch.year.isEmpty())
        header += QStringLiteral(" (") + ch.year + QStringLiteral(")");
    if (!ch.rating.isEmpty())
        header += QStringLiteral("  ★ ") + ch.rating;
    if (!ch.genre.isEmpty())
        header += QStringLiteral("  · ") + ch.genre;
    m_epgCurrentLabel->setText(header);
    m_epgNextLabel->setText(ch.plot.isEmpty() ? QString{} : ch.plot.left(220));
}

void MainWindow::updateEpgPanel(const Channel &ch)
{
    m_epgProgress->setVisible(true);
    const EpgProgram cur  = m_epg->currentProgram(ch.epgChannelId);
    const EpgProgram next = m_epg->nextProgram(ch.epgChannelId);

    if (cur.title.isEmpty()) {
        m_epgCurrentLabel->setText(t(QStringLiteral("no_epg_data")));
        m_epgCurrentLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-weight: 500; font-size: 12px; font-style: italic;").arg(m_theme.textDimmed));
        m_epgProgress->setValue(0);
        m_epgProgress->setVisible(false);
    } else {
        const qint64 now  = QDateTime::currentSecsSinceEpoch();
        const qint64 dur  = cur.stopTs - cur.startTs;
        const qint64 elapsed = now - cur.startTs;
        const int pct = dur > 0 ? static_cast<int>(elapsed * 100 / dur) : 0;
        const QString endTime = QDateTime::fromSecsSinceEpoch(cur.stopTs)
                                    .toString(QStringLiteral("HH:mm"));
        m_epgCurrentLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-weight: 600; font-size: 13px;").arg(m_theme.textPrimary));
        m_epgCurrentLabel->setText(cur.title + QStringLiteral("  (→ ") + endTime + QStringLiteral(")"));
        m_epgProgress->setVisible(true);
        m_epgProgress->setValue(pct);
    }

    m_epgNextLabel->setText(next.title.isEmpty()
        ? QString{}
        : t(QStringLiteral("next_prefix")) + next.title);
}

void MainWindow::refreshEpg()
{
    const int idx = m_sourceCombo->currentIndex();
    if (idx < 0 || idx >= m_config.sources.size()) return;
    const Source &src = m_config.sources[idx];

    if (!src.epgUrl.isEmpty()) {
        m_epg->load(src.epgUrl);
    } else if (m_client) {
        m_epg->load(m_client->epgUrl());
    }
}

void MainWindow::onToggleEpg()
{
    m_config.showEpg = !m_config.showEpg;
    m_epgPanel->setVisible(m_config.showEpg);
}

void MainWindow::updateFavoriteButton(const Channel &ch)
{
    m_favoriteBtn->setIcon(ch.isFavorite
        ? Icons::starFilled(m_theme.iconFavActive)
        : Icons::starOutline(m_theme.iconDefault));
}

void MainWindow::onToggleFavorite()
{
    if (m_currentChannel.streamUrl.isEmpty()) return;
    const bool isFav = m_config.toggleFavorite(m_currentChannel.streamUrl);
    m_currentChannel.isFavorite = isFav;
    for (int i = 0; i < m_chanModel->rowCount(); ++i) {
        auto *item = m_chanModel->item(i);
        if (item->data(Qt::UserRole).toString() == m_currentChannel.streamUrl) {
            item->setText((isFav ? QStringLiteral("★ ") : QString{}) + m_currentChannel.name);
            break;
        }
    }
    updateFavoriteButton(m_currentChannel);
}
