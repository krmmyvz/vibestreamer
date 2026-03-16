#include "mainwindow.h"
#include <QStandardItemModel>
#include <QStandardItem>
#include "theme.h"

#include "mpvwidget.h"
#include "sourcedialog.h"
#include "settingsdialog.h"
#include "xtreamclient.h"
#include "m3uparser.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include "icons.h"
#include "multiviewdialog.h"
#include "imagecache.h"
#include "localization.h"

#include <QContextMenuEvent>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QStyleFactory>
#include <QApplication>
#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPalette>
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QScreen>
#include <QGuiApplication>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QWindow>
#include <QtConcurrent>
#include <QFutureWatcher>

// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_epg(new EpgManager(this))
{
    setWindowTitle(QStringLiteral("Vibestreamer"));
    resize(m_config.windowWidth, m_config.windowHeight);

    applyTheme();
    setupUI();
    setupMenuBar();
    setupShortcuts();
    
    // Initial state for the loading progress
    m_loadingProgress->setVisible(false);

    loadSourceList();

    connect(m_epg, &EpgManager::loaded, this, [this]() {
        if (!m_currentChannel.epgChannelId.isEmpty())
            updateEpgPanel(m_currentChannel);
    });
    connect(m_epg, &EpgManager::loadError, this, [this](const QString &err) {
        statusBar()->showMessage(t(QStringLiteral("EPG yükleme hatası: "), QStringLiteral("EPG load error: ")) + err, 6000);
    });

    // Auto-refresh EPG every 5 minutes
    auto *epgTimer = new QTimer(this);
    epgTimer->setInterval(5 * 60 * 1000);
    connect(epgTimer, &QTimer::timeout, this, &MainWindow::refreshEpg);
    epgTimer->start();

    // Auto-update playlists every 10 minutes (checks if interval reached)
    m_autoUpdateTimer = new QTimer(this);
    m_autoUpdateTimer->setInterval(10 * 60 * 1000); // 10 minutes
    connect(m_autoUpdateTimer, &QTimer::timeout, this, &MainWindow::checkUpdateSchedules);
    m_autoUpdateTimer->start();
    
    // Initial check right after startup
    QTimer::singleShot(5000, this, &MainWindow::checkUpdateSchedules);
}

MainWindow::~MainWindow() = default;

QString MainWindow::t(const QString &tr, const QString &en) const
{
    return Localization::text(m_config.language, tr, en);
}

void MainWindow::retranslateUi()
{
    if (m_typeTab) {
        m_typeTab->setTabText(0, t(QStringLiteral("Canli"), QStringLiteral("Live")));
        m_typeTab->setTabText(1, t(QStringLiteral("Film"), QStringLiteral("Movies")));
        m_typeTab->setTabText(2, t(QStringLiteral("Dizi"), QStringLiteral("Series")));
    }

    if (m_addSourceBtn) m_addSourceBtn->setToolTip(t(QStringLiteral("Kaynak ekle"), QStringLiteral("Add source")));
    if (m_editSourceBtn) m_editSourceBtn->setToolTip(t(QStringLiteral("Kaynağı düzenle"), QStringLiteral("Edit source")));
    if (m_delSourceBtn) m_delSourceBtn->setToolTip(t(QStringLiteral("Kaynağı sil"), QStringLiteral("Delete source")));
    if (m_channelHeaderLabel) m_channelHeaderLabel->setText(t(QStringLiteral("Kanallar"), QStringLiteral("Channels")));
    if (m_searchEdit) m_searchEdit->setPlaceholderText(t(QStringLiteral("Kanal ara..."), QStringLiteral("Search channel...")));

    if (m_viewModeCombo) {
        const int mode = m_viewModeCombo->currentIndex();
        m_viewModeCombo->setItemText(0, t(QStringLiteral("Liste"), QStringLiteral("List")));
        m_viewModeCombo->setItemText(1, QStringLiteral("Grid"));
        m_viewModeCombo->setToolTip(t(QStringLiteral("Görünüm Modu"), QStringLiteral("View Mode")));
        m_viewModeCombo->setCurrentIndex(mode);
    }

    if (m_prevChBtn) m_prevChBtn->setToolTip(t(QStringLiteral("Onceki kanal (P)"), QStringLiteral("Previous channel (P)")));
    if (m_skipBackBtn) m_skipBackBtn->setToolTip(t(QStringLiteral("10 sn geri"), QStringLiteral("Back 10s")));
    if (m_playPauseBtn) m_playPauseBtn->setToolTip(t(QStringLiteral("Oynat / Duraklat"), QStringLiteral("Play / Pause")));
    if (m_skipFwdBtn) m_skipFwdBtn->setToolTip(t(QStringLiteral("10 sn ileri"), QStringLiteral("Forward 10s")));
    if (m_nextChBtn) m_nextChBtn->setToolTip(t(QStringLiteral("Sonraki kanal (N)"), QStringLiteral("Next channel (N)")));
    if (m_volumeSlider) m_volumeSlider->setToolTip(t(QStringLiteral("Ses"), QStringLiteral("Volume")));
    if (m_audioBtn) m_audioBtn->setToolTip(t(QStringLiteral("Ses parcasi sec (A)"), QStringLiteral("Select audio track (A)")));
    if (m_subBtn) m_subBtn->setToolTip(t(QStringLiteral("Altyazi sec (S)"), QStringLiteral("Select subtitle (S)")));
    if (m_infoBtn) m_infoBtn->setToolTip(t(QStringLiteral("Medya bilgisi (I)"), QStringLiteral("Media info (I)")));
    if (m_speedCombo) m_speedCombo->setToolTip(t(QStringLiteral("Oynatma hizi"), QStringLiteral("Playback speed")));
    if (m_favoriteBtn) m_favoriteBtn->setToolTip(t(QStringLiteral("Favorilere ekle / cikar (F)"), QStringLiteral("Toggle favorite (F)")));
    if (m_multiViewBtn) m_multiViewBtn->setToolTip(t(QStringLiteral("Çoklu Ekran (Multi-View)"), QStringLiteral("Multi-View")));
    if (m_epgToggleBtn) m_epgToggleBtn->setToolTip(t(QStringLiteral("EPG panelini goster/gizle"), QStringLiteral("Show/hide EPG panel")));
    if (m_pipBtn) m_pipBtn->setToolTip(t(QStringLiteral("Resim icinde resim"), QStringLiteral("Picture in picture")));
    if (m_fullscreenBtn) m_fullscreenBtn->setToolTip(t(QStringLiteral("Tam ekran (F11)"), QStringLiteral("Fullscreen (F11)")));
    if (m_recordBtn) {
        if (m_isRecording)
            m_recordBtn->setToolTip(t(QStringLiteral("Kayıt devam ediyor"), QStringLiteral("Recording in progress")));
        else
            m_recordBtn->setToolTip(t(QStringLiteral("Kaydı Başlat"), QStringLiteral("Start recording")));
    }
    if (m_recordPauseBtn) {
        if (m_recordPaused)
            m_recordPauseBtn->setToolTip(t(QStringLiteral("Kaydı Devam Ettir"), QStringLiteral("Resume recording")));
        else
            m_recordPauseBtn->setToolTip(t(QStringLiteral("Kaydı Duraklat"), QStringLiteral("Pause recording")));
    }
    if (m_recordStopBtn) m_recordStopBtn->setToolTip(t(QStringLiteral("Kaydı Durdur"), QStringLiteral("Stop recording")));

    if (m_catModel) {
        for (int i = 0; i < m_catModel->rowCount(); ++i) {
            auto *item = m_catModel->item(i);
            if (!item) continue;
            const QString key = item->data(Qt::UserRole).toString();
            if (key == QStringLiteral("__FAVORITES__"))
                item->setText(t(QStringLiteral("★ Favoriler"), QStringLiteral("★ Favorites")));
            else if (key.isEmpty())
                item->setText(t(QStringLiteral("Tümü"), QStringLiteral("All")));
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
        auto *toggleAct = new QAction(t(QStringLiteral("Arayüzü Göster/Gizle"), QStringLiteral("Show/Hide Interface")), trayMenu);
        connect(toggleAct, &QAction::triggered, this, [this]{
            if (isVisible()) hide();
            else { showNormal(); raise(); activateWindow(); }
        });

        auto *muteAct = new QAction(t(QStringLiteral("Sesi Kapat/Aç"), QStringLiteral("Mute/Unmute")), trayMenu);
        connect(muteAct, &QAction::triggered, this, [this]{
            if (m_mpv) m_mpv->setVolume(m_mpv->volume() > 0 ? 0 : m_config.volume);
        });

        auto *quitAct = new QAction(t(QStringLiteral("Çıkış"), QStringLiteral("Exit")), trayMenu);
        connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);

        trayMenu->addAction(toggleAct);
        trayMenu->addAction(muteAct);
        trayMenu->addSeparator();
        trayMenu->addAction(quitAct);
        m_trayIcon->setContextMenu(trayMenu);
    }
}

