#include "mainwindow.h"
#include "devstats.h"
#include "mpvwidget.h"
#include "multiviewwidget.h"
#include "sourcedialog.h"
#include "settingsdialog.h"
#include "xtreamclient.h"
#include "m3uparser.h"
#include "imagecache.h"
#include "localization.h"

#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>

// ─── Security helpers (file-local copies for use in this translation unit) ───

static bool isSafeMpvProperty(const QString &name)
{
    static const QSet<QString> kAllowed = {
        QStringLiteral("hwdec"),
        QStringLiteral("deband"),
        QStringLiteral("scale"),
        QStringLiteral("cscale"),
        QStringLiteral("dscale"),
        QStringLiteral("dither-depth"),
        QStringLiteral("correct-downscaling"),
        QStringLiteral("linear-downscaling"),
        QStringLiteral("sigmoid-upscaling"),
        QStringLiteral("interpolation"),
        QStringLiteral("video-sync"),
        QStringLiteral("framedrop"),
        QStringLiteral("audio-channels"),
        QStringLiteral("demuxer-max-bytes"),
        QStringLiteral("cache"),
        QStringLiteral("cache-secs"),
        QStringLiteral("network-timeout"),
        QStringLiteral("volume-max"),
        QStringLiteral("af"),
        QStringLiteral("vf"),
        QStringLiteral("deinterlace"),
        QStringLiteral("blend-subtitles"),
        QStringLiteral("sub-scale"),
        QStringLiteral("sub-bold"),
        QStringLiteral("sub-font"),
        QStringLiteral("sub-color"),
    };
    return kAllowed.contains(name);
}

static bool isSafeUrl(const QString &url)
{
    const QString s = url.trimmed();
    return s.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)
        || s.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive);
}

// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::loadSourceList()
{
    m_sourceCombo->blockSignals(true);
    m_sourceCombo->clear();
    for (const Source &s : m_config.sources)
        m_sourceCombo->addItem(s.name, s.id);
    m_sourceCombo->blockSignals(false);

    // Restore last stream type tab before loading source (so categories load for the right type)
    if (m_config.statePersistence && m_config.lastStreamType >= 0 && m_config.lastStreamType < m_typeTab->count()) {
        m_typeTab->blockSignals(true);
        m_typeTab->setCurrentIndex(m_config.lastStreamType);
        m_streamType = static_cast<StreamType>(m_config.lastStreamType);
        m_typeTab->blockSignals(false);
    }

    // Restore last selected source
    int idx = m_config.statePersistence ? m_sourceCombo->findData(m_config.lastSourceId) : -1;
    if (idx < 0 && m_sourceCombo->count() > 0) idx = 0;
    if (idx >= 0) {
        // If index is already the same (e.g. 0 after clear+add), Qt won't emit
        // currentIndexChanged, so call onSourceChanged explicitly.
        if (m_sourceCombo->currentIndex() == idx)
            onSourceChanged(idx);
        else
            m_sourceCombo->setCurrentIndex(idx);
    } else {
        onSourceChanged(-1);
    }
}

void MainWindow::invalidateCache()
{
    m_categoryCache.clear();
    m_channelCache.clear();
    m_m3uCache.clear();
}

void MainWindow::onSourceChanged(int index)
{
    // Abort any in-flight M3U download to prevent stale callbacks and wasted bandwidth
    if (m_m3uNam) {
        m_m3uNam->deleteLater();
        m_m3uNam = nullptr;
    }

    if(m_catModel) m_catModel->clear();
    if(m_chanModel) m_chanModel->clear();
    m_allChannels.clear();
    m_loadingChannels = false;
    invalidateCache();

    if (index < 0 || index >= m_config.sources.size()) {
        delete m_client; m_client = nullptr;
        return;
    }

    const Source &src = m_config.sources[index];
    m_config.lastSourceId = src.id;

    if (src.sourceType == SourceType::Xtream) {
        delete m_client;
        m_client = new XtreamClient(src, this);

        if (!src.epgUrl.isEmpty())
            m_epg->load(src.epgUrl);
        else if (!src.serverUrl.isEmpty())
            m_epg->load(m_client->epgUrl());
    } else {
        delete m_client;
        m_client = nullptr;

        if (!src.epgUrl.isEmpty())
            m_epg->load(src.epgUrl);
    }

    loadCategories(src.id, m_streamType);
}

