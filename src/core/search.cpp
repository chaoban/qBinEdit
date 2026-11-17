// qBinEdit - byte pattern search (no copies of the searched buffer)
#include "search.h"

#include <QByteArrayMatcher>
#include <QVarLengthArray>
#include <algorithm>
#include <array>

namespace Search {

namespace {

const std::array<uchar, 256> &foldTable()
{
    static const std::array<uchar, 256> t = [] {
        std::array<uchar, 256> a{};
        for (int i = 0; i < 256; ++i)
            a[i] = uchar((i >= 'A' && i <= 'Z') ? i + ('a' - 'A') : i);
        return a;
    }();
    return t;
}

bool hasAsciiLetter(const QByteArray &s)
{
    for (char c : s) {
        const uchar u = uchar(c);
        if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z'))
            return true;
    }
    return false;
}

// Case-insensitive Boyer-Moore-Horspool, forward.
class FoldedSearcher
{
public:
    explicit FoldedSearcher(const QByteArray &needle)
        : m_len(needle.size()), m_pat(needle.size())
    {
        const auto &f = foldTable();
        for (qint64 j = 0; j < m_len; ++j)
            m_pat[j] = f[uchar(needle.at(j))];
        m_skip.fill(m_len);
        for (qint64 j = 0; j + 1 < m_len; ++j)
            m_skip[m_pat[j]] = m_len - 1 - j;
        m_back.fill(m_len);
        for (qint64 j = m_len - 1; j >= 1; --j)
            m_back[m_pat[j]] = j;
    }

    qint64 indexIn(const uchar *h, qint64 n, qint64 from) const
    {
        const auto &f = foldTable();
        const uchar last = m_pat[m_len - 1];
        qint64 i = qMax<qint64>(0, from);
        while (i <= n - m_len) {
            const uchar c = f[h[i + m_len - 1]];
            if (c == last) {
                qint64 j = m_len - 2;
                while (j >= 0 && f[h[i + j]] == m_pat[j])
                    --j;
                if (j < 0)
                    return i;
            }
            i += m_skip[c];
        }
        return -1;
    }

    // Last match starting at or before 'from'.
    qint64 lastIndexIn(const uchar *h, qint64 n, qint64 from) const
    {
        const auto &f = foldTable();
        qint64 i = qMin(from, n - m_len);
        while (i >= 0) {
            const uchar c = f[h[i]];
            if (c == m_pat[0]) {
                qint64 j = 1;
                while (j < m_len && f[h[i + j]] == m_pat[j])
                    ++j;
                if (j == m_len)
                    return i;
            }
            i -= m_back[c];
        }
        return -1;
    }

private:
    qint64 m_len;
    QVarLengthArray<uchar, 256> m_pat;
    std::array<qint64, 256> m_skip{};
    std::array<qint64, 256> m_back{};
};

inline const uchar *bytes(const QByteArray &a)
{
    return reinterpret_cast<const uchar *>(a.constData());
}

} // namespace

qint64 indexOf(const QByteArray &hay, const QByteArray &needle, qint64 from, bool caseSensitive)
{
    if (needle.isEmpty() || from >= hay.size())
        return -1;
    from = qMax<qint64>(0, from);
    if (caseSensitive || !hasAsciiLetter(needle))
        return hay.indexOf(needle, from);
    return FoldedSearcher(needle).indexIn(bytes(hay), hay.size(), from);
}

qint64 lastIndexOf(const QByteArray &hay, const QByteArray &needle, qint64 from, bool caseSensitive)
{
    if (needle.isEmpty() || needle.size() > hay.size())
        return -1;
    if (from < 0 || from > hay.size() - needle.size())
        from = hay.size() - needle.size();
    if (caseSensitive || !hasAsciiLetter(needle))
        return hay.lastIndexOf(needle, from);
    return FoldedSearcher(needle).lastIndexIn(bytes(hay), hay.size(), from);
}

QVector<qint64> findAll(const QByteArray &hay, const QByteArray &needle, bool caseSensitive)
{
    QVector<qint64> hits;
    if (needle.isEmpty())
        return hits;
    const qint64 m = needle.size();
    if (caseSensitive || !hasAsciiLetter(needle)) {
        const QByteArrayMatcher matcher(needle);
        for (qint64 p = matcher.indexIn(hay, 0); p >= 0; p = matcher.indexIn(hay, p + m))
            hits.append(p);
    } else {
        const FoldedSearcher s(needle);
        for (qint64 p = s.indexIn(bytes(hay), hay.size(), 0); p >= 0; p = s.indexIn(bytes(hay), hay.size(), p + m))
            hits.append(p);
    }
    return hits;
}

qint64 count(const QByteArray &hay, const QByteArray &needle, bool caseSensitive)
{
    if (needle.isEmpty())
        return 0;
    const qint64 m = needle.size();
    qint64 n = 0;
    if (caseSensitive || !hasAsciiLetter(needle)) {
        const QByteArrayMatcher matcher(needle);
        for (qint64 p = matcher.indexIn(hay, 0); p >= 0; p = matcher.indexIn(hay, p + m))
            ++n;
    } else {
        const FoldedSearcher s(needle);
        for (qint64 p = s.indexIn(bytes(hay), hay.size(), 0); p >= 0; p = s.indexIn(bytes(hay), hay.size(), p + m))
            ++n;
    }
    return n;
}

} // namespace Search
