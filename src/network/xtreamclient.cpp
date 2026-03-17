#include "xtreamclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

XtreamClient::XtreamClient(const Source &source, QObject *parent)
    : QObject(parent)
    , m_source(source)
    , m_base(source.serverUrl.trimmed().trimmed())
    , m_nam(new QNetworkAccessManager(this))
{
    // Ensure no trailing slash
    while (m_base.endsWith(QLatin1Char('/')))
        m_base.chop(1);

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("username"), source.username);
    q.addQueryItem(QStringLiteral("password"), source.password);
    m_auth = q.toString(QUrl::FullyEncoded);
}

static bool isSafeUrl(const QString &url)
{
    const QString s = url.trimmed();
    return s.startsWith(QStringLiteral("http://"),  Qt::CaseInsensitive)
        || s.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

void XtreamClient::get(const QString &url,
                       std::function<void(QByteArray, QString)> cb)
{
    if (!isSafeUrl(url)) {
        cb({}, QStringLiteral("Rejected URL with disallowed scheme"));
        return;
    }
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "Vibestreamer/2.0");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(30000);

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [reply, cb]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            cb({}, reply->errorString());
            return;
        }
        cb(reply->readAll(), {});
    });
}

void XtreamClient::testConnection(std::function<void(bool, QString)> cb)
{
    const QString url = m_base + QStringLiteral("/player_api.php?") + m_auth;
    get(url, [cb](QByteArray data, QString err) {
        if (!err.isEmpty()) { cb(false, err); return; }
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        if (obj.contains(QStringLiteral("user_info")))
            cb(true, {});
        else
            cb(false, QStringLiteral("Invalid server response"));
    });
}

// ─── Categories ─────────────────────────────────────────────────────────────

static QList<Category> parseCategories(const QByteArray &data,
                                       StreamType        type,
                                       const QString    &sourceId)
{
    QList<Category> list;
    const QJsonArray arr = QJsonDocument::fromJson(data).array();
    list.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        Category cat;
        cat.id         = o[u"category_id"].toVariant().toString();
        cat.name       = o[u"category_name"].toString(QStringLiteral("Unknown"));
        cat.streamType = type;
        cat.sourceId   = sourceId;
        list.append(cat);
    }
    return list;
}

void XtreamClient::getLiveCategories(std::function<void(QList<Category>, QString)> cb)
{
    const QString url = m_base + QStringLiteral("/player_api.php?") + m_auth
                        + QStringLiteral("&action=get_live_categories");
    get(url, [cb, this](QByteArray data, QString err) {
        if (!err.isEmpty()) { cb({}, err); return; }
        cb(parseCategories(data, StreamType::Live, m_source.id), {});
    });
}

void XtreamClient::getVodCategories(std::function<void(QList<Category>, QString)> cb)
{
    const QString url = m_base + QStringLiteral("/player_api.php?") + m_auth
                        + QStringLiteral("&action=get_vod_categories");
    get(url, [cb, this](QByteArray data, QString err) {
        if (!err.isEmpty()) { cb({}, err); return; }
        cb(parseCategories(data, StreamType::VOD, m_source.id), {});
    });
}

void XtreamClient::getSeriesCategories(std::function<void(QList<Category>, QString)> cb)
{
    const QString url = m_base + QStringLiteral("/player_api.php?") + m_auth
                        + QStringLiteral("&action=get_series_categories");
    get(url, [cb, this](QByteArray data, QString err) {
        if (!err.isEmpty()) { cb({}, err); return; }
        cb(parseCategories(data, StreamType::Series, m_source.id), {});
    });
}

// ─── Streams ────────────────────────────────────────────────────────────────

void XtreamClient::getLiveStreams(const QString &categoryId,
                                  std::function<void(QList<Channel>, QString)> cb)
{
    QUrlQuery q;
    q.setQuery(m_auth);
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("get_live_streams"));
    if (!categoryId.isEmpty())
        q.addQueryItem(QStringLiteral("category_id"), categoryId);
    const QString url = m_base + QStringLiteral("/player_api.php?") + q.toString(QUrl::FullyEncoded);

    get(url, [cb, this](QByteArray data, QString err) {
        if (!err.isEmpty()) { cb({}, err); return; }
        QList<Channel> list;
        const QJsonArray arr = QJsonDocument::fromJson(data).array();
        list.reserve(arr.size());
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const QString sid   = o[u"stream_id"].toVariant().toString();
            Channel ch;
            ch.id           = sid;
            ch.name         = o[u"name"].toString();
            ch.streamUrl    = m_base + QStringLiteral("/live/") + m_source.username
                              + QLatin1Char('/') + m_source.password
                              + QLatin1Char('/') + sid + QStringLiteral(".ts");
            ch.streamType   = StreamType::Live;
            ch.sourceId     = m_source.id;
            ch.categoryId   = o[u"category_id"].toVariant().toString();
            ch.logoUrl      = o[u"stream_icon"].toString();
            ch.epgChannelId = o[u"epg_channel_id"].toString();
            ch.num          = o[u"num"].toInt();
            list.append(ch);
        }
        cb(list, {});
    });
}