void MainWindow::onAddSource()
{
    SourceDialog dlg(m_config.language, this);
    if (dlg.exec() == QDialog::Accepted) {
        Source src = dlg.source();
        m_config.addSource(src);
        m_config.lastSourceId = src.id;  // So loadSourceList selects the new source
        loadSourceList();
    }
}

void MainWindow::onEditSource()
{
    const int idx = m_sourceCombo->currentIndex();
    if (idx < 0 || idx >= m_config.sources.size()) return;
    SourceDialog dlg(m_config.sources[idx], m_config.language, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_config.updateSource(dlg.source());
        const QString id = m_sourceCombo->currentData().toString();
        loadSourceList();
        m_sourceCombo->setCurrentIndex(m_sourceCombo->findData(id));
    }
}

void MainWindow::onRemoveSource()
{
    const int idx = m_sourceCombo->currentIndex();
    if (idx < 0 || idx >= m_config.sources.size()) return;
    if (QMessageBox::question(this, t(QStringLiteral("delete_title")),
            t(QStringLiteral("delete_confirm")))
        != QMessageBox::Yes) return;
    m_config.removeSource(m_config.sources[idx].id);
    loadSourceList();
}

void MainWindow::onOpenSettings()
{
    const QString previousLanguage = m_config.language;
    SettingsDialog dlg(m_config, this);
    if (dlg.exec() == QDialog::Accepted) {
        dlg.applyTo(m_config);
        applyTheme();
        if (previousLanguage != m_config.language) {
            I18n::instance().setLanguage(m_config.language);
            retranslateUi();
            statusBar()->showMessage(
                t(QStringLiteral("language_updated")),
                2500
            );
        }
        if (m_trayIcon) {
            m_trayIcon->setVisible(m_config.minimizeToTray);
        }
    }
}

void MainWindow::onStreamTypeChanged(int tab)
{
    m_streamType = static_cast<StreamType>(tab);  // 0=Live 1=VOD 2=Series
    m_config.lastStreamType = tab;
    const int srcIdx = m_sourceCombo->currentIndex();
    if (srcIdx < 0 || srcIdx >= m_config.sources.size()) return;
    loadCategories(m_config.sources[srcIdx].id, m_streamType);
}

void MainWindow::loadCategories(const QString &sourceId, StreamType type)
{
    if(m_catModel) m_catModel->clear();
    if(m_chanModel) m_chanModel->clear();
    m_allChannels.clear();

    const Source *src = m_config.getSource(sourceId);
    if (!src) return;

    // Helper to populate category list from a QList<Category>
    auto populateCats = [this](const QList<Category> &cats) {
        m_categories = cats;
        m_catModel->clear();
        QList<QStandardItem*> items;
        auto *favItem = new QStandardItem(t(QStringLiteral("favorites")));
        favItem->setData(QStringLiteral("__FAVORITES__"), Qt::UserRole);
        favItem->setForeground(QBrush(QColor(m_theme.warning)));
        items.append(favItem);

        auto *allItem = new QStandardItem(t(QStringLiteral("all")));
        allItem->setData(QString{}, Qt::UserRole);
        items.append(allItem);

        for (const Category &c : cats) {
            auto *item = new QStandardItem(c.name);
            item->setData(c.id, Qt::UserRole);
            items.append(item);
        }
        m_catModel->appendColumn(items);
        if (m_catModel->rowCount() > 1) {
            int restoreIdx = 1; // default to "All"
            if (m_config.statePersistence && !m_config.lastCategoryId.isEmpty()) {
                for (int r = 0; r < m_catModel->rowCount(); ++r) {
                    if (m_catModel->item(r)->data(Qt::UserRole).toString() == m_config.lastCategoryId) {
                        restoreIdx = r;
                        break;
                    }
                }
            }
            m_categoryList->setCurrentIndex(m_catModel->index(restoreIdx, 0));
            loadChannels(m_catModel->item(restoreIdx)->data(Qt::UserRole).toString());
        }
    };

    if (src->sourceType == SourceType::M3U) {
        if (m_m3uCache.contains(src->id)) {
            // Extract categories from cached M3U
            QSet<QString> uniqueCats;
            const auto &channels = m_m3uCache.value(src->id);
            for (const Channel &ch : channels) {
                if (ch.streamType == type && !ch.categoryName.isEmpty())
                    uniqueCats.insert(ch.categoryName);
            }
            QList<Category> catList;
            for (const QString &cName : uniqueCats)
                catList.append({cName, cName}); // ID = Name for M3U

            // Sort categories alphabetically
            std::sort(catList.begin(), catList.end(), [](const Category &a, const Category &b){
                return a.name < b.name;
            });

            populateCats(catList);
        } else {
            // Not loaded yet, just add Favorites/All and trigger load
            populateCats({});
        }
        return;
    }

    if (!m_client) return;

    const CacheKey key{sourceId, type, {}};
    if (m_categoryCache.contains(key)) {
        populateCats(m_categoryCache.value(key));
        return;
    }

    statusBar()->showMessage(t(QStringLiteral("loading_categories")));
    m_loadingProgress->setVisible(true);
    m_loadingProgress->setRange(0, 0); // indeterminate

    auto onDone = [this, populateCats, key](QList<Category> cats, QString err) {
        statusBar()->clearMessage();
        m_loadingProgress->setVisible(false);
        if (!err.isEmpty()) {
            statusBar()->showMessage(t(QStringLiteral("category_load_error")) + err, 6000);
            return;
        }
        if (m_categoryCache.size() >= 50) m_categoryCache.clear();
        m_categoryCache.insert(key, cats);
        DEV_STAT("category_cache_keys", m_categoryCache.size());
        populateCats(cats);
    };

    switch (type) {
    case StreamType::Live:   m_client->getLiveCategories(onDone);   break;
    case StreamType::VOD:    m_client->getVodCategories(onDone);    break;
    case StreamType::Series: m_client->getSeriesCategories(onDone); break;
    }
}

