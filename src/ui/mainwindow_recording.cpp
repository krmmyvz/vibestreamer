#include "mainwindow.h"
#include "mpvwidget.h"
#include "icons.h"
#include "localization.h"

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStatusBar>

void MainWindow::onToggleRecord()
{
    MpvWidget *mpv = activeMpv();
    if (!mpv || m_currentChannel.streamUrl.isEmpty()) return;

    if (m_isRecording) {
        // If already recording, clicking record again acts as stop
        onRecordStop();
        return;
    }

    // Start recording — validate path stays within safe locations
    QString dir = m_config.recordPath;
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

    {
        // Resolve to canonical/absolute path and ensure it's under home directory
        const QString homePath = QDir::homePath();
        const QString absDir   = QDir(dir).absolutePath();
        if (!absDir.startsWith(homePath)) {
            // Fall back to Movies folder if outside home (e.g. /etc, /tmp)
            dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        }
    }

    QDir recordDir(dir);
    if (!recordDir.exists())
        recordDir.mkpath(QStringLiteral("."));

    QString safeName = m_currentChannel.name;
    safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));

    // Guard against Windows reserved filenames (CON, PRN, AUX, NUL, COM1-9, LPT1-9)
    static const QRegularExpression kWinReserved(
        QStringLiteral("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$"),
        QRegularExpression::CaseInsensitiveOption);
    if (safeName.isEmpty() || kWinReserved.match(safeName).hasMatch())
        safeName = QStringLiteral("Channel");

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_recordSegment = 0;
    const QString fileName = QStringLiteral("Record_%1_%2.ts").arg(safeName, timestamp);
    m_recordFilePath = recordDir.absoluteFilePath(fileName);

    mpv->setMpvProperty(QStringLiteral("stream-record"), m_recordFilePath);
    m_isRecording = true;
    m_recordPaused = false;

    // Update UI: show pause/stop, change record button to active state
    m_recordBtn->setIcon(Icons::record(m_theme.iconError));
    m_recordBtn->setToolTip(t(QStringLiteral("recording_in_progress")));
    m_recordBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background: %1; border-radius: 4px; }").arg(m_theme.errorAlpha));
    m_recordPauseBtn->setVisible(true);
    m_recordStopBtn->setVisible(true);
    statusBar()->showMessage(t(QStringLiteral("recording_started")) + m_recordFilePath, 5000);
}

void MainWindow::onRecordPauseResume()
{
    MpvWidget *mpv = activeMpv();
    if (!mpv || !m_isRecording) return;

    if (!m_recordPaused) {
        // Pause: stop writing to file but keep the partial recording
        mpv->setMpvProperty(QStringLiteral("stream-record"), QString());
        m_recordPaused = true;
        m_recordPauseBtn->setIcon(Icons::play(m_theme.iconAccent));
        m_recordPauseBtn->setToolTip(t(QStringLiteral("resume_recording")));
        m_recordBtn->setIcon(Icons::record(m_theme.iconRecord));
        m_recordBtn->setStyleSheet(QString{});
        statusBar()->showMessage(t(QStringLiteral("recording_paused")) + m_recordFilePath, 5000);
    } else {
        // Resume: start a new segment file
        m_recordSegment++;
        // Insert segment number before extension
        QString resumed = m_recordFilePath;
        const int dotPos = resumed.lastIndexOf(QLatin1Char('.'));
        if (dotPos > 0)
            resumed.insert(dotPos, QStringLiteral("_part%1").arg(m_recordSegment));
        else
            resumed += QStringLiteral("_part%1").arg(m_recordSegment);

        mpv->setMpvProperty(QStringLiteral("stream-record"), resumed);
        m_recordPaused = false;
        m_recordPauseBtn->setIcon(Icons::pause(m_theme.iconDefault));
        m_recordPauseBtn->setToolTip(t(QStringLiteral("pause_recording")));
        m_recordBtn->setIcon(Icons::record(m_theme.iconError));
        m_recordBtn->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border-radius: 4px; }").arg(m_theme.errorAlpha));
        statusBar()->showMessage(t(QStringLiteral("recording_resumed")) + resumed, 5000);
    }
}

void MainWindow::onRecordStop()
{
    MpvWidget *mpv = activeMpv();
    if (!mpv) return;

    // Stop recording — finalize file
    mpv->setMpvProperty(QStringLiteral("stream-record"), QString());
    m_isRecording = false;
    m_recordPaused = false;

    // Reset UI
    m_recordBtn->setIcon(Icons::record(m_theme.iconRecord));
    m_recordBtn->setToolTip(t(QStringLiteral("start_recording")));
    m_recordBtn->setStyleSheet(QString{});
    m_recordPauseBtn->setVisible(false);
    m_recordStopBtn->setVisible(false);

    statusBar()->showMessage(t(QStringLiteral("recording_saved")) + m_recordFilePath, 8000);
    m_recordFilePath.clear();
    m_recordSegment = 0;
}
