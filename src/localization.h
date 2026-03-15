#pragma once

#include <QString>

namespace Localization {

inline bool isEnglish(const QString &lang)
{
    return lang.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0;
}

inline QString text(const QString &lang, const QString &tr, const QString &en)
{
    return isEnglish(lang) ? en : tr;
}

} // namespace Localization
