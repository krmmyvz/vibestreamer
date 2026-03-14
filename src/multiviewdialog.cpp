#include "multiviewdialog.h"
#include "mpvwidget.h"
#include <QMouseEvent>
#include <QKeyEvent>

MultiViewDialog::MultiViewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Xtream Player - Çoklu Ekran (Multi-View)"));
    resize(1280, 720);
    setWindowFlags(Qt::Window); // behave like a normal window, not a blocking dialog

    setupUI();
}

MultiViewDialog::~MultiViewDialog() = default;

void MultiViewDialog::setupUI()
{
    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2); // small gap between players

    for (int i = 0; i < 4; ++i) {
        m_players[i] = new MpvWidget(this);
        // By default, mute all screens so they don't overlap noisy audio
        // Alternatively, we could unmute only the first one
        m_players[i]->setVolume(0); 

        int row = i / 2;
        int col = i % 2;
        layout->addWidget(m_players[i], row, col);
    }
}

void MultiViewDialog::playChannel(int screenIndex, const Channel &ch)
{
    if (screenIndex < 0 || screenIndex > 3) return;
    if (!m_players[screenIndex]) return;

    m_players[screenIndex]->play(ch.streamUrl);
    // Unmute the screen being commanded if we want it to grab focus
    for (int i=0; i<4; ++i) {
        m_players[i]->setVolume(i == screenIndex ? 100 : 0);
    }
}
