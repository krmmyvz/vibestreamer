#include "mainwindow.h"
#include "mpvwidget.h"
#include "sourcedialog.h"
#include "settingsdialog.h"
#include "icons.h"
#include "multiviewwidget.h"
#include "imagecache.h"
#include "localization.h"
#include "theme.h"

static bool isSafeMpvProperty(const QString &name)
{
    static const QSet<QString> kAllowed = {
        QStringLiteral("hwdec"),      QStringLiteral("deband"),
        QStringLiteral("scale"),      QStringLiteral("cscale"),
        QStringLiteral("dscale"),     QStringLiteral("dither-depth"),
        QStringLiteral("correct-downscaling"), QStringLiteral("linear-downscaling"),
        QStringLiteral("sigmoid-upscaling"),   QStringLiteral("interpolation"),
        QStringLiteral("video-sync"), QStringLiteral("framedrop"),
        QStringLiteral("audio-channels"), QStringLiteral("demuxer-max-bytes"),
        QStringLiteral("cache"),      QStringLiteral("cache-secs"),
        QStringLiteral("network-timeout"), QStringLiteral("volume-max"),
        QStringLiteral("af"),         QStringLiteral("vf"),
        QStringLiteral("deinterlace"), QStringLiteral("blend-subtitles"),
        QStringLiteral("sub-scale"),  QStringLiteral("sub-bold"),
        QStringLiteral("sub-font"),   QStringLiteral("sub-color"),
    };
    return kAllowed.contains(name);
}

#include <QApplication>
#include <QAction>
#include <QComboBox>
#include <QCompleter>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListView>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringListModel>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

void MainWindow::setupUI()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_mainSplitter = new QSplitter(Qt::Horizontal, central);
    rootLayout->addWidget(m_mainSplitter);

    setupSidebar();
    setupPlayerPanel();

    m_mainSplitter->addWidget(m_sidebar);
    m_mainSplitter->addWidget(m_playerPanel);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({320, 960});
}

