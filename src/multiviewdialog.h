#pragma once

#include "models.h"
#include <QDialog>
#include <QGridLayout>

class MpvWidget;

class MultiViewDialog : public QDialog {
    Q_OBJECT
public:
    explicit MultiViewDialog(QWidget *parent = nullptr);
    ~MultiViewDialog() override;

    // Send a channel to a specific screen index (0 to 3)
    void playChannel(int screenIndex, const Channel &ch);

private:
    void setupUI();

    MpvWidget *m_players[4];
};
