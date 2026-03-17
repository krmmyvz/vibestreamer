#pragma once
#include <QColor>
#include <QString>
#include "config.h"

// ─── Design Token System ────────────────────────────────────────────────────
// All semantic colors live here. No hardcoded hex values outside this file.

struct ThemeColors {
    // ── Surface ──
    QString bgWindow;          // Main window background
    QString bgPanel;           // Panel / card background
    QString bgSurface;         // Elevated surface (control bar, overlays)
    QString bgSurfaceAlpha;    // Semi-transparent surface (PiP overlay)
    QString bgInput;           // Text input background

    // ── Sidebar ──
    QString sidebarGradStart;  // Sidebar gradient top
    QString sidebarGradEnd;    // Sidebar gradient bottom
    QString sidebarBorder;     // Sidebar right border

    // ── Text ──
    QString textPrimary;       // Main body text
    QString textSecondary;     // Muted / secondary text
    QString textDimmed;        // Very dimmed text (section headers)
    QString textOnAccent;      // Text on accent-colored backgrounds

    // ── Accent ──
    QString accent;            // Primary accent color
    QString accentAlpha;       // Accent with transparency (pressed states)
    QString accentSelectedBg;  // List item selected background

    // ── Semantic ──
    QString success;           // Green / teal (add button, progress)
    QString error;             // Red (delete, error, live badge)
    QString errorAlpha;        // Red with alpha (live badge bg, recording)
    QString warning;           // Gold (favorites)

    // ── Borders & Separators ──
    QString border;            // Standard border
    QString borderSubtle;      // Very subtle border / separator
    QString separator;         // Control bar separators

    // ── Interactive ──
    QString hoverBg;           // Hover background
    QString pressedBg;         // Pressed background (uses accent alpha)

    // ── Scrollbar ──
    QString scrollBg;
    QString scrollHover;

    // ── Status Bar ──
    QString statusBarBg;

    // ── EPG ──
    QString epgProgressBg;     // EPG progress bar background
    QString epgProgressChunk;  // EPG progress bar fill

    // ── Icon default color ──
    QColor iconDefault;        // Default icon tint
    QColor iconAccent;         // Accent-tinted icons (play button)
    QColor iconMuted;          // Muted icons (volume)
    QColor iconSuccess;        // Success-colored icons (add)
    QColor iconError;          // Error-colored icons (delete, record active)
    QColor iconRecord;         // Record icon default
    QColor iconFavActive;      // Favorite star when active
};

class Theme {
public:
    // Helper: convert a hex color to rgba() string with given alpha (0.0–1.0)
    static QString toRgba(const QString &hex, double alpha)
    {
        const QColor c(hex);
        return QStringLiteral("rgba(%1,%2,%3,%4)")
            .arg(c.red()).arg(c.green()).arg(c.blue())
            .arg(alpha, 0, 'f', 2);
    }

