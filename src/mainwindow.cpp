#include "mainwindow.h"
#include "theme.h"

#include "mpvwidget.h"
#include "sourcedialog.h"
#include "settingsdialog.h"
#include "xtreamclient.h"
#include "m3uparser.h"
#include "icons.h"
#include "multiviewdialog.h"

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

// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_epg(new EpgManager(this))
{
    setWindowTitle(QStringLiteral("Xtream Player"));
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
        statusBar()->showMessage(QStringLiteral("EPG yükleme hatası: ") + err, 6000);
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

// ─── Dynamic theme ──────────────────────────────────────────────────────────────

void MainWindow::applyTheme()
{
    qApp->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    qApp->setStyleSheet(Theme::style(m_config));
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
        "stop:0 #16161e, stop:1 #101018); "
        "border-right: 1px solid rgba(255,255,255,0.04); }"));
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
        "QToolButton:hover { background: rgba(255,255,255,0.1); }");

    auto *addBtn  = new QToolButton;
    addBtn->setIcon(Icons::add(QColor("#03DAC6")));
    addBtn->setIconSize(QSize(16, 16));
    addBtn->setToolTip(QStringLiteral("Kaynak ekle"));
    addBtn->setFixedSize(30, 30);
    addBtn->setStyleSheet(srcBtnStyle);

    auto *editBtn = new QToolButton;
    editBtn->setIcon(Icons::edit());
    editBtn->setIconSize(QSize(16, 16));
    editBtn->setToolTip(QStringLiteral("Kaynağı düzenle"));
    editBtn->setFixedSize(30, 30);
    editBtn->setStyleSheet(srcBtnStyle);

    auto *delBtn  = new QToolButton;
    delBtn->setIcon(Icons::trash(QColor("#ff5555")));
    delBtn->setIconSize(QSize(16, 16));
    delBtn->setToolTip(QStringLiteral("Kaynağı sil"));
    delBtn->setFixedSize(30, 30);
    delBtn->setStyleSheet(srcBtnStyle);
    srcRow->addWidget(m_sourceCombo, 1);
    srcRow->addWidget(addBtn);
    srcRow->addWidget(editBtn);
    srcRow->addWidget(delBtn);
    lay->addLayout(srcRow);

    // Progress Bar for Loading
    m_loadingProgress = new QProgressBar;
    m_loadingProgress->setTextVisible(false);
    m_loadingProgress->setFixedHeight(4);
    // Use Theme secondary color for the loading bar
    m_loadingProgress->setStyleSheet("QProgressBar { background: transparent; border: none; } QProgressBar::chunk { background-color: #03DAC6; }");
    m_loadingProgress->setVisible(false);
    lay->addWidget(m_loadingProgress);

    // Stream type tabs
    m_typeTab = new QTabWidget;
    m_typeTab->setDocumentMode(true);
    m_typeTab->addTab(new QWidget, QStringLiteral("Canli"));
    m_typeTab->addTab(new QWidget, QStringLiteral("Film"));
    m_typeTab->addTab(new QWidget, QStringLiteral("Dizi"));
    lay->addWidget(m_typeTab);

    // Inner splitter: category | channel
    auto *innerSplit = new QSplitter(Qt::Vertical);

    m_categoryList = new QListWidget;
    m_categoryList->setAlternatingRowColors(false);
    m_categoryList->setSpacing(1);
    innerSplit->addWidget(m_categoryList);

    auto *chPanel = new QWidget;
    auto *chLay   = new QVBoxLayout(chPanel);
    chLay->setContentsMargins(0, 0, 0, 0);
    chLay->setSpacing(4);

    // Section header between categories and channels
    auto *chHeader = new QLabel(QStringLiteral("Kanallar"));
    chHeader->setStyleSheet(QStringLiteral(
        "color: rgba(255,255,255,0.35); font-size: 10px; font-weight: bold; "
        "text-transform: uppercase; padding: 6px 4px 2px 4px; "
        "border-top: 1px solid rgba(255,255,255,0.08);"));
    chLay->addWidget(chHeader);

    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText(QStringLiteral("Kanal ara..."));
    m_searchEdit->setClearButtonEnabled(true);
    chLay->addWidget(m_searchEdit);

    m_channelList = new QListWidget;
    m_channelList->setAlternatingRowColors(false);
    m_channelList->setSpacing(1);
    chLay->addWidget(m_channelList);
    innerSplit->addWidget(chPanel);
    innerSplit->setSizes({180, 420});

    lay->addWidget(innerSplit, 1);

    // Connections
    connect(m_sourceCombo, &QComboBox::currentIndexChanged, this, &MainWindow::onSourceChanged);
    connect(m_typeTab,     &QTabWidget::currentChanged,     this, &MainWindow::onStreamTypeChanged);
    connect(m_categoryList, &QListWidget::itemClicked,  this, &MainWindow::onCategorySelected);
    connect(m_channelList,  &QListWidget::itemClicked,  this, &MainWindow::onChannelSelected);
    connect(m_channelList,  &QListWidget::itemDoubleClicked, this, &MainWindow::onChannelDoubleClicked);
    connect(m_searchEdit,   &QLineEdit::textChanged,    this, &MainWindow::onSearchTextChanged);
    connect(addBtn,  &QToolButton::clicked, this, &MainWindow::onAddSource);
    connect(editBtn, &QToolButton::clicked, this, &MainWindow::onEditSource);
    connect(delBtn,  &QToolButton::clicked, this, &MainWindow::onRemoveSource);
}

