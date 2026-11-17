// qBinEdit - in-memory document with undo/redo
#include "document.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUndoCommand>
#include <QUndoStack>
#include <algorithm>
#include <cstring>
#include <utility>

namespace {

constexpr qint64 kMaxFileSize = qint64(1024) * 1024 * 1024;     // 1 GiB, everything is held in RAM
constexpr qint64 kMaxImageSpan = qint64(256) * 1024 * 1024;     // HEX/S-Record flattened image

class ReplaceCommand : public QUndoCommand
{
public:
    ReplaceCommand(Document *doc, qint64 pos, const QByteArray &oldBytes, const QByteArray &newBytes,
                   const QString &text, bool mergeable)
        : QUndoCommand(text), m_doc(doc), m_pos(pos), m_old(oldBytes), m_new(newBytes), m_mergeable(mergeable)
    {
    }

    void redo() override
    {
        m_before = m_doc->changeState();
        m_doc->rawReplace(m_pos, m_old.size(), m_new);
        m_doc->notifyCursorHint(m_pos + m_new.size());
    }
    void undo() override
    {
        m_doc->rawReplace(m_pos, m_new.size(), m_old);
        m_doc->restoreChangeState(m_before);
        m_doc->notifyCursorHint(m_pos);
    }

    int id() const override { return m_mergeable ? 1 : -1; }

    bool mergeWith(const QUndoCommand *other) override
    {
        const auto *o = static_cast<const ReplaceCommand *>(other);
        if (!o->m_mergeable || o->m_doc != m_doc)
            return false;
        const bool oOverwrite = o->m_old.size() == o->m_new.size();
        const bool thisOverwrite = m_old.size() == m_new.size();
        // other overwrites bytes that this command produced -> patch our "new" bytes
        if (oOverwrite && o->m_pos >= m_pos && o->m_pos + o->m_new.size() <= m_pos + m_new.size()) {
            m_new.replace(o->m_pos - m_pos, o->m_new.size(), o->m_new);
            return true;
        }
        // directly following edit of the same kind
        if (o->m_pos == m_pos + m_new.size()) {
            if (oOverwrite && thisOverwrite) {
                m_old += o->m_old;
                m_new += o->m_new;
                return true;
            }
            if (o->m_old.isEmpty() && m_old.isEmpty()) {     // consecutive inserts (append typing)
                m_new += o->m_new;
                return true;
            }
        }
        return false;
    }

private:
    Document *m_doc;
    qint64 m_pos;
    QByteArray m_old;
    QByteArray m_new;
    bool m_mergeable;
    Document::ChangeState m_before;     // change highlighting before redo(), restored by undo()
};

// Replace All as one undo step: same-length replacements patch in place, others run in a
// single pass, instead of one buffer move + repaint per occurrence.
class MultiReplaceCommand : public QUndoCommand
{
public:
    MultiReplaceCommand(Document *doc, const QVector<qint64> &positions, qint64 len, const QByteArray &oldBytes,
                        const QByteArray &oldMask, const QByteArray &newBytes, const QString &text)
        : QUndoCommand(text), m_doc(doc), m_pos(positions), m_len(len), m_old(oldBytes), m_oldMask(oldMask),
          m_new(newBytes)
    {
    }

    void redo() override
    {
        m_before = m_doc->changeState();
        m_doc->rawReplaceMany(m_pos, m_len, m_new, false, QByteArray());
        m_doc->notifyCursorHint(m_pos.first());
    }
    void undo() override
    {
        const qint64 delta = m_new.size() - m_len;
        QVector<qint64> cur(m_pos.size());
        for (int i = 0; i < m_pos.size(); ++i)
            cur[i] = m_pos[i] + qint64(i) * delta;
        m_doc->rawReplaceMany(cur, m_new.size(), m_old, true, m_oldMask);
        m_doc->restoreChangeState(m_before);
        m_doc->notifyCursorHint(m_pos.first());
    }

private:
    Document *m_doc;
    QVector<qint64> m_pos;      // positions before the replacement, ascending
    qint64 m_len;
    QByteArray m_old;           // replaced bytes, m_len per position
    QByteArray m_oldMask;       // their gap-mask values (empty if the document has no gaps)
    QByteArray m_new;
    Document::ChangeState m_before;
};

} // namespace

