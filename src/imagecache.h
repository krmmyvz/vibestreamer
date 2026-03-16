#pragma once
#include <QObject>
#include <QPixmap>
#include <QHash>
#include <QQueue>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDir>

class ImageCache : public QObject {
    Q_OBJECT
public:
    static ImageCache& instance() {
        static ImageCache cm;
        return cm;
    }

    // Requests an image. If cached, it returns the pixmap synchronously and sets `ok` to true.
    // Otherwise it sets `ok` to false, queues a download, and emits `imageLoaded` when ready.
    QPixmap get(const QString &url, bool *ok = nullptr);

signals:
    void imageLoaded(const QString &url, const QPixmap &pixmap);

private:
    ImageCache(QObject *parent = nullptr);
    ~ImageCache() override = default;

    void processQueue();
    QString getDiskCachePath(const QString &url) const;

    QNetworkAccessManager m_net;
    QHash<QString, QPixmap> m_cache;
    QHash<QString, QNetworkReply*> m_pending;
    QQueue<QString> m_queue;          // URLs waiting to be fetched
    QSet<QString> m_queuedSet;        // For O(1) check if already in queue
    QTimer m_throttleTimer;
    QDir m_cacheDir;
    static constexpr int MaxConcurrent = 6;   // max simultaneous downloads
};