void MainWindow::setupSidebar()
{
    m_sidebar = new QWidget;
    m_sidebar->setMinimumWidth(220);
    m_sidebar->setMaximumWidth(420);
    m_sidebar->setStyleSheet(QStringLiteral(
        "QWidget#sidebar { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "stop:0 %1, stop:1 %2); "
        "border-right: 1px solid %3; }")
        .arg(m_theme.sidebarGradStart, m_theme.sidebarGradEnd, m_theme.sidebarBorder));
    m_sidebar->setObjectName(QStringLiteral("sidebar"));
    auto *lay = new QVBoxLayout(m_sidebar);
    lay->setContentsMargins(10, 10, 10, 10);
    lay->setSpacing(8);

    // Source selector row
    auto *srcRow = new QHBoxLayout;
    srcRow->setSpacing(4);
    m_sourceCombo = new QComboBox;
    m_sourceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const QString srcBtnStyle = QStringLiteral(
        "QToolButton { border: none; border-radius: 4px; padding: 4px; background: transparent; }"
        "QToolButton:hover { background: %1; }").arg(m_theme.hoverBg);

    m_addSourceBtn  = new QToolButton;
    m_addSourceBtn->setIcon(Icons::add(m_theme.iconSuccess));
    m_addSourceBtn->setIconSize(QSize(16, 16));
    m_addSourceBtn->setToolTip(t(QStringLiteral("add_source")));
    m_addSourceBtn->setFixedSize(30, 30);
    m_addSourceBtn->setStyleSheet(srcBtnStyle);

    m_editSourceBtn = new QToolButton;
    m_editSourceBtn->setIcon(Icons::edit(m_theme.iconDefault));
    m_editSourceBtn->setIconSize(QSize(16, 16));
    m_editSourceBtn->setToolTip(t(QStringLiteral("edit_source")));
    m_editSourceBtn->setFixedSize(30, 30);
    m_editSourceBtn->setStyleSheet(srcBtnStyle);

    m_delSourceBtn  = new QToolButton;
    m_delSourceBtn->setIcon(Icons::trash(m_theme.iconError));
    m_delSourceBtn->setIconSize(QSize(16, 16));
    m_delSourceBtn->setToolTip(t(QStringLiteral("delete_source")));
    m_delSourceBtn->setFixedSize(30, 30);
    m_delSourceBtn->setStyleSheet(srcBtnStyle);
    srcRow->addWidget(m_sourceCombo, 1);
    srcRow->addWidget(m_addSourceBtn);
    srcRow->addWidget(m_editSourceBtn);
    srcRow->addWidget(m_delSourceBtn);
    lay->addLayout(srcRow);

    // Progress Bar for Loading
    m_loadingProgress = new QProgressBar;
    m_loadingProgress->setTextVisible(false);
    m_loadingProgress->setFixedHeight(4);
    // Use Theme secondary color for the loading bar
    m_loadingProgress->setStyleSheet(QStringLiteral(
        "QProgressBar { background: transparent; border: none; } "
        "QProgressBar::chunk { background-color: %1; }").arg(m_theme.success));
    m_loadingProgress->setVisible(false);
    lay->addWidget(m_loadingProgress);

    // Stream type tabs
    m_typeTab = new QTabWidget;
    m_typeTab->setDocumentMode(true);
    m_typeTab->addTab(new QWidget, t(QStringLiteral("tab_live")));
    m_typeTab->addTab(new QWidget, t(QStringLiteral("tab_movies")));
    m_typeTab->addTab(new QWidget, t(QStringLiteral("tab_series")));
    lay->addWidget(m_typeTab);

    // Inner splitter: category | channel
    auto *innerSplit = new QSplitter(Qt::Vertical);

    m_categoryList = new QListView; m_catModel = new QStandardItemModel(this); m_categoryList->setModel(m_catModel); m_categoryList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_categoryList->setAlternatingRowColors(false);
    m_categoryList->setSpacing(1);
    innerSplit->addWidget(m_categoryList);

    auto *chPanel = new QWidget;
    auto *chLay   = new QVBoxLayout(chPanel);
    chLay->setContentsMargins(0, 0, 0, 0);
    chLay->setSpacing(4);

    // Section header between categories and channels
    m_channelHeaderLabel = new QLabel(t(QStringLiteral("channels")));
    m_channelHeaderLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 10px; font-weight: bold; "
        "text-transform: uppercase; padding: 6px 4px 2px 4px; "
        "border-top: 1px solid %2;").arg(m_theme.textDimmed, m_theme.borderSubtle));
    chLay->addWidget(m_channelHeaderLabel);

    auto *filterLay = new QHBoxLayout;
    filterLay->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(t(QStringLiteral("search_channel")));
    m_searchEdit->setClearButtonEnabled(true);

    // Setup Search History Completer
    m_searchModel = new QStringListModel(m_config.searchHistory, this);
    m_searchCompleter = new QCompleter(m_searchModel, this);
    m_searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_searchCompleter->setFilterMode(Qt::MatchContains);
    m_searchEdit->setCompleter(m_searchCompleter);

    // Save search history on Enter
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        const QString text = m_searchEdit->text().trimmed();
        if (!text.isEmpty()) {
            m_config.searchHistory.removeAll(text); // Remove if exists to move to top
            m_config.searchHistory.prepend(text);
            if (m_config.searchHistory.size() > 20) // Keep at most 20 recent searches
                m_config.searchHistory.removeLast();
            m_searchModel->setStringList(m_config.searchHistory);
            m_config.save();
        }
    });

    filterLay->addWidget(m_searchEdit, 1);

    m_viewModeCombo = new QComboBox;
    m_viewModeCombo->addItem(t(QStringLiteral("list")));
    m_viewModeCombo->addItem(QStringLiteral("Grid"));
    m_viewModeCombo->setToolTip(t(QStringLiteral("view_mode")));
    filterLay->addWidget(m_viewModeCombo);

    chLay->addLayout(filterLay);

    m_channelList = new QListView; m_chanModel = new QStandardItemModel(this); m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_chanModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_channelList->setModel(m_proxyModel); m_channelList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_channelList->setAlternatingRowColors(false);
    m_channelList->setSpacing(1);
    m_channelList->setContextMenuPolicy(Qt::CustomContextMenu);
    chLay->addWidget(m_channelList);
    innerSplit->addWidget(chPanel);
    innerSplit->setSizes({180, 420});

    lay->addWidget(innerSplit, 1);

    // Connections
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onSourceChanged);
    connect(m_typeTab,     &QTabWidget::currentChanged,     this, &MainWindow::onStreamTypeChanged);
    connect(m_categoryList, &QListView::clicked,  this, &MainWindow::onCategorySelected);
    connect(m_channelList,  &QListView::clicked,  this, &MainWindow::onChannelSelected);
    connect(m_channelList,  &QListView::doubleClicked, this, &MainWindow::onChannelDoubleClicked);
    connect(m_searchEdit,   &QLineEdit::textChanged,    this, &MainWindow::onSearchTextChanged);
    connect(m_viewModeCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onViewModeChanged);
    connect(m_addSourceBtn,  &QToolButton::clicked, this, &MainWindow::onAddSource);
    connect(m_editSourceBtn, &QToolButton::clicked, this, &MainWindow::onEditSource);
    connect(m_delSourceBtn,  &QToolButton::clicked, this, &MainWindow::onRemoveSource);

    // Context menu for multi-view routing
    connect(m_channelList, &QListView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex item = m_channelList->indexAt(pos);
        if (!item.isValid()) return;

        QMenu menu(this);
        auto *mvMenu = menu.addMenu(Icons::gridView(m_theme.iconDefault), t(QStringLiteral("send_to_multiview")));

        for (int i = 0; i < 4; ++i) {
            auto *action = mvMenu->addAction(t(QStringLiteral("screen_n")).arg(i + 1));
            connect(action, &QAction::triggered, this, [this, item, i]() {
                const QString url = item.data(Qt::UserRole).toString();
                Channel ch;
                for (const Channel &c : m_allChannels) {
                    if (c.streamUrl == url) { ch = c; break; }
                }
                if (ch.streamUrl.isEmpty()) return;

                // Enter multiview if not already active
                if (!m_multiViewMode) onOpenMultiView();

                // Route channel to specific screen and activate it
                m_multiViewWidget->player(i)->play(ch.streamUrl);
                m_multiViewChannels[i] = ch;
                m_multiViewWidget->setActiveIndex(i);
            });
        }
        menu.exec(m_channelList->mapToGlobal(pos));
    });

    // Image cache — O(1) lookup via URL→row map
    connect(&ImageCache::instance(), &ImageCache::imageLoaded, this, [this](const QString &url, const QPixmap &pix) {
        auto it = m_logoUrlToRows.find(url);
        if (it == m_logoUrlToRows.end()) return;
        const QIcon icon(pix);
        for (int row : it.value()) {
            if (row < m_chanModel->rowCount()) {
                auto *item = m_chanModel->item(row);
                if (item) item->setIcon(icon);
            }
        }
    });
}

