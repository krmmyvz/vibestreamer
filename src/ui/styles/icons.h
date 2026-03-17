#pragma once
#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QSvgRenderer>
#include <QString>

// ─── Inline SVG Icon Factory ─────────────────────────────────────────────────
//  Material Design inspired icons rendered from SVG path data.
//  No .qrc files needed — everything is inline.

namespace Icons {

// Render a single SVG <path> into a QIcon (with 1× and 2× for HiDPI)
inline QIcon fromSvgPath(const QString &pathData,
                         const QColor &color = QColor(200, 200, 210),
                         int size = 18)
{
    const QString svg = QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
        "<path fill='%1' d='%2'/></svg>")
        .arg(color.name(), pathData);

    QSvgRenderer renderer(svg.toUtf8());

    QIcon icon;
    for (int scale : {1, 2}) {
        const int s = size * scale;
        QPixmap pm(s, s);
        pm.setDevicePixelRatio(scale);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
        icon.addPixmap(pm);
    }
    return icon;
}

// Render full SVG markup into a QIcon
inline QIcon fromSvg(const QString &svgMarkup, int size = 18)
{
    QSvgRenderer renderer(svgMarkup.toUtf8());
    QIcon icon;
    for (int scale : {1, 2}) {
        const int s = size * scale;
        QPixmap pm(s, s);
        pm.setDevicePixelRatio(scale);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        renderer.render(&p);
        icon.addPixmap(pm);
    }
    return icon;
}

// ─── Playback ────────────────────────────────────────────────────────────────

inline QIcon skipPrevious(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M6 6h2v12H6zm3.5 6l8.5 6V6z"), c);
}

inline QIcon rewind(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M11 18V6l-8.5 6 8.5 6zm.5-6l8.5 6V6l-8.5 6z"), c);
}

inline QIcon play(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M8 5v14l11-7z"), c);
}

inline QIcon pause(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M6 19h4V5H6v14zm8-14v14h4V5h-4z"), c);
}

inline QIcon fastForward(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M4 18l8.5-6L4 6v12zm9-12v12l8.5-6L13 6z"), c);
}

inline QIcon skipNext(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M6 18l8.5-6L6 6v12zM16 6v12h2V6h-2z"), c);
}

// ─── Audio / Subtitle ────────────────────────────────────────────────────────

inline QIcon volumeUp(const QColor &c = QColor(200,200,210)) {
    return fromSvg(QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
        "<path fill='%1' d='M3 9v6h4l5 5V4L7 9H3zm13.5 3c0-1.77-1.02-3.29-2.5-4.03v8.05c1.48-.73 2.5-2.25 2.5-4.02zM14 3.23v2.06c2.89.86 5 3.54 5 6.71s-2.11 5.85-5 6.71v2.06c4.01-.91 7-4.49 7-8.77s-2.99-7.86-7-8.77z'/>"
        "</svg>").arg(c.name()));
}

inline QIcon closedCaption(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M19 4H5c-1.11 0-2 .9-2 2v12c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V6c0-1.1-.9-2-2-2zm-8 7H9.5v-.5h-2v3h2V13H11v1c0 .55-.45 1-1 1H7c-.55 0-1-.45-1-1v-4c0-.55.45-1 1-1h3c.55 0 1 .45 1 1v1zm7 0h-1.5v-.5h-2v3h2V13H18v1c0 .55-.45 1-1 1h-3c-.55 0-1-.45-1-1v-4c0-.55.45-1 1-1h3c.55 0 1 .45 1 1v1z"), c);
}

inline QIcon audioTrack(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M12 3v9.28c-.47-.17-.97-.28-1.5-.28C8.01 12 6 14.01 6 16.5S8.01 21 10.5 21c2.31 0 4.2-1.75 4.45-4H15V6h4V3h-7z"), c);
}

// ─── Utility ─────────────────────────────────────────────────────────────────

inline QIcon info(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z"), c);
}

inline QIcon starFilled(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M12 17.27L18.18 21l-1.64-7.03L22 9.24l-7.19-.61L12 2 9.19 8.63 2 9.24l5.46 4.73L5.82 21z"), c);
}

inline QIcon starOutline(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M22 9.24l-7.19-.62L12 2 9.19 8.63 2 9.24l5.46 4.73L5.82 21 12 17.27 18.18 21l-1.63-7.03L22 9.24zM12 15.4l-3.76 2.27 1-4.28-3.32-2.88 4.38-.38L12 6.1l1.71 4.04 4.38.38-3.32 2.88 1 4.28L12 15.4z"), c);
}

inline QIcon tvGuide(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M21 3H3c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H3V5h18v14zM5 15h2v2H5zm0-4h2v2H5zm0-4h2v2H5zm4 8h8v2H9zm0-4h8v2H9zm0-4h8v2H9z"), c);
}

inline QIcon pictureInPicture(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M19 7h-8v6h8V7zm2-4H3c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h18c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm0 16H3V5h18v14z"), c);
}

inline QIcon fullscreen(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M7 14H5v5h5v-2H7v-3zm-2-4h2V7h3V5H5v5zm12 7h-3v2h5v-5h-2v3zM14 5v2h3v3h2V5h-5z"), c);
}

inline QIcon fullscreenExit(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M5 16h3v3h2v-5H5v2zm3-8H5v2h5V5H8v3zm6 11h2v-3h3v-2h-5v5zm2-11V5h-2v5h5V8h-3z"), c);
}

inline QIcon record(const QColor &c = QColor(220,50,50)) {
    // A simple filled circle for recording
    return fromSvgPath(QStringLiteral("M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2z"), c);
}

inline QIcon stop(const QColor &c = QColor(220,50,50)) {
    // A filled square for stop
    return fromSvgPath(QStringLiteral("M6 6h12v12H6z"), c);
}

inline QIcon gridView(const QColor &c = QColor(200,200,210)) {
    // A 4-pane grid representation for multi-view
    return fromSvgPath(QStringLiteral("M4 4h6v6H4V4zm8 0h6v6h-6V4zm-8 8h6v6H4v-6zm8 0h6v6h-6v-6z"), c);
}

// ─── Source Management ───────────────────────────────────────────────────────

inline QIcon add(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M19 13h-6v6h-2v-6H5v-2h6V5h2v6h6v2z"), c);
}

inline QIcon edit(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"), c);
}

inline QIcon trash(const QColor &c = QColor(200,200,210)) {
    return fromSvgPath(QStringLiteral("M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"), c);
}

} // namespace Icons