Document::Document(QObject *parent)
    : QObject(parent), m_undo(new QUndoStack(this))
{
    connect(m_undo, &QUndoStack::cleanChanged, this, [this](bool clean) { emit modificationChanged(!clean); });
}

bool Document::load(const QString &path, int forceFormat, QString *error, QStringList *warnings)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        *error = tr("Cannot open file:\n%1\n\n%2").arg(QDir::toNativeSeparators(path), f.errorString());
        return false;
    }
    if (f.size() > kMaxFileSize) {
        *error = tr("The file is %1 MiB. qBinEdit keeps files in memory and supports up to %2 MiB.")
                     .arg(f.size() / (1024 * 1024)).arg(kMaxFileSize / (1024 * 1024));
        return false;
    }
    const QByteArray raw = f.readAll();
    if (f.error() != QFileDevice::NoError) {
        *error = tr("Read error: %1").arg(f.errorString());
        return false;
    }

    const FileFormat fmt = forceFormat >= 0 ? FileFormat(forceFormat) : Formats::detect(path, raw.left(1024));
    Formats::ParseResult pr;
    QByteArray image;
    QByteArray gaps;
    quint64 base = 0;

    if (fmt == FileFormat::Binary) {
        image = raw;
    } else {
        QString perr;
        const bool ok = (fmt == FileFormat::IntelHex) ? Formats::parseIntelHex(raw, &pr, &perr)
                                                      : Formats::parseSRecord(raw, &pr, &perr);
        if (!ok) {
            *error = tr("%1 parse error: %2").arg(Formats::formatName(fmt), perr);
            return false;
        }
        if (!Formats::flatten(pr, m_fill, kMaxImageSpan, &image, &base, &gaps, &perr)) {
            *error = perr;
            return false;
        }
        if (warnings)
            *warnings = pr.warnings;
    }

    m_data = image;
    m_original = image;
    m_gapMask = gaps;
    m_base = base;
    m_path = QFileInfo(path).absoluteFilePath();
    m_name.clear();
    m_format = fmt;
    m_info = pr;
    resetChanges();
    m_undo->clear();
    m_undo->setClean();
    emit contentsChanged(0, 0, m_data.size());
    emit filePathChanged();
    emit baseAddressChanged();
    return true;
}

bool Document::save(const QString &path, FileFormat format, QString *error)
{
    QByteArray out;
    if (format == FileFormat::Binary) {
        out = m_data;
    } else {
        Formats::WriteOptions opt;
        opt.crlf = m_info.crlf;
        opt.hasStart = m_info.hasStart;
        opt.startAddress = m_info.startAddress;
        opt.startRecordType = m_info.startRecordType;
        opt.srecHeader = m_info.srecHeader;
        opt.srecAddressBytes = m_info.srecAddressBytes;
        const QByteArray mask = (m_gapMask.size() == m_data.size()) ? m_gapMask : QByteArray();
        const bool ok = (format == FileFormat::IntelHex)
                            ? Formats::writeIntelHex(m_data, m_base, mask, m_fill, opt, &out, error)
                            : Formats::writeSRecord(m_data, m_base, mask, m_fill, opt, &out, error);
        if (!ok)
            return false;
    }

    QSaveFile sf(path);
    if (!sf.open(QIODevice::WriteOnly)) {
        *error = tr("Cannot write file:\n%1\n\n%2").arg(QDir::toNativeSeparators(path), sf.errorString());
        return false;
    }
    if (sf.write(out) != out.size() || !sf.commit()) {
        *error = tr("Write error: %1").arg(sf.errorString());
        return false;
    }
    m_path = QFileInfo(path).absoluteFilePath();
    m_name.clear();
    m_format = format;
    m_original = m_data;
    resetChanges();
    m_undo->setClean();
    emit filePathChanged();
    emit contentsChanged(0, m_data.size(), m_data.size());   // refresh change highlighting
    return true;
}

void Document::setContent(const QByteArray &data, quint64 baseAddress, const QString &displayName)
{
    m_data = data;
    m_original = data;
    m_gapMask.clear();
    m_base = baseAddress;
    m_name = displayName;
    m_path.clear();
    m_format = FileFormat::Binary;
    m_info = Formats::ParseResult();
    resetChanges();
    m_undo->clear();
    emit contentsChanged(0, 0, m_data.size());
    emit filePathChanged();
    emit baseAddressChanged();
}

