#include "imagecache.h"
#include <QNetworkRequest>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QFile>

ImageCache::ImageCache(QObject *parent) : QObject(parent)
{
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QDir::separator() + "logos";
    m_cacheDir.setPath(cachePath);
    if (!m_cacheDir.exists()) {
        m_cacheDir.mkpath(".");
    }

    m_throttleTimer.setSingleShot(true);
    m_throttleTimer.setInterval(10);
    connect(&m_throttleTimer, &QTimer::timeout, this, &ImageCache::processQueue);
}

QString ImageCache::getDiskCachePath(const QString &url) const
{
    QString hash = QString(QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Md5).toHex());
    return m_cacheDir.absoluteFilePath(hash + ".png");
}

QPixmap ImageCache::get(const QString &url, bool *ok)
{
    if (url.isEmpty()) {
        if (ok) *ok = false;
        return QPixmap();
    }

    // Fast path: memory cache only (no disk I/O)
    if (m_cache.contains(url)) {
        if (ok) *ok = true;
        return m_cache.value(url);
    }

    if (ok) *ok = false;

    // Already downloading or queued
    if (m_pending.contains(url) || m_queuedSet.contains(url) || m_diskPending.contains(url))
        return QPixmap();

    // Check disk cache asynchronously to avoid blocking the main thread
    QString diskPath = getDiskCachePath(url);
    m_diskPending.insert(url);

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, url, watcher]() {
        m_diskPending.remove(url);
        QImage img = watcher->result();
        watcher->deleteLater();

        if (!img.isNull()) {
            QPixmap pixmap = QPixmap::fromImage(img);
            m_cache.insert(url, pixmap);
            emit imageLoaded(url, pixmap);
            return;
        }

        // Not on disk — queue for network download
        if (!m_cache.contains(url) && !m_pending.contains(url) && !m_queuedSet.contains(url)) {
            m_queue.enqueue(url);
            m_queuedSet.insert(url);
            if (!m_throttleTimer.isActive())
                m_throttleTimer.start();
        }
    });

    watcher->setFuture(QtConcurrent::run([diskPath]() {
        QImage img;
        if (QFile::exists(diskPath))
            img.load(diskPath);
        return img;
    }));

    return QPixmap();
}

void ImageCache::processQueue()
{
    while (!m_queue.isEmpty() && m_pending.size() < MaxConcurrent) {
        const QString url = m_queue.dequeue();
        m_queuedSet.remove(url);

        // Skip duplicates that got cached while queued
        if (m_cache.contains(url))
            continue;

        // Only fetch http(s) URLs — reject file:// or other schemes
        if (!url.startsWith(QStringLiteral("http://"),  Qt::CaseInsensitive) &&
            !url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
            if (!m_queue.isEmpty() && !m_throttleTimer.isActive())
                m_throttleTimer.start();
            continue;
        }

        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *reply = m_net.get(req);
        m_pending.insert(url, reply);

        connect(reply, &QNetworkReply::finished, this, [this, url, reply]() {
            m_pending.remove(url);
            reply->deleteLater();

            if (reply->error() == QNetworkReply::NoError) {
                constexpr qint64 kMaxLogoBytes = 2LL * 1024 * 1024; // 2 MB
                const QByteArray rawData = reply->read(kMaxLogoBytes + 1);
                if (rawData.size() > kMaxLogoBytes) {
                    if (!m_queue.isEmpty() && !m_throttleTimer.isActive())
                        m_throttleTimer.start();
                    return;
                }

                // Process image in background (QImage is thread-safe, QPixmap is not)
                auto *watcher = new QFutureWatcher<QImage>(this);
                QString diskPath = getDiskCachePath(url);

                connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, url, watcher]() {
                    QImage img = watcher->result();
                    watcher->deleteLater();

                    if (!img.isNull()) {
                        QPixmap pixmap = QPixmap::fromImage(img);
                        m_cache.insert(url, pixmap);
                        emit imageLoaded(url, pixmap);
                    }
                });

                watcher->setFuture(QtConcurrent::run([rawData, diskPath]() {
                    QImage img;
                    if (img.loadFromData(rawData)) {
                        QImage scaled = img.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        scaled.save(diskPath, "PNG");
                        return scaled;
                    }
                    return QImage();
                }));
            }

            // Schedule next batch via timer — never call processQueue() directly
            // to avoid recursive call chains that starve the GUI event loop
            if (!m_queue.isEmpty() && !m_throttleTimer.isActive())
                m_throttleTimer.start();
        });
    }
}
