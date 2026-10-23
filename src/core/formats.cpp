// qBinEdit - firmware file formats: Intel HEX and Motorola S-Record
#include "formats.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <algorithm>

namespace Formats {

namespace {

inline QString tr(const char *s) { return QCoreApplication::translate("Formats", s); }

int hexNibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Decode an even-length hex string into bytes. Returns false on bad characters.
bool decodeHex(const QByteArray &s, int from, QByteArray *out)
{
    const int n = s.size() - from;
    if (n < 0 || (n & 1))
        return false;
    out->resize(n / 2);
    for (int i = 0; i < n / 2; ++i) {
        const int hi = hexNibble(s.at(from + 2 * i));
        const int lo = hexNibble(s.at(from + 2 * i + 1));
        if (hi < 0 || lo < 0)
            return false;
        (*out)[i] = char((hi << 4) | lo);
    }
    return true;
}

struct RawRecord {
    quint64 address;
    QByteArray data;
    int line;
};

// Sort + merge records into contiguous chunks; later records override earlier ones.
QVector<Chunk> mergeRecords(QVector<RawRecord> recs, QStringList *warnings)
{
    // stable sort keeps file order for equal addresses so later data wins below
    std::stable_sort(recs.begin(), recs.end(),
                     [](const RawRecord &a, const RawRecord &b) { return a.address < b.address; });
    QVector<Chunk> chunks;
    int overlapWarnings = 0;
    for (const RawRecord &r : recs) {
        if (r.data.isEmpty())
            continue;
        if (!chunks.isEmpty()) {
            Chunk &c = chunks.last();
            const quint64 end = c.address + quint64(c.data.size());
            if (r.address <= end) {
                const qint64 off = qint64(r.address - c.address);
                const qint64 overlap = qMin<qint64>(qint64(end - r.address), r.data.size());
                if (overlap > 0 && overlapWarnings++ < 10)
                    warnings->append(tr("Line %1: data at 0x%2 overlaps earlier data (later record wins).")
                                         .arg(r.line).arg(QString::number(r.address, 16).toUpper().rightJustified(8, QLatin1Char('0'))));
                if (off + r.data.size() > c.data.size())
                    c.data.resize(int(off + r.data.size()));
                std::copy(r.data.cbegin(), r.data.cend(), c.data.begin() + off);
                continue;
            }
        }
        chunks.append({ r.address, r.data });
    }
    return chunks;
}

QList<QByteArray> splitLines(const QByteArray &text, bool *crlf)
{
    *crlf = text.contains("\r\n");
    QList<QByteArray> lines = text.split('\n');
    for (QByteArray &l : lines)
        l = l.trimmed();
    return lines;
}

QByteArray hexByte(quint8 b)
{
    static const char digits[] = "0123456789ABCDEF";
    QByteArray r(2, '0');
    r[0] = digits[b >> 4];
    r[1] = digits[b & 0xF];
    return r;
}

// Iterate runs of bytes that must be written (non-gap or modified gap).
template <typename F>
void forEachRun(const QByteArray &image, const QByteArray &gapMask, quint8 fill, F fn)
{
    const qint64 n = image.size();
    const bool hasMask = gapMask.size() == image.size();
    qint64 i = 0;
    auto present = [&](qint64 k) {
        return !hasMask || gapMask.at(k) == 0 || quint8(image.at(k)) != fill;
    };
    while (i < n) {
        while (i < n && !present(i))
            ++i;
        const qint64 start = i;
        while (i < n && present(i))
            ++i;
        if (i > start)
            fn(start, i - start);
    }
}

} // namespace

QString formatName(FileFormat f)
{
    switch (f) {
    case FileFormat::IntelHex: return QStringLiteral("Intel HEX");
    case FileFormat::SRecord:  return QStringLiteral("Motorola S-Record");
    case FileFormat::Binary:   break;
    }
    return tr("Binary");
}

FileFormat detect(const QString &fileName, const QByteArray &head)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    QByteArray h = head.left(256).trimmed();
    // must look like printable text
    auto isText = [&]() {
        for (char c : head.left(512)) {
            const uchar u = uchar(c);
            if (u < 0x09 || (u > 0x0D && u < 0x20) || u > 0x7E)
                return false;
        }
        return true;
    };
    static const QStringList hexExt = { QStringLiteral("hex"), QStringLiteral("ihx"), QStringLiteral("ihex"),
                                        QStringLiteral("h86"), QStringLiteral("a43"), QStringLiteral("a90") };
    static const QStringList srecExt = { QStringLiteral("s19"), QStringLiteral("s28"), QStringLiteral("s37"),
                                         QStringLiteral("srec"), QStringLiteral("srecord"), QStringLiteral("mot"),
                                         QStringLiteral("mhx"), QStringLiteral("s"), QStringLiteral("sx"),
                                         QStringLiteral("exo") };
    if (h.isEmpty() || !isText())
        return FileFormat::Binary;
    if (h.startsWith(':') && (hexExt.contains(ext) || h.size() >= 11))
        return FileFormat::IntelHex;
    if (h.size() >= 10 && h.at(0) == 'S' && h.at(1) >= '0' && h.at(1) <= '9'
        && (srecExt.contains(ext) || hexNibble(h.at(2)) >= 0))
        return FileFormat::SRecord;
    return FileFormat::Binary;
}