void Document::setBaseAddress(quint64 base)
{
    if (base == m_base)
        return;
    m_base = base;
    emit baseAddressChanged();
}

QString Document::displayName() const
{
    if (!m_name.isEmpty())
        return m_name;
    if (!m_path.isEmpty())
        return QFileInfo(m_path).fileName();
    return tr("Untitled");
}

void Document::setReadOnly(bool ro)
{
    if (ro == m_readOnly)
        return;
    m_readOnly = ro;
    emit readOnlyChanged(ro);
}

bool Document::isModified() const
{
    return !m_undo->isClean();
}

bool Document::isChangedByte(qint64 pos) const
{
    if (pos < 0 || pos >= m_data.size())
        return false;
    if (!m_changes.shifted && m_original.size() == m_data.size())
        return m_original.at(pos) != m_data.at(pos);     // exact: offsets still match the file
    const auto &r = m_changes.ranges;
    auto it = std::upper_bound(r.cbegin(), r.cend(), pos,
                               [](qint64 p, const QPair<qint64, qint64> &x) { return p < x.first; });
    return it != r.cbegin() && pos < (it - 1)->second;
}

void Document::resetChanges()
{
    m_changes.ranges.clear();
    m_changes.shifted = false;
    ++m_changes.epoch;
}

void Document::restoreChangeState(const ChangeState &s)
{
    // A snapshot from before the last load/save refers to another baseline: keep the
    // tracked state, which already marks the undone bytes as different from the saved file.
    if (s.epoch == m_changes.epoch)
        m_changes = s;
}

void Document::trackEdits(const QVector<qint64> &positions, qint64 removed, qint64 added)
{
    const int n = int(positions.size());
    if (n == 0)
        return;
    const qint64 d = added - removed;           // shift contributed by each edit
    if (d != 0)
        m_changes.shifted = true;

    // 1) previously edited ranges, cut where they were overwritten/removed and shifted
    QVector<QPair<qint64, qint64>> kept;
    kept.reserve(m_changes.ranges.size());
    int first = 0;      // first edit that ends after the current range start
    for (const auto &r : std::as_const(m_changes.ranges)) {
        while (first < n && positions[first] + removed <= r.first)
            ++first;
        qint64 cur = r.first;
        int k = first;
        while (cur < r.second) {
            if (k < n && positions[k] < r.second) {
                if (cur < positions[k])
                    kept.append({ cur + qint64(k) * d, positions[k] + qint64(k) * d });
                cur = qMax(cur, positions[k] + removed);
                ++k;
            } else {
                kept.append({ cur + qint64(k) * d, r.second + qint64(k) * d });
                cur = r.second;
            }
        }
    }

    // 2) merge with the newly written bytes; both streams are sorted
    QVector<QPair<qint64, qint64>> merged;
    merged.reserve(kept.size() + (added > 0 ? qMin(n, 1024) : 0));
    auto push = [&merged](qint64 a, qint64 b) {
        if (b <= a)
            return;
        if (!merged.isEmpty() && a <= merged.last().second)
            merged.last().second = qMax(merged.last().second, b);
        else
            merged.append({ a, b });
    };
    int i = 0, k = 0;
    while (i < kept.size() || (added > 0 && k < n)) {
        const qint64 editStart = (added > 0 && k < n) ? positions[k] + qint64(k) * d : -1;
        if (i < kept.size() && (editStart < 0 || kept[i].first <= editStart)) {
            push(kept[i].first, kept[i].second);
            ++i;
        } else {
            push(editStart, editStart + added);
            ++k;
        }
    }
    m_changes.ranges = merged;
}

bool Document::isGap(qint64 pos) const
{
    return pos >= 0 && pos < m_gapMask.size() && pos < m_data.size()
           && m_gapMask.at(pos) != 0 && quint8(m_data.at(pos)) == m_fill;
}

void Document::replace(qint64 pos, qint64 len, const QByteArray &bytes, const QString &text, bool mergeable)
{
    if (m_readOnly || pos < 0 || pos > m_data.size())
        return;
    len = qMin(len, m_data.size() - pos);
    const QByteArray old = m_data.mid(pos, len);
    if (old == bytes)
        return;
    m_undo->push(new ReplaceCommand(this, pos, old, bytes, text, mergeable));
}

