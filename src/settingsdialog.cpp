#include "settingsdialog.h"
#include "localization.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QTabWidget>

SettingsDialog::SettingsDialog(const Config &config, QWidget *parent)
    : QDialog(parent)
{
    const auto &i = I18n::instance();
    setWindowTitle(i.get(QStringLiteral("pref_title")));
    setMinimumWidth(400);

    auto *mainLay = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    mainLay->addWidget(tabs);

    auto *generalTab = new QWidget;
    auto *lay = new QVBoxLayout(generalTab);

    // ── Player group ──────────────────────────────────────────────────
    auto *playerGroup = new QGroupBox(i.get(QStringLiteral("pref_player")));
    auto *pf          = new QFormLayout(playerGroup);

    m_languageCombo = new QComboBox;
    m_languageCombo->addItem(QStringLiteral("Türkçe"), QStringLiteral("tr"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    const int langIdx = m_languageCombo->findData(config.language);
    m_languageCombo->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
    pf->addRow(i.get(QStringLiteral("pref_language")), m_languageCombo);

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
    pf->addRow(i.get(QStringLiteral("pref_hw_decode")), m_hwDecodeCombo);

    m_extraArgsEdit = new QLineEdit(config.mpvExtraArgs);
    m_extraArgsEdit->setPlaceholderText(QStringLiteral("key=value key2=value2 …"));
    pf->addRow(i.get(QStringLiteral("pref_extra_mpv")), m_extraArgsEdit);

    m_minimizeTrayCheck = new QCheckBox(i.get(QStringLiteral("pref_minimize_tray")));
    m_minimizeTrayCheck->setChecked(config.minimizeToTray);
    pf->addRow(m_minimizeTrayCheck);

    m_statePersistenceCheck = new QCheckBox(i.get(QStringLiteral("pref_state_persistence")));
    m_statePersistenceCheck->setChecked(config.statePersistence);
    pf->addRow(m_statePersistenceCheck);

    auto *recordPathRow = new QHBoxLayout;
    m_recordPathEdit = new QLineEdit(config.recordPath);
    m_recordPathEdit->setPlaceholderText(i.get(QStringLiteral("pref_record_path_placeholder")));
    auto *browseRecordBtn = new QPushButton(i.get(QStringLiteral("pref_browse")));
    connect(browseRecordBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseRecordPath);
    recordPathRow->addWidget(m_recordPathEdit);
    recordPathRow->addWidget(browseRecordBtn);
    pf->addRow(i.get(QStringLiteral("pref_record_path")), recordPathRow);

    lay->addWidget(playerGroup);

    // ── Theme group ───────────────────────────────────────────────────
    auto *themeGroup = new QGroupBox(i.get(QStringLiteral("pref_appearance")));
    auto *tf         = new QFormLayout(themeGroup);

    m_themeModeCombo = new QComboBox;
    m_themeModeCombo->addItem(i.get(QStringLiteral("pref_dark_theme")), 0);
    m_themeModeCombo->addItem(i.get(QStringLiteral("pref_light_theme")), 1);
    m_themeModeCombo->setCurrentIndex(config.themeMode);
    tf->addRow(i.get(QStringLiteral("pref_theme")), m_themeModeCombo);

    m_accentColorEdit = new QLineEdit(config.accentColor);
    m_accentColorEdit->setPlaceholderText(i.get(QStringLiteral("pref_accent_placeholder")));
    tf->addRow(i.get(QStringLiteral("pref_accent_color")), m_accentColorEdit);

    lay->addWidget(themeGroup);
    lay->addStretch();
    tabs->addTab(generalTab, i.get(QStringLiteral("pref_general")));

    // ── Shortcuts Tab ─────────────────────────────────────────────────
    auto *shortcutsTab = new QWidget;
    auto *scLay        = new QFormLayout(shortcutsTab);

    struct ShortcutItem { QString id; QString key; };
    const QList<ShortcutItem> scNames = {
        {QStringLiteral("play_pause"), QStringLiteral("sc_play_pause")},
        {QStringLiteral("fullscreen"), QStringLiteral("sc_fullscreen")},
        {QStringLiteral("mute"),       QStringLiteral("sc_mute")},
        {QStringLiteral("vol_up"),     QStringLiteral("sc_vol_up")},
        {QStringLiteral("vol_down"),   QStringLiteral("sc_vol_down")},
        {QStringLiteral("ch_next"),    QStringLiteral("sc_ch_next")},
        {QStringLiteral("ch_prev"),    QStringLiteral("sc_ch_prev")},
        {QStringLiteral("info"),       QStringLiteral("sc_info")},
        {QStringLiteral("audio"),      QStringLiteral("sc_audio")},
        {QStringLiteral("subs"),       QStringLiteral("sc_subs")},
        {QStringLiteral("favorite"),   QStringLiteral("sc_favorite")},
    };

    for (const auto &item : scNames) {
        auto *edit = new QKeySequenceEdit(shortcutsTab);
        edit->setKeySequence(QKeySequence::fromString(config.shortcuts.value(item.id)));
        m_shortcutEdits.insert(item.id, edit);
        scLay->addRow(i.get(item.key) + QStringLiteral(":"), edit);
    }
    tabs->addTab(shortcutsTab, i.get(QStringLiteral("pref_shortcuts")));

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLay->addWidget(btns);

    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SettingsDialog::applyTo(Config &config) const
{
    config.language       = m_languageCombo->currentData().toString();
    config.mpvHwDecode    = m_hwDecodeCombo->currentText();
    config.mpvExtraArgs   = m_extraArgsEdit->text().trimmed();
    config.minimizeToTray = m_minimizeTrayCheck->isChecked();
    config.statePersistence = m_statePersistenceCheck->isChecked();
    config.recordPath     = m_recordPathEdit->text().trimmed();
    config.themeMode      = m_themeModeCombo->currentData().toInt();
    config.accentColor    = m_accentColorEdit->text().trimmed();
    if (config.accentColor.isEmpty()) {
        config.accentColor = QStringLiteral("#BB86FC");
    }

    for (auto it = m_shortcutEdits.constBegin(); it != m_shortcutEdits.constEnd(); ++it) {
        config.shortcuts.insert(it.key(), it.value()->keySequence().toString());
    }

    config.save();
}

void SettingsDialog::onBrowseRecordPath()
{
    const QString dir = QFileDialog::getExistingDirectory(this,
        I18n::instance().get(QStringLiteral("pref_select_record_folder")),
        m_recordPathEdit->text());
    if (!dir.isEmpty()) {
        m_recordPathEdit->setText(dir);
    }
}