    // Build the full token palette from config
    static ThemeColors colors(const Config &config)
    {
        const bool isLight = (config.themeMode == 1);
        const QString accent = config.accentColor.isEmpty()
            ? QStringLiteral("#BB86FC") : config.accentColor;

        ThemeColors t;

        // ── Accent-derived (shared by both themes) ──
        t.accent          = accent;
        t.iconAccent      = QColor(accent);
        t.iconFavActive   = QColor(accent);

        if (isLight) {
            // ── Light Theme ──
            t.bgWindow        = QStringLiteral("#F5F5F5");
            t.bgPanel         = QStringLiteral("#FFFFFF");
            t.bgSurface       = QStringLiteral("rgba(245,245,245,0.95)");
            t.bgSurfaceAlpha  = QStringLiteral("rgba(0,0,0,0.45)");
            t.bgInput         = QStringLiteral("#FFFFFF");

            t.sidebarGradStart = QStringLiteral("#F0F0F4");
            t.sidebarGradEnd   = QStringLiteral("#E8E8EE");
            t.sidebarBorder    = QStringLiteral("rgba(0,0,0,0.08)");

            t.textPrimary     = QStringLiteral("#111111");
            t.textSecondary   = QStringLiteral("#666666");
            t.textDimmed      = QStringLiteral("rgba(0,0,0,0.35)");
            t.textOnAccent    = QStringLiteral("#FFFFFF");

            t.accentAlpha     = toRgba(accent, 0.30);
            t.accentSelectedBg = toRgba(accent, 0.18);

            t.success         = QStringLiteral("#03DAC6");
            t.error           = QStringLiteral("#D32F2F");
            t.errorAlpha      = QStringLiteral("rgba(211,47,47,0.12)");
            t.warning         = QStringLiteral("#F9A825");

            t.border          = QStringLiteral("#DDDDDD");
            t.borderSubtle    = QStringLiteral("rgba(0,0,0,0.06)");
            t.separator       = QStringLiteral("rgba(0,0,0,0.1)");

            t.hoverBg         = QStringLiteral("rgba(0,0,0,0.06)");
            t.pressedBg       = toRgba(accent, 0.20);

            t.scrollBg        = QStringLiteral("#E0E0E0");
            t.scrollHover     = QStringLiteral("#CCCCCC");

            t.statusBarBg     = QStringLiteral("#EEEEEE");

            t.epgProgressBg   = QStringLiteral("#E0E0E0");
            t.epgProgressChunk = QStringLiteral("#6366f1");

            t.iconDefault     = QColor(80, 80, 100);
            t.iconMuted       = QColor(120, 120, 140);
            t.iconSuccess     = QColor(0, 180, 160);
            t.iconError       = QColor(211, 47, 47);
            t.iconRecord      = QColor(211, 47, 47);
        } else {
            // ── Dark Theme ──
            t.bgWindow        = QStringLiteral("#121218");
            t.bgPanel         = QStringLiteral("#1E1E24");
            t.bgSurface       = QStringLiteral("rgba(18,18,24,0.95)");
            t.bgSurfaceAlpha  = QStringLiteral("rgba(0,0,0,0.55)");
            t.bgInput         = QStringLiteral("rgba(255,255,255,0.05)");

            t.sidebarGradStart = QStringLiteral("#16161e");
            t.sidebarGradEnd   = QStringLiteral("#101018");
            t.sidebarBorder    = QStringLiteral("rgba(255,255,255,0.04)");

            t.textPrimary     = QStringLiteral("#E0E0E0");
            t.textSecondary   = QStringLiteral("#8888AA");
            t.textDimmed      = QStringLiteral("rgba(255,255,255,0.35)");
            t.textOnAccent    = QStringLiteral("#121218");

            t.accentAlpha     = toRgba(accent, 0.30);
            t.accentSelectedBg = toRgba(accent, 0.15);

            t.success         = QStringLiteral("#03DAC6");
            t.error           = QStringLiteral("#ff5555");
            t.errorAlpha      = QStringLiteral("rgba(255,68,68,0.15)");
            t.warning         = QStringLiteral("#FFD700");

            t.border          = QStringLiteral("rgba(255,255,255,0.05)");
            t.borderSubtle    = QStringLiteral("rgba(255,255,255,0.06)");
            t.separator       = QStringLiteral("rgba(255,255,255,0.1)");

            t.hoverBg         = QStringLiteral("rgba(255,255,255,0.1)");
            t.pressedBg       = toRgba(accent, 0.30);

            t.scrollBg        = QStringLiteral("#333340");
            t.scrollHover     = QStringLiteral("#555566");

            t.statusBarBg     = QStringLiteral("#121212");

            t.epgProgressBg   = QStringLiteral("#2a2a35");
            t.epgProgressChunk = QStringLiteral("#6366f1");

            t.iconDefault     = QColor(200, 200, 210);
            t.iconMuted       = QColor(150, 150, 170);
            t.iconSuccess     = QColor(3, 218, 198);
            t.iconError       = QColor(255, 85, 85);
            t.iconRecord      = QColor(220, 50, 50);
        }

        return t;
    }

    // Generate QSS stylesheet from tokens
    static QString style(const Config &config)
    {
        const ThemeColors t = colors(config);

        return QStringLiteral(R"(
            QMainWindow, QDialog {
                background-color: %1;
            }
            QWidget {
                color: %2;
                font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, sans-serif;
                font-size: 13px;
            }
            QMenuBar {
                background-color: %1;
                color: %2;
                border-bottom: 1px solid %5;
            }
            QMenuBar::item {
                background: transparent;
                padding: 4px 10px;
            }
            QMenuBar::item:selected {
                background: %7;
            }
            QMenuBar::item:pressed {
                background: %8;
                color: %2;
            }

            /* ─── Scrollbars ─── */
            QScrollBar:vertical {
                border: none;
                background: %1;
                width: 10px;
                margin: 0px;
            }
            QScrollBar::handle:vertical {
                background: %9;
                min-height: 20px;
                border-radius: 4px;
                margin: 2px;
            }
            QScrollBar::handle:vertical:hover { background: %10; }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }

            QScrollBar:horizontal {
                border: none;
                background: %1;
                height: 10px;
                margin: 0px;
            }
            QScrollBar::handle:horizontal {
                background: %9;
                min-width: 20px;
                border-radius: 4px;
                margin: 2px;
            }
            QScrollBar::handle:horizontal:hover { background: %10; }
            QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
            QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }

