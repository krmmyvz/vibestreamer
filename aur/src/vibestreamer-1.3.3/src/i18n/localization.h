#pragma once

#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

class I18n {
public:
    static I18n &instance()
    {
        static I18n inst;
        return inst;
    }

    void setLanguage(const QString &lang)
    {
        if (lang == m_lang && !m_strings.isEmpty())
            return;
        m_lang = lang;
        load(lang);
    }

    QString language() const { return m_lang; }

    // Primary lookup — returns key itself if not found
    QString get(const QString &key) const
    {
        return m_strings.value(key, key);
    }

    // Convenience with arg substitution: get("track_n").arg(1)
    // is handled by caller since get() returns QString

private:
    I18n() = default;

    void load(const QString &lang)
    {
        m_strings.clear();
        const QString path = QStringLiteral(":/translations/%1.json").arg(lang);
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            // Fallback to English
            QFile fb(QStringLiteral(":/translations/en.json"));
            if (!fb.open(QIODevice::ReadOnly))
                return;
            parseJson(fb.readAll());
            return;
        }
        parseJson(f.readAll());
    }

    void parseJson(const QByteArray &data)
    {
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
            m_strings.insert(it.key(), it.value().toString());
    }

    QString m_lang;
    QHash<QString, QString> m_strings;
};

// Convenience macro — short, readable, zero-overhead
#define tr_key(k) I18n::instance().get(QStringLiteral(k))