void MainWindow::setupPlayerPanel()
{
    m_playerPanel = new QWidget;
    auto *lay = new QVBoxLayout(m_playerPanel);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    try {
        m_mpv = new MpvWidget;
        
        // Apply hardware decoding setting from config (defaults to "no" for safety)
        if (!m_config.mpvHwDecode.isEmpty())
            m_mpv->setOption(QStringLiteral("hwdec"), m_config.mpvHwDecode);
            
        // Apply extra args if any
        if (!m_config.mpvExtraArgs.isEmpty()) {
            const QStringList args = m_config.mpvExtraArgs.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            for (const QString &arg : args) {
                const int eq = arg.indexOf(QLatin1Char('='));
                if (eq > 0)
                    m_mpv->setOption(arg.left(eq), arg.mid(eq + 1));
            }
        }
    } catch (const std::exception &e) {
        QMessageBox::critical(this, QStringLiteral("Kritik Hata"),
            QStringLiteral("Video oynatıcı başlatılamadı:\n%1\n\nLütfen grafik sürücülerinizi kontrol edin.").arg(e.what()));
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
            statusBar()->showMessage(QStringLiteral("Oynatma hatası: ") + msg, 8000);
        });
    } else {
        auto *errLbl = new QLabel(QStringLiteral("Oynatıcı Yüklenemedi\n(Grafik sürücü hatası)"));
        errLbl->setAlignment(Qt::AlignCenter);
        errLbl->setStyleSheet(QStringLiteral("color: #ff5555; font-size: 16px; font-weight: bold;"));
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
        "color: #e6e6f0; font-weight: 600; font-size: 13px;"));

    m_epgNextLabel = new QLabel;
    m_epgProgress = new QProgressBar;
    m_epgProgress->setTextVisible(false);
    m_epgProgress->setFixedHeight(4);
    m_epgProgress->setStyleSheet("QProgressBar { background: #2a2a35; border: none; } QProgressBar::chunk { background-color: #6366f1; }");

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
        "QWidget#controlBar { background: rgba(18,18,24,0.95); border-top: 1px solid rgba(255,255,255,0.06); }"));

    auto *lay = new QHBoxLayout(m_controlBar);
    lay->setContentsMargins(10, 4, 10, 4);
    lay->setSpacing(6);

    // Button factory — icon-based, consistent look
    const QString btnStyle = QStringLiteral(
        "QToolButton { border-radius: 4px; padding: 0px; border: none; background: transparent; }"
        "QToolButton:hover { background: rgba(255,255,255,0.1); }"
        "QToolButton:pressed { background: rgba(187,134,252,0.3); }");

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
        sep->setStyleSheet(QStringLiteral("background: rgba(255,255,255,0.1);"));
        return sep;
    };

    // ── Playback group ──
    m_prevChBtn    = makeBtn(Icons::skipPrevious(), QStringLiteral("Onceki kanal (P)"), 30);
    m_skipBackBtn  = makeBtn(Icons::rewind(),       QStringLiteral("10 sn geri"), 30);
    m_playPauseBtn = makeBtn(Icons::play(QColor("#BB86FC")), QStringLiteral("Oynat / Duraklat"), 36);
    m_skipFwdBtn   = makeBtn(Icons::fastForward(),  QStringLiteral("10 sn ileri"), 30);
    m_recordBtn    = makeBtn(Icons::record(),       QStringLiteral("Kaydı Başlat / Durdur"), 30);
    m_nextChBtn    = makeBtn(Icons::skipNext(),     QStringLiteral("Sonraki kanal (N)"), 30);

    // ── Seek + time ──
    m_seekSlider = new QSlider(Qt::Horizontal);
    m_seekSlider->setRange(0, 1000);

    m_timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"));
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setStyleSheet(QStringLiteral(
        "color: #8888aa; font-size: 11px; min-width: 90px; font-weight: 500;"));

    m_liveLabel = new QLabel(QStringLiteral("CANLI"));
    m_liveLabel->setAlignment(Qt::AlignCenter);
    m_liveLabel->setStyleSheet(QStringLiteral(
        "color: #ff4444; font-size: 10px; font-weight: bold; "
        "background: rgba(255,68,68,0.15); border-radius: 3px; "
        "padding: 1px 6px;"));
    m_liveLabel->setVisible(false);

    // ── Volume group ──
    auto *volIcon = new QToolButton;
    volIcon->setIcon(Icons::volumeUp(QColor(150, 150, 170)));
    volIcon->setIconSize(QSize(16, 16));
    volIcon->setFixedSize(24, 24);
    volIcon->setStyleSheet(QStringLiteral("QToolButton { border: none; background: transparent; }"));
    volIcon->setEnabled(false);

    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(m_config.volume);
    m_volumeSlider->setFixedWidth(75);
    m_volumeSlider->setToolTip(QStringLiteral("Ses"));

    // ── Track / tools group ──
    m_audioBtn   = makeBtn(Icons::audioTrack(),     QStringLiteral("Ses parcasi sec (A)"), 32);
    m_subBtn     = makeBtn(Icons::closedCaption(),  QStringLiteral("Altyazi sec (S)"), 32);
    m_infoBtn    = makeBtn(Icons::info(),           QStringLiteral("Medya bilgisi (I)"), 28);

    m_speedCombo = new QComboBox;
    m_speedCombo->addItems({QStringLiteral("0.5x"), QStringLiteral("0.75x"),
                            QStringLiteral("1x"), QStringLiteral("1.25x"),
                            QStringLiteral("1.5x"), QStringLiteral("2x")});
    m_speedCombo->setCurrentIndex(2);
    m_speedCombo->setFixedWidth(58);
    m_speedCombo->setToolTip(QStringLiteral("Oynatma hizi"));

    // ── Utility group ──
    m_favoriteBtn   = makeBtn(Icons::starOutline(),      QStringLiteral("Favorilere ekle / cikar (F)"), 32);
    m_multiViewBtn  = makeBtn(Icons::gridView(),         QStringLiteral("Çoklu Ekran (Multi-View)"), 32);
    m_epgToggleBtn  = makeBtn(Icons::tvGuide(),          QStringLiteral("EPG panelini goster/gizle"), 32);
    m_pipBtn        = makeBtn(Icons::pictureInPicture(), QStringLiteral("Resim icinde resim"), 32);
    m_fullscreenBtn = makeBtn(Icons::fullscreen(),       QStringLiteral("Tam ekran (F11)"), 32);

    // ── Layout ──
    lay->addWidget(m_prevChBtn);
    lay->addWidget(m_skipBackBtn);
    lay->addWidget(m_playPauseBtn);
    lay->addWidget(m_skipFwdBtn);
    lay->addWidget(m_recordBtn);
    lay->addWidget(m_nextChBtn);
    lay->addWidget(makeSep());
    lay->addWidget(m_seekSlider, 1);
    lay->addWidget(m_liveLabel);
    lay->addWidget(m_timeLabel);
    lay->addWidget(makeSep());
    lay->addWidget(volIcon);
    lay->addWidget(m_volumeSlider);
    lay->addWidget(makeSep());
    lay->addWidget(m_audioBtn);
    lay->addWidget(m_subBtn);
    lay->addWidget(m_speedCombo);
    lay->addWidget(makeSep());
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
    connect(m_epgToggleBtn,  &QToolButton::clicked,        this, &MainWindow::onToggleEpg);
    connect(m_pipBtn,        &QToolButton::clicked,        this, &MainWindow::onTogglePip);
    connect(m_fullscreenBtn, &QToolButton::clicked,        this, &MainWindow::toggleFullScreen);
    connect(m_speedCombo,    &QComboBox::currentIndexChanged, this, &MainWindow::onSpeedChanged);

    m_mpv->setVolume(m_config.volume);
}

