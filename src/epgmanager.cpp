#include "epgmanager.h"

#include <QDateTime>
#include <QTimeZone>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <QFutureWatcher>

#include <algorithm>

#include <zlib.h>

// ─── Gzip decompression ─────────────────────────────────────────────────────

static QByteArray gzipDecompress(const QByteArray &data)
{
    if (data.size() < 2)
        return data;

    // Check gzip magic bytes
    if (static_cast<unsigned char>(data[0]) != 0x1f ||
        static_cast<unsigned char>(data[1]) != 0x8b)
        return data;  // not gzip, return as-is

    z_stream strm{};
    strm.next_in  = reinterpret_cast<Bytef *>(const_cast<char *>(data.data()));
    strm.avail_in = static_cast<uInt>(data.size());

    // 15 + 32 = auto-detect gzip/zlib wrapper
    if (inflateInit2(&strm, 15 + 32) != Z_OK)
        return data;

    QByteArray out;
    out.reserve(data.size() * 4);
    char buf[32768];
    int ret;
    do {
        strm.next_out  = reinterpret_cast<Bytef *>(buf);
        strm.avail_out = sizeof(buf);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret == Z_MEM_ERROR || ret == Z_DATA_ERROR) break;
        out.append(buf, static_cast<int>(sizeof(buf) - strm.avail_out));
    } while (ret == Z_OK);

    inflateEnd(&strm);
    return (ret == Z_STREAM_END) ? out : data;
}

// ─── EpgManager ──────────────────────────────────────────────────────────────

EpgManager::EpgManager(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{}

void EpgManager::load(const QString &urlsStr)
{
    if (urlsStr.isEmpty()) return;

    const int generation = ++m_loadGeneration;

    const QStringList urls = urlsStr.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (urls.isEmpty()) return;
    
    // We don't clear m_data instantly, so the UI can still show old EPG while loading
    m_pendingResults.clear();
    m_pendingJobs = urls.size();

    for (const QString &u : urls) {
        const QString url = u.trimmed();
        if (url.isEmpty()) {
            if (--m_pendingJobs == 0) emit loaded();
            continue;
        }

        // Reject non-http(s) schemes (file://, ftp://, etc.)
        const bool safeScheme = url.startsWith(QStringLiteral("http://"),  Qt::CaseInsensitive)
                             || url.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
        if (!safeScheme) {
            if (--m_pendingJobs == 0) emit loaded();
            continue;
        }

        QNetworkRequest req{QUrl(url)};
        req.setRawHeader("User-Agent", "Vibestreamer/2.0");
        req.setRawHeader("Accept-Encoding", "gzip, deflate");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(60000);

        QNetworkReply *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
            if (generation != m_loadGeneration) {
                reply->deleteLater();
                return;
            }

            if (reply->error() != QNetworkReply::NoError) {
                reply->deleteLater();
                if (--m_pendingJobs == 0) emit loaded();
                return;
            }
            constexpr qint64 kMaxEpgBytes = 200LL * 1024 * 1024; // 200 MB
            QByteArray raw = reply->read(kMaxEpgBytes + 1);
            reply->deleteLater();
            if (raw.size() > kMaxEpgBytes) {
                if (--m_pendingJobs == 0) emit loaded();
                return;
            }

            // Perform decompression AND parsing in a background thread
            auto *watcher = new QFutureWatcher<ParseResult>(this);
            connect(watcher, &QFutureWatcher<ParseResult>::finished, this, [this, watcher, generation]() {
                if (generation != m_loadGeneration) {
                    watcher->deleteLater();
                    return;
                }

                m_pendingResults.append(watcher->result());
                watcher->deleteLater();
                
                if (--m_pendingJobs == 0) {
                    // Start a final background job to merge and sort everything
                    auto *mergeWatcher = new QFutureWatcher<EpgState>(this);
                    
                    QList<ParseResult> resultsToMerge = m_pendingResults;
                    m_pendingResults.clear();
                    
                    connect(mergeWatcher, &QFutureWatcher<EpgState>::finished, this, [this, mergeWatcher, generation]() {
                        if (generation == m_loadGeneration) {
                            EpgState finalState = mergeWatcher->result();
                            m_data = finalState.data;
                            m_nameToId = finalState.nameToId;
                            m_channelIdByLower = finalState.channelIdByLower;
                            emit loaded();
                        }
                        mergeWatcher->deleteLater();
                    });
                    
                    mergeWatcher->setFuture(QtConcurrent::run([resultsToMerge]() {
                        EpgState state;
                        // Merge all partial results
                        for (const auto &res : resultsToMerge) {
                            for (auto it = res.data.constBegin(); it != res.data.constEnd(); ++it) {
                                state.data[it.key()].append(it.value());
                            }
                            for (auto it = res.nameToId.constBegin(); it != res.nameToId.constEnd(); ++it) {
                                state.nameToId.insert(it.key(), it.value());
                            }
                        }
                        
                        // Sort programs and build lower-case index
                        for (auto it = state.data.begin(); it != state.data.end(); ++it) {
                            auto &programs = it.value();
                            std::sort(programs.begin(), programs.end(), [](const EpgProgram &a, const EpgProgram &b) {
                                return a.startTs < b.startTs;
                            });
                            state.channelIdByLower.insert(it.key().toLower(), it.key());
                        }
                        
                        return state;
                    }));
                }
            });
            
            // Run decompression and parsing in the background
            watcher->setFuture(QtConcurrent::run([raw]() {
                QByteArray xml = gzipDecompress(raw);
                return parseXmltv(xml);
            }));
        });
    }
}

