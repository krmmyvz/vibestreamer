#pragma once
#include <QString>
#include <QList>

enum class SourceType { Xtream, M3U };
enum class StreamType { Live, VOD, Series };

struct Source {
    QString id;
    QString name;
    SourceType sourceType = SourceType::Xtream;
    QString serverUrl;
    QString username;
    QString password;
    QString m3uUrl;
    QString epgUrl;
    int autoUpdateIntervalHours = 0;
    qint64 lastUpdated = 0; // Unix timestamp
};

struct Category {
    QString id;
    QString name;
    StreamType streamType = StreamType::Live;
    QString sourceId;
    int channelCount = 0;
};

struct Channel {
    QString id;
    QString name;
    QString streamUrl;
    StreamType streamType = StreamType::Live;
    QString sourceId;
    QString categoryId;
    QString categoryName;
    QString logoUrl;
    QString epgChannelId;
    int num = 0;
    bool isFavorite = false;
    // VOD / Series metadata
    QString rating;
    QString year;
    QString plot;
    QString genre;
    QString duration;
    QString director;
    QString cast;
    QString backdropUrl;
};

struct EpgProgram {
    QString channelId;
    QString title;
    QString description;
    QString category;
    qint64 startTs = 0;   // Unix timestamp
    qint64 stopTs  = 0;
};
