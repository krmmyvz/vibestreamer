#include "mainwindow.h"
#include "mpvwidget.h"
#include "localization.h"

#include <QGuiApplication>
#include <QMenuBar>
#include <QTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScreen>
#include <QStatusBar>
#include <QToolButton>
#include <QWindow>

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
        backBtn->setText(t(QStringLiteral("back")));
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