int Document::replaceAll(const QVector<qint64> &positions, qint64 len, const QByteArray &bytes, const QString &text)
{
    if (m_readOnly || len <= 0)
        return 0;
    const bool hasMask = !m_gapMask.isEmpty();
    QVector<qint64> pos;
    QByteArray old, oldMask;
    qint64 next = 0;
    for (qint64 p : positions) {
        if (p < next || p + len > m_data.size())
            continue;                   // overlapping or out of range: skip
        next = p + len;
        if (len == bytes.size() && std::memcmp(m_data.constData() + p, bytes.constData(), size_t(len)) == 0)
            continue;                   // already equal
        pos.append(p);
        old.append(m_data.constData() + p, len);
        if (hasMask)
            oldMask.append(m_gapMask.constData() + p, len);
    }
    if (pos.isEmpty())
        return 0;
    m_undo->push(new MultiReplaceCommand(this, pos, len, old, oldMask, bytes, text));
    return int(pos.size());
}

void Document::rawReplace(qint64 pos, qint64 len, const QByteArray &bytes)
{
    m_data.replace(pos, len, bytes);
    // Overwrites keep the gap mask (a gap byte only counts as gap while it equals the fill byte).
    if (!m_gapMask.isEmpty() && len != bytes.size())
        m_gapMask.replace(pos, len, QByteArray(bytes.size(), char(0)));
    trackEdits({ pos }, len, bytes.size());
    emit contentsChanged(pos, len, bytes.size());
}

void Document::rawReplaceMany(const QVector<qint64> &positions, qint64 removed, const QByteArray &bytes,
                              bool perPosition, const QByteArray &maskBytes)
{
    const int n = int(positions.size());
    if (n == 0)
        return;
    const qint64 added = perPosition ? bytes.size() / n : bytes.size();
    auto chunk = [&](int i) { return bytes.constData() + (perPosition ? qint64(i) * added : 0); };
    auto maskChunk = [&](int i) { return maskBytes.constData() + (perPosition ? qint64(i) * added : 0); };
    const bool hasMask = !m_gapMask.isEmpty();
    const bool restoreMask = hasMask && maskBytes.size() == bytes.size();
    const qint64 oldSize = m_data.size();
    const qint64 newSize = oldSize + qint64(n) * (added - removed);

    // Apply the same edit list to the data and (if present) the gap mask, in place.
    auto apply = [&](QByteArray &buf, bool isMask) {
        auto put = [&](char *dst, int i) {
            if (!isMask)
                std::memcpy(dst, chunk(i), size_t(added));
            else if (restoreMask)
                std::memcpy(dst, maskChunk(i), size_t(added));
            else
                std::memset(dst, 0, size_t(added));
        };
        if (added == removed) {
            if (isMask && !restoreMask)
                return;                 // overwrites keep the gap mask
            char *d = buf.data();
            for (int i = 0; i < n; ++i)
                put(d + positions[i], i);
        } else if (added < removed) {   // shrink: compact towards the front
            char *d = buf.data();
            qint64 dst = positions[0];
            for (int i = 0; i < n; ++i) {
                put(d + dst, i);
                dst += added;
                const qint64 src = positions[i] + removed;
                const qint64 end = (i + 1 < n) ? positions[i + 1] : oldSize;
                std::memmove(d + dst, d + src, size_t(end - src));
                dst += end - src;
            }
            buf.resize(newSize);
        } else {                        // grow: resize, then move from the back
            buf.resize(newSize);
            char *d = buf.data();
            qint64 src = oldSize, dst = newSize;
            for (int i = n - 1; i >= 0; --i) {
                const qint64 segStart = positions[i] + removed;
                dst -= src - segStart;
                std::memmove(d + dst, d + segStart, size_t(src - segStart));
                dst -= added;
                put(d + dst, i);
                src = positions[i];
            }
        }
    };
    apply(m_data, false);
    if (hasMask)
        apply(m_gapMask, true);

    trackEdits(positions, removed, added);
    const qint64 from = positions[0];
    emit contentsChanged(from, oldSize - from, newSize - from);
}
