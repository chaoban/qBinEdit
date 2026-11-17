// qBinEdit - byte pattern search (no copies of the searched buffer)
#pragma once

#include <QByteArray>
#include <QVector>

namespace Search {

// Case folding, when caseSensitive is false, applies to ASCII letters only, so a
// byte pattern never matches a different non-letter byte.

// First match at or after 'from'. -1 if none.
qint64 indexOf(const QByteArray &hay, const QByteArray &needle, qint64 from, bool caseSensitive);

// Last match that starts at or before 'from' (from < 0: search the whole buffer). -1 if none.
qint64 lastIndexOf(const QByteArray &hay, const QByteArray &needle, qint64 from, bool caseSensitive);

// Start offsets of all non-overlapping matches, in ascending order.
QVector<qint64> findAll(const QByteArray &hay, const QByteArray &needle, bool caseSensitive);

// Number of non-overlapping matches.
qint64 count(const QByteArray &hay, const QByteArray &needle, bool caseSensitive);

} // namespace Search
