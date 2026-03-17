#pragma once
#include "models.h"

#include <QList>
#include <QObject>
#include <QString>
#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

class XtreamClient : public QObject {
    Q_OBJECT
public:
    explicit XtreamClient(const Source &source, QObject *parent = nullptr);

    void testConnection(std::function<void(bool ok, QString err)> cb);
    void getLiveCategories(std::function<void(QList<Category>, QString err)> cb);
    void getVodCategories(std::function<void(QList<Category>, QString err)> cb);
    void getSeriesCategories(std::function<void(QList<Category>, QString err)> cb);
    void getLiveStreams(const QString &categoryId,
                       std::function<void(QList<Channel>, QString err)> cb);
    void getVodStreams(const QString &categoryId,
                      std::function<void(QList<Channel>, QString err)> cb);
    void getSeries(const QString &categoryId,
                   std::function<void(QList<Channel>, QString err)> cb);
    QString epgUrl() const;

private:
    void get(const QString &url,
             std::function<void(QByteArray data, QString err)> cb);

    Source                  m_source;
    QString                 m_base;
    QString                 m_auth;  // "username=X&password=Y"
    QNetworkAccessManager  *m_nam;
};
