#pragma once
#include <QSortFilterProxyModel>

// FuzzyProxyModel — subsequence matching for channel search.
// "bbc" matches "BBC World News", "bbc_one", "BBCNEWS" etc.
// All characters of the query must appear in order in the item text.
class FuzzyProxyModel : public QSortFilterProxyModel {
public:
    explicit FuzzyProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent) {}

    void setFuzzyPattern(const QString &pattern)
    {
        if (m_pattern == pattern) return;
        m_pattern = pattern.toLower();
        invalidateFilter();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (m_pattern.isEmpty()) return true;

        const QString text = sourceModel()
            ->data(sourceModel()->index(sourceRow, 0, sourceParent))
            .toString().toLower();

        // Subsequence check: every char of pattern must appear in order in text
        int pi = 0;
        for (int ti = 0; ti < text.size() && pi < m_pattern.size(); ++ti) {
            if (text[ti] == m_pattern[pi]) ++pi;
        }
        return pi == m_pattern.size();
    }

private:
    QString m_pattern;
};
