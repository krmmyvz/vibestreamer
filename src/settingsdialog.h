#pragma once
#include "config.h"

#include <QComboBox>
#include <QDialog>
#include <QLineEdit>
#include <QSlider>
#include <QCheckBox>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const Config &config, QWidget *parent = nullptr);
    void applyTo(Config &config) const;

private slots:
    void onBrowseRecordPath();

private:
    QComboBox  *m_hwDecodeCombo;
    QLineEdit  *m_extraArgsEdit;
    QLineEdit  *m_recordPathEdit;
    QSlider    *m_volumeSlider;
    QCheckBox  *m_minimizeTrayCheck;

    // Theme
    QComboBox  *m_themeModeCombo;
    QLineEdit  *m_accentColorEdit;
};
