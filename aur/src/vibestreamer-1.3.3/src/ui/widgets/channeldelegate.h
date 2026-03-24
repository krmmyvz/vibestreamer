#pragma once
#include <QApplication>
#include <QPainter>
#include <QStyledItemDelegate>

// Custom data roles stored on QStandardItems in the channel list
static constexpr int ChannelEpgProgressRole = Qt::UserRole + 2; // int 0-100
static constexpr int ChannelEpgTitleRole    = Qt::UserRole + 3; // QString

// ChannelItemDelegate — draws an EPG progress bar + program title below the
// channel name when EPG data is available. Falls back to default rendering
// when no EPG data is set on the item.
class ChannelItemDelegate : public QStyledItemDelegate {
public:
    explicit ChannelItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QSize s = QStyledItemDelegate::sizeHint(option, index);
        if (!index.data(ChannelEpgTitleRole).toString().isEmpty())
            s.setHeight(s.height() + 13);
        return s;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const QString epgTitle = index.data(ChannelEpgTitleRole).toString();
        if (epgTitle.isEmpty()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }

        // Draw channel name in upper portion
        QStyleOptionViewItem nameOpt = option;
        initStyleOption(&nameOpt, index);
        nameOpt.rect.setBottom(option.rect.bottom() - 13);
        QStyledItemDelegate::paint(painter, nameOpt, index);

        painter->save();
        painter->setClipRect(option.rect);

        const int progress = index.data(ChannelEpgProgressRole).toInt();
        const bool selected = (option.state & QStyle::State_Selected);

        // Colors
        const QColor dimColor = selected
            ? option.palette.highlightedText().color()
            : option.palette.text().color();
        const QColor barBg  = dimColor.darker(300);
        const QColor barFg  = option.palette.highlight().color();

        // Progress bar (3px tall, full width minus margins)
        const int margin = 4;
        const QRect barRect(option.rect.left() + margin,
                            option.rect.bottom() - 10,
                            option.rect.width() - margin * 2, 3);
        painter->fillRect(barRect, barBg);
        if (progress > 0) {
            const int fillW = barRect.width() * progress / 100;
            painter->fillRect(QRect(barRect.left(), barRect.top(), fillW, barRect.height()), barFg);
        }

        // EPG title text (tiny, above bar)
        QFont f = painter->font();
        f.setPointSizeF(f.pointSizeF() * 0.75);
        painter->setFont(f);
        painter->setPen(dimColor.darker(selected ? 110 : 160));
        const QRect textRect(option.rect.left() + margin,
                             option.rect.bottom() - 22,
                             option.rect.width() - margin * 2, 12);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
            painter->fontMetrics().elidedText(epgTitle, Qt::ElideRight, textRect.width()));

        painter->restore();
    }
};
