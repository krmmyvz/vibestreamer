#pragma once
#include "models.h"
#include <QList>
#include <QString>
#include <QMap>
#include <QSet>

class Config {
public:
    Config();
    void save();

    void addSource(Source source);
    void removeSource(const QString &sourceId);
    void updateSource(const Source &source);
    Source *getSource(const QString &sourceId);

    bool toggleFavorite(const QString &streamUrl);
    bool isFavorite(const QString &streamUrl) const;

    // Persisted state
    QList<Source> sources;
    QStringList   favorites;
    QString       lastSourceId;
    QString       lastChannelUrl;
    QString       lastCategoryId;
    int           lastStreamType = 0;  // 0=Live 1=VOD 2=Series
    QStringList   searchHistory;
    QMap<QString, QString> shortcuts;
    int           volume        = 100;
    bool          statePersistence = true;
    int           windowWidth   = 1280;
    int           windowHeight  = 720;
    bool          showEpg       = true;
    bool          minimizeToTray = false;
    QString       language      = QStringLiteral("tr"); // tr | en
    QString       recordPath;
    int           themeMode     = 0; // 0: Dark, 1: Light
    QString       accentColor   = QStringLiteral("#BB86FC");

    QString       mpvHwDecode   = QStringLiteral("auto-safe");
    QString       mpvExtraArgs;

private:
    void load();
    QString configFilePath() const;

    QSet<QString> m_favoriteSet;
};
