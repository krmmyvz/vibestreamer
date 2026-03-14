#include "m3uparser.h"

#include <QRegularExpression>
#include <QStringList>

// Extract a named attribute value from an #EXTINF line
static QString attr(const QString &line, const QString &key)
{
    const QRegularExpression re(key + QStringLiteral(R"(="([^"]*"))"));
    const auto m = re.match(line);
    return m.hasMatch() ? m.captured(1) : QString{};
}

M3UParser::Result M3UParser::parse(const QString &text, const QString &sourceId)
{
    Result res;
    const QStringList lines = text.split(QLatin1Char('\n'));

    Channel pending;
    bool hasPending = false;
    int  idx        = 0;

    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QStringLiteral("#EXTM3U"))) {
            res.epgUrl = attr(line, QStringLiteral("url-tvg"));
            if (res.epgUrl.isEmpty())
                res.epgUrl = attr(line, QStringLiteral("x-tvg-url"));
            continue;
        }

        if (line.startsWith(QStringLiteral("#EXTINF:"))) {
            pending = Channel{};
            hasPending = true;

            // Duration (between #EXTINF: and first comma)
            const int commaPos = line.indexOf(QLatin1Char(','));
            if (commaPos != -1)
                pending.name = line.mid(commaPos + 1).trimmed();

            pending.sourceId     = sourceId;
            pending.id           = QString::number(++idx);
            pending.logoUrl      = attr(line, QStringLiteral("tvg-logo"));
            pending.epgChannelId = attr(line, QStringLiteral("tvg-id"));
            pending.categoryName = attr(line, QStringLiteral("group-title"));

            // Prefer tvg-name if present
            const QString tvgName = attr(line, QStringLiteral("tvg-name"));
            if (!tvgName.isEmpty())
                pending.name = tvgName;

            // Determine stream type from group-title heuristic
            const QString grp = pending.categoryName.toLower();
            if (grp.contains(QLatin1String("movie")) || grp.contains(QLatin1String("film"))
                || grp.contains(QLatin1String("vod")))
                pending.streamType = StreamType::VOD;
            else if (grp.contains(QLatin1String("series")) || grp.contains(QLatin1String("dizi")))
                pending.streamType = StreamType::Series;
            else
                pending.streamType = StreamType::Live;

        } else if (hasPending && !line.startsWith(QLatin1Char('#'))) {
            pending.streamUrl = line;
            res.channels.append(pending);
            hasPending = false;
        }
    }

    return res;
}