// ─── Dynamic theme ──────────────────────────────────────────────────────────────

void MainWindow::applyTheme()
{
    m_theme = Theme::colors(m_config);
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    qApp->setStyleSheet(Theme::style(m_config));

    // Only re-apply inline styles if UI has been set up already
    if (m_controlBar)
        applyInlineStyles();
}

void MainWindow::applyInlineStyles()
{
    // Sidebar
    if (m_sidebar)
        m_sidebar->setStyleSheet(QStringLiteral(
            "QWidget#sidebar { background: qlineargradient(x1:0,y1:0,x2:0,y2:1,"
            "stop:0 %1, stop:1 %2); "
            "border-right: 1px solid %3; }")
            .arg(m_theme.sidebarGradStart, m_theme.sidebarGradEnd, m_theme.sidebarBorder));

    // Control bar
    if (m_controlBar) {
        m_controlBar->setStyleSheet(QStringLiteral(
            "QWidget#controlBar { background: %1; border-top: 1px solid %2; }")
            .arg(m_theme.bgSurface, m_theme.borderSubtle));

        const QString btnStyle = QStringLiteral(
            "QToolButton { border-radius: 4px; padding: 0px; border: none; background: transparent; }"
            "QToolButton:hover { background: %1; }"
            "QToolButton:pressed { background: %2; }").arg(m_theme.hoverBg, m_theme.pressedBg);
            
        auto applyBtnStyle = [&](QToolButton* btn) {
            if (btn) btn->setStyleSheet(btnStyle);
        };
        applyBtnStyle(m_prevChBtn);
        applyBtnStyle(m_skipBackBtn);
        applyBtnStyle(m_playPauseBtn);
        applyBtnStyle(m_skipFwdBtn);
        applyBtnStyle(m_recordBtn);
        applyBtnStyle(m_recordPauseBtn);
        applyBtnStyle(m_recordStopBtn);
        applyBtnStyle(m_nextChBtn);
        applyBtnStyle(m_audioBtn);
        applyBtnStyle(m_subBtn);
        applyBtnStyle(m_infoBtn);
        applyBtnStyle(m_favoriteBtn);
        applyBtnStyle(m_multiViewBtn);
        applyBtnStyle(m_epgToggleBtn);
        applyBtnStyle(m_pipBtn);
        applyBtnStyle(m_fullscreenBtn);
        
        for (auto *frame : m_controlBar->findChildren<QFrame *>()) {
            if (frame->frameShape() == QFrame::VLine) {
                frame->setStyleSheet(QStringLiteral("background: %1;").arg(m_theme.separator));
            }
        }
    }

    // Time / live labels
    if (m_timeLabel)
        m_timeLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 11px; min-width: 90px; font-weight: 500;").arg(m_theme.textSecondary));
    if (m_liveLabel)
        m_liveLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; font-weight: bold; "
            "background: %2; border-radius: 3px; padding: 1px 6px;")
            .arg(m_theme.error, m_theme.errorAlpha));

    // EPG labels
    if (m_epgCurrentLabel)
        m_epgCurrentLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-weight: 600; font-size: 13px;").arg(m_theme.textPrimary));
    if (m_epgProgress)
        m_epgProgress->setStyleSheet(QStringLiteral(
            "QProgressBar { background: %1; border: none; } "
            "QProgressBar::chunk { background-color: %2; }")
            .arg(m_theme.epgProgressBg, m_theme.epgProgressChunk));

    // Icons
    if (m_playPauseBtn && m_mpv)
        m_playPauseBtn->setIcon(m_mpv->isPaused()
            ? Icons::play(m_theme.iconAccent) : Icons::pause(m_theme.iconAccent));
    if (m_favoriteBtn)
        m_favoriteBtn->setIcon(m_currentChannel.isFavorite
            ? Icons::starFilled(m_theme.iconFavActive)
            : Icons::starOutline(m_theme.iconDefault));

    if (m_prevChBtn)    m_prevChBtn->setIcon(Icons::skipPrevious(m_theme.iconDefault));
    if (m_skipBackBtn)  m_skipBackBtn->setIcon(Icons::rewind(m_theme.iconDefault));
    if (m_skipFwdBtn)   m_skipFwdBtn->setIcon(Icons::fastForward(m_theme.iconDefault));
    if (m_nextChBtn)    m_nextChBtn->setIcon(Icons::skipNext(m_theme.iconDefault));

    bool isRecording = !m_recordFilePath.isEmpty();
    if (m_recordBtn) {
        if (isRecording) {
            m_recordBtn->setIcon(m_recordPaused ? Icons::record(m_theme.iconRecord) : Icons::record(m_theme.iconError));
        } else {
            m_recordBtn->setIcon(Icons::record(m_theme.iconRecord));
        }
    }
    if (m_recordPauseBtn) m_recordPauseBtn->setIcon(m_recordPaused ? Icons::play(m_theme.iconAccent) : Icons::pause(m_theme.iconDefault));
    if (m_recordStopBtn)  m_recordStopBtn->setIcon(Icons::stop(m_theme.iconError));

    if (m_audioBtn)       m_audioBtn->setIcon(Icons::audioTrack(m_theme.iconDefault));
    if (m_subBtn)         m_subBtn->setIcon(Icons::closedCaption(m_theme.iconDefault));
    if (m_infoBtn)        m_infoBtn->setIcon(Icons::info(m_theme.iconDefault));
            
    if (m_multiViewBtn)   m_multiViewBtn->setIcon(Icons::gridView(m_theme.iconDefault));
    if (m_epgToggleBtn)   m_epgToggleBtn->setIcon(Icons::tvGuide(m_theme.iconDefault));
    if (m_pipBtn)         m_pipBtn->setIcon(Icons::pictureInPicture(m_theme.iconDefault));
    if (m_fullscreenBtn)  m_fullscreenBtn->setIcon(Icons::fullscreen(m_theme.iconDefault));

    if (m_volIcon)        m_volIcon->setIcon(Icons::volumeUp(m_theme.iconMuted));

    // Source panel buttons
    const QString srcBtnStyle = QStringLiteral(
        "QToolButton { border: none; border-radius: 4px; background: transparent; }"
        "QToolButton:hover { background: %1; }"
        "QToolButton:pressed { background: %2; }").arg(m_theme.hoverBg, m_theme.pressedBg);

    if (m_addSourceBtn) {
        m_addSourceBtn->setStyleSheet(srcBtnStyle);
        m_addSourceBtn->setIcon(Icons::add(m_theme.iconSuccess));
    }
    if (m_editSourceBtn) {
        m_editSourceBtn->setStyleSheet(srcBtnStyle);
        m_editSourceBtn->setIcon(Icons::edit(m_theme.iconDefault));
    }
    if (m_delSourceBtn) {
        m_delSourceBtn->setStyleSheet(srcBtnStyle);
        m_delSourceBtn->setIcon(Icons::trash(m_theme.iconError));
    }
}