// Parse an XMLTV timestamp: "20240101120000 +0000" → Unix
static qint64 parseXmltvTime(const QString &s)
{
    // Format: YYYYMMDDHHmmss ±HHMM  (space or no space before tz)
    QString clean = s.trimmed();
    if (clean.length() < 14) return 0;
    QString dt = clean.left(14);  // "20240101120000"

    QDateTime parsed = QDateTime::fromString(dt, QStringLiteral("yyyyMMddHHmmss"));
    if (!parsed.isValid()) return 0;
    parsed.setTimeZone(QTimeZone::utc());

    // Find timezone offset: skip the 14 digits and any whitespace
    QString remainder = clean.mid(14).trimmed();
    if (!remainder.isEmpty() && remainder != QLatin1String("+0000")) {
        int sign = (remainder[0] == QLatin1Char('-')) ? -1 : 1;
        QString digits = remainder.mid(1);
        int hh = digits.mid(0, 2).toInt();
        int mm = digits.mid(2, 2).toInt();
        parsed = parsed.addSecs(-sign * (hh * 3600 + mm * 60));
    }

    return parsed.toSecsSinceEpoch();
}

EpgManager::ParseResult EpgManager::parseXmltv(const QByteArray &data)
{
    ParseResult result;

    QXmlStreamReader xml(data);
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement()) continue;

        if (xml.name() == QLatin1String("channel")) {
            const QString chId = xml.attributes().value(QStringLiteral("id")).toString();
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("channel"))
                    break;
                if (xml.isStartElement() && xml.name() == QLatin1String("display-name")) {
                    const QString name = xml.readElementText().trimmed();
                    if (!name.isEmpty() && !chId.isEmpty()) {
                        result.nameToId.insert(name.toLower(), chId);
                        // Also store without spaces/dots as alias
                        QString simplified = name.toLower().remove(QLatin1Char(' ')).remove(QLatin1Char('.'));
                        result.nameToId.insert(simplified, chId);
                    }
                }
            }
        } else if (xml.name() == QLatin1String("programme")) {
            EpgProgram prog;
            prog.channelId = xml.attributes().value(QStringLiteral("channel")).toString();
            prog.startTs   = parseXmltvTime(
                xml.attributes().value(QStringLiteral("start")).toString());
            prog.stopTs    = parseXmltvTime(
                xml.attributes().value(QStringLiteral("stop")).toString());

            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isEndElement() && xml.name() == QLatin1String("programme"))
                    break;
                if (!xml.isStartElement()) continue;

                if (xml.name() == QLatin1String("title"))
                    prog.title = xml.readElementText();
                else if (xml.name() == QLatin1String("desc"))
                    prog.description = xml.readElementText();
                else if (xml.name() == QLatin1String("category"))
                    prog.category = xml.readElementText();
            }

            if (!prog.channelId.isEmpty())
                result.data[prog.channelId].append(prog);
        }
    }

    return result;
}

QString EpgManager::resolveChannelId(const QString &epgChannelId) const
{
    if (epgChannelId.isEmpty()) return {};

    // 1. Exact match
    if (m_data.contains(epgChannelId))
        return epgChannelId;

    // 2. Case-insensitive match on data keys
    const QString lower = epgChannelId.toLower();
    if (m_channelIdByLower.contains(lower))
        return m_channelIdByLower.value(lower);

    // 3. Match by display name
    if (m_nameToId.contains(lower))
        return m_nameToId.value(lower);

    // 4. Simplified name match (no spaces/dots)
    const QString simplified = lower.simplified().remove(QLatin1Char(' ')).remove(QLatin1Char('.'));
    if (m_nameToId.contains(simplified))
        return m_nameToId.value(simplified);

    return epgChannelId; // fall back to original
}

EpgProgram EpgManager::currentProgram(const QString &channelId) const
{
    const QString resolved = resolveChannelId(channelId);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QList<EpgProgram> &list = m_data.value(resolved);
    if (list.isEmpty())
        return {};

    auto it = std::upper_bound(list.begin(), list.end(), now, [](qint64 value, const EpgProgram &program) {
        return value < program.startTs;
    });

    if (it != list.begin()) {
        const EpgProgram &candidate = *std::prev(it);
        if (candidate.startTs <= now && now < candidate.stopTs)
            return candidate;
    }

    if (it != list.end() && it->startTs <= now && now < it->stopTs)
        return *it;

    return {};
}

EpgProgram EpgManager::nextProgram(const QString &channelId) const
{
    const QString resolved = resolveChannelId(channelId);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const QList<EpgProgram> &list = m_data.value(resolved);
    if (list.isEmpty())
        return {};

    auto it = std::upper_bound(list.begin(), list.end(), now, [](qint64 value, const EpgProgram &program) {
        return value < program.startTs;
    });

    return it == list.end() ? EpgProgram{} : *it;
}

QList<EpgProgram> EpgManager::programs(const QString &channelId) const
{
    return m_data.value(resolveChannelId(channelId));
}
