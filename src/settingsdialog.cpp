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
    const QString &lang = config.language;
    setWindowTitle(Localization::text(lang, QStringLiteral("Tercihler"), QStringLiteral("Preferences")));
    setMinimumWidth(400);

    auto *mainLay = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    mainLay->addWidget(tabs);

    auto *generalTab = new QWidget;
    auto *lay = new QVBoxLayout(generalTab);

    // ── Player group ──────────────────────────────────────────────────
    auto *playerGroup = new QGroupBox(Localization::text(lang, QStringLiteral("Oynatıcı"), QStringLiteral("Player")));
    auto *pf          = new QFormLayout(playerGroup);

    m_languageCombo = new QComboBox;
    m_languageCombo->addItem(QStringLiteral("Türkçe"), QStringLiteral("tr"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    const int langIdx = m_languageCombo->findData(config.language);
    m_languageCombo->setCurrentIndex(langIdx >= 0 ? langIdx : 0);
    pf->addRow(Localization::text(lang, QStringLiteral("Dil:"), QStringLiteral("Language:")), m_languageCombo);

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
    pf->addRow(Localization::text(lang, QStringLiteral("Donanım Decode:"), QStringLiteral("Hardware Decode:")), m_hwDecodeCombo);

    m_extraArgsEdit = new QLineEdit(config.mpvExtraArgs);
    m_extraArgsEdit->setPlaceholderText(QStringLiteral("key=value key2=value2 …"));
    pf->addRow(Localization::text(lang, QStringLiteral("Ekstra MPV Seçenekleri:"), QStringLiteral("Extra MPV Options:")), m_extraArgsEdit);

    m_minimizeTrayCheck = new QCheckBox(Localization::text(lang, QStringLiteral("Kapatınca sistem tepsisine küçült"), QStringLiteral("Minimize to tray on close")));
    m_minimizeTrayCheck->setChecked(config.minimizeToTray);
    pf->addRow(m_minimizeTrayCheck);
    
    auto *recordPathRow = new QHBoxLayout;
    m_recordPathEdit = new QLineEdit(config.recordPath);
    m_recordPathEdit->setPlaceholderText(Localization::text(lang, QStringLiteral("Varsayılan: Videolar klasörü"), QStringLiteral("Default: Movies folder")));
    auto *browseRecordBtn = new QPushButton(Localization::text(lang, QStringLiteral("Gözat"), QStringLiteral("Browse")));
    connect(browseRecordBtn, &QPushButton::clicked, this, &SettingsDialog::onBrowseRecordPath);
    recordPathRow->addWidget(m_recordPathEdit);
    recordPathRow->addWidget(browseRecordBtn);
    pf->addRow(Localization::text(lang, QStringLiteral("Kayıt Konumu:"), QStringLiteral("Recording Path:")), recordPathRow);

    lay->addWidget(playerGroup);

    // ── Theme group ───────────────────────────────────────────────────
    auto *themeGroup = new QGroupBox(Localization::text(lang, QStringLiteral("Görünüm"), QStringLiteral("Appearance")));
    auto *tf         = new QFormLayout(themeGroup);

    m_themeModeCombo = new QComboBox;
    m_themeModeCombo->addItem(Localization::text(lang, QStringLiteral("Koyu Tema"), QStringLiteral("Dark Theme")), 0);
    m_themeModeCombo->addItem(Localization::text(lang, QStringLiteral("Açık Tema"), QStringLiteral("Light Theme")), 1);
    m_themeModeCombo->setCurrentIndex(config.themeMode);
    tf->addRow(Localization::text(lang, QStringLiteral("Tema:"), QStringLiteral("Theme:")), m_themeModeCombo);

    m_accentColorEdit = new QLineEdit(config.accentColor);
    m_accentColorEdit->setPlaceholderText(Localization::text(
        lang,
        QStringLiteral("#RRGGBB formatında (Örn: #BB86FC)"),
        QStringLiteral("#RRGGBB format (e.g. #BB86FC)")
    ));
    tf->addRow(Localization::text(lang, QStringLiteral("Vurgu Rengi:"), QStringLiteral("Accent Color:")), m_accentColorEdit);

    lay->addWidget(themeGroup);
    lay->addStretch();
    tabs->addTab(generalTab, Localization::text(lang, QStringLiteral("Genel"), QStringLiteral("General")));

    // ── Shortcuts Tab ─────────────────────────────────────────────────
    auto *shortcutsTab = new QWidget;
    auto *scLay        = new QFormLayout(shortcutsTab);
    
    struct ShortcutItem { QString id; QString tr; QString en; };
    const QList<ShortcutItem> scNames = {
        {QStringLiteral("play_pause"), QStringLiteral("Oynat / Duraklat"), QStringLiteral("Play / Pause")},
        {QStringLiteral("fullscreen"), QStringLiteral("Tam Ekran"), QStringLiteral("Fullscreen")},
        {QStringLiteral("mute"),       QStringLiteral("Sesi Kapat/Aç"), QStringLiteral("Mute / Unmute")},
        {QStringLiteral("vol_up"),     QStringLiteral("Sesi Artır"), QStringLiteral("Volume Up")},
        {QStringLiteral("vol_down"),   QStringLiteral("Sesi Azalt"), QStringLiteral("Volume Down")},
        {QStringLiteral("ch_next"),    QStringLiteral("Sonraki Kanal"), QStringLiteral("Next Channel")},
        {QStringLiteral("ch_prev"),    QStringLiteral("Önceki Kanal"), QStringLiteral("Previous Channel")},
        {QStringLiteral("info"),       QStringLiteral("Medya Bilgisi Göster"), QStringLiteral("Show Media Info")},
        {QStringLiteral("audio"),      QStringLiteral("Ses Parçası Seç"), QStringLiteral("Select Audio Track")},
        {QStringLiteral("subs"),       QStringLiteral("Altyazı Seç"), QStringLiteral("Select Subtitle")},
        {QStringLiteral("favorite"),   QStringLiteral("Favoriye Ekle/Çıkar"), QStringLiteral("Toggle Favorite")},
    };

    for (const auto &item : scNames) {
        auto *edit = new QKeySequenceEdit(shortcutsTab);
        edit->setKeySequence(QKeySequence::fromString(config.shortcuts.value(item.id)));
        m_shortcutEdits.insert(item.id, edit);
        scLay->addRow(Localization::text(lang, item.tr, item.en) + QStringLiteral(":"), edit);
    }
    tabs->addTab(shortcutsTab, Localization::text(lang, QStringLiteral("Kısayollar"), QStringLiteral("Shortcuts")));

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
    const QString dialogTitle = Localization::text(
        m_languageCombo->currentData().toString(),
        QStringLiteral("Kayıt Konumunu Seç"),
        QStringLiteral("Select Recording Folder")
    );
    const QString dir = QFileDialog::getExistingDirectory(this, dialogTitle,
                                                          m_recordPathEdit->text());
    if (!dir.isEmpty()) {
        m_recordPathEdit->setText(dir);
    }
}
