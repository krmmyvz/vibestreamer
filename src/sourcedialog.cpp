#include "sourcedialog.h"
#include "xtreamclient.h"
#include "localization.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

SourceDialog::SourceDialog(const QString &language, QWidget *parent)
    : QDialog(parent)
    , m_language(language)
{
    buildUi(nullptr);
}

SourceDialog::SourceDialog(const Source &source, const QString &language, QWidget *parent)
    : QDialog(parent)
    , m_sourceId(source.id)
    , m_language(language)
{
    buildUi(&source);
}

QString SourceDialog::t(const QString &tr, const QString &en) const
{
    return Localization::text(m_language, tr, en);
}

void SourceDialog::buildUi(const Source *existing)
{
    setWindowTitle(existing ? t(QStringLiteral("Kaynağı Düzenle"), QStringLiteral("Edit Source"))
                            : t(QStringLiteral("Kaynak Ekle"), QStringLiteral("Add Source")));
    setMinimumWidth(420);

    auto *lay = new QVBoxLayout(this);

    // Name
    auto *nameForm = new QFormLayout;
    m_nameEdit = new QLineEdit;
    nameForm->addRow(t(QStringLiteral("İsim:"), QStringLiteral("Name:")), m_nameEdit);
    lay->addLayout(nameForm);

    // Type selection
    auto *typeBox = new QGroupBox(t(QStringLiteral("Kaynak Tipi"), QStringLiteral("Source Type")));
    auto *typeRow = new QHBoxLayout(typeBox);
    m_xtreamRadio = new QRadioButton(QStringLiteral("Xtream Codes"));
    m_m3uRadio    = new QRadioButton(QStringLiteral("M3U / M3U8 URL"));
    m_xtreamRadio->setChecked(true);
    typeRow->addWidget(m_xtreamRadio);
    typeRow->addWidget(m_m3uRadio);
    lay->addWidget(typeBox);

    // Stacked pages
    m_stack = new QStackedWidget;

    // Page 0: Xtream
    auto *xtreamPage = new QWidget;
    auto *xf         = new QFormLayout(xtreamPage);
    m_serverEdit = new QLineEdit; m_serverEdit->setPlaceholderText(QStringLiteral("http://server.com:8080"));
    m_userEdit   = new QLineEdit;
    m_passEdit   = new QLineEdit; m_passEdit->setEchoMode(QLineEdit::Password);
    xf->addRow(t(QStringLiteral("Sunucu URL:"), QStringLiteral("Server URL:")), m_serverEdit);
    xf->addRow(t(QStringLiteral("Kullanıcı:"),  QStringLiteral("Username:")),  m_userEdit);
    xf->addRow(t(QStringLiteral("Şifre:"),      QStringLiteral("Password:")),      m_passEdit);
    auto *testBtn = new QPushButton(t(QStringLiteral("Bağlantıyı Test Et"), QStringLiteral("Test Connection")));
    xf->addRow(QString{}, testBtn);
    m_stack->addWidget(xtreamPage);

    // Page 1: M3U (URL or local file)
    auto *m3uPage = new QWidget;
    auto *mf      = new QFormLayout(m3uPage);
    auto *m3uRow  = new QHBoxLayout;
    m_m3uEdit = new QLineEdit;
    m_m3uEdit->setPlaceholderText(t(QStringLiteral("http://…/playlist.m3u  veya  dosya seçin →"), QStringLiteral("http://…/playlist.m3u  or  choose file →")));
    auto *browseBtn = new QPushButton(t(QStringLiteral("Gözat…"), QStringLiteral("Browse…")));
    browseBtn->setFixedWidth(72);
    m3uRow->addWidget(m_m3uEdit);
    m3uRow->addWidget(browseBtn);
    mf->addRow(t(QStringLiteral("M3U URL / Dosya:"), QStringLiteral("M3U URL / File:")), m3uRow);
    m_stack->addWidget(m3uPage);

    lay->addWidget(m_stack);

    // Common: EPG & Auto-update
    auto *commonForm = new QFormLayout;
    m_epgEdit = new QLineEdit; m_epgEdit->setPlaceholderText(t(QStringLiteral("(isteğe bağlı)"), QStringLiteral("(optional)")));
    commonForm->addRow(QStringLiteral("EPG URL:"), m_epgEdit);

    m_autoUpdateCombo = new QComboBox;
    m_autoUpdateCombo->addItem(t(QStringLiteral("Kapalı (Manuel)"), QStringLiteral("Off (Manual)")), 0);
    m_autoUpdateCombo->addItem(t(QStringLiteral("Her saat başı"), QStringLiteral("Every hour")), 1);
    m_autoUpdateCombo->addItem(t(QStringLiteral("4 saatte bir"), QStringLiteral("Every 4 hours")), 4);
    m_autoUpdateCombo->addItem(t(QStringLiteral("12 saatte bir"), QStringLiteral("Every 12 hours")), 12);
    m_autoUpdateCombo->addItem(t(QStringLiteral("24 saatte bir (Günlük)"), QStringLiteral("Every 24 hours (Daily)")), 24);
    commonForm->addRow(t(QStringLiteral("Oto-Güncelleme:"), QStringLiteral("Auto-Update:")), m_autoUpdateCombo);

    lay->addLayout(commonForm);

    // Buttons
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(btns);

    // Populate if editing
    if (existing) {
        m_nameEdit->setText(existing->name);
        m_epgEdit->setText(existing->epgUrl);
        // Find correct index by user data
        int comboIdx = m_autoUpdateCombo->findData(existing->autoUpdateIntervalHours);
        if (comboIdx >= 0) m_autoUpdateCombo->setCurrentIndex(comboIdx);
        if (existing->sourceType == SourceType::M3U) {
            m_m3uRadio->setChecked(true);
            m_stack->setCurrentIndex(1);
            m_m3uEdit->setText(existing->m3uUrl);
        } else {
            m_serverEdit->setText(existing->serverUrl);
            m_userEdit->setText(existing->username);
            m_passEdit->setText(existing->password);
        }
    }

    connect(m_xtreamRadio, &QRadioButton::toggled, this, &SourceDialog::onTypeToggled);
    connect(browseBtn,     &QPushButton::clicked,  this, &SourceDialog::onBrowseFile);
    connect(testBtn,       &QPushButton::clicked,  this, &SourceDialog::onTestConnection);
    connect(btns,          &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns,          &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void SourceDialog::onTypeToggled()
{
    m_stack->setCurrentIndex(m_xtreamRadio->isChecked() ? 0 : 1);
}

void SourceDialog::onBrowseFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
                t(QStringLiteral("M3U Dosyası Aç"), QStringLiteral("Open M3U File")),
        QString{},
                t(QStringLiteral("Playlist Dosyaları (*.m3u *.m3u8);;Tüm Dosyalar (*)"),
                    QStringLiteral("Playlist Files (*.m3u *.m3u8);;All Files (*)"))
    );
    if (!path.isEmpty())
        m_m3uEdit->setText(QUrl::fromLocalFile(path).toString());
}