void MainWindow::setupMenuBar()
{
    auto *fileMenu = menuBar()->addMenu(QStringLiteral("Dosya"));
    fileMenu->addAction(QStringLiteral("Kaynak Ekle…"),  this, &MainWindow::onAddSource);
    fileMenu->addAction(QStringLiteral("Kaynağı Düzenle…"), this, &MainWindow::onEditSource);
    fileMenu->addAction(QStringLiteral("Kaynağı Sil"),   this, &MainWindow::onRemoveSource);
    fileMenu->addSeparator();
    fileMenu->addAction(QStringLiteral("Çıkış"), qApp, &QApplication::quit);

    auto *viewMenu = menuBar()->addMenu(QStringLiteral("Görünüm"));
    viewMenu->addAction(QStringLiteral("Tam Ekran (F11)"), this, &MainWindow::toggleFullScreen);
    viewMenu->addAction(QStringLiteral("EPG Panelini Göster/Gizle"), this, &MainWindow::onToggleEpg);

    auto *settingsMenu = menuBar()->addMenu(QStringLiteral("Ayarlar"));
    settingsMenu->addAction(QStringLiteral("Tercihler…"), this, &MainWindow::onOpenSettings);

    auto *helpMenu = menuBar()->addMenu(QStringLiteral("Yardım"));
    viewMenu->addAction(QStringLiteral("PiP Modu"), this, &MainWindow::onTogglePip);

    helpMenu->addAction(QStringLiteral("Klavye Kısayolları"), this, [this]() {
        QMessageBox::information(this, QStringLiteral("Klavye Kısayolları"),
            QStringLiteral(
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
                "S            Altyazı Seç"
            ));
    });
    helpMenu->addSeparator();
    helpMenu->addAction(QStringLiteral("Hakkında"), this, [this](){
        QMessageBox::about(this, QStringLiteral("Xtream Player"),
            QStringLiteral("Xtream Player v2.0\nC++ + Qt6 + libmpv"));
    });
}

