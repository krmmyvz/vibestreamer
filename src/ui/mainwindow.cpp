#include "mainwindow.h"
#include "mpvwidget.h"
#include "multiviewwidget.h"
#include "localization.h"

#include <QApplication>
#include <QTimer>
#include <QStatusBar>
#include <QMessageBox>
#include <QCloseEvent>
#include <QKeyEvent>

// ─── Security helpers ─────────────────────────────────────────────────────────

// Whitelist of safe mpv properties users may override via Extra MPV Args.
// Dangerous ones (stream-record, screenshot-dir, config, script, …) are excluded.
static bool isSafeMpvProperty(const QString &name)
{
    static const QSet<QString> kAllowed = {
        QStringLiteral("hwdec"),
        QStringLiteral("deband"),
        QStringLiteral("scale"),
        QStringLiteral("cscale"),
        QStringLiteral("dscale"),
        QStringLiteral("dither-depth"),
        QStringLiteral("correct-downscaling"),
        QStringLiteral("linear-downscaling"),
        QStringLiteral("sigmoid-upscaling"),
        QStringLiteral("interpolation"),
        QStringLiteral("video-sync"),
        QStringLiteral("framedrop"),
        QStringLiteral("audio-channels"),
        QStringLiteral("demuxer-max-bytes"),
        QStringLiteral("cache"),
        QStringLiteral("cache-secs"),
        QStringLiteral("network-timeout"),
        QStringLiteral("volume-max"),
        QStringLiteral("af"),
        QStringLiteral("vf"),
        QStringLiteral("deinterlace"),
        QStringLiteral("blend-subtitles"),
        QStringLiteral("sub-scale"),
        QStringLiteral("sub-bold"),
        QStringLiteral("sub-font"),
        QStringLiteral("sub-color"),
    };
    return kAllowed.contains(name);
}

// Accept only http:// and https:// URLs to prevent file:// / ftp:// injection.
static bool isSafeUrl(const QString &url)
{
    const QString s = url.trimmed();
    return s.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || s.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_epg(new EpgManager(this))
{
    I18n::instance().setLanguage(m_config.language);

    setWindowTitle(QStringLiteral("Vibestreamer"));
    resize(m_config.windowWidth, m_config.windowHeight);

    applyTheme();
    setupUI();
    setupMenuBar();
    setupShortcuts();

    // Initial state for the loading progress
    m_loadingProgress->setVisible(false);

    loadSourceList();

    connect(m_epg, &EpgManager::loaded, this, [this]() {
        if (!m_currentChannel.epgChannelId.isEmpty())
            updateEpgPanel(m_currentChannel);
    });
    connect(m_epg, &EpgManager::loadError, this, [this](const QString &err) {
        statusBar()->showMessage(t(QStringLiteral("epg_load_error")) + err, 6000);
    });

    // Auto-refresh EPG every 5 minutes
    auto *epgTimer = new QTimer(this);
    epgTimer->setInterval(5 * 60 * 1000);
    connect(epgTimer, &QTimer::timeout, this, &MainWindow::refreshEpg);
    epgTimer->start();

    // Auto-update playlists every 10 minutes (checks if interval reached)
    m_autoUpdateTimer = new QTimer(this);
    m_autoUpdateTimer->setInterval(10 * 60 * 1000); // 10 minutes
    connect(m_autoUpdateTimer, &QTimer::timeout, this, &MainWindow::checkUpdateSchedules);
    m_autoUpdateTimer->start();

    // Initial check right after startup
    QTimer::singleShot(5000, this, &MainWindow::checkUpdateSchedules);
}

MainWindow::~MainWindow() = default;

QString MainWindow::t(const QString &key) const
{
    return I18n::instance().get(key);
}

MpvWidget* MainWindow::activeMpv() const
{
    if (m_multiViewMode && m_multiViewWidget)
        return m_multiViewWidget->activePlayer();
    return m_mpv;
}

void MainWindow::connectActiveMpvSignals()
{
    MpvWidget *mpv = activeMpv();
    if (!mpv) return;
    connect(mpv, &MpvWidget::positionChanged, this, &MainWindow::onPositionChanged);
    connect(mpv, &MpvWidget::durationChanged,  this, &MainWindow::onDurationChanged);
    connect(mpv, &MpvWidget::pauseChanged,     this, &MainWindow::onPauseChanged);
    connect(mpv, &MpvWidget::errorOccurred,    this, [this, mpv](const QString &msg) {
        mpv->stop();
        statusBar()->showMessage(t(QStringLiteral("playback_error")) + msg, 8000);
    });
}

void MainWindow::disconnectMpvSignals(MpvWidget *mpv)
{
    if (mpv) mpv->disconnect(this);
}

QString MainWindow::formatTime(double secs) const
{
    if (secs <= 0) return QStringLiteral("0:00");
    const int s = static_cast<int>(secs);
    const int h = s / 3600;
    const int m = (s % 3600) / 60;
    const int ss = s % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
               .arg(h).arg(m, 2, 10, QLatin1Char('0'))
               .arg(ss, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m)
           .arg(ss, 2, 10, QLatin1Char('0'));
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    // If in PiP mode, exit PiP instead of closing the app
    if (m_pipMode) {
        onTogglePip();
        e->ignore();
        return;
    }
    // Stop any active recording before closing
    if (m_isRecording)
        onRecordStop();

    m_config.windowWidth  = width();
    m_config.windowHeight = height();
    m_config.save();
    e->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && isFullScreen())
        toggleFullScreen();
    else
        QMainWindow::keyPressEvent(e);
}
