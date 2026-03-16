#pragma once

#include <QWidget>
#include <QFrame>

class MpvWidget;

class MultiViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit MultiViewWidget(QWidget *parent = nullptr);

    MpvWidget* player(int i) const;
    MpvWidget* activePlayer() const;
    int        activeIndex() const { return m_activeIndex; }
    void       setActiveIndex(int i);

signals:
    void activeChanged(int newIndex, int oldIndex);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void updateHighlight();

    MpvWidget *m_players[4];
    QFrame    *m_frames[4];
    int        m_activeIndex = 0;
};
