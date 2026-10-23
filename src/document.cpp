// qBinEdit - in-memory document with undo/redo
#include "document.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QUndoCommand>
#include <QUndoStack>

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
        m_doc->rawReplace(m_pos, m_old.size(), m_new);
        m_doc->notifyCursorHint(m_pos + m_new.size());
    }
    void undo() override
    {
        m_doc->rawReplace(m_pos, m_new.size(), m_old);
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
    return m_original.size() == m_data.size() && pos >= 0 && pos < m_data.size()
           && m_original.at(pos) != m_data.at(pos);
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

void Document::rawReplace(qint64 pos, qint64 len, const QByteArray &bytes)
{
    m_data.replace(pos, len, bytes);
    // Overwrites keep the gap mask (a gap byte only counts as gap while it equals the fill byte).
    if (!m_gapMask.isEmpty() && len != bytes.size())
        m_gapMask.replace(pos, len, QByteArray(bytes.size(), char(0)));
    emit contentsChanged(pos, len, bytes.size());
}