void MainWindow::setupPlayerPanel()
{
    m_playerPanel = new QWidget;
    auto *lay = new QVBoxLayout(m_playerPanel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    try {
        m_mpv = new MpvWidget;

        // Apply hardware decoding setting from config (uses mpv property, works after init)
        if (!m_config.mpvHwDecode.isEmpty())
            m_mpv->setMpvProperty(QStringLiteral("hwdec"), m_config.mpvHwDecode);

        // Apply extra args if any — whitelisted properties only
        if (!m_config.mpvExtraArgs.isEmpty()) {
            const QStringList args = m_config.mpvExtraArgs.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &arg : args) {
                const int eq = arg.indexOf(QLatin1Char('='));
                if (eq > 0) {
                    const QString key = arg.left(eq);
                    if (isSafeMpvProperty(key))
                        m_mpv->setMpvProperty(key, arg.mid(eq + 1));
                }
            }
        }
    } catch (const std::exception &e) {
                QMessageBox::critical(this, t(QStringLiteral("critical_error")),
                        t(QStringLiteral("player_init_error")).arg(e.what()));
        m_mpv = nullptr;
    }

    m_playerStack = new QStackedWidget;

    if (m_mpv) {
        m_playerStack->addWidget(m_mpv); // index 0
        connectActiveMpvSignals();
    } else {
        auto *errLbl = new QLabel(t(QStringLiteral("player_load_failed")));
        errLbl->setAlignment(Qt::AlignCenter);
        errLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: bold;").arg(m_theme.error));
        m_playerStack->addWidget(errLbl);
    }

    lay->addWidget(m_playerStack, 1);

    // EPG panel
    m_epgPanel = new QWidget;
    // Remove custom style sheet
    m_epgPanel->setObjectName(QStringLiteral("epgPanel"));
    auto *epgLay = new QVBoxLayout(m_epgPanel);
    epgLay->setContentsMargins(16, 8, 16, 8);
    epgLay->setSpacing(4);

    m_epgCurrentLabel = new QLabel(QStringLiteral("—"));
    m_epgCurrentLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-weight: 600; font-size: 13px;").arg(m_theme.textPrimary));

    m_epgNextLabel = new QLabel;
    m_epgProgress = new QProgressBar;
    m_epgProgress->setTextVisible(false);
    m_epgProgress->setFixedHeight(4);
    m_epgProgress->setStyleSheet(QStringLiteral(
        "QProgressBar { background: %1; border: none; } "
        "QProgressBar::chunk { background-color: %2; }")
        .arg(m_theme.epgProgressBg, m_theme.epgProgressChunk));

    epgLay->addWidget(m_epgCurrentLabel);
    epgLay->addWidget(m_epgProgress);
    epgLay->addWidget(m_epgNextLabel);
    lay->addWidget(m_epgPanel);

    setupControlBar();

    m_epgPanel->setVisible(m_config.showEpg);

    setupTrayIcon();
}

