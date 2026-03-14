#include "sourcedialog.h"
#include "xtreamclient.h"

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

SourceDialog::SourceDialog(QWidget *parent)
    : QDialog(parent)
{
    buildUi(nullptr);
}

SourceDialog::SourceDialog(const Source &source, QWidget *parent)
    : QDialog(parent)
    , m_sourceId(source.id)
{
    buildUi(&source);
}

void SourceDialog::buildUi(const Source *existing)
{
    setWindowTitle(existing ? QStringLiteral("Kaynağı Düzenle") : QStringLiteral("Kaynak Ekle"));
    setMinimumWidth(420);

    auto *lay = new QVBoxLayout(this);

    // Name
    auto *nameForm = new QFormLayout;
    m_nameEdit = new QLineEdit;
    nameForm->addRow(QStringLiteral("İsim:"), m_nameEdit);
    lay->addLayout(nameForm);

    // Type selection
    auto *typeBox = new QGroupBox(QStringLiteral("Kaynak Tipi"));
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
    xf->addRow(QStringLiteral("Sunucu URL:"), m_serverEdit);
    xf->addRow(QStringLiteral("Kullanıcı:"),  m_userEdit);
    xf->addRow(QStringLiteral("Şifre:"),      m_passEdit);
    auto *testBtn = new QPushButton(QStringLiteral("Bağlantıyı Test Et"));
    xf->addRow(QString{}, testBtn);
    m_stack->addWidget(xtreamPage);

    // Page 1: M3U (URL or local file)
    auto *m3uPage = new QWidget;
    auto *mf      = new QFormLayout(m3uPage);
    auto *m3uRow  = new QHBoxLayout;
    m_m3uEdit = new QLineEdit;
    m_m3uEdit->setPlaceholderText(QStringLiteral("http://…/playlist.m3u  veya  dosya seçin →"));
    auto *browseBtn = new QPushButton(QStringLiteral("Gözat…"));
    browseBtn->setFixedWidth(72);
    m3uRow->addWidget(m_m3uEdit);
    m3uRow->addWidget(browseBtn);
    mf->addRow(QStringLiteral("M3U URL / Dosya:"), m3uRow);
    m_stack->addWidget(m3uPage);

    lay->addWidget(m_stack);

    // Common: EPG & Auto-update
    auto *commonForm = new QFormLayout;
    m_epgEdit = new QLineEdit; m_epgEdit->setPlaceholderText(QStringLiteral("(isteğe bağlı)"));
    commonForm->addRow(QStringLiteral("EPG URL:"), m_epgEdit);

    m_autoUpdateCombo = new QComboBox;
    m_autoUpdateCombo->addItem(QStringLiteral("Kapalı (Manuel)"), 0);
    m_autoUpdateCombo->addItem(QStringLiteral("Her saat başı"), 1);
    m_autoUpdateCombo->addItem(QStringLiteral("4 saatte bir"), 4);
    m_autoUpdateCombo->addItem(QStringLiteral("12 saatte bir"), 12);
    m_autoUpdateCombo->addItem(QStringLiteral("24 saatte bir (Günlük)"), 24);
    commonForm->addRow(QStringLiteral("Oto-Güncelleme:"), m_autoUpdateCombo);

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
        QStringLiteral("M3U Dosyası Aç"),
        QString{},
        QStringLiteral("Playlist Dosyaları (*.m3u *.m3u8);;Tüm Dosyalar (*)")
    );
    if (!path.isEmpty())
        m_m3uEdit->setText(QUrl::fromLocalFile(path).toString());
}

void SourceDialog::onTestConnection()
{
    Source src = source();
    if (src.sourceType != SourceType::Xtream) return;
    if (src.serverUrl.isEmpty() || src.username.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Eksik Bilgi"),
                             QStringLiteral("Sunucu URL, kullanıcı adı ve şifre gerekli."));
        return;
    }
    auto *client = new XtreamClient(src, this);
    client->testConnection([this, client](bool ok, QString err) {
        client->deleteLater();
        if (ok)
            QMessageBox::information(this, QStringLiteral("Başarılı"),
                                     QStringLiteral("Bağlantı başarılı!"));
        else
            QMessageBox::critical(this, QStringLiteral("Hata"),
                                  QStringLiteral("Bağlantı hatası: ") + err);
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
