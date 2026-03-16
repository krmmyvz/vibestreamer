#include "multiviewwidget.h"
#include "mpvwidget.h"

#include <QEvent>
#include <QGridLayout>
#include <QVBoxLayout>

MultiViewWidget::MultiViewWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);

    for (int i = 0; i < 4; ++i) {
        m_frames[i] = new QFrame(this);
        m_frames[i]->setFrameShape(QFrame::Box);

        auto *lay = new QVBoxLayout(m_frames[i]);
        lay->setContentsMargins(2, 2, 2, 2);
        lay->setSpacing(0);

        m_players[i] = new MpvWidget(m_frames[i]);
        m_players[i]->setVolume(0); // muted by default; active player will be unmuted by MainWindow
        lay->addWidget(m_players[i]);

        m_players[i]->installEventFilter(this);

        grid->addWidget(m_frames[i], i / 2, i % 2);
        grid->setRowStretch(i / 2, 1);
        grid->setColumnStretch(i % 2, 1);
    }

    updateHighlight();
}

MpvWidget* MultiViewWidget::player(int i) const
{
    if (i < 0 || i > 3) return nullptr;
    return m_players[i];
}

MpvWidget* MultiViewWidget::activePlayer() const
{
    return m_players[m_activeIndex];
}

void MultiViewWidget::setActiveIndex(int i)
{
    if (i < 0 || i > 3 || i == m_activeIndex) return;
    const int old = m_activeIndex;
    m_activeIndex = i;
    updateHighlight();
    emit activeChanged(i, old);
}

void MultiViewWidget::updateHighlight()
{
    for (int i = 0; i < 4; ++i) {
        if (i == m_activeIndex)
            m_frames[i]->setStyleSheet(QStringLiteral("QFrame { border: 2px solid #BB86FC; }"));
        else
            m_frames[i]->setStyleSheet(QStringLiteral("QFrame { border: 2px solid #333333; }"));
    }
}

bool MultiViewWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        for (int i = 0; i < 4; ++i) {
            if (obj == m_players[i]) {
                setActiveIndex(i);
                break;
            }
        }
    }
    return false; // don't consume — let mpv handle it too
}