void MainWindow::setupControlBar()
{
    if (!m_mpv) return;

    m_controlBar = new QWidget;
    m_controlBar->setObjectName(QStringLiteral("controlBar"));
    m_controlBar->setFixedHeight(48);
    m_controlBar->setStyleSheet(QStringLiteral(
        "QWidget#controlBar { background: %1; border-top: 1px solid %2; }")
        .arg(m_theme.bgSurface, m_theme.borderSubtle));

    auto *lay = new QHBoxLayout(m_controlBar);
    lay->setContentsMargins(10, 4, 10, 4);
    lay->setSpacing(6);

    // Button factory — icon-based, consistent look
    const QString btnStyle = QStringLiteral(
        "QToolButton { border-radius: 4px; padding: 0px; border: none; background: transparent; }"
        "QToolButton:hover { background: %1; }"
        "QToolButton:pressed { background: %2; }").arg(m_theme.hoverBg, m_theme.pressedBg);

    const QSize iconSz(18, 18);
    auto makeBtn = [&](const QIcon &icon, const QString &tip, int w = 32) {
        auto *btn = new QToolButton;
        btn->setIcon(icon);
        btn->setIconSize(iconSz);
        btn->setToolTip(tip);
        btn->setFixedSize(w, 32);
        btn->setStyleSheet(btnStyle);
        return btn;
    };

    // Visual separator
    auto makeSep = [this]() {
        auto *sep = new QFrame(m_controlBar);
        sep->setFrameShape(QFrame::VLine);
        sep->setFixedWidth(1);
        sep->setFixedHeight(24);
        sep->setStyleSheet(QStringLiteral("background: %1;").arg(m_theme.separator));
        return sep;
    };

    // ── Playback group ──
    m_prevChBtn    = makeBtn(Icons::skipPrevious(m_theme.iconDefault), t(QStringLiteral("prev_channel")), 30);
    m_skipBackBtn  = makeBtn(Icons::rewind(m_theme.iconDefault),       t(QStringLiteral("back_10s")), 30);
    m_playPauseBtn = makeBtn(Icons::play(m_theme.iconAccent),          t(QStringLiteral("play_pause")), 36);
    m_skipFwdBtn   = makeBtn(Icons::fastForward(m_theme.iconDefault),  t(QStringLiteral("forward_10s")), 30);
    m_recordBtn      = makeBtn(Icons::record(m_theme.iconRecord),       t(QStringLiteral("start_recording")), 30);
    m_recordPauseBtn = makeBtn(Icons::pause(m_theme.iconDefault),       t(QStringLiteral("pause_recording")), 30);
    m_recordStopBtn  = makeBtn(Icons::stop(m_theme.iconError),          t(QStringLiteral("stop_recording")), 30);
    m_recordPauseBtn->setVisible(false);
    m_recordStopBtn->setVisible(false);
    m_nextChBtn    = makeBtn(Icons::skipNext(m_theme.iconDefault),     t(QStringLiteral("next_channel")), 30);

    // ── Seek + time ──
    m_seekSlider = new QSlider(Qt::Horizontal);
    m_seekSlider->setRange(0, 1000);

    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"));
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 11px; min-width: 90px; font-weight: 500;").arg(m_theme.textSecondary));

    m_liveLabel = new QLabel(QStringLiteral("CANLI"));
    m_liveLabel->setAlignment(Qt::AlignCenter);
    m_liveLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 10px; font-weight: bold; "
        "background: %2; border-radius: 3px; "
        "padding: 1px 6px;").arg(m_theme.error, m_theme.errorAlpha));
    m_liveLabel->setVisible(false);

    // ── Volume group ──
    m_volIcon = new QToolButton;
    m_volIcon->setIcon(Icons::volumeUp(m_theme.iconMuted));
    m_volIcon->setIconSize(QSize(16, 16));
    m_volIcon->setFixedSize(24, 24);
    m_volIcon->setStyleSheet(QStringLiteral("QToolButton { border: none; background: transparent; }"));
    m_volIcon->setEnabled(false);

    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(m_config.statePersistence ? m_config.volume : 100);
    m_volumeSlider->setFixedWidth(75);
    m_volumeSlider->setToolTip(t(QStringLiteral("volume")));

    // ── Track / tools group ──
    m_audioBtn   = makeBtn(Icons::audioTrack(m_theme.iconDefault),     t(QStringLiteral("select_audio")), 32);
    m_subBtn     = makeBtn(Icons::closedCaption(m_theme.iconDefault),  t(QStringLiteral("select_subtitle")), 32);
    m_infoBtn    = makeBtn(Icons::info(m_theme.iconDefault),           t(QStringLiteral("media_info")), 28);

    m_speedCombo = new QComboBox;
    m_speedCombo->addItems({QStringLiteral("0.5x"), QStringLiteral("0.75x"),
                            QStringLiteral("1x"), QStringLiteral("1.25x"),
                            QStringLiteral("1.5x"), QStringLiteral("2x")});
    m_speedCombo->setCurrentIndex(2);
    m_speedCombo->setFixedWidth(58);
    m_speedCombo->setToolTip(t(QStringLiteral("playback_speed")));

    // ── Utility group ──
    m_favoriteBtn   = makeBtn(Icons::starOutline(m_theme.iconDefault),      t(QStringLiteral("toggle_favorite")), 32);
    m_multiViewBtn  = makeBtn(Icons::gridView(m_theme.iconDefault),         t(QStringLiteral("multi_view")), 32);
    m_multiViewBtn->setCheckable(true);
    m_epgToggleBtn  = makeBtn(Icons::tvGuide(m_theme.iconDefault),          t(QStringLiteral("epg_toggle")), 32);
    m_pipBtn        = makeBtn(Icons::pictureInPicture(m_theme.iconDefault), t(QStringLiteral("pip")), 32);
    m_fullscreenBtn = makeBtn(Icons::fullscreen(m_theme.iconDefault),       t(QStringLiteral("fullscreen")), 32);

    // ── Layout ──
    // ── Playback group ──
    lay->addWidget(m_prevChBtn);
    lay->addWidget(m_skipBackBtn);
    lay->addWidget(m_playPauseBtn);
    lay->addWidget(m_skipFwdBtn);
    lay->addWidget(m_nextChBtn);
    lay->addWidget(makeSep());
    // ── Seek + time ──
    lay->addWidget(m_seekSlider, 1);
    lay->addWidget(m_liveLabel);
    lay->addWidget(m_timeLabel);
    lay->addWidget(makeSep());
    // ── Volume ──
    lay->addWidget(m_volIcon);
    lay->addWidget(m_volumeSlider);
    lay->addWidget(makeSep());
    // ── Track / speed ──
    lay->addWidget(m_audioBtn);
    lay->addWidget(m_subBtn);
    lay->addWidget(m_speedCombo);
    lay->addWidget(makeSep());
    // ── Utility group ──
    lay->addWidget(m_recordBtn);
    lay->addWidget(m_recordPauseBtn);
    lay->addWidget(m_recordStopBtn);
    lay->addWidget(m_infoBtn);
    lay->addWidget(m_favoriteBtn);
    lay->addWidget(m_multiViewBtn);
    lay->addWidget(m_epgToggleBtn);
    lay->addWidget(m_pipBtn);
    lay->addWidget(m_fullscreenBtn);

    auto *playerLay = static_cast<QVBoxLayout *>(m_playerPanel->layout());
    playerLay->addWidget(m_controlBar);

    // Connections
    connect(m_prevChBtn,    &QToolButton::clicked, this, &MainWindow::onPrevChannel);
    connect(m_skipBackBtn,  &QToolButton::clicked, this, &MainWindow::onSkipBack);
    connect(m_playPauseBtn, &QToolButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_skipFwdBtn,   &QToolButton::clicked, this, &MainWindow::onSkipForward);
    connect(m_nextChBtn,    &QToolButton::clicked, this, &MainWindow::onNextChannel);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    connect(m_seekSlider,   &QSlider::sliderPressed,  this, [this]{ m_seeking = true; });
    connect(m_seekSlider,   &QSlider::sliderReleased, this, [this]{
        m_seeking = false;
        if (MpvWidget *mpv = activeMpv())
            mpv->seek(m_seekSlider->value() / 1000.0 * mpv->duration());
    });
    connect(m_audioBtn,      &QToolButton::clicked,        this, &MainWindow::onShowAudioMenu);
    connect(m_subBtn,        &QToolButton::clicked,        this, &MainWindow::onShowSubMenu);
    connect(m_infoBtn,       &QToolButton::clicked,        this, &MainWindow::onShowMediaInfo);
    connect(m_favoriteBtn,   &QToolButton::clicked,        this, &MainWindow::onToggleFavorite);
    connect(m_multiViewBtn,  &QToolButton::clicked,        this, &MainWindow::onOpenMultiView);
    connect(m_recordBtn,     &QToolButton::clicked,        this, &MainWindow::onToggleRecord);
    connect(m_recordPauseBtn, &QToolButton::clicked,      this, [this]{ onRecordPauseResume(); });
    connect(m_recordStopBtn,  &QToolButton::clicked,      this, [this]{ onRecordStop(); });
    connect(m_epgToggleBtn,  &QToolButton::clicked,        this, &MainWindow::onToggleEpg);
    connect(m_pipBtn,        &QToolButton::clicked,        this, &MainWindow::onTogglePip);
    connect(m_fullscreenBtn, &QToolButton::clicked,        this, &MainWindow::toggleFullScreen);
    connect(m_speedCombo,    &QComboBox::currentIndexChanged, this, &MainWindow::onSpeedChanged);

    m_mpv->setVolume(m_config.statePersistence ? m_config.volume : 100);
}

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu(t(QStringLiteral("menu_file")));
    fileMenu->addAction(t(QStringLiteral("menu_add_source")),  this, &MainWindow::onAddSource);
    fileMenu->addAction(t(QStringLiteral("menu_edit_source")), this, &MainWindow::onEditSource);
    fileMenu->addAction(t(QStringLiteral("menu_delete_source")),   this, &MainWindow::onRemoveSource);
    fileMenu->addSeparator();
    fileMenu->addAction(t(QStringLiteral("exit")), qApp, &QApplication::quit);

    auto *viewMenu = menuBar()->addMenu(t(QStringLiteral("menu_view")));
    viewMenu->addAction(t(QStringLiteral("fullscreen")), this, &MainWindow::toggleFullScreen);
    viewMenu->addAction(t(QStringLiteral("menu_epg_toggle")), this, &MainWindow::onToggleEpg);

    auto *settingsMenu = menuBar()->addMenu(t(QStringLiteral("menu_settings")));
    settingsMenu->addAction(t(QStringLiteral("menu_preferences")), this, &MainWindow::onOpenSettings);

    auto *helpMenu = menuBar()->addMenu(t(QStringLiteral("menu_help")));
    viewMenu->addAction(t(QStringLiteral("menu_pip")), this, &MainWindow::onTogglePip);

    helpMenu->addAction(t(QStringLiteral("menu_shortcuts")), this, [this]() {
        QMessageBox::information(this, t(QStringLiteral("menu_shortcuts")),
            t(QStringLiteral("shortcuts_body")));
    });
    helpMenu->addSeparator();
    helpMenu->addAction(t(QStringLiteral("menu_about")), this, [this](){
        QMessageBox::about(this, QStringLiteral("Vibestreamer"),
            QStringLiteral("Vibestreamer v2.0\nC++ + Qt6 + libmpv"));
    });
}

