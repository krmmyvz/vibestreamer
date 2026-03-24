#include "mainwindow.h"
#include "mpvwidget.h"
#include "icons.h"
#include "theme.h"

#include <QApplication>
#include <QFrame>
#include <QStyleFactory>

void MainWindow::applyTheme()
{
    m_theme = Theme::colors(m_config);
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    qApp->setStyleSheet(Theme::style(m_config));

    // Only re-apply inline styles if UI has been set up already
    if (m_controlBar)
        applyInlineStyles();
}

void MainWindow::applyInlineStyles()
{
    // Sidebar
    if (m_sidebar)
        m_sidebar->setStyleSheet(QStringLiteral(
            "QWidget#sidebar { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 %1, stop:1 %2); "
            "border-right: 1px solid %3; }")
            .arg(m_theme.sidebarGradStart, m_theme.sidebarGradEnd, m_theme.sidebarBorder));

    // Control bar
    if (m_controlBar) {
        m_controlBar->setStyleSheet(QStringLiteral(
            "QWidget#controlBar { background: %1; border-top: 1px solid %2; }")
            .arg(m_theme.bgSurface, m_theme.borderSubtle));

        const QString btnStyle = QStringLiteral(
            "QToolButton { border-radius: 4px; padding: 0px; border: none; background: transparent; }"
            "QToolButton:hover { background: %1; }"
            "QToolButton:pressed { background: %2; }").arg(m_theme.hoverBg, m_theme.pressedBg);

        auto applyBtnStyle = [&](QToolButton* btn) {
            if (btn) btn->setStyleSheet(btnStyle);
        };
        applyBtnStyle(m_prevChBtn);
        applyBtnStyle(m_skipBackBtn);
        applyBtnStyle(m_playPauseBtn);
        applyBtnStyle(m_skipFwdBtn);
        applyBtnStyle(m_recordBtn);
        applyBtnStyle(m_recordPauseBtn);
        applyBtnStyle(m_recordStopBtn);
        applyBtnStyle(m_nextChBtn);
        applyBtnStyle(m_audioBtn);
        applyBtnStyle(m_subBtn);
        applyBtnStyle(m_infoBtn);
        applyBtnStyle(m_favoriteBtn);
        applyBtnStyle(m_multiViewBtn);
        applyBtnStyle(m_epgToggleBtn);
        applyBtnStyle(m_pipBtn);
        applyBtnStyle(m_fullscreenBtn);

        for (auto *frame : m_controlBar->findChildren<QFrame *>()) {
            if (frame->frameShape() == QFrame::VLine) {
                frame->setStyleSheet(QStringLiteral("background: %1;").arg(m_theme.separator));
            }
        }
    }

    // Time / live labels
    if (m_timeLabel)
        m_timeLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; min-width: 90px; font-weight: 500;").arg(m_theme.textSecondary));
    if (m_liveLabel)
        m_liveLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: bold; "
            "background: %2; border-radius: 3px; padding: 1px 6px;")
            .arg(m_theme.error, m_theme.errorAlpha));

    // EPG labels
    if (m_epgCurrentLabel)
        m_epgCurrentLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-weight: 600; font-size: 13px;").arg(m_theme.textPrimary));
    if (m_epgProgress)
        m_epgProgress->setStyleSheet(QStringLiteral(
            "QProgressBar { background: %1; border: none; } "
            "QProgressBar::chunk { background-color: %2; }")
            .arg(m_theme.epgProgressBg, m_theme.epgProgressChunk));

    // Icons
    if (m_playPauseBtn && m_mpv)
        m_playPauseBtn->setIcon(m_mpv->isPaused()
            ? Icons::play(m_theme.iconAccent) : Icons::pause(m_theme.iconAccent));
    if (m_favoriteBtn)
        m_favoriteBtn->setIcon(m_currentChannel.isFavorite
            ? Icons::starFilled(m_theme.iconFavActive)
            : Icons::starOutline(m_theme.iconDefault));

    if (m_prevChBtn)    m_prevChBtn->setIcon(Icons::skipPrevious(m_theme.iconDefault));
    if (m_skipBackBtn)  m_skipBackBtn->setIcon(Icons::rewind(m_theme.iconDefault));
    if (m_skipFwdBtn)   m_skipFwdBtn->setIcon(Icons::fastForward(m_theme.iconDefault));
    if (m_nextChBtn)    m_nextChBtn->setIcon(Icons::skipNext(m_theme.iconDefault));

    bool isRecording = !m_recordFilePath.isEmpty();
    if (m_recordBtn) {
        if (isRecording) {
            m_recordBtn->setIcon(m_recordPaused ? Icons::record(m_theme.iconRecord) : Icons::record(m_theme.iconError));
        } else {
            m_recordBtn->setIcon(Icons::record(m_theme.iconRecord));
        }
    }
    if (m_recordPauseBtn) m_recordPauseBtn->setIcon(m_recordPaused ? Icons::play(m_theme.iconAccent) : Icons::pause(m_theme.iconDefault));
    if (m_recordStopBtn)  m_recordStopBtn->setIcon(Icons::stop(m_theme.iconError));

    if (m_audioBtn)       m_audioBtn->setIcon(Icons::audioTrack(m_theme.iconDefault));
    if (m_subBtn)         m_subBtn->setIcon(Icons::closedCaption(m_theme.iconDefault));
    if (m_infoBtn)        m_infoBtn->setIcon(Icons::info(m_theme.iconDefault));

    if (m_multiViewBtn)   m_multiViewBtn->setIcon(Icons::gridView(m_theme.iconDefault));
    if (m_epgToggleBtn)   m_epgToggleBtn->setIcon(Icons::tvGuide(m_theme.iconDefault));
    if (m_pipBtn)         m_pipBtn->setIcon(Icons::pictureInPicture(m_theme.iconDefault));
    if (m_fullscreenBtn)  m_fullscreenBtn->setIcon(Icons::fullscreen(m_theme.iconDefault));

    if (m_volIcon)        m_volIcon->setIcon(Icons::volumeUp(m_theme.iconMuted));

    // Source panel buttons
    const QString srcBtnStyle = QStringLiteral(
        "QToolButton { border: none; border-radius: 4px; background: transparent; }"
        "QToolButton:hover { background: %1; }"
        "QToolButton:pressed { background: %2; }").arg(m_theme.hoverBg, m_theme.pressedBg);

    if (m_addSourceBtn) {
        m_addSourceBtn->setStyleSheet(srcBtnStyle);
        m_addSourceBtn->setIcon(Icons::add(m_theme.iconSuccess));
    }
    if (m_editSourceBtn) {
        m_editSourceBtn->setStyleSheet(srcBtnStyle);
        m_editSourceBtn->setIcon(Icons::edit(m_theme.iconDefault));
    }
    if (m_delSourceBtn) {
        m_delSourceBtn->setStyleSheet(srcBtnStyle);
        m_delSourceBtn->setIcon(Icons::trash(m_theme.iconError));
    }
}

void MainWindow::updateStyle()
{
    applyTheme();
}