bool parseIntelHex(const QByteArray &text, ParseResult *out, QString *error)
{
    *out = ParseResult();
    const QList<QByteArray> lines = splitLines(text, &out->crlf);
    QVector<RawRecord> recs;
    quint64 upper = 0;      // from type 02/04
    bool eof = false;
    int checksumErrors = 0;
    QByteArray bytes;

    for (int ln = 0; ln < lines.size(); ++ln) {
        const QByteArray &l = lines.at(ln);
        if (l.isEmpty())
            continue;
        if (eof) {
            out->warnings.append(tr("Data after end-of-file record (line %1) ignored.").arg(ln + 1));
            break;
        }
        if (l.at(0) != ':') {
            *error = tr("Line %1: record does not start with ':'.").arg(ln + 1);
            return false;
        }
        if (!decodeHex(l, 1, &bytes) || bytes.size() < 5) {
            *error = tr("Line %1: invalid hexadecimal record.").arg(ln + 1);
            return false;
        }
        const int count = uchar(bytes.at(0));
        if (bytes.size() != count + 5) {
            *error = tr("Line %1: byte count %2 does not match record length.").arg(ln + 1).arg(count);
            return false;
        }
        quint8 sum = 0;
        for (char c : bytes)
            sum += uchar(c);
        if (sum != 0 && checksumErrors++ < 10)
            out->warnings.append(tr("Line %1: checksum error.").arg(ln + 1));

        const quint16 addr16 = quint16((uchar(bytes.at(1)) << 8) | uchar(bytes.at(2)));
        const int type = uchar(bytes.at(3));
        const QByteArray payload = bytes.mid(4, count);
        ++out->recordCount;

        switch (type) {
        case 0x00:
            recs.append({ upper + addr16, payload, ln + 1 });
            break;
        case 0x01:
            eof = true;
            break;
        case 0x02:
            if (count != 2) { *error = tr("Line %1: bad extended segment address record.").arg(ln + 1); return false; }
            upper = quint64((uchar(payload.at(0)) << 8) | uchar(payload.at(1))) << 4;
            break;
        case 0x04:
            if (count != 2) { *error = tr("Line %1: bad extended linear address record.").arg(ln + 1); return false; }
            upper = quint64((uchar(payload.at(0)) << 8) | uchar(payload.at(1))) << 16;
            break;
        case 0x03:
        case 0x05:
            if (count != 4) { *error = tr("Line %1: bad start address record.").arg(ln + 1); return false; }
            out->hasStart = true;
            out->startRecordType = type;
            out->startAddress = (quint64(uchar(payload.at(0))) << 24) | (quint64(uchar(payload.at(1))) << 16)
                              | (quint64(uchar(payload.at(2))) << 8) | quint64(uchar(payload.at(3)));
            break;
        default:
            out->warnings.append(tr("Line %1: unknown record type %2 ignored.").arg(ln + 1).arg(type, 2, 16, QLatin1Char('0')));
            break;
        }
    }
    if (checksumErrors > 10)
        out->warnings.append(tr("... %1 checksum errors in total.").arg(checksumErrors));
    if (!eof)
        out->warnings.append(tr("No end-of-file record (:00000001FF) found."));
    out->chunks = mergeRecords(recs, &out->warnings);
    if (out->chunks.isEmpty())
        out->warnings.append(tr("File contains no data records."));
    return true;
}

