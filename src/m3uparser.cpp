#include "m3uparser.h"

#include <QStringView>
#include <QLatin1String>

// Extract a named attribute value from an #EXTINF line using QStringView to prevent copies
static QStringView attr(QStringView line, QLatin1String key)
{
    int start = line.indexOf(key);
    while (start >= 0) {
        int afterKey = start + key.size();
        if (afterKey + 1 < line.size() && line[afterKey] == QLatin1Char('=') && line[afterKey + 1] == QLatin1Char('"')) {
            int valueStart = afterKey + 2;
            int valueEnd = line.indexOf(QLatin1Char('"'), valueStart);
            if (valueEnd > valueStart) {
                return line.mid(valueStart, valueEnd - valueStart);
            }
            break;
        }
        start = line.indexOf(key, start + 1);
    }
    return {};
}

M3UParser::Result M3UParser::parse(const QString &text, const QString &sourceId)
{
    Result res;
    // Pre-allocate memory to avoid multiple reallocations during large playlist parsing
    res.channels.reserve(10000); 

    Channel pending;
    bool hasPending = false;
    int  idx        = 0;

    QStringView textV(text);
    int start = 0;
    const int length = textV.size();
    
    while (start < length) {
        int end = textV.indexOf(QLatin1Char('\n'), start);
        if (end < 0)
            end = length;

        // Extract a view of the line without allocating a new QString
        QStringView line = textV.mid(start, end - start).trimmed();
        start = end + 1;

        if (line.isEmpty()) continue;

        if (line.startsWith(QLatin1String("#EXTM3U"))) {
            res.epgUrl = attr(line, QLatin1String("url-tvg")).toString();
            if (res.epgUrl.isEmpty())
                res.epgUrl = attr(line, QLatin1String("x-tvg-url")).toString();
            continue;
        }

        if (line.startsWith(QLatin1String("#EXTINF:"))) {
            pending = Channel{};
            hasPending = true;

            // Duration (between #EXTINF: and first comma)
            const int commaPos = line.indexOf(QLatin1Char(','));
            if (commaPos != -1) {
                pending.name = line.mid(commaPos + 1).trimmed().toString();
            }

            pending.sourceId     = sourceId;
            pending.id           = QString::number(++idx);
            pending.logoUrl      = attr(line, QLatin1String("tvg-logo")).toString();
            pending.epgChannelId = attr(line, QLatin1String("tvg-id")).toString();
            pending.categoryName = attr(line, QLatin1String("group-title")).toString();

            // Prefer tvg-name if present
            const QStringView tvgName = attr(line, QLatin1String("tvg-name"));
            if (!tvgName.isEmpty())
                pending.name = tvgName.toString();

            // Determine stream type from group-title heuristic without allocating a new lowercase string
            if (pending.categoryName.contains(QLatin1String("movie"), Qt::CaseInsensitive) || 
                pending.categoryName.contains(QLatin1String("film"), Qt::CaseInsensitive) ||
                pending.categoryName.contains(QLatin1String("vod"), Qt::CaseInsensitive)) {
                pending.streamType = StreamType::VOD;
            } else if (pending.categoryName.contains(QLatin1String("series"), Qt::CaseInsensitive) || 
                       pending.categoryName.contains(QLatin1String("dizi"), Qt::CaseInsensitive)) {
                pending.streamType = StreamType::Series;
            } else {
                pending.streamType = StreamType::Live;
            }

        } else if (hasPending && !line.startsWith(QLatin1Char('#'))) {
            pending.streamUrl = line.toString(); // Resolve the URL into a string
            res.channels.append(pending);        // Add to the reserved list
            hasPending = false;
        }
    }

    return res;
}