void MainWindow::onCategorySelected(const QModelIndex &item)
{
    if (!item.isValid()) return;
    const QString catId = item.data(Qt::UserRole).toString();
    m_config.lastCategoryId = catId;
    loadChannels(catId);
}

void MainWindow::loadChannels(const QString &categoryId)
{
    if(m_chanModel) m_chanModel->clear();
    m_allChannels.clear();

    const int srcIdx = m_sourceCombo->currentIndex();
    if (srcIdx < 0 || srcIdx >= m_config.sources.size()) return;
    const Source &src = m_config.sources[srcIdx];

    // Helper to finalize channels after loading (sort + mark favorites + filter)
    auto finalize = [this](QList<Channel> channels) {
        // Sort: by channel number first (if present), then alphabetically
        std::sort(channels.begin(), channels.end(), [](const Channel &a, const Channel &b) {
            if (a.num > 0 && b.num > 0) return a.num < b.num;
            if (a.num > 0) return true;
            if (b.num > 0) return false;
            return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
        });
        m_allChannels = std::move(channels);
        for (Channel &ch : m_allChannels)
            ch.isFavorite = m_config.isFavorite(ch.streamUrl);
        populateChannelList(m_allChannels);
        applySearchFilter();
        m_loadingChannels = false;
        m_loadingProgress->setVisible(false);
        statusBar()->clearMessage();

        // Resume last channel on first load after startup
        // Deferred via single-shot timer so the GUI can finish painting first
        if (m_config.statePersistence && m_resumePending && !m_config.lastChannelId.isEmpty()) {
            m_resumePending = false;
            const QString resumeId       = m_config.lastChannelId;
            const QString resumeSourceId = m_config.lastChannelSourceId;
            QTimer::singleShot(1000, this, [this, resumeId, resumeSourceId]() {
                for (int i = 0; i < m_allChannels.size(); ++i) {
                    if (m_allChannels[i].id == resumeId && m_allChannels[i].sourceId == resumeSourceId) {
                        QModelIndex sourceIdx = m_chanModel->index(i, 0);
                        QModelIndex proxyIdx = m_proxyModel->mapFromSource(sourceIdx);
                        if (proxyIdx.isValid()) {
                            m_channelList->setCurrentIndex(proxyIdx);
                        }
                        m_currentChannel = m_allChannels[i];
                        updateFavoriteButton(m_currentChannel);
                        if (m_currentChannel.streamType == StreamType::Live)
                            updateEpgPanel(m_currentChannel);
                        else
                            updateInfoPanel(m_currentChannel);
                        playChannel(m_currentChannel);
                        break;
                    }
                }
            });
        }
    };

    // ── M3U source ──────────────────────────────────────────────────
    if (src.sourceType == SourceType::M3U) {
        // Check M3U cache — the full channel list (all types) is cached per sourceId
        if (m_m3uCache.contains(src.id)) {
            QList<Channel> filtered;
            const auto &all = m_m3uCache.value(src.id);

            // First pass: identify favorites (since they are stored in config)
            // Ideally we do this once, but let's just do it on filter for now.
            // Actually finalizing sets them, so we assume finalize handles UI,
            // but we need to know isFavorite BEFORE filtering if user selected Favorites.

            for (const Channel &ch : all) {
                if (ch.streamType != m_streamType) continue; // Must match current tab (Live/VOD/Series)

                // Check favorite status now for filtering
                bool isFav = m_config.isFavorite(ch.streamUrl);

                if (categoryId == QStringLiteral("__FAVORITES__")) {
                    if (isFav) filtered.append(ch);
                }
                else if (categoryId.isEmpty()) {
                    // "All" category
                    filtered.append(ch);
                }
                else {
                    // Specific category
                    if (ch.categoryName == categoryId)
                        filtered.append(ch);
                }
            }
            finalize(filtered);
            return;
        }

        if (m_loadingChannels) return;  // prevent duplicate requests
        m_loadingChannels = true;
        statusBar()->showMessage(t(QStringLiteral("loading_m3u")));

        m_loadingProgress->setVisible(true);
        m_loadingProgress->setRange(0, 0); // indeterminate

        if (!isSafeUrl(src.m3uUrl)) {
            m_loadingChannels = false;
            m_loadingProgress->setVisible(false);
            statusBar()->showMessage(t(QStringLiteral("m3u_load_error")) + QStringLiteral("Invalid URL scheme"), 6000);
            return;
        }

        m_m3uNam = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(src.m3uUrl)};
        req.setRawHeader("User-Agent", "Vibestreamer/2.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(60000); // Increased timeout to 60s
        QNetworkReply *reply = m_m3uNam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, src, finalize]() {
            // NAM was deleted on source switch — nothing to do
            if (!m_m3uNam) {
                reply->deleteLater();
                return;
            }
            m_m3uNam->deleteLater();
            m_m3uNam = nullptr;

            if (reply->error() != QNetworkReply::NoError) {
                const int currentSrcIdx = m_sourceCombo->currentIndex();
                bool isSameSource = (currentSrcIdx >= 0 && m_config.sources[currentSrcIdx].id == src.id);
                if (isSameSource) {
                    m_loadingChannels = false;
                    m_loadingProgress->setVisible(false);
                    statusBar()->showMessage(
                        t(QStringLiteral("m3u_load_error")) + reply->errorString(), 5000);
                } else {
                    m_loadingChannels = false;
                }
                reply->deleteLater();
                return;
            }

            constexpr qint64 kMaxM3UBytes = 100LL * 1024 * 1024; // 100 MB
            const QByteArray raw = reply->read(kMaxM3UBytes + 1);
            reply->deleteLater();

            if (raw.size() > kMaxM3UBytes) {
                m_loadingChannels = false;
                m_loadingProgress->setVisible(false);
                statusBar()->showMessage(t(QStringLiteral("m3u_load_error")) + QStringLiteral("Response too large (>100 MB)"), 6000);
                return;
            }

            // Offload parsing to background thread
            auto *watcher = new QFutureWatcher<M3UParser::Result>(this);
            connect(watcher, &QFutureWatcher<M3UParser::Result>::finished, this, [this, watcher, src, finalize]() {
                M3UParser::Result result = watcher->result();
                watcher->deleteLater();

                const int currentSrcIdx = m_sourceCombo->currentIndex();
                bool isSameSource = (currentSrcIdx >= 0 && m_config.sources[currentSrcIdx].id == src.id);

                QList<Channel> all = std::move(result.channels);
                if (all.isEmpty()) {
                    if (isSameSource) {
                        m_loadingChannels = false;
                        m_loadingProgress->setVisible(false);
                        statusBar()->showMessage(t(QStringLiteral("m3u_empty")), 4000);
                    }
                    return;
                }

                m_m3uCache.insert(src.id, all);
                DEV_STAT("m3u_cache_sources", m_m3uCache.size());
                DEV_STAT("m3u_total_channels", all.size());

                // Check for embedded EPG URL
                if (src.epgUrl.isEmpty() && !result.epgUrl.isEmpty()) {
                    m_epg->load(result.epgUrl);
                }

                if (isSameSource) {
                    m_loadingChannels = false;
                    m_loadingProgress->setVisible(false);
                    loadCategories(src.id, m_streamType);
                } else {
                    m_loadingChannels = false;
                }
            });

            watcher->setFuture(QtConcurrent::run([raw, srcId = src.id]() {
                const QString text = QString::fromUtf8(raw);
                return M3UParser::parse(text, srcId);
            }));
        });
        return;
    }

    // ── Xtream source ───────────────────────────────────────────────
    if (!m_client) return;

    const CacheKey key{src.id, m_streamType, categoryId};
    if (m_channelCache.contains(key)) {
        finalize(m_channelCache.value(key));
        return;
    }

    if (m_loadingChannels) return;  // prevent duplicate requests

    // Xtream Favorites: load all channels (or use cache) then filter
    if (categoryId == QLatin1String("__FAVORITES__")) {
        const CacheKey allKey{src.id, m_streamType, {}};
        if (m_channelCache.contains(allKey)) {
            QList<Channel> favs;
            for (const Channel &ch : m_channelCache.value(allKey))
                if (m_config.isFavorite(ch.streamUrl))
                    favs.append(ch);
            finalize(favs);
            return;
        }
        // Need to fetch all channels first, then filter
        m_loadingChannels = true;
        m_loadingProgress->setVisible(true);
        m_loadingProgress->setRange(0, 0);
        statusBar()->showMessage(t(QStringLiteral("loading_favorites")));
        auto onAllDone = [this, finalize, allKey](QList<Channel> channels, QString err) {
            m_loadingChannels = false;
            m_loadingProgress->setVisible(false);
            statusBar()->clearMessage();
            if (!err.isEmpty()) {
                statusBar()->showMessage(t(QStringLiteral("channel_load_error")) + err, 6000);
                return;
            }
            if (m_channelCache.size() >= 50) m_channelCache.clear();
            m_channelCache.insert(allKey, channels);
            DEV_STAT("channel_cache_keys", m_channelCache.size());
            QList<Channel> favs;
            for (const Channel &ch : channels)
                if (m_config.isFavorite(ch.streamUrl))
                    favs.append(ch);
            finalize(favs);
        };
        switch (m_streamType) {
        case StreamType::Live:   m_client->getLiveStreams({}, onAllDone);  break;
        case StreamType::VOD:    m_client->getVodStreams({}, onAllDone);   break;
        case StreamType::Series: m_client->getSeries({}, onAllDone);      break;
        }
        return;
    }

    m_loadingChannels = true;
    m_loadingProgress->setVisible(true);
    m_loadingProgress->setRange(0, 0);
    statusBar()->showMessage(t(QStringLiteral("loading_channels")));

    auto onDone = [this, finalize, key](QList<Channel> channels, QString err) {
        m_loadingChannels = false;
        m_loadingProgress->setVisible(false);
        statusBar()->clearMessage();
        if (!err.isEmpty()) {
            statusBar()->showMessage(t(QStringLiteral("channel_load_error")) + err, 6000);
            return;
        }
        if (m_channelCache.size() >= 50) m_channelCache.clear();
        m_channelCache.insert(key, channels);
        DEV_STAT("channel_cache_keys", m_channelCache.size());
        finalize(channels);
    };

    switch (m_streamType) {
    case StreamType::Live:   m_client->getLiveStreams(categoryId, onDone);  break;
    case StreamType::VOD:    m_client->getVodStreams(categoryId, onDone);   break;
    case StreamType::Series: m_client->getSeries(categoryId, onDone);      break;
    }
}

