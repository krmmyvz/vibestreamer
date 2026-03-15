#include "imagecache.h"
#include <QNetworkRequest>
#include <QtConcurrent>
#include <QFutureWatcher>

ImageCache::ImageCache(QObject *parent) : QObject(parent)
{
    m_throttleTimer.setSingleShot(true);
    m_throttleTimer.setInterval(10);
    connect(&m_throttleTimer, &QTimer::timeout, this, &ImageCache::processQueue);
}

QPixmap ImageCache::get(const QString &url, bool *ok)
{
    if (url.isEmpty()) {
        if (ok) *ok = false;
        return QPixmap();
    }

    if (m_cache.contains(url)) {
        if (ok) *ok = true;
        return m_cache.value(url);
    }

    if (ok) *ok = false;

    // Already downloading or queued
    if (m_pending.contains(url) || m_queuedSet.contains(url))
        return QPixmap();

    // Enqueue and kick off processing
    m_queue.enqueue(url);
    m_queuedSet.insert(url);
    if (!m_throttleTimer.isActive())
        m_throttleTimer.start();

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

        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

        QNetworkReply *reply = m_net.get(req);
        m_pending.insert(url, reply);

        connect(reply, &QNetworkReply::finished, this, [this, url, reply]() {
            m_pending.remove(url);
            reply->deleteLater();

            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray rawData = reply->readAll();
                
                // Process image in background (QImage is thread-safe, QPixmap is not)
                auto *watcher = new QFutureWatcher<QImage>(this);
                connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, url, watcher]() {
                    QImage img = watcher->result();
                    watcher->deleteLater();
                    
                    if (!img.isNull()) {
                        QPixmap pixmap = QPixmap::fromImage(img);
                        m_cache.insert(url, pixmap);
                        emit imageLoaded(url, pixmap);
                    }
                });
                
                watcher->setFuture(QtConcurrent::run([rawData]() {
                    QImage img;
                    if (img.loadFromData(rawData)) {
                        return img.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation);
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
