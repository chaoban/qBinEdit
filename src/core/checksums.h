// qBinEdit - checksum / CRC / hash algorithms
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Checksums {

struct CrcSpec {
    const char *name;   // catalogue name, e.g. "CRC-32/ISO-HDLC"
    int width;          // 8, 16 or 32
    quint32 poly;
    quint32 init;
    bool refIn;
    bool refOut;
    quint32 xorOut;
    quint32 check;      // CRC of ASCII "123456789" (for self-test)
};

// Generic table-free bitwise CRC (fast enough for firmware-sized images when
// combined with the per-spec lookup table built in crcCompute()).
quint32 crcCompute(const CrcSpec &spec, const uchar *data, qint64 len);

const QVector<CrcSpec> &crcCatalog();

struct Result {
    QString name;
    QString value;      // formatted (hex, upper case)
    QString note;
};

// Compute every supported algorithm over [data, data+len).
QVector<Result> computeAll(const uchar *data, qint64 len);

quint32 adler32(const uchar *data, qint64 len);

} // namespace Checksums
