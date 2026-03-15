#pragma once
#include "models.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

class EpgManager : public QObject {
    Q_OBJECT
public:
    explicit EpgManager(QObject *parent = nullptr);

    void load(const QString &url);

    // Returns current and next programme for a given epg channel id
    // Tries exact channelId match first, then falls back to display name
    EpgProgram currentProgram(const QString &channelId) const;
    EpgProgram nextProgram(const QString &channelId) const;
    QList<EpgProgram> programs(const QString &channelId) const;

    // Resolve an epg_channel_id to the actual XMLTV channel id
    QString resolveChannelId(const QString &epgChannelId) const;

signals:
    void loaded();
    void loadError(const QString &message);

private:
    // Result struct for background parsing
    struct ParseResult {
        QHash<QString, QList<EpgProgram>> data;
        QHash<QString, QString>           nameToId;
    };

    static ParseResult parseXmltv(const QByteArray &data);

    QNetworkAccessManager              *m_nam;
    QHash<QString, QList<EpgProgram>>   m_data;         // channelId → programs
    QHash<QString, QString>             m_nameToId;     // display-name (lower) → channelId
    QHash<QString, QString>             m_channelIdByLower;
    int                                 m_pendingJobs = 0;
    int                                 m_loadGeneration = 0;
};