void MainWindow::populateChannelList(const QList<Channel> &channels)
{
    m_channelList->setUpdatesEnabled(false);
    if(m_chanModel) m_chanModel->clear();
    m_logoUrlToRows.clear();

    int idx = 0;
    for (const Channel &ch : channels) {
        const QString prefix = ch.isFavorite ? QStringLiteral("★ ") : QString{};
        const QString numStr = ch.num > 0
            ? QString::number(ch.num) + QStringLiteral(". ")
            : QString::number(idx + 1) + QStringLiteral(". ");

        auto *item = new QStandardItem(prefix + numStr + ch.name);
        m_chanModel->appendRow(item);
        item->setData(ch.streamUrl, Qt::UserRole);
        item->setToolTip(ch.plot.isEmpty() ? ch.name : ch.plot.left(200));

        if (!ch.logoUrl.isEmpty()) {
            m_logoUrlToRows[ch.logoUrl].append(idx);
            bool ok = false;
            QPixmap pix = ImageCache::instance().get(ch.logoUrl, &ok);
            if (ok)
                item->setIcon(QIcon(pix));
        }
        ++idx;
    }

    m_channelList->setUpdatesEnabled(true);
}

void MainWindow::onChannelSelected(const QModelIndex &item)
{
    if (!item.isValid()) return;
    const QString url = item.data(Qt::UserRole).toString();
    for (const Channel &ch : m_allChannels)
        if (ch.streamUrl == url) { m_currentChannel = ch; break; }
    updateFavoriteButton(m_currentChannel);
    if (m_currentChannel.streamType == StreamType::Live)
        updateEpgPanel(m_currentChannel);
    else
        updateInfoPanel(m_currentChannel);
}

