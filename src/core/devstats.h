#pragma once

// ─── DevStats ────────────────────────────────────────────────────────────────
// Developer diagnostics — compiled only when DEV_MODE is defined (cmake -DDEV_MODE=ON).
// In Release builds this header contributes zero code: all macros are no-ops.
//
// Usage:
//   DEV_STAT("image_cache_entries", m_cache.size())
//   DEV_STAT("epg_channels",        m_data.size())
//
// Output:
//   Log  → <AppData>/Vibestreamer/dev.log   (all qDebug/qWarning/qCritical)
//   Stats→ <Temp>/vibestreamer_stats.json   (JSON snapshot, refreshed every 10 s)
// ─────────────────────────────────────────────────────────────────────────────

#ifdef DEV_MODE

#include <QObject>
#include <QTimer>
#include <QVariant>
#include <QVariantMap>
#include <QString>

class DevStats : public QObject {
    Q_OBJECT
public:
    static DevStats &instance();

    // Call once from main() after QApplication is created.
    void init();

    // Update a named metric (thread-safe from main thread).
    void set(const QString &key, const QVariant &value);

private:
    DevStats() = default;
    void flush();
    static qint64 processRssKb();

    QTimer      m_timer;
    QVariantMap m_stats;
    QString     m_logPath;
    QString     m_statsPath;
};

#define DEV_STAT(key, val) DevStats::instance().set(QStringLiteral(key), (val))

#else  // ── Release: everything compiles away ──────────────────────────────────

#define DEV_STAT(key, val) do {} while (0)

#endif // DEV_MODE