bool parseSRecord(const QByteArray &text, ParseResult *out, QString *error)
{
    *out = ParseResult();
    const QList<QByteArray> lines = splitLines(text, &out->crlf);
    QVector<RawRecord> recs;
    int checksumErrors = 0;
    int dataRecords = 0;
    QByteArray bytes;

    for (int ln = 0; ln < lines.size(); ++ln) {
        const QByteArray &l = lines.at(ln);
        if (l.isEmpty())
            continue;
        if (l.size() < 4 || l.at(0) != 'S' || l.at(1) < '0' || l.at(1) > '9') {
            *error = tr("Line %1: not a valid S-Record.").arg(ln + 1);
            return false;
        }
        const int type = l.at(1) - '0';
        if (!decodeHex(l, 2, &bytes) || bytes.size() < 3) {
            *error = tr("Line %1: invalid hexadecimal record.").arg(ln + 1);
            return false;
        }
        const int count = uchar(bytes.at(0));
        if (bytes.size() != count + 1) {
            *error = tr("Line %1: byte count %2 does not match record length.").arg(ln + 1).arg(count);
            return false;
        }
        quint8 sum = 0;
        for (char c : bytes)
            sum += uchar(c);
        if (sum != 0xFF && checksumErrors++ < 10)
            out->warnings.append(tr("Line %1: checksum error.").arg(ln + 1));

        int abytes = 0;
        switch (type) {
        case 0: case 1: case 5: case 9: abytes = 2; break;
        case 2: case 6: case 8:         abytes = 3; break;
        case 3: case 7:                 abytes = 4; break;
        default:
            out->warnings.append(tr("Line %1: record type S%2 ignored.").arg(ln + 1).arg(type));
            continue;
        }
        if (count < abytes + 1) {
            *error = tr("Line %1: record too short.").arg(ln + 1);
            return false;
        }
        quint64 addr = 0;
        for (int i = 0; i < abytes; ++i)
            addr = (addr << 8) | uchar(bytes.at(1 + i));
        const QByteArray payload = bytes.mid(1 + abytes, count - abytes - 1);
        ++out->recordCount;

        switch (type) {
        case 0:
            out->srecHeader = payload;
            break;
        case 1: case 2: case 3:
            out->srecAddressBytes = qMax(out->srecAddressBytes, abytes);
            recs.append({ addr, payload, ln + 1 });
            ++dataRecords;
            break;
        case 5: case 6:
            if (addr != quint64(dataRecords))
                out->warnings.append(tr("Line %1: record count %2 does not match %3 data records.")
                                         .arg(ln + 1).arg(addr).arg(dataRecords));
            break;
        case 7: case 8: case 9:
            out->hasStart = true;
            out->startAddress = addr;
            break;
        }
    }
    if (checksumErrors > 10)
        out->warnings.append(tr("... %1 checksum errors in total.").arg(checksumErrors));
    out->chunks = mergeRecords(recs, &out->warnings);
    if (out->chunks.isEmpty())
        out->warnings.append(tr("File contains no data records."));
    return true;
}

bool flatten(const ParseResult &pr, quint8 fill, qint64 maxSpan,
             QByteArray *image, quint64 *baseAddress, QByteArray *gapMask, QString *error)
{
    image->clear();
    gapMask->clear();
    *baseAddress = 0;
    if (pr.chunks.isEmpty())
        return true;
    const quint64 lo = pr.chunks.first().address;
    const quint64 hi = pr.chunks.last().address + quint64(pr.chunks.last().data.size());
    const quint64 span = hi - lo;
    if (span > quint64(maxSpan)) {
        QString list;
        int n = 0;
        for (const Chunk &c : pr.chunks) {
            if (n++ == 12) { list += QStringLiteral("\n  ..."); break; }
            list += QStringLiteral("\n  0x%1 - 0x%2 (%3 bytes)")
                        .arg(c.address, 8, 16, QLatin1Char('0'))
                        .arg(c.address + quint64(c.data.size()) - 1, 8, 16, QLatin1Char('0'))
                        .arg(c.data.size());
        }
        *error = tr("Address range 0x%1 - 0x%2 spans %3 MiB, which is too large to show as one image.\nSegments:%4")
                     .arg(lo, 8, 16, QLatin1Char('0')).arg(hi - 1, 8, 16, QLatin1Char('0'))
                     .arg(double(span) / (1024.0 * 1024.0), 0, 'f', 1).arg(list);
        return false;
    }
    *baseAddress = lo;
    image->fill(char(fill), qsizetype(span));
    gapMask->fill(char(1), qsizetype(span));
    for (const Chunk &c : pr.chunks) {
        const qint64 off = qint64(c.address - lo);
        std::copy(c.data.cbegin(), c.data.cend(), image->begin() + off);
        std::fill(gapMask->begin() + off, gapMask->begin() + off + c.data.size(), char(0));
    }
    return true;
}