void MainWindow::onChannelDoubleClicked(const QModelIndex &item)
{
    onChannelSelected(item);
    playChannel(m_currentChannel);
}

void MainWindow::playChannel(const Channel &ch)
{
    if (ch.streamUrl.isEmpty()) return;

    MpvWidget *mpv = activeMpv();
    if (!mpv) return;

    // Stop any active recording before switching channels
    if (m_isRecording)
        onRecordStop();

    m_config.lastChannelId       = ch.id;
    m_config.lastChannelSourceId = ch.sourceId;

    // Apply hw decode option (runtime property, not pre-init option)
    mpv->setMpvProperty(QStringLiteral("hwdec"), m_config.mpvHwDecode);
    if (!m_config.mpvExtraArgs.isEmpty()) {
        const QStringList parts = m_config.mpvExtraArgs.split(QLatin1Char(' '),
                                                               Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const int eq = part.indexOf(QLatin1Char('='));
            if (eq > 0) {
                const QString key = part.left(eq);
                if (isSafeMpvProperty(key))
                    mpv->setMpvProperty(key, part.mid(eq + 1));
            }
        }
    }

    mpv->play(ch.streamUrl);

    // Track channel in active multiview cell
    if (m_multiViewMode && m_multiViewWidget)
        m_multiViewChannels[m_multiViewWidget->activeIndex()] = ch;

    // Apply speed
    if (m_speedCombo)
        onSpeedChanged(m_speedCombo->currentIndex());

    setWindowTitle(ch.name + QStringLiteral(" — Vibestreamer"));
    statusBar()->showMessage(ch.name);
}

