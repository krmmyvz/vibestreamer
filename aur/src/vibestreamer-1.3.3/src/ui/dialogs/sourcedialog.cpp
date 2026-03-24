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

QString SourceDialog::t(const QString &key) const
{
    return I18n::instance().get(key);
}

void SourceDialog::buildUi(const Source *existing)
{
    setWindowTitle(existing ? t(QStringLiteral("src_edit_title"))
                            : t(QStringLiteral("src_add_title")));
    setMinimumWidth(420);

    auto *lay = new QVBoxLayout(this);

    // Name
    auto *nameForm = new QFormLayout;
    m_nameEdit = new QLineEdit;
    nameForm->addRow(t(QStringLiteral("src_name")), m_nameEdit);
    lay->addLayout(nameForm);

    // Type selection
    auto *typeBox = new QGroupBox(t(QStringLiteral("src_type")));
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
    xf->addRow(t(QStringLiteral("src_server_url")), m_serverEdit);
    xf->addRow(t(QStringLiteral("src_username")),  m_userEdit);
    xf->addRow(t(QStringLiteral("src_password")),      m_passEdit);
    auto *testBtn = new QPushButton(t(QStringLiteral("src_test_connection")));
    xf->addRow(QString{}, testBtn);
    m_stack->addWidget(xtreamPage);

    // Page 1: M3U (URL or local file)
    auto *m3uPage = new QWidget;
    auto *mf      = new QFormLayout(m3uPage);
    auto *m3uRow  = new QHBoxLayout;
    m_m3uEdit = new QLineEdit;
    m_m3uEdit->setPlaceholderText(t(QStringLiteral("src_m3u_placeholder")));
    auto *browseBtn = new QPushButton(t(QStringLiteral("src_browse")));
    browseBtn->setFixedWidth(72);
    m3uRow->addWidget(m_m3uEdit);
    m3uRow->addWidget(browseBtn);
    mf->addRow(t(QStringLiteral("src_m3u_label")), m3uRow);
    m_stack->addWidget(m3uPage);

    lay->addWidget(m_stack);

    // Common: EPG & Auto-update
    auto *commonForm = new QFormLayout;
    m_epgEdit = new QLineEdit; m_epgEdit->setPlaceholderText(t(QStringLiteral("src_epg_placeholder")));
    commonForm->addRow(QStringLiteral("EPG URL:"), m_epgEdit);

    m_autoUpdateCombo = new QComboBox;
    m_autoUpdateCombo->addItem(t(QStringLiteral("src_update_off")), 0);
    m_autoUpdateCombo->addItem(t(QStringLiteral("src_update_1h")), 1);
    m_autoUpdateCombo->addItem(t(QStringLiteral("src_update_4h")), 4);
    m_autoUpdateCombo->addItem(t(QStringLiteral("src_update_12h")), 12);
    m_autoUpdateCombo->addItem(t(QStringLiteral("src_update_24h")), 24);
    commonForm->addRow(t(QStringLiteral("src_auto_update")), m_autoUpdateCombo);

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
                t(QStringLiteral("src_open_m3u")),
        QString{},
                t(QStringLiteral("src_m3u_filter"))
    );
    if (!path.isEmpty())
        m_m3uEdit->setText(QUrl::fromLocalFile(path).toString());
}

void SourceDialog::onTestConnection()
{
    Source src = source();
    if (src.sourceType != SourceType::Xtream) return;
    if (src.serverUrl.isEmpty() || src.username.isEmpty()) {
        QMessageBox::warning(this, t(QStringLiteral("src_missing_info")),
                             t(QStringLiteral("src_fields_required")));
        return;
    }
    auto *client = new XtreamClient(src, this);
    client->testConnection([this, client](bool ok, QString err) {
        client->deleteLater();
        if (ok)
            QMessageBox::information(this, t(QStringLiteral("src_success")),
                                     t(QStringLiteral("src_connected")));
        else
            QMessageBox::critical(this, t(QStringLiteral("src_error")),
                                  t(QStringLiteral("src_connection_error")) + err);
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