void MainWindow::setupShortcuts()
{
    if (!m_mpv) return;

    auto sc = [this](const QKeySequence &key, auto slot) {
        auto *s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, slot);
    };

    sc(Qt::Key_F11,                    &MainWindow::toggleFullScreen);
    sc(Qt::Key_Space,                  &MainWindow::onPlayPause);
    sc(QKeySequence(Qt::Key_M),        [this]{ m_mpv->setVolume(m_mpv->volume() == 0 ? m_config.volume : 0); });
    sc(QKeySequence(Qt::Key_Right),    [this]{ m_mpv->seek(m_mpv->position() + 10); });
    sc(QKeySequence(Qt::Key_Left),     [this]{ m_mpv->seek(m_mpv->position() - 10); });
    sc(QKeySequence(Qt::Key_Up),       [this]{ m_volumeSlider->setValue(qMin(100, m_volumeSlider->value() + 5)); });
    sc(QKeySequence(Qt::Key_Down),     [this]{ m_volumeSlider->setValue(qMax(0,   m_volumeSlider->value() - 5)); });
    sc(QKeySequence(Qt::Key_Escape),   [this]{ if (isFullScreen()) toggleFullScreen(); else if (m_pipMode) onTogglePip(); });
    // These shortcuts should not fire when typing in the search box
    auto scNoEdit = [this](const QKeySequence &key, auto slot) {
        auto *s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, [this, slot]{
            if (m_searchEdit && m_searchEdit->hasFocus()) return;
            (this->*slot)();
        });
    };
    sc(QKeySequence(Qt::Key_F),        [this]{ if (!m_searchEdit->hasFocus()) onToggleFavorite(); });
    scNoEdit(QKeySequence(Qt::Key_N),  &MainWindow::onNextChannel);
    scNoEdit(QKeySequence(Qt::Key_P),  &MainWindow::onPrevChannel);
    scNoEdit(QKeySequence(Qt::Key_I),  &MainWindow::onShowMediaInfo);
    scNoEdit(QKeySequence(Qt::Key_A),  &MainWindow::onShowAudioMenu);
    scNoEdit(QKeySequence(Qt::Key_S),  &MainWindow::onShowSubMenu);
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
    m_categoryList->clear();
    m_channelList->clear();
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
    SourceDialog dlg(this);
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
    SourceDialog dlg(m_config.sources[idx], this);
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
    if (QMessageBox::question(this, QStringLiteral("Sil"),
            QStringLiteral("Kaynağı silmek istiyor musunuz?"))
        != QMessageBox::Yes) return;
    m_config.removeSource(m_config.sources[idx].id);
    loadSourceList();
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(m_config, this);
    if (dlg.exec() == QDialog::Accepted) {
        dlg.applyTo(m_config);
        applyTheme();
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
    m_categoryList->clear();
    m_channelList->clear();
    m_allChannels.clear();

    const Source *src = m_config.getSource(sourceId);
    if (!src) return;

    // Helper to populate category list from a QList<Category>
    auto populateCats = [this](const QList<Category> &cats) {
        m_categories = cats;
        m_categoryList->clear();
        
        // Add "Favoriler" first
        auto *favItem = new QListWidgetItem(QStringLiteral("★ Favoriler"), m_categoryList);
        favItem->setData(Qt::UserRole, QStringLiteral("__FAVORITES__"));
        favItem->setForeground(QBrush(QColor(255, 215, 0))); // Gold color

        auto *allItem = new QListWidgetItem(QStringLiteral("Tümü"), m_categoryList);
        allItem->setData(Qt::UserRole, QString{});
        
        for (const Category &c : cats) {
            auto *item = new QListWidgetItem(c.name, m_categoryList);
            item->setData(Qt::UserRole, c.id);
        }
        if (m_categoryList->count() > 1) { // Default to "All" (index 1) to avoid empty favorites initially
            m_categoryList->setCurrentRow(1);
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

    statusBar()->showMessage(QStringLiteral("Kategoriler yükleniyor…"));
    m_loadingProgress->setVisible(true);
    m_loadingProgress->setRange(0, 0); // indeterminate

    auto onDone = [this, populateCats, key](QList<Category> cats, QString err) {
        statusBar()->clearMessage();
        m_loadingProgress->setVisible(false);
        if (!err.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Kategori yükleme hatası: ") + err, 6000);
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

void MainWindow::onCategorySelected(QListWidgetItem *item)
{
    if (!item) return;
    loadChannels(item->data(Qt::UserRole).toString());
}

void MainWindow::loadChannels(const QString &categoryId)
{
    m_channelList->clear();
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
        applySearchFilter();
        m_loadingChannels = false;
        m_loadingProgress->setVisible(false);
        statusBar()->clearMessage();

        // Resume last channel on first load after startup
        if (m_resumePending && !m_config.lastChannelUrl.isEmpty()) {
            m_resumePending = false;
            for (int i = 0; i < m_filteredChannels.size(); ++i) {
                if (m_filteredChannels[i].streamUrl == m_config.lastChannelUrl) {
                    m_channelList->setCurrentRow(i);
                    m_currentChannel = m_filteredChannels[i];
                    updateFavoriteButton(m_currentChannel);
                    if (m_currentChannel.streamType == StreamType::Live)
                        updateEpgPanel(m_currentChannel);
                    else
                        updateInfoPanel(m_currentChannel);
                    playChannel(m_currentChannel);
                    break;
                }
            }
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
        statusBar()->showMessage(QStringLiteral("M3U yükleniyor… (Bu işlem birkaç dakika sürebilir)"));
        
        m_loadingProgress->setVisible(true);
        m_loadingProgress->setRange(0, 0); // indeterminate

        auto *nam  = new QNetworkAccessManager(this);
        QNetworkRequest req{QUrl(src.m3uUrl)};
        req.setRawHeader("User-Agent", "XtreamPlayer/2.0");
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(60000); // Increased timeout to 60s
        QNetworkReply *reply = nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, nam, src, finalize]() {
            reply->deleteLater();
            nam->deleteLater();
            
            // Check if user changed source while loading
            const int currentSrcIdx = m_sourceCombo->currentIndex();
            bool isSameSource = (currentSrcIdx >= 0 && m_config.sources[currentSrcIdx].id == src.id);

            if (reply->error() == QNetworkReply::NoError) {
                const QByteArray raw = reply->readAll();
                const QString text = QString::fromUtf8(raw);
                auto result = M3UParser::parse(text, src.id);
                QList<Channel> all = std::move(result.channels);
                
                if (all.isEmpty()) {
                    if (isSameSource) {
                        m_loadingChannels = false;
                        m_loadingProgress->setVisible(false);
                        statusBar()->showMessage(QStringLiteral("M3U boş veya geçersiz format"), 4000);
                    }
                    return;
                }
                
                m_m3uCache.insert(src.id, all);

                // Check for embedded EPG URL
                if (src.epgUrl.isEmpty() && !result.epgUrl.isEmpty()) {
                    m_epg->load(result.epgUrl);
                }
                
                // Only update UI if we are still on the same source
                if (isSameSource) {
                    m_loadingChannels = false; // reset flag before calling loadCategories
                    m_loadingProgress->setVisible(false);
                    // Reload categories now that we have data
                    loadCategories(src.id, m_streamType);
                } else {
                     m_loadingChannels = false;
                }
            } else {
                if (isSameSource) {
                    m_loadingChannels = false;
                    m_loadingProgress->setVisible(false);
                    statusBar()->showMessage(
                        QStringLiteral("M3U yükleme hatası: ") + reply->errorString(), 5000);
                } else {
                    m_loadingChannels = false;
                }
            }
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
        statusBar()->showMessage(QStringLiteral("Favoriler yükleniyor…"));
        auto onAllDone = [this, finalize, allKey](QList<Channel> channels, QString err) {
            m_loadingChannels = false;
            m_loadingProgress->setVisible(false);
            statusBar()->clearMessage();
            if (!err.isEmpty()) {
                statusBar()->showMessage(QStringLiteral("Kanal yükleme hatası: ") + err, 6000);
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
    statusBar()->showMessage(QStringLiteral("Kanallar yükleniyor…"));

    auto onDone = [this, finalize, key](QList<Channel> channels, QString err) {
        m_loadingChannels = false;
        m_loadingProgress->setVisible(false);
        statusBar()->clearMessage();
        if (!err.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Kanal yükleme hatası: ") + err, 6000);
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
    const QString q = m_searchEdit->text().trimmed().toLower();
    m_filteredChannels.clear();

    for (const Channel &ch : m_allChannels) {
        if (q.isEmpty() || ch.name.toLower().contains(q))
            m_filteredChannels.append(ch);
    }

    populateChannelList(m_filteredChannels);
}

void MainWindow::populateChannelList(const QList<Channel> &channels)
{
    m_channelList->clear();
    int idx = 0;
    for (const Channel &ch : channels) {
        ++idx;
        const QString prefix = ch.isFavorite ? QStringLiteral("★ ") : QString{};
        const QString numStr = ch.num > 0
            ? QString::number(ch.num) + QStringLiteral(". ")
            : QString::number(idx) + QStringLiteral(". ");
        auto *item = new QListWidgetItem(prefix + numStr + ch.name, m_channelList);
        item->setData(Qt::UserRole, ch.streamUrl);
        item->setToolTip(ch.plot.isEmpty() ? ch.name : ch.plot.left(200));
    }
}

void MainWindow::onChannelSelected(QListWidgetItem *item)
{
    if (!item) return;
    const QString url = item->data(Qt::UserRole).toString();
    for (const Channel &ch : m_filteredChannels)
        if (ch.streamUrl == url) { m_currentChannel = ch; break; }
    updateFavoriteButton(m_currentChannel);
    if (m_currentChannel.streamType == StreamType::Live)
        updateEpgPanel(m_currentChannel);
    else
        updateInfoPanel(m_currentChannel);
}

void MainWindow::onChannelDoubleClicked(QListWidgetItem *item)
{
    onChannelSelected(item);
    playChannel(m_currentChannel);
}

void MainWindow::playChannel(const Channel &ch)
{
    if (ch.streamUrl.isEmpty()) return;
    m_config.lastChannelUrl = ch.streamUrl;

    // Apply hw decode option
    m_mpv->setOption(QStringLiteral("hwdec"), m_config.mpvHwDecode);
    if (!m_config.mpvExtraArgs.isEmpty()) {
        // Extra args parsed as key=value pairs separated by spaces
        const QStringList parts = m_config.mpvExtraArgs.split(QLatin1Char(' '),
                                                               Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const int eq = part.indexOf(QLatin1Char('='));
            if (eq > 0)
                m_mpv->setOption(part.left(eq), part.mid(eq + 1));
        }
    }

    m_mpv->play(ch.streamUrl);

    // Apply speed — allow any speed for all stream types so users can catch up
    if (m_speedCombo)
        onSpeedChanged(m_speedCombo->currentIndex());

    setWindowTitle(ch.name + QStringLiteral(" — Xtream Player"));
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
        m_epgCurrentLabel->setText(QStringLiteral("EPG verisi bulunamadı"));
        m_epgCurrentLabel->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,0.3); font-weight: 500; font-size: 12px; font-style: italic;"));
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
            "color: #e6e6f0; font-weight: 600; font-size: 13px;"));
        m_epgCurrentLabel->setText(cur.title + QStringLiteral("  (→ ") + endTime + QStringLiteral(")"));
        m_epgProgress->setVisible(true);
        m_epgProgress->setValue(pct);
    }

    m_epgNextLabel->setText(next.title.isEmpty()
        ? QString{}
        : QStringLiteral("Sonraki: ") + next.title);
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
    m_playPauseBtn->setIcon(paused ? Icons::play(QColor("#BB86FC")) : Icons::pause(QColor("#BB86FC")));
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
                m_trayIcon->showMessage(QStringLiteral("Playlist Güncellemesi"), 
                                        src.name + QStringLiteral(" kaynak verileri yenileniyor..."), 
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
    // Update list item
    for (int i = 0; i < m_channelList->count(); ++i) {
        auto *item = m_channelList->item(i);
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
        ? Icons::starFilled(QColor("#BB86FC"))
        : Icons::starOutline());
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
    const int row = m_channelList->currentRow();
    if (row > 0) {
        m_channelList->setCurrentRow(row - 1);
        onChannelDoubleClicked(m_channelList->currentItem());
    }
}

void MainWindow::onNextChannel()
{
    const int row = m_channelList->currentRow();
    if (row < m_channelList->count() - 1) {
        m_channelList->setCurrentRow(row + 1);
        onChannelDoubleClicked(m_channelList->currentItem());
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
        QString label = QStringLiteral("Parça %1").arg(t.id);
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
        menu.addAction(QStringLiteral("Ses parçası bulunamadı"))->setEnabled(false);
    menu.exec(m_audioBtn->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void MainWindow::onShowSubMenu()
{
    QMenu menu(this);
    const auto tracks = m_mpv->trackList();
    bool hasSub = false;

    // "Off" option
    QAction *offAct = menu.addAction(QStringLiteral("Kapalı"));
    offAct->setCheckable(true);
    bool anySel = false;

    for (const TrackInfo &t : tracks) {
        if (t.type != QLatin1String("sub")) continue;
        hasSub = true;
        if (t.selected) anySel = true;
        QString label = QStringLiteral("Altyazı %1").arg(t.id);
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
        menu.addAction(QStringLiteral("Altyazı bulunamadı"))->setEnabled(false);
    menu.exec(m_subBtn->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void MainWindow::onShowMediaInfo()
{
    const auto tracks = m_mpv->trackList();
    QString info;

    // General
    info += QStringLiteral("Kanal: %1\n").arg(m_currentChannel.name);
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
    info += QStringLiteral("── Parçalar ──\n");
    for (const TrackInfo &t : tracks) {
        QString line = QStringLiteral("#%1 %2").arg(t.id).arg(t.type);
        if (!t.lang.isEmpty()) line += QStringLiteral(" [%1]").arg(t.lang);
        if (!t.title.isEmpty()) line += QStringLiteral(" %1").arg(t.title);
        if (!t.codec.isEmpty()) line += QStringLiteral(" (%1)").arg(t.codec);
        if (t.selected) line += QStringLiteral(" ✓");
        info += line + QLatin1Char('\n');
    }

    QMessageBox::information(this, QStringLiteral("Medya Bilgisi"), info);
}

void MainWindow::setupTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon::fromTheme(QStringLiteral("video-x-generic"), Icons::tvGuide(QColor("#BB86FC"))));
    m_trayIcon->setToolTip(QStringLiteral("Xtream Player"));

    auto *trayMenu = new QMenu(this);
    
    auto *toggleAct = new QAction(QStringLiteral("Arayüzü Göster/Gizle"), this);
    connect(toggleAct, &QAction::triggered, this, [this]{
        if (isVisible()) hide();
        else { showNormal(); raise(); activateWindow(); }
    });
    
    auto *muteAct = new QAction(QStringLiteral("Sesi Kapat/Aç"), this);
    connect(muteAct, &QAction::triggered, this, [this]{
        if (m_mpv) m_mpv->setVolume(m_mpv->volume() > 0 ? 0 : m_config.volume);
    });
    
    auto *quitAct = new QAction(QStringLiteral("Çıkış"), this);
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
        // Stop recording
        m_mpv->setProperty("stream-record", QString());
        m_isRecording = false;
        m_recordBtn->setIcon(Icons::record()); // normal color
        statusBar()->showMessage(QStringLiteral("Kayıt durduruldu."), 3000);
    } else {
        // Start recording
        QString dir = m_config.recordPath;
        if (dir.isEmpty()) {
            dir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        }
        
        QDir recordDir(dir);
        if (!recordDir.exists()) {
            recordDir.mkpath(QStringLiteral("."));
        }

        QString safeName = m_currentChannel.name;
        safeName.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_]")), QStringLiteral("_"));
        
        const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QString fileName = QStringLiteral("Record_%1_%2.ts").arg(safeName, timestamp);
        const QString filePath = recordDir.absoluteFilePath(fileName);

        m_mpv->setProperty("stream-record", filePath);
        m_isRecording = true;
        
        m_recordBtn->setIcon(Icons::record(QColor(255, 100, 100))); // bright red when recording
        statusBar()->showMessage(QStringLiteral("Kayıt başladı: ") + filePath, 5000);
    }
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
            "background: rgba(0,0,0,0.55); border-radius: 0px;"));
        m_pipOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        auto *tbLay = new QHBoxLayout(m_pipOverlay);
        tbLay->setContentsMargins(8, 6, 8, 6);

        const QString ovBtnStyle = QStringLiteral(
            "QToolButton { color: rgba(255,255,255,0.75); font-size: 11px; font-weight: bold; "
            "padding: 3px 10px; border: none; background: rgba(255,255,255,0.08); border-radius: 4px; }"
            "QToolButton:hover { color: #fff; background: rgba(255,255,255,0.25); }");

        auto *backBtn = new QToolButton;
        backBtn->setText(QStringLiteral("← Geri"));
        backBtn->setCursor(Qt::PointingHandCursor);
        backBtn->setStyleSheet(ovBtnStyle);
        connect(backBtn, &QToolButton::clicked, this, &MainWindow::onTogglePip);

        auto *closeBtn = new QToolButton;
        closeBtn->setText(QStringLiteral("✕"));
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(QStringLiteral(
            "QToolButton { color: rgba(255,100,100,0.85); font-size: 13px; font-weight: bold; "
            "padding: 3px 10px; border: none; background: rgba(255,255,255,0.08); border-radius: 4px; }"
            "QToolButton:hover { color: #fff; background: rgba(255,85,85,0.5); }"));
        connect(closeBtn, &QToolButton::clicked, this, [this]() {
            onTogglePip();
            if (m_mpv) m_mpv->stop();
        });

        m_pipChannelLabel = new QLabel(m_currentChannel.name);
        m_pipChannelLabel->setStyleSheet(QStringLiteral(
            "color: rgba(255,255,255,0.7); font-size: 10px; background: transparent;"));
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
