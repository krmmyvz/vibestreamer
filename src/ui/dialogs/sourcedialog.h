#pragma once
#include "models.h"

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QRadioButton>
#include <QStackedWidget>

class SourceDialog : public QDialog {
    Q_OBJECT
public:
    // New source
    explicit SourceDialog(const QString &language = QStringLiteral("en"), QWidget *parent = nullptr);
    // Edit existing
    explicit SourceDialog(const Source &source, const QString &language = QStringLiteral("en"), QWidget *parent = nullptr);

    Source source() const;

private slots:
    void onTypeToggled();
    void onTestConnection();
    void onBrowseFile();

private:
    void buildUi(const Source *existing);
    QString t(const QString &key) const;

    QLineEdit     *m_nameEdit;
    QRadioButton  *m_xtreamRadio;
    QRadioButton  *m_m3uRadio;
    QStackedWidget *m_stack;

    // Xtream fields
    QLineEdit *m_serverEdit;
    QLineEdit *m_userEdit;
    QLineEdit *m_passEdit;

    // M3U fields
    QLineEdit *m_m3uEdit;

    // Common EPG & settings
    QLineEdit *m_epgEdit;
    QComboBox *m_autoUpdateCombo;

    QString    m_sourceId;
    QString    m_language;
};
