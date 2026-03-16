#include "config.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

Config::Config()
{
    shortcuts.insert(QStringLiteral("play_pause"), QStringLiteral("Space"));
    shortcuts.insert(QStringLiteral("fullscreen"), QStringLiteral("F11"));
    shortcuts.insert(QStringLiteral("mute"),       QStringLiteral("M"));
    shortcuts.insert(QStringLiteral("vol_up"),     QStringLiteral("Up"));
    shortcuts.insert(QStringLiteral("vol_down"),   QStringLiteral("Down"));
    shortcuts.insert(QStringLiteral("ch_next"),    QStringLiteral("N"));
    shortcuts.insert(QStringLiteral("ch_prev"),    QStringLiteral("P"));
    shortcuts.insert(QStringLiteral("info"),       QStringLiteral("I"));
    shortcuts.insert(QStringLiteral("audio"),      QStringLiteral("A"));
    shortcuts.insert(QStringLiteral("subs"),       QStringLiteral("S"));
    shortcuts.insert(QStringLiteral("favorite"),   QStringLiteral("F"));

    load();
}

QString Config::configFilePath() const
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/config.json");
}

void Config::load()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();

    for (const QJsonValue &v : root[u"sources"].toArray()) {
        const QJsonObject s = v.toObject();
        Source src;
        src.id         = s[u"id"].toString();
        src.name       = s[u"name"].toString();
        src.sourceType = s[u"source_type"].toString() == QLatin1String("m3u")
                             ? SourceType::M3U
                             : SourceType::Xtream;
        src.serverUrl  = s[u"server_url"].toString();
        src.username   = s[u"username"].toString();
        src.password   = s[u"password"].toString();
        src.m3uUrl     = s[u"m3u_url"].toString();
        src.epgUrl     = s[u"epg_url"].toString();
        src.autoUpdateIntervalHours = s[u"auto_update_interval_hours"].toInt(0);
        src.lastUpdated = s[u"last_updated"].toInteger(0);
        sources.append(src);
    }

    for (const QJsonValue &v : root[u"favorites"].toArray())
        favorites.append(v.toString());

    m_favoriteSet = QSet<QString>(favorites.begin(), favorites.end());

    for (const QJsonValue &v : root[u"search_history"].toArray())
        searchHistory.append(v.toString());

    if (root.contains(u"shortcuts")) {
        const QJsonObject shObj = root[u"shortcuts"].toObject();
        for (auto it = shObj.constBegin(); it != shObj.constEnd(); ++it) {
            shortcuts.insert(it.key(), it.value().toString());
        }
    }

    lastSourceId    = root[u"last_source_id"].toString();
    lastChannelUrl  = root[u"last_channel_url"].toString();
    lastCategoryId  = root[u"last_category_id"].toString();
    lastStreamType  = root[u"last_stream_type"].toInt(0);
    volume          = root[u"volume"].toInt(100);
    statePersistence= root[u"state_persistence"].toBool(true);
    windowWidth     = root[u"window_width"].toInt(1280);
    windowHeight    = root[u"window_height"].toInt(720);
    showEpg         = root[u"show_epg"].toBool(true);
    minimizeToTray  = root[u"minimize_to_tray"].toBool(false);
    language        = root[u"language"].toString(QStringLiteral("en"));
    recordPath      = root[u"record_path"].toString();
    themeMode       = root[u"theme_mode"].toInt(0);
    accentColor     = root[u"accent_color"].toString(QStringLiteral("#BB86FC"));
    mpvHwDecode     = root[u"mpv_hw_decode"].toString(QStringLiteral("auto-safe"));
    mpvExtraArgs    = root[u"mpv_extra_args"].toString();
}

void Config::save()
{
    QJsonArray srcArr;
    for (const Source &s : sources) {
        QJsonObject o;
        o[u"id"]          = s.id;
        o[u"name"]        = s.name;
        o[u"source_type"] = s.sourceType == SourceType::M3U
                                ? QLatin1String("m3u")
                                : QLatin1String("xtream");
        o[u"server_url"]  = s.serverUrl;
        o[u"username"]    = s.username;
        o[u"password"]    = s.password;
        o[u"m3u_url"]     = s.m3uUrl;
        o[u"epg_url"]     = s.epgUrl;
        o[u"auto_update_interval_hours"] = s.autoUpdateIntervalHours;
        o[u"last_updated"] = s.lastUpdated;
        srcArr.append(o);
    }

    QJsonArray favArr;
    for (const QString &f : favorites)
        favArr.append(f);

    QJsonArray histArr;
    for (const QString &s : searchHistory)
        histArr.append(s);

    QJsonObject shObj;
    for (auto it = shortcuts.constBegin(); it != shortcuts.constEnd(); ++it) {
        shObj[it.key()] = it.value();
    }

    QJsonObject root;
    root[u"sources"]          = srcArr;
    root[u"favorites"]        = favArr;
    root[u"search_history"]   = histArr;
    root[u"shortcuts"]        = shObj;
    root[u"last_source_id"]   = lastSourceId;
    root[u"last_channel_url"] = lastChannelUrl;
    root[u"last_category_id"] = lastCategoryId;
    root[u"last_stream_type"] = lastStreamType;
    root[u"volume"]           = volume;
    root[u"state_persistence"] = statePersistence;
    root[u"window_width"]     = windowWidth;
    root[u"window_height"]    = windowHeight;
    root[u"show_epg"]         = showEpg;
    root[u"minimize_to_tray"] = minimizeToTray;
    root[u"language"]         = language;
    root[u"record_path"]      = recordPath;
    root[u"theme_mode"]       = themeMode;
    root[u"accent_color"]     = accentColor;
    root[u"mpv_hw_decode"]    = mpvHwDecode;
    root[u"mpv_extra_args"]   = mpvExtraArgs;

    const QString path    = configFilePath();
    const QString tmpPath = path + QStringLiteral(".tmp");
    {
        QFile f(tmpPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return;
        f.write(QJsonDocument(root).toJson());
        f.flush();
    }
    QFile::remove(path);
    QFile::rename(tmpPath, path);
}

void Config::addSource(Source source)
{
    if (source.id.isEmpty())
        source.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    sources.append(source);
    save();
}

void Config::removeSource(const QString &sourceId)
{
    sources.removeIf([&](const Source &s){ return s.id == sourceId; });
    save();
}

void Config::updateSource(const Source &source)
{
    for (Source &s : sources) {
        if (s.id == source.id) { s = source; break; }
    }
    save();
}

Source *Config::getSource(const QString &sourceId)
{
    for (Source &s : sources)
        if (s.id == sourceId) return &s;
    return nullptr;
}

bool Config::toggleFavorite(const QString &streamUrl)
{
    if (m_favoriteSet.contains(streamUrl)) {
        m_favoriteSet.remove(streamUrl);
        favorites.removeAll(streamUrl);
        save();
        return false;
    }
    m_favoriteSet.insert(streamUrl);
    favorites.append(streamUrl);
    save();
    return true;
}

bool Config::isFavorite(const QString &streamUrl) const
{
    return m_favoriteSet.contains(streamUrl);
}