void SourceDialog::onTestConnection()
{
    Source src = source();
    if (src.sourceType != SourceType::Xtream) return;
    if (src.serverUrl.isEmpty() || src.username.isEmpty()) {
        QMessageBox::warning(this, t(QStringLiteral("Eksik Bilgi"), QStringLiteral("Missing Information")),
                             t(QStringLiteral("Sunucu URL, kullanıcı adı ve şifre gerekli."),
                               QStringLiteral("Server URL, username and password are required.")));
        return;
    }
    auto *client = new XtreamClient(src, this);
    client->testConnection([this, client](bool ok, QString err) {
        client->deleteLater();
        if (ok)
            QMessageBox::information(this, t(QStringLiteral("Başarılı"), QStringLiteral("Success")),
                                     t(QStringLiteral("Bağlantı başarılı!"), QStringLiteral("Connection successful!")));
        else
            QMessageBox::critical(this, t(QStringLiteral("Hata"), QStringLiteral("Error")),
                                  t(QStringLiteral("Bağlantı hatası: "), QStringLiteral("Connection error: ")) + err);
    });
}

Source SourceDialog::source() const
{
    Source s;
    s.id         = m_sourceId;
    s.name       = m_nameEdit->text().trimmed();
    s.epgUrl     = m_epgEdit->text().trimmed();
    s.autoUpdateIntervalHours = m_autoUpdateCombo->currentData().toInt();

    if (m_xtreamRadio->isChecked()) {
        s.sourceType = SourceType::Xtream;
        s.serverUrl  = m_serverEdit->text().trimmed();
        s.username   = m_userEdit->text().trimmed();
        s.password   = m_passEdit->text();
    } else {
        s.sourceType = SourceType::M3U;
        s.m3uUrl     = m_m3uEdit->text().trimmed();
    }
    return s;
}