            /* ─── Lists ─── */
            QListView {
                background-color: transparent;
                border: none;
                outline: none;
                padding: 4px;
            }
            QListView::item {
                border-radius: 6px;
                padding: 8px 12px;
                margin-bottom: 2px;
            }
            QListView::item:hover {
                background-color: %7;
            }
            QListView::item:selected {
                background-color: %8;
                color: %2;
                font-weight: bold;
                border-left: 3px solid %3;
            }

            /* ─── Tabs ─── */
            QTabWidget::pane {
                border: none;
                border-top: 1px solid %5;
            }
            QTabBar::tab {
                background: transparent;
                color: %4;
                padding: 8px 16px;
                border-bottom: 2px solid transparent;
                font-weight: 600;
            }
            QTabBar::tab:hover {
                color: %2;
            }
            QTabBar::tab:selected {
                color: %2;
                border-bottom: 2px solid %3;
            }

            /* ─── Inputs ─── */
            QLineEdit {
                background-color: %6;
                border: 1px solid %5;
                border-radius: 6px;
                padding: 8px 12px;
                color: %2;
            }
            QLineEdit:focus {
                border: 1px solid %3;
            }

            /* ─── Menus ─── */
            QMenu {
                background-color: %1;
                border: 1px solid %5;
                border-radius: 8px;
                padding: 5px;
                color: %2;
            }
            QMenu::item {
                padding: 6px 20px;
                border-radius: 4px;
            }
            QMenu::item:selected {
                background-color: %3;
                color: %1;
            }

            /* ─── Sidebar ─── */
            QWidget#sidebar {
                background-color: %1;
                border-right: 1px solid %5;
            }

            QComboBox {
                background-color: %6;
                border: 1px solid %5;
                border-radius: 6px;
                padding: 6px 12px;
                color: %2;
            }
            QComboBox:hover {
                border: 1px solid %3;
            }
            QComboBox::drop-down { border: none; }
            QComboBox::down-arrow {
                image: none;
                border-left: 5px solid transparent;
                border-right: 5px solid transparent;
                border-top: 5px solid %4;
                margin-right: 8px;
            }
            QComboBox:on {
                border-bottom-left-radius: 0;
                border-bottom-right-radius: 0;
            }
            QComboBox QAbstractItemView {
                background-color: %1;
                border: 1px solid %5;
                border-radius: 0 0 6px 6px;
                padding: 4px;
                color: %2;
                outline: none;
                selection-background-color: %8;
                selection-color: %2;
            }
            QComboBox QAbstractItemView::item {
                padding: 6px 12px;
                border-radius: 4px;
                margin: 1px 0;
            }
            QComboBox QAbstractItemView::item:hover {
                background-color: %7;
            }
            QComboBox QAbstractItemView::item:selected {
                background-color: %8;
                color: %2;
            }

            /* ─── Buttons ─── */
            QToolButton, QPushButton {
                background-color: %6;
                border: 1px solid %5;
                border-radius: 6px;
                color: %2;
                padding: 6px 12px;
            }
            QToolButton:hover, QPushButton:hover {
                background-color: %7;
            }
            QToolButton:pressed, QPushButton:pressed {
                background-color: %3;
                color: %1;
            }

            QPushButton#primaryBtn {
                background-color: %3;
                color: %1;
                border: none;
            }
            QPushButton#primaryBtn:hover {
                background-color: %3;
            }

            /* ─── Progress Bar ─── */
            QProgressBar {
                background-color: %6;
                border-radius: 2px;
                text-align: center;
            }
            QProgressBar::chunk {
                background-color: %3;
                border-radius: 2px;
            }

            /* ─── Status Bar ─── */
            QStatusBar {
                background: %11;
                color: %4;
                border-top: 1px solid %5;
            }

        )")
        .arg(t.bgWindow)          // %1
        .arg(t.textPrimary)       // %2
        .arg(t.accent)            // %3
        .arg(t.textSecondary)     // %4
        .arg(t.border)            // %5
        .arg(t.bgInput)           // %6
        .arg(t.hoverBg)           // %7
        .arg(t.accentSelectedBg)  // %8
        .arg(t.scrollBg)          // %9
        .arg(t.scrollHover)       // %10
        .arg(t.statusBarBg);      // %11
    }
};
