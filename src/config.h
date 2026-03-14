#pragma once
#include "models.h"
#include <QList>
#include <QString>

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
    int           volume        = 100;
    int           windowWidth   = 1280;
    int           windowHeight  = 720;
    bool          showEpg       = true;
    bool          minimizeToTray = false;
    QString       recordPath;
    int           themeMode     = 0; // 0: Dark, 1: Light
    QString       accentColor   = QStringLiteral("#BB86FC");

    // Default to software decoding ("no") to prevent crashes on systems with partial/broken drivers (e.g. missing CUDA/VDPAU)
    QString       mpvHwDecode   = QStringLiteral("no");
    QString       mpvExtraArgs;

private:
    void load();
    QString configFilePath() const;
};