void MainWindow::setupShortcuts()
{
    if (!m_mpv) return;

    auto sc = [this](const QString &keyStr, auto slot) {
        if (keyStr.isEmpty()) return;
        auto *s = new QShortcut(QKeySequence::fromString(keyStr), this);
        connect(s, &QShortcut::activated, this, slot);
    };

    sc(m_config.shortcuts.value(QStringLiteral("fullscreen")), &MainWindow::toggleFullScreen);
    sc(m_config.shortcuts.value(QStringLiteral("play_pause")), &MainWindow::onPlayPause);
    sc(m_config.shortcuts.value(QStringLiteral("mute")), [this]{
        if (auto *m = activeMpv()) m->setVolume(m->volume() == 0 ? m_config.volume : 0);
    });

    // Fixed seeking shortcuts
    sc(QStringLiteral("Right"), [this]{ if (auto *m = activeMpv()) m->seek(m->position() + 10); });
    sc(QStringLiteral("Left"),  [this]{ if (auto *m = activeMpv()) m->seek(m->position() - 10); });

    sc(m_config.shortcuts.value(QStringLiteral("vol_up")),     [this]{ m_volumeSlider->setValue(qMin(100, m_volumeSlider->value() + 5)); });
    sc(m_config.shortcuts.value(QStringLiteral("vol_down")),   [this]{ m_volumeSlider->setValue(qMax(0,   m_volumeSlider->value() - 5)); });
    sc(QStringLiteral("Esc"),                                  [this]{ if (isFullScreen()) toggleFullScreen(); else if (m_pipMode) onTogglePip(); });

    // These shortcuts should not fire when typing in the search box
    auto scNoEdit = [this](const QString &keyStr, auto slot) {
        if (keyStr.isEmpty()) return;
        auto *s = new QShortcut(QKeySequence::fromString(keyStr), this);
        connect(s, &QShortcut::activated, this, [this, slot]{
            if (m_searchEdit && m_searchEdit->hasFocus()) return;
            (this->*slot)();
        });
    };

    sc(m_config.shortcuts.value(QStringLiteral("favorite")),   [this]{ if (!m_searchEdit->hasFocus()) onToggleFavorite(); });
    scNoEdit(m_config.shortcuts.value(QStringLiteral("ch_next")),  &MainWindow::onNextChannel);
    scNoEdit(m_config.shortcuts.value(QStringLiteral("ch_prev")),  &MainWindow::onPrevChannel);
    scNoEdit(m_config.shortcuts.value(QStringLiteral("info")),     &MainWindow::onShowMediaInfo);
    scNoEdit(m_config.shortcuts.value(QStringLiteral("audio")),    &MainWindow::onShowAudioMenu);
    scNoEdit(m_config.shortcuts.value(QStringLiteral("subs")),     &MainWindow::onShowSubMenu);
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(QStringLiteral(":/logo_concept1.svg")));
    m_trayIcon->setToolTip(QStringLiteral("Vibestreamer"));

    auto *trayMenu = new QMenu(this);

    auto *toggleAct = new QAction(t(QStringLiteral("show_hide_interface")), this);
    connect(toggleAct, &QAction::triggered, this, [this]{
        if (isVisible()) hide();
        else { showNormal(); raise(); activateWindow(); }
    });

    auto *muteAct = new QAction(t(QStringLiteral("mute_unmute")), this);
    connect(muteAct, &QAction::triggered, this, [this]{
        if (auto *m = activeMpv()) m->setVolume(m->volume() > 0 ? 0 : m_config.volume);
    });

    auto *quitAct = new QAction(t(QStringLiteral("exit")), this);
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

    trayMenu->addAction(toggleAct);
    trayMenu->addAction(muteAct);
    trayMenu->addSeparator();
    trayMenu->addAction(quitAct);

    m_trayIcon->setContextMenu(trayMenu);

    // Only show the tray icon if the setting is enabled
    m_trayIcon->setVisible(m_config.minimizeToTray);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
            if (isVisible()) hide();
            else { showNormal(); raise(); activateWindow(); }
        }
    });
}