// ─── UI setup ────────────────────────────────────────────────────────────────

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
    m_addSourceBtn->setToolTip(t(QStringLiteral("Kaynak ekle"), QStringLiteral("Add source")));
    m_addSourceBtn->setFixedSize(30, 30);
    m_addSourceBtn->setStyleSheet(srcBtnStyle);

    m_editSourceBtn = new QToolButton;
    m_editSourceBtn->setIcon(Icons::edit(m_theme.iconDefault));
    m_editSourceBtn->setIconSize(QSize(16, 16));
    m_editSourceBtn->setToolTip(t(QStringLiteral("Kaynağı düzenle"), QStringLiteral("Edit source")));
    m_editSourceBtn->setFixedSize(30, 30);
    m_editSourceBtn->setStyleSheet(srcBtnStyle);

    m_delSourceBtn  = new QToolButton;
    m_delSourceBtn->setIcon(Icons::trash(m_theme.iconError));
    m_delSourceBtn->setIconSize(QSize(16, 16));
    m_delSourceBtn->setToolTip(t(QStringLiteral("Kaynağı sil"), QStringLiteral("Delete source")));
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
    m_typeTab->addTab(new QWidget, t(QStringLiteral("Canli"), QStringLiteral("Live")));
    m_typeTab->addTab(new QWidget, t(QStringLiteral("Film"), QStringLiteral("Movies")));
    m_typeTab->addTab(new QWidget, t(QStringLiteral("Dizi"), QStringLiteral("Series")));
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
    m_channelHeaderLabel = new QLabel(t(QStringLiteral("Kanallar"), QStringLiteral("Channels")));
    m_channelHeaderLabel->setStyleSheet(QStringLiteral(
        "color: %1; font-size: 10px; font-weight: bold; "
        "text-transform: uppercase; padding: 6px 4px 2px 4px; "
        "border-top: 1px solid %2;").arg(m_theme.textDimmed, m_theme.borderSubtle));
    chLay->addWidget(m_channelHeaderLabel);

    auto *filterLay = new QHBoxLayout;
    filterLay->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(t(QStringLiteral("Kanal ara..."), QStringLiteral("Search channel...")));
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
    m_viewModeCombo->addItem(t(QStringLiteral("Liste"), QStringLiteral("List")));
    m_viewModeCombo->addItem(QStringLiteral("Grid"));
    m_viewModeCombo->setToolTip(t(QStringLiteral("Görünüm Modu"), QStringLiteral("View Mode")));
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
        auto *mvMenu = menu.addMenu(Icons::gridView(m_theme.iconDefault), t(QStringLiteral("Çoklu Ekrana Gönder..."), QStringLiteral("Send to Multi View...")));

        for (int i = 0; i < 4; ++i) {
            auto *action = mvMenu->addAction(t(QStringLiteral("Ekran %1"), QStringLiteral("Screen %1")).arg(i + 1));
            connect(action, &QAction::triggered, this, [this, item, i]() {
                const QString url = item.data(Qt::UserRole).toString();
                Channel ch;
                for (const Channel &c : m_allChannels) {
                    if (c.streamUrl == url) { ch = c; break; }
                }
                if (ch.streamUrl.isEmpty()) return;

                if (!m_multiViewDialog) {
                    onOpenMultiView(); 
                } else {
                    m_multiViewDialog->show();
                    m_multiViewDialog->raise();
                    m_multiViewDialog->activateWindow();
                }
                m_multiViewDialog->playChannel(i, ch);
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

        // Apply extra args if any
        if (!m_config.mpvExtraArgs.isEmpty()) {
            const QStringList args = m_config.mpvExtraArgs.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &arg : args) {
                const int eq = arg.indexOf(QLatin1Char('='));
                if (eq > 0)
                    m_mpv->setMpvProperty(arg.left(eq), arg.mid(eq + 1));
            }
        }
    } catch (const std::exception &e) {
                QMessageBox::critical(this, t(QStringLiteral("Kritik Hata"), QStringLiteral("Critical Error")),
                        t(QStringLiteral("Video oynatıcı başlatılamadı:\n%1\n\nLütfen grafik sürücülerinizi kontrol edin."),
                            QStringLiteral("Video player could not be initialized:\n%1\n\nPlease check your graphics drivers.")).arg(e.what()));
        m_mpv = nullptr;
    }

    if (m_mpv) {
        lay->addWidget(m_mpv, 1);
        
        // Connect mpv signals
        connect(m_mpv, &MpvWidget::positionChanged, this, &MainWindow::onPositionChanged);
        connect(m_mpv, &MpvWidget::durationChanged,  this, &MainWindow::onDurationChanged);
        connect(m_mpv, &MpvWidget::pauseChanged,     this, &MainWindow::onPauseChanged);
        
        // Non-blocking error handler — avoids freeze on bad channels
        connect(m_mpv, &MpvWidget::errorOccurred, this, [this](const QString &msg){
            m_mpv->stop(); // Stop playback to prevent further errors
            statusBar()->showMessage(t(QStringLiteral("Oynatma hatası: "), QStringLiteral("Playback error: ")) + msg, 8000);
        });
    } else {
        auto *errLbl = new QLabel(t(QStringLiteral("Oynatıcı Yüklenemedi\n(Grafik sürücü hatası)"),
                        QStringLiteral("Player Failed to Load\n(Graphics driver error)")));
        errLbl->setAlignment(Qt::AlignCenter);
        errLbl->setStyleSheet(QStringLiteral("color: %1; font-size: 16px; font-weight: bold;").arg(m_theme.error));
        lay->addWidget(errLbl, 1);
    }

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
    m_prevChBtn    = makeBtn(Icons::skipPrevious(m_theme.iconDefault), t(QStringLiteral("Onceki kanal (P)"), QStringLiteral("Previous channel (P)")), 30);
    m_skipBackBtn  = makeBtn(Icons::rewind(m_theme.iconDefault),       t(QStringLiteral("10 sn geri"), QStringLiteral("Back 10s")), 30);
    m_playPauseBtn = makeBtn(Icons::play(m_theme.iconAccent),          t(QStringLiteral("Oynat / Duraklat"), QStringLiteral("Play / Pause")), 36);
    m_skipFwdBtn   = makeBtn(Icons::fastForward(m_theme.iconDefault),  t(QStringLiteral("10 sn ileri"), QStringLiteral("Forward 10s")), 30);
    m_recordBtn      = makeBtn(Icons::record(m_theme.iconRecord),       t(QStringLiteral("Kaydı Başlat"), QStringLiteral("Start recording")), 30);
    m_recordPauseBtn = makeBtn(Icons::pause(m_theme.iconDefault),       t(QStringLiteral("Kaydı Duraklat"), QStringLiteral("Pause recording")), 30);
    m_recordStopBtn  = makeBtn(Icons::stop(m_theme.iconError),          t(QStringLiteral("Kaydı Durdur"), QStringLiteral("Stop recording")), 30);
    m_recordPauseBtn->setVisible(false);
    m_recordStopBtn->setVisible(false);
    m_nextChBtn    = makeBtn(Icons::skipNext(m_theme.iconDefault),     t(QStringLiteral("Sonraki kanal (N)"), QStringLiteral("Next channel (N)")), 30);

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
    m_volumeSlider->setValue(m_config.volume);
    m_volumeSlider->setFixedWidth(75);
    m_volumeSlider->setToolTip(t(QStringLiteral("Ses"), QStringLiteral("Volume")));

    // ── Track / tools group ──
    m_audioBtn   = makeBtn(Icons::audioTrack(m_theme.iconDefault),     t(QStringLiteral("Ses parcasi sec (A)"), QStringLiteral("Select audio track (A)")), 32);
    m_subBtn     = makeBtn(Icons::closedCaption(m_theme.iconDefault),  t(QStringLiteral("Altyazi sec (S)"), QStringLiteral("Select subtitle (S)")), 32);
    m_infoBtn    = makeBtn(Icons::info(m_theme.iconDefault),           t(QStringLiteral("Medya bilgisi (I)"), QStringLiteral("Media info (I)")), 28);

    m_speedCombo = new QComboBox;
    m_speedCombo->addItems({QStringLiteral("0.5x"), QStringLiteral("0.75x"),
                            QStringLiteral("1x"), QStringLiteral("1.25x"),
                            QStringLiteral("1.5x"), QStringLiteral("2x")});
    m_speedCombo->setCurrentIndex(2);
    m_speedCombo->setFixedWidth(58);
    m_speedCombo->setToolTip(t(QStringLiteral("Oynatma hizi"), QStringLiteral("Playback speed")));

    // ── Utility group ──
    m_favoriteBtn   = makeBtn(Icons::starOutline(m_theme.iconDefault),      t(QStringLiteral("Favorilere ekle / cikar (F)"), QStringLiteral("Toggle favorite (F)")), 32);
    m_multiViewBtn  = makeBtn(Icons::gridView(m_theme.iconDefault),         t(QStringLiteral("Çoklu Ekran (Multi-View)"), QStringLiteral("Multi-View")), 32);
    m_epgToggleBtn  = makeBtn(Icons::tvGuide(m_theme.iconDefault),          t(QStringLiteral("EPG panelini goster/gizle"), QStringLiteral("Show/hide EPG panel")), 32);
    m_pipBtn        = makeBtn(Icons::pictureInPicture(m_theme.iconDefault), t(QStringLiteral("Resim icinde resim"), QStringLiteral("Picture in picture")), 32);
    m_fullscreenBtn = makeBtn(Icons::fullscreen(m_theme.iconDefault),       t(QStringLiteral("Tam ekran (F11)"), QStringLiteral("Fullscreen (F11)")), 32);

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
        m_mpv->seek(m_seekSlider->value() / 1000.0 * m_mpv->duration());
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

    m_mpv->setVolume(m_config.volume);
}

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu(t(QStringLiteral("Dosya"), QStringLiteral("File")));
    fileMenu->addAction(t(QStringLiteral("Kaynak Ekle…"), QStringLiteral("Add Source…")),  this, &MainWindow::onAddSource);
    fileMenu->addAction(t(QStringLiteral("Kaynağı Düzenle…"), QStringLiteral("Edit Source…")), this, &MainWindow::onEditSource);
    fileMenu->addAction(t(QStringLiteral("Kaynağı Sil"), QStringLiteral("Delete Source")),   this, &MainWindow::onRemoveSource);
    fileMenu->addSeparator();
    fileMenu->addAction(t(QStringLiteral("Çıkış"), QStringLiteral("Exit")), qApp, &QApplication::quit);

    auto *viewMenu = menuBar()->addMenu(t(QStringLiteral("Görünüm"), QStringLiteral("View")));
    viewMenu->addAction(t(QStringLiteral("Tam Ekran (F11)"), QStringLiteral("Fullscreen (F11)")), this, &MainWindow::toggleFullScreen);
    viewMenu->addAction(t(QStringLiteral("EPG Panelini Göster/Gizle"), QStringLiteral("Show/Hide EPG Panel")), this, &MainWindow::onToggleEpg);

    auto *settingsMenu = menuBar()->addMenu(t(QStringLiteral("Ayarlar"), QStringLiteral("Settings")));
    settingsMenu->addAction(t(QStringLiteral("Tercihler…"), QStringLiteral("Preferences…")), this, &MainWindow::onOpenSettings);

    auto *helpMenu = menuBar()->addMenu(t(QStringLiteral("Yardım"), QStringLiteral("Help")));
    viewMenu->addAction(t(QStringLiteral("PiP Modu"), QStringLiteral("PiP Mode")), this, &MainWindow::onTogglePip);

    helpMenu->addAction(t(QStringLiteral("Klavye Kısayolları"), QStringLiteral("Keyboard Shortcuts")), this, [this]() {
        QMessageBox::information(this, t(QStringLiteral("Klavye Kısayolları"), QStringLiteral("Keyboard Shortcuts")),
            t(QStringLiteral(
                "Space        Oynat / Duraklat\n"
                "F11          Tam Ekran\n"
                "Escape       Tam Ekrandan Çık\n"
                "M            Sessiz Aç / Kapat\n"
                "←  →         10 Saniye Geri / İleri\n"
                "↑  ↓         Ses +5 / -5\n"
                "F            Favori Ekle / Çıkar\n"
                "N            Sonraki Kanal\n"
                "P            Önceki Kanal\n"
                "I            Medya Bilgisi\n"
                "A            Ses Parçası Seç\n"
                "S            Altyazı Seç"),
              QStringLiteral(
                "Space        Play / Pause\n"
                "F11          Fullscreen\n"
                "Escape       Exit Fullscreen\n"
                "M            Mute / Unmute\n"
                "←  →         Seek -10s / +10s\n"
                "↑  ↓         Volume +5 / -5\n"
                "F            Toggle Favorite\n"
                "N            Next Channel\n"
                "P            Previous Channel\n"
                "I            Media Info\n"
                "A            Select Audio Track\n"
                "S            Select Subtitle")));
    });
    helpMenu->addSeparator();
    helpMenu->addAction(t(QStringLiteral("Hakkında"), QStringLiteral("About")), this, [this](){
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
    sc(m_config.shortcuts.value(QStringLiteral("mute")),       [this]{ m_mpv->setVolume(m_mpv->volume() == 0 ? m_config.volume : 0); });
    
    // Fixed seeking shortcuts
    sc(QStringLiteral("Right"), [this]{ m_mpv->seek(m_mpv->position() + 10); });
    sc(QStringLiteral("Left"),  [this]{ m_mpv->seek(m_mpv->position() - 10); });
    
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

// ─── Source management ───────────────────────────────────────────────────────

void MainWindow::loadSourceList()
{
    m_sourceCombo->blockSignals(true);
    m_sourceCombo->clear();
    for (const Source &s : m_config.sources)
        m_sourceCombo->addItem(s.name, s.id);
    m_sourceCombo->blockSignals(false);

    // Restore last selected source
    int idx = m_sourceCombo->findData(m_config.lastSourceId);
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
    if (QMessageBox::question(this, t(QStringLiteral("Sil"), QStringLiteral("Delete")),
            t(QStringLiteral("Kaynağı silmek istiyor musunuz?"), QStringLiteral("Do you want to delete this source?")))
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
            retranslateUi();
            statusBar()->showMessage(
                t(QStringLiteral("Dil anında güncellendi."), QStringLiteral("Language updated instantly.")),
                2500
            );
        }
        if (m_trayIcon) {
            m_trayIcon->setVisible(m_config.minimizeToTray);
        }
    }
}

// ─── Navigation ──────────────────────────────────────────────────────────────

void MainWindow::onStreamTypeChanged(int tab)
{
    m_streamType = static_cast<StreamType>(tab);  // 0=Live 1=VOD 2=Series
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
        auto *favItem = new QStandardItem(t(QStringLiteral("★ Favoriler"), QStringLiteral("★ Favorites")));
        favItem->setData(QStringLiteral("__FAVORITES__"), Qt::UserRole);
        favItem->setForeground(QBrush(QColor(m_theme.warning)));
        items.append(favItem);

        auto *allItem = new QStandardItem(t(QStringLiteral("Tümü"), QStringLiteral("All")));
        allItem->setData(QString{}, Qt::UserRole);
        items.append(allItem);

        for (const Category &c : cats) {
            auto *item = new QStandardItem(c.name);
            item->setData(c.id, Qt::UserRole);
            items.append(item);
        }
        m_catModel->appendColumn(items);
        if (m_catModel->rowCount() > 1) {
            m_categoryList->setCurrentIndex(m_catModel->index(1, 0));
            loadChannels({});
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

    statusBar()->showMessage(t(QStringLiteral("Kategoriler yükleniyor…"), QStringLiteral("Loading categories…")));
    m_loadingProgress->setVisible(true);
    m_loadingProgress->setRange(0, 0); // indeterminate

    auto onDone = [this, populateCats, key](QList<Category> cats, QString err) {
        statusBar()->clearMessage();
        m_loadingProgress->setVisible(false);
        if (!err.isEmpty()) {
            statusBar()->showMessage(t(QStringLiteral("Kategori yükleme hatası: "), QStringLiteral("Category load error: ")) + err, 6000);
            return;
        }
        m_categoryCache.insert(key, cats);
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
    loadChannels(item.data(Qt::UserRole).toString());
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
        if (m_resumePending && !m_config.lastChannelUrl.isEmpty()) {
            m_resumePending = false;
            const QString resumeUrl = m_config.lastChannelUrl;
            QTimer::singleShot(1000, this, [this, resumeUrl]() {
                for (int i = 0; i < m_allChannels.size(); ++i) {
                    if (m_allChannels[i].streamUrl == resumeUrl) {
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
        statusBar()->showMessage(t(QStringLiteral("M3U yükleniyor… (Bu işlem birkaç dakika sürebilir)"),
                       QStringLiteral("Loading M3U… (This may take a few minutes)")));
        
        m_loadingProgress->setVisible(true);
        m_loadingProgress->setRange(0, 0); // indeterminate

        auto *nam  = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(src.m3uUrl)};
        req.setRawHeader("User-Agent", "Vibestreamer/2.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(60000); // Increased timeout to 60s
        QNetworkReply *reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, nam, src, finalize]() {
            if (reply->error() != QNetworkReply::NoError) {
                const int currentSrcIdx = m_sourceCombo->currentIndex();
                bool isSameSource = (currentSrcIdx >= 0 && m_config.sources[currentSrcIdx].id == src.id);
                if (isSameSource) {
                    m_loadingChannels = false;
                    m_loadingProgress->setVisible(false);
                    statusBar()->showMessage(
                        t(QStringLiteral("M3U yükleme hatası: "), QStringLiteral("M3U load error: ")) + reply->errorString(), 5000);
                } else {
                    m_loadingChannels = false;
                }
                reply->deleteLater();
                nam->deleteLater();
                return;
            }

            const QByteArray raw = reply->readAll();
            reply->deleteLater();
            nam->deleteLater();

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
                        statusBar()->showMessage(t(QStringLiteral("M3U boş veya geçersiz format"), QStringLiteral("M3U is empty or invalid format")), 4000);
                    }
                    return;
                }

                m_m3uCache.insert(src.id, all);

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
        statusBar()->showMessage(t(QStringLiteral("Favoriler yükleniyor…"), QStringLiteral("Loading favorites…")));
        auto onAllDone = [this, finalize, allKey](QList<Channel> channels, QString err) {
            m_loadingChannels = false;
            m_loadingProgress->setVisible(false);
            statusBar()->clearMessage();
            if (!err.isEmpty()) {
                statusBar()->showMessage(t(QStringLiteral("Kanal yükleme hatası: "), QStringLiteral("Channel load error: ")) + err, 6000);
                return;
            }
            m_channelCache.insert(allKey, channels);
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
    statusBar()->showMessage(t(QStringLiteral("Kanallar yükleniyor…"), QStringLiteral("Loading channels…")));

    auto onDone = [this, finalize, key](QList<Channel> channels, QString err) {
        m_loadingChannels = false;
        m_loadingProgress->setVisible(false);
        statusBar()->clearMessage();
        if (!err.isEmpty()) {
            statusBar()->showMessage(t(QStringLiteral("Kanal yükleme hatası: "), QStringLiteral("Channel load error: ")) + err, 6000);
            return;
        }
        m_channelCache.insert(key, channels);
        finalize(channels);
    };

    switch (m_streamType) {
    case StreamType::Live:   m_client->getLiveStreams(categoryId, onDone);  break;
    case StreamType::VOD:    m_client->getVodStreams(categoryId, onDone);   break;
    case StreamType::Series: m_client->getSeries(categoryId, onDone);      break;
    }
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

    // Stop any active recording before switching channels
    if (m_isRecording)
        onRecordStop();

    m_config.lastChannelUrl = ch.streamUrl;

    // Apply hw decode option (runtime property, not pre-init option)
    m_mpv->setMpvProperty(QStringLiteral("hwdec"), m_config.mpvHwDecode);
    if (!m_config.mpvExtraArgs.isEmpty()) {
        const QStringList parts = m_config.mpvExtraArgs.split(QLatin1Char(' '),
                                                               Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const int eq = part.indexOf(QLatin1Char('='));
            if (eq > 0)
                m_mpv->setMpvProperty(part.left(eq), part.mid(eq + 1));
        }
    }

    m_mpv->play(ch.streamUrl);

    // Apply speed — allow any speed for all stream types so users can catch up
    if (m_speedCombo)
        onSpeedChanged(m_speedCombo->currentIndex());

    setWindowTitle(ch.name + QStringLiteral(" — Vibestreamer"));
    statusBar()->showMessage(ch.name);
}

// ─── EPG ─────────────────────────────────────────────────────────────────────

void MainWindow::updateInfoPanel(const Channel &ch)
{
    m_epgProgress->setVisible(false);
    QString header = ch.name;
    if (!ch.year.isEmpty())
        header += QStringLiteral(" (") + ch.year + QStringLiteral(")");
    if (!ch.rating.isEmpty())
        header += QStringLiteral("  ★ ") + ch.rating;
    if (!ch.genre.isEmpty())
        header += QStringLiteral("  · ") + ch.genre;
    m_epgCurrentLabel->setText(header);
    m_epgNextLabel->setText(ch.plot.isEmpty() ? QString{} : ch.plot.left(220));
}

void MainWindow::updateEpgPanel(const Channel &ch)
{
    m_epgProgress->setVisible(true);
    const EpgProgram cur  = m_epg->currentProgram(ch.epgChannelId);
    const EpgProgram next = m_epg->nextProgram(ch.epgChannelId);

    if (cur.title.isEmpty()) {
        m_epgCurrentLabel->setText(t(QStringLiteral("EPG verisi bulunamadı"), QStringLiteral("No EPG data")));
        m_epgCurrentLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-weight: 500; font-size: 12px; font-style: italic;").arg(m_theme.textDimmed));
        m_epgProgress->setValue(0);
        m_epgProgress->setVisible(false);
    } else {
        const qint64 now  = QDateTime::currentSecsSinceEpoch();
        const qint64 dur  = cur.stopTs - cur.startTs;
        const qint64 elapsed = now - cur.startTs;
        const int pct = dur > 0 ? static_cast<int>(elapsed * 100 / dur) : 0;
        const QString endTime = QDateTime::fromSecsSinceEpoch(cur.stopTs)
                                    .toString(QStringLiteral("HH:mm"));
        m_epgCurrentLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-weight: 600; font-size: 13px;").arg(m_theme.textPrimary));
        m_epgCurrentLabel->setText(cur.title + QStringLiteral("  (→ ") + endTime + QStringLiteral(")"));
        m_epgProgress->setVisible(true);
        m_epgProgress->setValue(pct);
    }

    m_epgNextLabel->setText(next.title.isEmpty()
        ? QString{}
        : t(QStringLiteral("Sonraki: "), QStringLiteral("Next: ")) + next.title);
}

void MainWindow::refreshEpg()
{
    const int idx = m_sourceCombo->currentIndex();
    if (idx < 0 || idx >= m_config.sources.size()) return;
    const Source &src = m_config.sources[idx];

    if (!src.epgUrl.isEmpty()) {
        m_epg->load(src.epgUrl);
    } else if (m_client) {
        m_epg->load(m_client->epgUrl());
    }
}

// ─── Controls ────────────────────────────────────────────────────────────────

void MainWindow::onPlayPause()
{
    m_mpv->setPause(!m_mpv->isPaused());
}

void MainWindow::onPauseChanged(bool paused)
{
    m_playPauseBtn->setIcon(paused ? Icons::play(m_theme.iconAccent) : Icons::pause(m_theme.iconAccent));
}

void MainWindow::onVolumeChanged(int value)
{
    m_mpv->setVolume(value);
    m_config.volume = value;
    statusBar()->showMessage(QStringLiteral("Ses: %1%").arg(value), 1500);
}

void MainWindow::onSpeedChanged(int index)
{
    if (!m_mpv || !m_speedCombo) return;
    constexpr double speeds[] = {0.5, 0.75, 1.0, 1.25, 1.5, 2.0};
    if (index >= 0 && index < 6)
        m_mpv->setSpeed(speeds[index]);
}

void MainWindow::onPositionChanged(double secs)
{
    if (!m_seeking) {
        const double dur = m_mpv->duration();
        if (dur > 0)
            m_seekSlider->setValue(static_cast<int>(secs / dur * 1000));
        m_timeLabel->setText(formatTime(secs) + QStringLiteral(" / ")
                             + formatTime(dur));
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
                m_trayIcon->showMessage(t(QStringLiteral("Playlist Güncellemesi"), QStringLiteral("Playlist Update")), 
                                        src.name + t(QStringLiteral(" kaynak verileri yenileniyor..."), QStringLiteral(" source data is being refreshed...")), 
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

void MainWindow::onDurationChanged(double secs)
{
    const bool isLive = (m_currentChannel.streamType == StreamType::Live);

    // Seek slider stays visible for all streams (live allows rewind)
    m_seekSlider->setEnabled(secs > 0);
    m_liveLabel->setVisible(isLive);

    m_timeLabel->setText(formatTime(m_mpv->position()) + QStringLiteral(" / ")
                         + formatTime(secs));
}

void MainWindow::onToggleFavorite()
{
    if (m_currentChannel.streamUrl.isEmpty()) return;
    const bool isFav = m_config.toggleFavorite(m_currentChannel.streamUrl);
    m_currentChannel.isFavorite = isFav;
    for (int i = 0; i < m_chanModel->rowCount(); ++i) {
        auto *item = m_chanModel->item(i);
        if (item->data(Qt::UserRole).toString() == m_currentChannel.streamUrl) {
            item->setText((isFav ? QStringLiteral("★ ") : QString{}) + m_currentChannel.name);
            break;
        }
    }
    updateFavoriteButton(m_currentChannel);
}

void MainWindow::updateFavoriteButton(const Channel &ch)
{
    m_favoriteBtn->setIcon(ch.isFavorite
        ? Icons::starFilled(m_theme.iconFavActive)
        : Icons::starOutline(m_theme.iconDefault));
}

void MainWindow::onToggleEpg()
{
    m_config.showEpg = !m_config.showEpg;
    m_epgPanel->setVisible(m_config.showEpg);
}

void MainWindow::toggleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
        m_sidebar->setVisible(true);
        menuBar()->setVisible(true);
        statusBar()->setVisible(true);
        if (!m_savedSplitterSizes.isEmpty())
            m_mainSplitter->setSizes(m_savedSplitterSizes);
    } else {
        m_savedSplitterSizes = m_mainSplitter->sizes();
        m_sidebar->setVisible(false);
        menuBar()->setVisible(false);
        statusBar()->setVisible(false);
        showFullScreen();
    }
}

void MainWindow::updateStyle()
{
    applyTheme();
}

// ─── New player controls ─────────────────────────────────────────────────────

void MainWindow::onSkipBack()
{
    m_mpv->seek(qMax(0.0, m_mpv->position() - 10));
}

void MainWindow::onSkipForward()
{
    m_mpv->seek(m_mpv->position() + 10);
}

void MainWindow::onPrevChannel()
{
    const int row = m_channelList->currentIndex().row();
    if (row > 0) {
        auto idx = m_proxyModel->index(row - 1, 0);
        m_channelList->setCurrentIndex(idx);
        onChannelDoubleClicked(idx);
    }
}

void MainWindow::onNextChannel()
{
    const int row = m_channelList->currentIndex().row();
    if (row >= 0 && row < m_proxyModel->rowCount() - 1) {
        auto idx = m_proxyModel->index(row + 1, 0);
        m_channelList->setCurrentIndex(idx);
        onChannelDoubleClicked(idx);
    }
}

void MainWindow::onShowAudioMenu()
{
    QMenu menu(this);
    const auto tracks = m_mpv->trackList();
    bool hasAudio = false;
    for (const TrackInfo &t : tracks) {
        if (t.type != QLatin1String("audio")) continue;
        hasAudio = true;
        QString label = this->t(QStringLiteral("Parça %1"), QStringLiteral("Track %1")).arg(t.id);
        if (!t.lang.isEmpty()) label += QStringLiteral(" [%1]").arg(t.lang);
        if (!t.title.isEmpty()) label += QStringLiteral(" - %1").arg(t.title);
        if (!t.codec.isEmpty()) label += QStringLiteral(" (%1)").arg(t.codec);
        QAction *act = menu.addAction(label);
        act->setCheckable(true);
        act->setChecked(t.selected);
        const int64_t id = t.id;
        connect(act, &QAction::triggered, this, [this, id]{ m_mpv->setAudioTrack(id); });
    }
    if (!hasAudio)
        menu.addAction(t(QStringLiteral("Ses parçası bulunamadı"), QStringLiteral("No audio tracks found")))->setEnabled(false);
    menu.exec(m_audioBtn->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void MainWindow::onShowSubMenu()
{
    QMenu menu(this);
    const auto tracks = m_mpv->trackList();
    bool hasSub = false;

    // "Off" option
    QAction *offAct = menu.addAction(t(QStringLiteral("Kapalı"), QStringLiteral("Off")));
    offAct->setCheckable(true);
    bool anySel = false;

    for (const TrackInfo &t : tracks) {
        if (t.type != QLatin1String("sub")) continue;
        hasSub = true;
        if (t.selected) anySel = true;
        QString label = this->t(QStringLiteral("Altyazı %1"), QStringLiteral("Subtitle %1")).arg(t.id);
        if (!t.lang.isEmpty()) label += QStringLiteral(" [%1]").arg(t.lang);
        if (!t.title.isEmpty()) label += QStringLiteral(" - %1").arg(t.title);
        QAction *act = menu.addAction(label);
        act->setCheckable(true);
        act->setChecked(t.selected);
        const int64_t id = t.id;
        connect(act, &QAction::triggered, this, [this, id]{ m_mpv->setSubtitleTrack(id); });
    }
    offAct->setChecked(!anySel);
    connect(offAct, &QAction::triggered, this, [this]{
        m_mpv->setSubtitleTrack(0);  // sid=0 disables subtitles
    });

    if (!hasSub && menu.actions().size() == 1)
        menu.addAction(t(QStringLiteral("Altyazı bulunamadı"), QStringLiteral("No subtitles found")))->setEnabled(false);
    menu.exec(m_subBtn->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void MainWindow::onShowMediaInfo()
{
    const auto tracks = m_mpv->trackList();
    QString info;

    // General
    info += t(QStringLiteral("Kanal: %1\n"), QStringLiteral("Channel: %1\n")).arg(m_currentChannel.name);
    if (!m_currentChannel.streamUrl.isEmpty())
        info += QStringLiteral("URL: %1\n").arg(m_currentChannel.streamUrl);
    info += QLatin1Char('\n');

    // Video
    const int vw = m_mpv->videoWidth();
    const int vh = m_mpv->videoHeight();
    if (vw > 0 && vh > 0)
        info += QStringLiteral("Video: %1×%2").arg(vw).arg(vh);
    const QString vc = m_mpv->videoCodec();
    if (!vc.isEmpty())
        info += QStringLiteral("  Codec: %1").arg(vc);
    info += QLatin1Char('\n');

    // Audio
    const QString ac = m_mpv->audioCodec();
    if (!ac.isEmpty())
        info += QStringLiteral("Audio Codec: %1\n").arg(ac);
    info += QLatin1Char('\n');

    // All tracks
    info += t(QStringLiteral("── Parçalar ──\n"), QStringLiteral("── Tracks ──\n"));
    for (const TrackInfo &t : tracks) {
        QString line = QStringLiteral("#%1 %2").arg(t.id).arg(t.type);
        if (!t.lang.isEmpty()) line += QStringLiteral(" [%1]").arg(t.lang);
        if (!t.title.isEmpty()) line += QStringLiteral(" %1").arg(t.title);
        if (!t.codec.isEmpty()) line += QStringLiteral(" (%1)").arg(t.codec);
        if (t.selected) line += QStringLiteral(" ✓");
        info += line + QLatin1Char('\n');
    }

    QMessageBox::information(this, t(QStringLiteral("Medya Bilgisi"), QStringLiteral("Media Info")), info);
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(QStringLiteral(":/logo_concept1.svg")));
    m_trayIcon->setToolTip(QStringLiteral("Vibestreamer"));

    auto *trayMenu = new QMenu(this);
    
    auto *toggleAct = new QAction(t(QStringLiteral("Arayüzü Göster/Gizle"), QStringLiteral("Show/Hide Interface")), this);
    connect(toggleAct, &QAction::triggered, this, [this]{
        if (isVisible()) hide();
        else { showNormal(); raise(); activateWindow(); }
    });
    
    auto *muteAct = new QAction(t(QStringLiteral("Sesi Kapat/Aç"), QStringLiteral("Mute/Unmute")), this);
    connect(muteAct, &QAction::triggered, this, [this]{
        if (m_mpv) m_mpv->setVolume(m_mpv->volume() > 0 ? 0 : m_config.volume);
    });
    
    auto *quitAct = new QAction(t(QStringLiteral("Çıkış"), QStringLiteral("Exit")), this);
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

void MainWindow::onToggleRecord()
{
    if (!m_mpv || m_currentChannel.streamUrl.isEmpty()) return;

    if (m_isRecording) {
        // If already recording, clicking record again acts as stop
        onRecordStop();
        return;
    }

    // Start recording
    QString dir = m_config.recordPath;
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

    QDir recordDir(dir);
    if (!recordDir.exists())
        recordDir.mkpath(QStringLiteral("."));

    QString safeName = m_currentChannel.name;
    safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    m_recordSegment = 0;
    const QString fileName = QStringLiteral("Record_%1_%2.ts").arg(safeName, timestamp);
    m_recordFilePath = recordDir.absoluteFilePath(fileName);

    m_mpv->setMpvProperty(QStringLiteral("stream-record"), m_recordFilePath);
    m_isRecording = true;
    m_recordPaused = false;

    // Update UI: show pause/stop, change record button to active state
    m_recordBtn->setIcon(Icons::record(m_theme.iconError));
    m_recordBtn->setToolTip(t(QStringLiteral("Kayıt devam ediyor"), QStringLiteral("Recording in progress")));
    m_recordBtn->setStyleSheet(QStringLiteral(
        "QToolButton { background: %1; border-radius: 4px; }").arg(m_theme.errorAlpha));
    m_recordPauseBtn->setVisible(true);
    m_recordStopBtn->setVisible(true);
    statusBar()->showMessage(t(QStringLiteral("Kayıt başladı: "), QStringLiteral("Recording started: ")) + m_recordFilePath, 5000);
}

void MainWindow::onRecordPauseResume()
{
    if (!m_mpv || !m_isRecording) return;

    if (!m_recordPaused) {
        // Pause: stop writing to file but keep the partial recording
        m_mpv->setMpvProperty(QStringLiteral("stream-record"), QString());
        m_recordPaused = true;
        m_recordPauseBtn->setIcon(Icons::play(m_theme.iconAccent));
        m_recordPauseBtn->setToolTip(t(QStringLiteral("Kaydı Devam Ettir"), QStringLiteral("Resume recording")));
        m_recordBtn->setIcon(Icons::record(m_theme.iconRecord));
        m_recordBtn->setStyleSheet(QString{});
        statusBar()->showMessage(t(QStringLiteral("Kayıt duraklatıldı. Dosya: "), QStringLiteral("Recording paused. File: ")) + m_recordFilePath, 5000);
    } else {
        // Resume: start a new segment file
        m_recordSegment++;
        // Insert segment number before extension
        QString resumed = m_recordFilePath;
        const int dotPos = resumed.lastIndexOf(QLatin1Char('.'));
        if (dotPos > 0)
            resumed.insert(dotPos, QStringLiteral("_part%1").arg(m_recordSegment));
        else
            resumed += QStringLiteral("_part%1").arg(m_recordSegment);

        m_mpv->setMpvProperty(QStringLiteral("stream-record"), resumed);
        m_recordPaused = false;
        m_recordPauseBtn->setIcon(Icons::pause(m_theme.iconDefault));
        m_recordPauseBtn->setToolTip(t(QStringLiteral("Kaydı Duraklat"), QStringLiteral("Pause recording")));
        m_recordBtn->setIcon(Icons::record(m_theme.iconError));
        m_recordBtn->setStyleSheet(QStringLiteral(
            "QToolButton { background: %1; border-radius: 4px; }").arg(m_theme.errorAlpha));
        statusBar()->showMessage(t(QStringLiteral("Kayıt devam ediyor: "), QStringLiteral("Recording resumed: ")) + resumed, 5000);
    }
}

void MainWindow::onRecordStop()
{
    if (!m_mpv) return;

    // Stop recording — finalize file
    m_mpv->setMpvProperty(QStringLiteral("stream-record"), QString());
    m_isRecording = false;
    m_recordPaused = false;

    // Reset UI
    m_recordBtn->setIcon(Icons::record(m_theme.iconRecord));
    m_recordBtn->setToolTip(t(QStringLiteral("Kaydı Başlat"), QStringLiteral("Start recording")));
    m_recordBtn->setStyleSheet(QString{});
    m_recordPauseBtn->setVisible(false);
    m_recordStopBtn->setVisible(false);

    statusBar()->showMessage(t(QStringLiteral("Kayıt kaydedildi: "), QStringLiteral("Recording saved: ")) + m_recordFilePath, 8000);
    m_recordFilePath.clear();
    m_recordSegment = 0;
}

void MainWindow::onOpenMultiView()
{
    if (!m_multiViewDialog) {
        m_multiViewDialog = new MultiViewDialog(this);
    }
    
    m_multiViewDialog->show();
    m_multiViewDialog->raise();
    m_multiViewDialog->activateWindow();
    
    // Allow it to run independently. You could also connect a signal to play the current channel
    if (!m_currentChannel.streamUrl.isEmpty()) {
        m_multiViewDialog->playChannel(0, m_currentChannel);
    }
}

void MainWindow::onTogglePip()
{
    if (m_pipMode) {
        // ── Exit PiP ──
        m_pipMode = false;

        // Stop raise timer
        if (m_pipRaiseTimer) {
            m_pipRaiseTimer->stop();
            m_pipRaiseTimer->deleteLater();
            m_pipRaiseTimer = nullptr;
        }

        // Remove overlay
        if (m_pipOverlay) {
            m_pipOverlay->deleteLater();
            m_pipOverlay = nullptr;
        }
        m_pipChannelLabel = nullptr; // parented to overlay, deleted with it

        // Restore min/max sizes
        setMinimumSize(m_savedMinSize);
        setMaximumSize(m_savedMaxSize);

        // Restore window flags (remove always-on-top + frameless)
        setWindowFlags(m_savedWindowFlags);
        show();

        // Restore UI
        m_sidebar->setVisible(true);
        menuBar()->setVisible(true);
        statusBar()->setVisible(true);
        m_controlBar->setVisible(true);
        m_epgPanel->setVisible(m_config.showEpg);
        setGeometry(m_savedGeometry);
        if (!m_savedSplitterSizes.isEmpty())
            m_mainSplitter->setSizes(m_savedSplitterSizes);

        // Remove event filter and restore cursor
        if (m_mpv) {
            m_mpv->removeEventFilter(this);
            m_mpv->setMouseTracking(false);
        }
        setCursor(Qt::ArrowCursor);
    } else {
        if (!m_mpv) return;
        // ── Enter PiP ──
        m_pipMode = true;
        m_savedGeometry = geometry();
        m_savedSplitterSizes = m_mainSplitter->sizes();
        m_savedWindowFlags = windowFlags();
        m_savedMinSize = minimumSize();
        m_savedMaxSize = maximumSize();

        // Hide everything except the player
        m_sidebar->setVisible(false);
        menuBar()->setVisible(false);
        statusBar()->setVisible(false);
        m_controlBar->setVisible(false);
        m_epgPanel->setVisible(false);

        // Make window frameless + always-on-top
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

        // Compute size from actual video aspect ratio
        const int vw = m_mpv->videoWidth();
        const int vh = m_mpv->videoHeight();
        double aspect = 16.0 / 9.0; // default
        if (vw > 0 && vh > 0)
            aspect = static_cast<double>(vw) / vh;

        const int pipW = 400;
        const int pipH = static_cast<int>(pipW / aspect);

        // Allow resize within reasonable bounds
        setMinimumSize(200, static_cast<int>(200 / aspect));
        setMaximumSize(800, static_cast<int>(800 / aspect));

        // Position at bottom-right of screen
        const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
        setGeometry(screen.right() - pipW - 20, screen.bottom() - pipH - 20, pipW, pipH);

        // Install event filter on m_mpv so we intercept mouse events
        // (child widgets consume events before MainWindow sees them)
        if (m_mpv) {
            m_mpv->installEventFilter(this);
            m_mpv->setMouseTracking(true);
        }
        show();
        raise();
        activateWindow();

        // Create overlay at the TOP of the player
        m_pipOverlay = new QWidget(m_playerPanel);
        m_pipOverlay->setStyleSheet(QStringLiteral(
            "background: %1; border-radius: 0px;").arg(m_theme.bgSurfaceAlpha));
        m_pipOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        auto *tbLay = new QHBoxLayout(m_pipOverlay);
        tbLay->setContentsMargins(8, 6, 8, 6);

        const QString ovBtnStyle = QStringLiteral(
            "QToolButton { color: rgba(255,255,255,0.75); font-size: 11px; font-weight: bold; "
            "padding: 3px 10px; border: none; background: %1; border-radius: 4px; }"
            "QToolButton:hover { color: #fff; background: %2; }")
            .arg(m_theme.bgInput, m_theme.hoverBg);

        auto *backBtn = new QToolButton;
        backBtn->setText(t(QStringLiteral("← Geri"), QStringLiteral("← Back")));
        backBtn->setCursor(Qt::PointingHandCursor);
        backBtn->setStyleSheet(ovBtnStyle);
        connect(backBtn, &QToolButton::clicked, this, &MainWindow::onTogglePip);

        auto *closeBtn = new QToolButton;
        closeBtn->setText(QStringLiteral("✕"));
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(QStringLiteral(
            "QToolButton { color: %1; font-size: 13px; font-weight: bold; "
            "padding: 3px 10px; border: none; background: %2; border-radius: 4px; }"
            "QToolButton:hover { color: #fff; background: %3; }")
            .arg(m_theme.error, m_theme.bgInput, m_theme.errorAlpha));
        connect(closeBtn, &QToolButton::clicked, this, [this]() {
            onTogglePip();
            if (m_mpv) m_mpv->stop();
        });

        m_pipChannelLabel = new QLabel(m_currentChannel.name);
        m_pipChannelLabel->setStyleSheet(QStringLiteral(
            "color: %1; font-size: 10px; background: transparent;").arg(m_theme.textSecondary));
        m_pipChannelLabel->setAlignment(Qt::AlignCenter);

        tbLay->addWidget(backBtn);
        tbLay->addWidget(m_pipChannelLabel, 1);
        tbLay->addWidget(closeBtn);

        // Position overlay at the TOP — hidden by default, shown on hover
        m_pipOverlay->setFixedHeight(34);
        m_pipOverlay->move(0, 0);
        m_pipOverlay->resize(m_playerPanel->width(), 34);
        m_pipOverlay->setVisible(false);
        m_pipOverlay->raise();
    }
}

// ─── Misc ─────────────────────────────────────────────────────────────────────

QString MainWindow::formatTime(double secs) const
{
    if (secs <= 0) return QStringLiteral("0:00");
    const int s = static_cast<int>(secs);
    const int h = s / 3600;
    const int m = (s % 3600) / 60;
    const int ss = s % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
               .arg(h).arg(m, 2, 10, QLatin1Char('0'))
               .arg(ss, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m)
           .arg(ss, 2, 10, QLatin1Char('0'));
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    // If in PiP mode, exit PiP instead of closing the app
    if (m_pipMode) {
        onTogglePip();
        e->ignore();
        return;
    }
    // Stop any active recording before closing
    if (m_isRecording)
        onRecordStop();

    m_config.windowWidth  = width();
    m_config.windowHeight = height();
    m_config.save();
    e->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape && isFullScreen())
        toggleFullScreen();
    else
        QMainWindow::keyPressEvent(e);
}

// ─── PiP event filter (drag & resize) ────────────────────────────────────────

static const int kResizeMargin = 8;

// Hit-test using LOCAL coordinates (0,0 = top-left of window)
static Qt::Edges hitTestEdges(int winW, int winH, const QPoint &localPos)
{
    Qt::Edges edges;
    if (localPos.x() <= kResizeMargin)          edges |= Qt::LeftEdge;
    if (localPos.x() >= winW - kResizeMargin)   edges |= Qt::RightEdge;
    if (localPos.y() <= kResizeMargin)           edges |= Qt::TopEdge;
    if (localPos.y() >= winH - kResizeMargin)    edges |= Qt::BottomEdge;
    return edges;
}

static Qt::CursorShape cursorForEdges(Qt::Edges edges)
{
    if ((edges & Qt::LeftEdge) && (edges & Qt::TopEdge))     return Qt::SizeFDiagCursor;
    if ((edges & Qt::RightEdge) && (edges & Qt::BottomEdge)) return Qt::SizeFDiagCursor;
    if ((edges & Qt::LeftEdge) && (edges & Qt::BottomEdge))  return Qt::SizeBDiagCursor;
    if ((edges & Qt::RightEdge) && (edges & Qt::TopEdge))    return Qt::SizeBDiagCursor;
    if (edges & (Qt::LeftEdge | Qt::RightEdge))              return Qt::SizeHorCursor;
    if (edges & (Qt::TopEdge | Qt::BottomEdge))              return Qt::SizeVerCursor;
    return Qt::ArrowCursor;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (!m_pipMode)
        return QMainWindow::eventFilter(obj, event);

    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
            return QMainWindow::eventFilter(obj, event);

        // Map child widget coords → MainWindow local coords
        const QPoint localPos = static_cast<QWidget *>(obj)->mapTo(this, me->pos());
        const Qt::Edges edges = hitTestEdges(width(), height(), localPos);

        // Try native system move/resize first
        QWindow *wh = windowHandle();
        if (edges) {
            if (wh && wh->startSystemResize(edges))
                return true;
            // Manual fallback
            m_pipResizing = true;
            m_pipDragging = false;
            m_pipResizeEdges = edges;
            m_pipResizeOrigin = me->globalPosition().toPoint();
            m_pipResizeGeom = geometry();
            return true;
        } else {
            if (wh && wh->startSystemMove())
                return true;
            // Manual fallback
            m_pipDragging = true;
            m_pipResizing = false;
            m_pipDragPos = me->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
    }

    if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(event);

        // Manual resize in progress
        if (m_pipResizing) {
            const QPoint gp = me->globalPosition().toPoint();
            const QPoint delta = gp - m_pipResizeOrigin;
            QRect g = m_pipResizeGeom;
            if (m_pipResizeEdges & Qt::LeftEdge)   g.setLeft(g.left() + delta.x());
            if (m_pipResizeEdges & Qt::RightEdge)  g.setRight(g.right() + delta.x());
            if (m_pipResizeEdges & Qt::TopEdge)    g.setTop(g.top() + delta.y());
            if (m_pipResizeEdges & Qt::BottomEdge) g.setBottom(g.bottom() + delta.y());
            g.setWidth(qBound(minimumWidth(), g.width(), maximumWidth()));
            g.setHeight(qBound(minimumHeight(), g.height(), maximumHeight()));
            setGeometry(g);
            return true;
        }

        // Manual drag in progress
        if (m_pipDragging) {
            move(me->globalPosition().toPoint() - m_pipDragPos);
            return true;
        }

        // Hover: update cursor based on edge proximity
        const QPoint localPos = static_cast<QWidget *>(obj)->mapTo(this, me->pos());
        const Qt::Edges edges = hitTestEdges(width(), height(), localPos);
        setCursor(cursorForEdges(edges));
        return false;
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && (m_pipDragging || m_pipResizing)) {
            m_pipDragging = false;
            m_pipResizing = false;
            return true;
        }
    }

    // Show/hide entire overlay on hover
    if (event->type() == QEvent::Enter) {
        if (m_pipOverlay) m_pipOverlay->setVisible(true);
    } else if (event->type() == QEvent::Leave) {
        if (m_pipOverlay && !m_pipDragging && !m_pipResizing)
            m_pipOverlay->setVisible(false);
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
    QMainWindow::resizeEvent(e);
    if (m_pipMode && m_pipOverlay) {
        m_pipOverlay->move(0, 0);
        m_pipOverlay->resize(m_playerPanel->width(), 34);
    }
}