void XtreamClient::getVodStreams(const QString &categoryId,
                                 std::function<void(QList<Channel>, QString)> cb)
{
    QUrlQuery q;
    q.setQuery(m_auth);
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("get_vod_streams"));
    if (!categoryId.isEmpty())
        q.addQueryItem(QStringLiteral("category_id"), categoryId);
    const QString url = m_base + QStringLiteral("/player_api.php?") + q.toString(QUrl::FullyEncoded);

    get(url, [cb, this](QByteArray data, QString err) {
        if (!err.isEmpty()) { cb({}, err); return; }
        QList<Channel> list;
        const QJsonArray arr = QJsonDocument::fromJson(data).array();
        list.reserve(arr.size());
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const QString sid   = o[u"stream_id"].toVariant().toString();
            const QString ext   = o[u"container_extension"].toString(QStringLiteral("mp4"));
            Channel ch;
            ch.id          = sid;
            ch.name        = o[u"name"].toString();
            ch.streamUrl   = m_base + QStringLiteral("/movie/") + m_source.username
                             + QLatin1Char('/') + m_source.password
                             + QLatin1Char('/') + sid + QLatin1Char('.') + ext;
            ch.streamType  = StreamType::VOD;
            ch.sourceId    = m_source.id;
            ch.categoryId  = o[u"category_id"].toVariant().toString();
            ch.logoUrl     = o[u"stream_icon"].toString();
            ch.rating      = o[u"rating"].toVariant().toString();
            ch.year        = o[u"year"].toVariant().toString();
            ch.plot        = o[u"plot"].toString();
            ch.genre       = o[u"genre"].toString();
            ch.duration    = o[u"duration"].toVariant().toString();
            ch.director    = o[u"director"].toString();
            ch.cast        = o[u"cast"].toString();
            const QJsonValue bp = o[u"backdrop_path"];
            if (bp.isArray() && !bp.toArray().isEmpty())
                ch.backdropUrl = bp.toArray().first().toString();
            list.append(ch);
        }
        cb(list, {});
    });
}

void XtreamClient::getSeries(const QString &categoryId,
                              std::function<void(QList<Channel>, QString)> cb)
{
    QUrlQuery q;
    q.setQuery(m_auth);
    q.addQueryItem(QStringLiteral("action"), QStringLiteral("get_series"));
    if (!categoryId.isEmpty())
        q.addQueryItem(QStringLiteral("category_id"), categoryId);
    const QString url = m_base + QStringLiteral("/player_api.php?") + q.toString(QUrl::FullyEncoded);

    get(url, [cb, this](QByteArray data, QString err) {
        if (!err.isEmpty()) { cb({}, err); return; }
        QList<Channel> list;
        const QJsonArray arr = QJsonDocument::fromJson(data).array();
        list.reserve(arr.size());
        for (const QJsonValue &v : arr) {
            const QJsonObject o = v.toObject();
            const QString sid   = o[u"series_id"].toVariant().toString();
            Channel ch;
            ch.id         = sid;
            ch.name       = o[u"name"].toString();
            ch.streamUrl  = m_base + QStringLiteral("/series/") + m_source.username
                            + QLatin1Char('/') + m_source.password
                            + QLatin1Char('/') + sid;
            ch.streamType = StreamType::Series;
            ch.sourceId   = m_source.id;
            ch.categoryId = o[u"category_id"].toVariant().toString();
            ch.logoUrl    = o[u"cover"].toString();
            ch.rating     = o[u"rating"].toVariant().toString();
            ch.year       = o[u"releaseDate"].toString();
            ch.plot       = o[u"plot"].toString();
            ch.genre      = o[u"genre"].toString();
            ch.cast       = o[u"cast"].toString();
            ch.director   = o[u"director"].toString();
            list.append(ch);
        }
        cb(list, {});
    });
}

QString XtreamClient::epgUrl() const
{
    return m_base + QStringLiteral("/xmltv.php?") + m_auth;
}