void MainWindow::onSearchTextChanged(const QString &)
{
    applySearchFilter();
}

void MainWindow::applySearchFilter()
{
    const QString q = m_searchEdit->text().trimmed();
    m_proxyModel->setFilterFixedString(q);
}

void MainWindow::onViewModeChanged(int index)
{
    if (index == 0) {
        // List Mode
        m_channelList->setViewMode(QListView::ListMode);
        m_channelList->setResizeMode(QListView::Fixed);
        m_channelList->setWordWrap(false);
        m_channelList->setSpacing(1);
        m_channelList->setGridSize(QSize()); // reset
        m_channelList->setIconSize(QSize(24, 24));
    } else {
        // Grid Mode
        m_channelList->setViewMode(QListView::IconMode);
        m_channelList->setResizeMode(QListView::Adjust);
        m_channelList->setWordWrap(true);
        m_channelList->setSpacing(4);
        m_channelList->setGridSize(QSize(130, 150));
        m_channelList->setIconSize(QSize(110, 80));
    }
}

void MainWindow::checkUpdateSchedules()
{
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    bool updated = false;

    for (int i = 0; i < m_config.sources.size(); ++i) {
        Source &src = m_config.sources[i];
        if (src.autoUpdateIntervalHours <= 0) continue;

        const qint64 intervalSecs = src.autoUpdateIntervalHours * 3600;
        if (src.lastUpdated == 0 || (now - src.lastUpdated) >= intervalSecs) {
            src.lastUpdated = now;
            updated = true;

            // Just basic notification. Since caching relies on actual network requests,
            // we can forcefully clear the cache for this source.
            CacheKey liveKey{src.id, StreamType::Live, QString{}};
            CacheKey vodKey{src.id, StreamType::VOD, QString{}};
            CacheKey seriesKey{src.id, StreamType::Series, QString{}};

            m_categoryCache.remove(liveKey); m_channelCache.remove(liveKey);
            m_categoryCache.remove(vodKey); m_channelCache.remove(vodKey);
            m_categoryCache.remove(seriesKey); m_channelCache.remove(seriesKey);
            m_m3uCache.remove(src.id);

            if (m_trayIcon && m_trayIcon->isVisible()) {
                m_trayIcon->showMessage(t(QStringLiteral("playlist_update")),
                                        src.name + t(QStringLiteral("source_refreshing")),
                                        QSystemTrayIcon::Information, 3000);
            }

            // If it's the current selected source, refresh categories
            if (m_sourceCombo->currentIndex() == i) {
                loadCategories(src.id, m_streamType);
            }
        }
    }

    if (updated) {
        m_config.save();
    }
}
