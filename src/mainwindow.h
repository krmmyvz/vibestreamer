#pragma once
#include "config.h"
#include "epgmanager.h"
#include "models.h"
#include "theme.h"

#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QStandardItemModel>
#include <QMainWindow>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
#include <QToolButton>
#include <QComboBox>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QCompleter>
#include <QStringListModel>

class MpvWidget;
class XtreamClient;
class MultiViewDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

public slots:
    void toggleFullScreen();

protected:
    void closeEvent(QCloseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // Sources
    void onSourceChanged(int index);
    void onAddSource();
    void onEditSource();
    void onRemoveSource();
    void onOpenSettings();

    // Navigation
    void onStreamTypeChanged(int tab);
    void onCategorySelected(const QModelIndex &idx);
    void onChannelSelected(const QModelIndex &idx);
    void onChannelDoubleClicked(const QModelIndex &idx);
    void onSearchTextChanged(const QString &text);

    // Player controls
    void onPlayPause();
    void onVolumeChanged(int value);
    void onPositionChanged(double secs);
    void onDurationChanged(double secs);
    void onPauseChanged(bool paused);
    void onToggleFavorite();
    void onToggleEpg();
    void onSpeedChanged(int index);
    void onToggleRecord();
    void onRecordPauseResume();
    void onRecordStop();
    void onViewModeChanged(int index);

    // New player controls
    void onSkipBack();
    void onSkipForward();
    void onPrevChannel();
    void onNextChannel();
    void onShowAudioMenu();
    void onShowSubMenu();
    void onShowMediaInfo();
    void onTogglePip();
    void onOpenMultiView();

    // EPG refresh
    void refreshEpg();

    // Auto update
    void checkUpdateSchedules();

private:
    void setupUI();
    void setupTrayIcon();
    void setupMenuBar();
    void setupSidebar();
    void setupPlayerPanel();
    void setupControlBar();
    void applyTheme();
    void applyInlineStyles();
    void retranslateUi();
    void setupShortcuts();

    void loadSourceList();
    void loadCategories(const QString &sourceId, StreamType type);
    void loadChannels(const QString &categoryId);
    void populateChannelList(const QList<Channel> &channels);
    void applySearchFilter();
    void playChannel(const Channel &ch);
    void updateEpgPanel(const Channel &ch);
    void updateInfoPanel(const Channel &ch);
    void updateFavoriteButton(const Channel &ch);
    QString formatTime(double secs) const;
    void updateStyle();
    QString t(const QString &tr, const QString &en) const;

    // ── Cache key helpers ──────────────────────────────────────────────
    struct CacheKey {
        QString sourceId;
        StreamType type;
        QString categoryId;
        bool operator==(const CacheKey &o) const {
            return sourceId == o.sourceId && type == o.type && categoryId == o.categoryId;
        }
    };
    friend size_t qHash(const CacheKey &k, size_t seed = 0) {
        return qHash(k.sourceId, seed) ^ qHash(int(k.type), seed) ^ qHash(k.categoryId, seed);
    }

    void invalidateCache();

    // ── State ──────────────────────────────────────────────────────────
    Config          m_config;
    ThemeColors     m_theme;
    EpgManager     *m_epg;
    XtreamClient   *m_client = nullptr;

    QList<Category> m_categories;
    QList<Channel>  m_allChannels;   // full list for current category
    QList<Channel>  m_filteredChannels;
    Channel         m_currentChannel;
    StreamType      m_streamType = StreamType::Live;
    bool            m_seeking    = false;
    bool            m_loadingChannels = false;
    bool            m_resumePending   = true;   // try to resume lastChannelUrl on first load
    bool            m_pipMode    = false;
    bool            m_isRecording = false;
    bool            m_recordPaused = false;
    QString         m_recordFilePath;    // current recording file path
    int             m_recordSegment = 0; // segment counter for pause/resume
    QList<int>      m_savedSplitterSizes;

    QProgressBar   *m_loadingProgress = nullptr;
    QHash<CacheKey, QList<Category>>  m_categoryCache;
    // Cache: (sourceId, streamType, categoryId) → channels
    QHash<CacheKey, QList<Channel>>   m_channelCache;
    // Cache: sourceId → parsed M3U channels (all types)
    QHash<QString, QList<Channel>>    m_m3uCache;
    // Logo URL → list widget row indices (for O(1) icon update)
    QHash<QString, QList<int>>        m_logoUrlToRows;

    // ── Widgets ────────────────────────────────────────────────────────
    QSplitter      *m_mainSplitter;

    // Sidebar
    QWidget        *m_sidebar;
    QComboBox      *m_sourceCombo;
    QToolButton    *m_addSourceBtn = nullptr;
    QToolButton    *m_editSourceBtn = nullptr;
    QToolButton    *m_delSourceBtn = nullptr;
    QTabWidget     *m_typeTab;
    QListView *m_categoryList;
    QStandardItemModel *m_catModel;
    QLabel         *m_channelHeaderLabel = nullptr;
    QLineEdit      *m_searchEdit;
    QCompleter     *m_searchCompleter = nullptr;
    QStringListModel *m_searchModel = nullptr;
    QComboBox      *m_viewModeCombo;
    QListView *m_channelList;
    QStandardItemModel *m_chanModel;

    // Player panel
    QWidget        *m_playerPanel;
    MpvWidget      *m_mpv;

    // Control bar
    QToolButton    *m_playPauseBtn;
    QToolButton    *m_skipBackBtn;
    QToolButton    *m_skipFwdBtn;
    QToolButton    *m_prevChBtn;
    QToolButton    *m_nextChBtn;
    QSlider        *m_seekSlider;
    QLabel         *m_timeLabel;
    QLabel         *m_liveLabel;
    QToolButton    *m_volIcon;
    QSlider        *m_volumeSlider;
    QComboBox      *m_speedCombo = nullptr;
    QToolButton    *m_audioBtn;
    QToolButton    *m_subBtn;
    QToolButton    *m_infoBtn;
    QToolButton    *m_pipBtn;
    QToolButton    *m_fullscreenBtn;
    QToolButton    *m_favoriteBtn;
    QToolButton    *m_recordBtn;
    QToolButton    *m_recordPauseBtn = nullptr;
    QToolButton    *m_recordStopBtn  = nullptr;
    QToolButton    *m_multiViewBtn;
    QToolButton    *m_epgToggleBtn;

    // EPG / info panel
    QWidget        *m_epgPanel;
    QLabel         *m_epgCurrentLabel;
    QLabel         *m_epgNextLabel;
    QProgressBar   *m_epgProgress;

    // System Tray
    QSystemTrayIcon *m_trayIcon = nullptr;

    MultiViewDialog *m_multiViewDialog = nullptr;

    // Auto Update Timer
    QTimer         *m_autoUpdateTimer = nullptr;

    // Control bar widget (for fullscreen hide/show)
    QWidget        *m_controlBar = nullptr;

    // PiP overlay + saved state
    QWidget        *m_pipOverlay = nullptr;
    QLabel         *m_pipChannelLabel = nullptr;
    QTimer         *m_pipRaiseTimer = nullptr;
    QRect           m_savedGeometry;
    Qt::WindowFlags m_savedWindowFlags;
    QSize           m_savedMinSize;
    QSize           m_savedMaxSize;

    // PiP manual drag/resize fallback
    bool            m_pipDragging  = false;
    QPoint          m_pipDragPos;
    bool            m_pipResizing  = false;
    Qt::Edges       m_pipResizeEdges;
    QPoint          m_pipResizeOrigin;
    QRect           m_pipResizeGeom;
};
