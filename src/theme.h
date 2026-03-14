#pragma once
#include <QString>
#include "config.h"

class Theme {
public:
    static QString style(const Config &config)
    {
        // 0: Dark, 1: Light
        const bool isLight = (config.themeMode == 1);
        const QString accent = config.accentColor.isEmpty() ? QStringLiteral("#BB86FC") : config.accentColor;
        
        // Base Colors
        const QString bgWindow   = isLight ? QStringLiteral("#F5F5F5") : QStringLiteral("#121218");
        const QString bgPanel    = isLight ? QStringLiteral("#FFFFFF") : QStringLiteral("#1E1E24");
        const QString textMain   = isLight ? QStringLiteral("#111111") : QStringLiteral("#E0E0E0");
        const QString textMuted  = isLight ? QStringLiteral("#666666") : QStringLiteral("#8888AA");
        const QString borderLine = isLight ? QStringLiteral("#DDDDDD") : QStringLiteral("rgba(255,255,255,0.05)");
        const QString bgInput    = isLight ? QStringLiteral("#FFFFFF") : QStringLiteral("rgba(255, 255, 255, 0.05)");
        const QString itemHover  = isLight ? QStringLiteral("#E0E0E0") : QStringLiteral("rgba(255, 255, 255, 0.05)");
        const QString itemSelectedBg = isLight ? accent + "33" : accent + "26"; // add alpha hex
        const QString scrollBg   = isLight ? QStringLiteral("#E0E0E0") : QStringLiteral("#333340");
        const QString scrollHover= isLight ? QStringLiteral("#CCCCCC") : QStringLiteral("#555566");
        const QString statusBarBg= isLight ? QStringLiteral("#EEEEEE") : QStringLiteral("#121212");

        return QStringLiteral(R"(
            QMainWindow {
                background-color: %1;
            }
            QWidget {
                color: %2;
                font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, sans-serif;
                font-size: 13px;
            }

            /* ─── Scrollbars ─── */
            QScrollBar:vertical {
                border: none;
                background: %1;
                width: 10px;
                margin: 0px 0px 0px 0px;
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
                margin: 0px 0px 0px 0px;
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
            QListWidget {
                background-color: transparent;
                border: none;
                outline: none;
                padding: 4px;
            }
            QListWidget::item {
                border-radius: 6px;
                padding: 8px 12px;
                margin-bottom: 2px;
            }
            QListWidget::item:hover {
                background-color: %7;
            }
            QListWidget::item:selected {
                background-color: %8;
                color: %3;
                font-weight: bold;
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

            /* ─── Sidebar (Glass Effect) ─── */
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
            
            /* Primary Action Button (if any needed) */
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
        .arg(bgWindow)       // %1
        .arg(textMain)       // %2
        .arg(accent)         // %3
        .arg(textMuted)      // %4
        .arg(borderLine)     // %5
        .arg(bgInput)        // %6
        .arg(itemHover)      // %7
        .arg(itemSelectedBg) // %8
        .arg(scrollBg)       // %9
        .arg(scrollHover)    // %10
        .arg(statusBarBg);   // %11
    }
};