bool writeIntelHex(const QByteArray &image, quint64 baseAddress, const QByteArray &gapMask,
                   quint8 fill, const WriteOptions &opt, QByteArray *out, QString *error)
{
    out->clear();
    const QByteArray nl = opt.crlf ? QByteArrayLiteral("\r\n") : QByteArrayLiteral("\n");
    const int recLen = qBound(1, opt.bytesPerRecord, 255);
    if (baseAddress + quint64(image.size()) > 0x100000000ULL) {
        *error = tr("Intel HEX can only address 4 GiB (end address exceeds 0xFFFFFFFF).");
        return false;
    }
    auto emitRecord = [&](int type, quint16 addr, const char *data, int len) {
        quint8 sum = quint8(len) + quint8(addr >> 8) + quint8(addr) + quint8(type);
        QByteArray line = ":" + hexByte(quint8(len)) + hexByte(quint8(addr >> 8)) + hexByte(quint8(addr)) + hexByte(quint8(type));
        for (int i = 0; i < len; ++i) {
            line += hexByte(quint8(data[i]));
            sum += quint8(data[i]);
        }
        line += hexByte(quint8(0x100 - sum));
        *out += line + nl;
    };
    quint64 currentUpper = 0;
    forEachRun(image, gapMask, fill, [&](qint64 start, qint64 len) {
        qint64 pos = start;
        while (pos < start + len) {
            const quint64 addr = baseAddress + quint64(pos);
            const quint64 upper = addr >> 16;
            if (upper != currentUpper) {
                const char ext[2] = { char(upper >> 8), char(upper) };
                emitRecord(0x04, 0, ext, 2);
                currentUpper = upper;
            }
            // do not cross a 64 KiB boundary inside one record
            const qint64 toBoundary = qint64(0x10000 - (addr & 0xFFFF));
            const int n = int(qMin<qint64>(qMin<qint64>(recLen, start + len - pos), toBoundary));
            emitRecord(0x00, quint16(addr & 0xFFFF), image.constData() + pos, n);
            pos += n;
        }
    });
    if (opt.hasStart) {
        const char s[4] = { char(opt.startAddress >> 24), char(opt.startAddress >> 16),
                            char(opt.startAddress >> 8), char(opt.startAddress) };
        emitRecord(opt.startRecordType == 3 ? 0x03 : 0x05, 0, s, 4);
    }
    *out += ":00000001FF" + nl;
    return true;
}

bool writeSRecord(const QByteArray &image, quint64 baseAddress, const QByteArray &gapMask,
                  quint8 fill, const WriteOptions &opt, QByteArray *out, QString *error)
{
    out->clear();
    const QByteArray nl = opt.crlf ? QByteArrayLiteral("\r\n") : QByteArrayLiteral("\n");
    const quint64 end = baseAddress + quint64(image.size());
    if (end > 0x100000000ULL) {
        *error = tr("S-Record can only address 4 GiB (end address exceeds 0xFFFFFFFF).");
        return false;
    }
    int abytes = opt.srecAddressBytes;
    const int needed = (end <= 0x10000ULL) ? 2 : (end <= 0x1000000ULL ? 3 : 4);
    if (abytes < needed)
        abytes = needed;
    const int recLen = qBound(1, opt.bytesPerRecord, 255 - abytes - 1);

    auto emitRecord = [&](int type, int ab, quint64 addr, const char *data, int len) {
        const int count = ab + len + 1;
        quint8 sum = quint8(count);
        QByteArray line = "S" + QByteArray::number(type) + hexByte(quint8(count));
        for (int i = ab - 1; i >= 0; --i) {
            const quint8 b = quint8(addr >> (8 * i));
            line += hexByte(b);
            sum += b;
        }
        for (int i = 0; i < len; ++i) {
            line += hexByte(quint8(data[i]));
            sum += quint8(data[i]);
        }
        line += hexByte(quint8(~sum));
        *out += line + nl;
    };

    emitRecord(0, 2, 0, opt.srecHeader.constData(), int(qMin<qsizetype>(opt.srecHeader.size(), 64)));
    const int dataType = abytes - 1;            // S1/S2/S3
    int dataCount = 0;
    forEachRun(image, gapMask, fill, [&](qint64 start, qint64 len) {
        qint64 pos = start;
        while (pos < start + len) {
            const int n = int(qMin<qint64>(recLen, start + len - pos));
            emitRecord(dataType, abytes, baseAddress + quint64(pos), image.constData() + pos, n);
            pos += n;
            ++dataCount;
        }
    });
    if (dataCount <= 0xFFFF)
        emitRecord(5, 2, quint64(dataCount), nullptr, 0);
    else if (dataCount <= 0xFFFFFF)
        emitRecord(6, 3, quint64(dataCount), nullptr, 0);
    const int termType = 10 - dataType;         // S9 / S8 / S7
    emitRecord(termType, abytes, opt.hasStart ? opt.startAddress : 0, nullptr, 0);
    return true;
}

} // namespace Formats
