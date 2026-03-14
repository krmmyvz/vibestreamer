#include "settingsdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>

SettingsDialog::SettingsDialog(const Config &config, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Tercihler"));
    setMinimumWidth(380);

    auto *lay = new QVBoxLayout(this);

    // ── Player group ──────────────────────────────────────────────────
    auto *playerGroup = new QGroupBox(QStringLiteral("Oynatıcı"));
    auto *pf          = new QFormLayout(playerGroup);

    m_hwDecodeCombo = new QComboBox;
    const QStringList hwOptions = {
        QStringLiteral("auto-safe"),
        QStringLiteral("auto"),
        QStringLiteral("vaapi"),
        QStringLiteral("vdpau"),
        QStringLiteral("nvdec"),
        QStringLiteral("videotoolbox"),
        QStringLiteral("d3d11va"),
        QStringLiteral("no"),
    };
    m_hwDecodeCombo->addItems(hwOptions);
    m_hwDecodeCombo->setCurrentText(config.mpvHwDecode);
    pf->addRow(QStringLiteral("Donanım Decode:"), m_hwDecodeCombo);

    m_extraArgsEdit = new QLineEdit(config.mpvExtraArgs);
    m_extraArgsEdit->setPlaceholderText(QStringLiteral("key=value key2=value2 …"));
    pf->addRow(QStringLiteral("Ekstra MPV Seçenekleri:"), m_extraArgsEdit);

    m_minimizeTrayCheck = new QCheckBox(QStringLiteral("Kapatınca sistem tepsisine küçült"));
    m_minimizeTrayCheck->setChecked(config.minimizeToTray);
    pf->addRow(m_minimizeTrayCheck);
    
    auto *recordPathRow = new QHBoxLayout;
    m_recordPathEdit = new QLineEdit(config.recordPath);
    m_recordPathEdit->setPlaceholderText(QStringLiteral("Varsayılan: Videolar klasörü"));
    auto *browseRecordBtn = new QPushButton(QStringLiteral("Gözat"));
    connect(browseRecordBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseRecordPath);
    recordPathRow->addWidget(m_recordPathEdit);
    recordPathRow->addWidget(browseRecordBtn);
    pf->addRow(QStringLiteral("Kayıt Konumu:"), recordPathRow);

    lay->addWidget(playerGroup);

    // ── Audio group ───────────────────────────────────────────────────
    auto *audioGroup = new QGroupBox(QStringLiteral("Ses"));
    auto *af         = new QFormLayout(audioGroup);

    auto *volRow = new QHBoxLayout;
    m_volumeSlider = new QSlider(Qt::Horizontal);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(config.volume);
    auto *volLabel = new QLabel(QString::number(config.volume));
    volLabel->setMinimumWidth(28);
    connect(m_volumeSlider, &QSlider::valueChanged, volLabel,
            [volLabel](int v){ volLabel->setText(QString::number(v)); });
    volRow->addWidget(m_volumeSlider);
    volRow->addWidget(volLabel);
    af->addRow(QStringLiteral("Varsayılan Ses:"), volRow);
    lay->addWidget(audioGroup);

    // ── Theme group ───────────────────────────────────────────────────
    auto *themeGroup = new QGroupBox(QStringLiteral("Görünüm"));
    auto *tf         = new QFormLayout(themeGroup);

    m_themeModeCombo = new QComboBox;
    m_themeModeCombo->addItem(QStringLiteral("Koyu Tema"), 0);
    m_themeModeCombo->addItem(QStringLiteral("Açık Tema"), 1);
    m_themeModeCombo->setCurrentIndex(config.themeMode);
    tf->addRow(QStringLiteral("Tema:"), m_themeModeCombo);

    m_accentColorEdit = new QLineEdit(config.accentColor);
    m_accentColorEdit->setPlaceholderText(QStringLiteral("#RRGGBB formatında (Örn: #BB86FC)"));
    tf->addRow(QStringLiteral("Vurgu Rengi:"), m_accentColorEdit);

    lay->addWidget(themeGroup);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::applyTo(Config &config) const
{
    config.mpvHwDecode    = m_hwDecodeCombo->currentText();
    config.mpvExtraArgs   = m_extraArgsEdit->text().trimmed();
    config.minimizeToTray = m_minimizeTrayCheck->isChecked();
    config.recordPath     = m_recordPathEdit->text().trimmed();
    config.themeMode      = m_themeModeCombo->currentData().toInt();
    config.accentColor    = m_accentColorEdit->text().trimmed();
    if (config.accentColor.isEmpty()) {
        config.accentColor = QStringLiteral("#BB86FC");
    }
    config.volume         = m_volumeSlider->value();
    config.save();
}

void SettingsDialog::onBrowseRecordPath()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Kayıt Konumunu Seç"),
                                                          m_recordPathEdit->text());
    if (!dir.isEmpty()) {
        m_recordPathEdit->setText(dir);
    }
}
