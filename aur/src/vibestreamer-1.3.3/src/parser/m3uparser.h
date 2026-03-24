#pragma once
#include "models.h"
#include <QList>
#include <QString>

class M3UParser {
public:
    struct Result {
        QList<Channel> channels;
        QString        epgUrl;
    };

    // Parse from raw text (already downloaded)
    static Result parse(const QString &text, const QString &sourceId);
};