void MainWindow::retranslateUi()
{
    if (m_typeTab) {
        m_typeTab->setTabText(0, t(QStringLiteral("tab_live")));
        m_typeTab->setTabText(1, t(QStringLiteral("tab_movies")));
        m_typeTab->setTabText(2, t(QStringLiteral("tab_series")));
    }

    if (m_addSourceBtn) m_addSourceBtn->setToolTip(t(QStringLiteral("add_source")));
    if (m_editSourceBtn) m_editSourceBtn->setToolTip(t(QStringLiteral("edit_source")));
    if (m_delSourceBtn) m_delSourceBtn->setToolTip(t(QStringLiteral("delete_source")));
    if (m_channelHeaderLabel) m_channelHeaderLabel->setText(t(QStringLiteral("channels")));
    if (m_searchEdit) m_searchEdit->setPlaceholderText(t(QStringLiteral("search_channel")));

    if (m_viewModeCombo) {
        const int mode = m_viewModeCombo->currentIndex();
        m_viewModeCombo->setItemText(0, t(QStringLiteral("list")));
        m_viewModeCombo->setItemText(1, QStringLiteral("Grid"));
        m_viewModeCombo->setToolTip(t(QStringLiteral("view_mode")));
        m_viewModeCombo->setCurrentIndex(mode);
    }

    if (m_prevChBtn) m_prevChBtn->setToolTip(t(QStringLiteral("prev_channel")));
    if (m_skipBackBtn) m_skipBackBtn->setToolTip(t(QStringLiteral("back_10s")));
    if (m_playPauseBtn) m_playPauseBtn->setToolTip(t(QStringLiteral("play_pause")));
    if (m_skipFwdBtn) m_skipFwdBtn->setToolTip(t(QStringLiteral("forward_10s")));
    if (m_nextChBtn) m_nextChBtn->setToolTip(t(QStringLiteral("next_channel")));
    if (m_volumeSlider) m_volumeSlider->setToolTip(t(QStringLiteral("volume")));
    if (m_audioBtn) m_audioBtn->setToolTip(t(QStringLiteral("select_audio")));
    if (m_subBtn) m_subBtn->setToolTip(t(QStringLiteral("select_subtitle")));
    if (m_infoBtn) m_infoBtn->setToolTip(t(QStringLiteral("media_info")));
    if (m_speedCombo) m_speedCombo->setToolTip(t(QStringLiteral("playback_speed")));
    if (m_favoriteBtn) m_favoriteBtn->setToolTip(t(QStringLiteral("toggle_favorite")));
    if (m_multiViewBtn) m_multiViewBtn->setToolTip(t(QStringLiteral("multi_view")));
    if (m_epgToggleBtn) m_epgToggleBtn->setToolTip(t(QStringLiteral("epg_toggle")));
    if (m_pipBtn) m_pipBtn->setToolTip(t(QStringLiteral("pip")));
    if (m_fullscreenBtn) m_fullscreenBtn->setToolTip(t(QStringLiteral("fullscreen")));
    if (m_recordBtn) {
        if (m_isRecording)
            m_recordBtn->setToolTip(t(QStringLiteral("recording_in_progress")));
        else
            m_recordBtn->setToolTip(t(QStringLiteral("start_recording")));
    }
    if (m_recordPauseBtn) {
        if (m_recordPaused)
            m_recordPauseBtn->setToolTip(t(QStringLiteral("resume_recording")));
        else
            m_recordPauseBtn->setToolTip(t(QStringLiteral("pause_recording")));
    }
    if (m_recordStopBtn) m_recordStopBtn->setToolTip(t(QStringLiteral("stop_recording")));

    if (m_catModel) {
        for (int i = 0; i < m_catModel->rowCount(); ++i) {
            auto *item = m_catModel->item(i);
            if (!item) continue;
            const QString key = item->data(Qt::UserRole).toString();
            if (key == QStringLiteral("__FAVORITES__"))
                item->setText(t(QStringLiteral("favorites")));
            else if (key.isEmpty())
                item->setText(t(QStringLiteral("all")));
        }
    }

    menuBar()->clear();
    setupMenuBar();

    if (m_trayIcon) {
        m_trayIcon->setToolTip(QStringLiteral("Vibestreamer"));
        if (auto *oldMenu = m_trayIcon->contextMenu()) {
            oldMenu->deleteLater();
        }

        auto *trayMenu = new QMenu(this);
        auto *toggleAct = new QAction(t(QStringLiteral("show_hide_interface")), trayMenu);
        connect(toggleAct, &QAction::triggered, this, [this]{
            if (isVisible()) hide();
            else { showNormal(); raise(); activateWindow(); }
        });

        auto *muteAct = new QAction(t(QStringLiteral("mute_unmute")), trayMenu);
        connect(muteAct, &QAction::triggered, this, [this]{
            if (m_mpv) m_mpv->setVolume(m_mpv->volume() > 0 ? 0 : m_config.volume);
        });

        auto *quitAct = new QAction(t(QStringLiteral("exit")), trayMenu);
        connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

        trayMenu->addAction(toggleAct);
        trayMenu->addAction(muteAct);
        trayMenu->addSeparator();
        trayMenu->addAction(quitAct);
        m_trayIcon->setContextMenu(trayMenu);
    }
}
