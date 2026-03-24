// Compiled only when DEV_MODE is defined — not included in Release builds.
#ifdef DEV_MODE

#include "devstats.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTextStream>
#include <QtGlobal>

// ─── Log handler ─────────────────────────────────────────────────────────────

static QMutex   s_logMutex;
static QFile   *s_logFile  = nullptr;

static void devMessageHandler(QtMsgType type,
                               const QMessageLogContext &ctx,
                               const QString &msg)
{
    const char *level = "DEBUG";
    switch (type) {
    case QtInfoMsg:     level = "INFO ";  break;
    case QtWarningMsg:  level = "WARN ";  break;
    case QtCriticalMsg: level = "ERROR";  break;
    case QtFatalMsg:    level = "FATAL";  break;
    default: break;
    }

    const QString file = ctx.file
        ? QString::fromUtf8(ctx.file).section(QLatin1Char('/'), -2)  // last 2 path components
        : QStringLiteral("?");

    const QString line = QString::asprintf(
        "[%s] [%s] %s:%d — %s\n",
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz").toUtf8().constData(),
        level,
        file.toUtf8().constData(),
        ctx.line,
        msg.toUtf8().constData());

    // Write to log file
    {
        QMutexLocker lock(&s_logMutex);
        if (s_logFile && s_logFile->isOpen()) {
            s_logFile->write(line.toUtf8());
            s_logFile->flush();
        }
    }

    // Also print to stderr so Qt Creator / terminal still shows output
    fprintf(stderr, "%s", line.toUtf8().constData());

    if (type == QtFatalMsg)
        abort();
}

// ─── DevStats ─────────────────────────────────────────────────────────────────

DevStats &DevStats::instance()
{
    static DevStats s;
    return s;
}

void DevStats::init()
{
    // ── Paths ──────────────────────────────────────────────────────────────
    const QString dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dataDir);
    m_logPath   = dataDir + QStringLiteral("/dev.log");

    const QString tmpDir = QStandardPaths::writableLocation(
        QStandardPaths::TempLocation);
    m_statsPath = tmpDir + QStringLiteral("/vibestreamer_stats.json");

    // ── Log file ───────────────────────────────────────────────────────────
    s_logFile = new QFile(m_logPath, this);
    s_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    qInstallMessageHandler(devMessageHandler);

    qInfo() << "=== DevStats started ===";
    qInfo() << "Log  :" << m_logPath;
    qInfo() << "Stats:" << m_statsPath;

    // ── Stats flush timer ──────────────────────────────────────────────────
    m_timer.setInterval(10'000);  // every 10 seconds
    connect(&m_timer, &QTimer::timeout, this, &DevStats::flush);
    m_timer.start();

    flush(); // write initial snapshot immediately
}

void DevStats::set(const QString &key, const QVariant &value)
{
    m_stats.insert(key, value);
}

void DevStats::flush()
{
    m_stats.insert(QStringLiteral("updated_at"),
                   QDateTime::currentDateTime().toString(Qt::ISODate));
    m_stats.insert(QStringLiteral("process_rss_kb"), processRssKb());

    QJsonObject obj;
    for (auto it = m_stats.constBegin(); it != m_stats.constEnd(); ++it)
        obj.insert(it.key(), QJsonValue::fromVariant(it.value()));

    QFile f(m_statsPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(obj).toJson());
    }
}

qint64 DevStats::processRssKb()
{
#if defined(Q_OS_LINUX)
    QFile f(QStringLiteral("/proc/self/status"));
    if (!f.open(QIODevice::ReadOnly)) return -1;
    for (const QByteArray &line : f.readAll().split('\n')) {
        if (line.startsWith("VmRSS:"))
            return line.split(':').last().trimmed().split(' ').first().toLongLong();
    }
#endif
    return -1;
}

#endif // DEV_MODE
