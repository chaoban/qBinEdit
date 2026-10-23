// qBinEdit - checksum / CRC / hash algorithms
#include "checksums.h"

#include <QCryptographicHash>
#include <array>

namespace Checksums {

namespace {

quint32 reflect(quint32 v, int bits)
{
    quint32 r = 0;
    for (int i = 0; i < bits; ++i) {
        if (v & (1u << i))
            r |= 1u << (bits - 1 - i);
    }
    return r;
}

QString hex(quint64 v, int digits)
{
    return QStringLiteral("0x") + QStringLiteral("%1").arg(v, digits, 16, QLatin1Char('0')).toUpper();
}

} // namespace

const QVector<CrcSpec> &crcCatalog()
{
    // Parameters & check values from the "Catalogue of parametrised CRC algorithms" (reveng).
    static const QVector<CrcSpec> specs = {
        { "CRC-8/SMBUS",         8,  0x07,       0x00,       false, false, 0x00,       0xF4 },
        { "CRC-8/MAXIM-DOW",     8,  0x31,       0x00,       true,  true,  0x00,       0xA1 },
        { "CRC-16/IBM-3740 (CCITT-FALSE)", 16, 0x1021, 0xFFFF, false, false, 0x0000,   0x29B1 },
        { "CRC-16/XMODEM",       16, 0x1021,     0x0000,     false, false, 0x0000,     0x31C3 },
        { "CRC-16/KERMIT",       16, 0x1021,     0x0000,     true,  true,  0x0000,     0x2189 },
        { "CRC-16/MODBUS",       16, 0x8005,     0xFFFF,     true,  true,  0x0000,     0x4B37 },
        { "CRC-16/ARC",          16, 0x8005,     0x0000,     true,  true,  0x0000,     0xBB3D },
        { "CRC-32/ISO-HDLC (zip)", 32, 0x04C11DB7, 0xFFFFFFFF, true,  true,  0xFFFFFFFF, 0xCBF43926 },
        { "CRC-32/MPEG-2 (STM32)", 32, 0x04C11DB7, 0xFFFFFFFF, false, false, 0x00000000, 0x0376E6E7 },
        { "CRC-32/BZIP2",        32, 0x04C11DB7, 0xFFFFFFFF, false, false, 0xFFFFFFFF, 0xFC891918 },
        { "CRC-32C (Castagnoli)", 32, 0x1EDC6F41, 0xFFFFFFFF, true,  true,  0xFFFFFFFF, 0xE3069283 },
    };
    return specs;
}

quint32 crcCompute(const CrcSpec &spec, const uchar *data, qint64 len)
{
    const int w = spec.width;
    const quint32 mask = (w == 32) ? 0xFFFFFFFFu : ((1u << w) - 1u);

    // Work in a reflected or a left-aligned (MSB-first, 32-bit) register.
    std::array<quint32, 256> table{};
    if (spec.refIn) {
        const quint32 rpoly = reflect(spec.poly, w);
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (c >> 1) ^ rpoly : (c >> 1);
            table[i] = c;
        }
        quint32 crc = reflect(spec.init, w);
        for (qint64 i = 0; i < len; ++i)
            crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xFFu];
        if (!spec.refOut)
            crc = reflect(crc, w);
        return (crc ^ spec.xorOut) & mask;
    }

    // MSB-first: shift polynomial to top of a 32-bit register.
    const int shift = 32 - w;
    const quint32 tpoly = spec.poly << shift;
    for (quint32 i = 0; i < 256; ++i) {
        quint32 c = i << 24;
        for (int k = 0; k < 8; ++k)
            c = (c & 0x80000000u) ? (c << 1) ^ tpoly : (c << 1);
        table[i] = c;
    }
    quint32 crc = spec.init << shift;
    for (qint64 i = 0; i < len; ++i)
        crc = (crc << 8) ^ table[((crc >> 24) ^ data[i]) & 0xFFu];
    crc >>= shift;
    if (spec.refOut)
        crc = reflect(crc, w);
    return (crc ^ spec.xorOut) & mask;
}

quint32 adler32(const uchar *data, qint64 len)
{
    quint32 a = 1, b = 0;
    const quint32 MOD = 65521;
    while (len > 0) {
        qint64 n = len < 5552 ? len : 5552;   // avoid overflow
        len -= n;
        while (n--) {
            a += *data++;
            b += a;
        }
        a %= MOD;
        b %= MOD;
    }
    return (b << 16) | a;
}

QVector<Result> computeAll(const uchar *data, qint64 len)
{
    QVector<Result> out;

    quint64 sum = 0;
    quint8 x8 = 0;
    for (qint64 i = 0; i < len; ++i) {
        sum += data[i];
        x8 ^= data[i];
    }
    quint32 wsumLE = 0, wsumBE = 0;
    for (qint64 i = 0; i + 1 < len; i += 2) {
        wsumLE += quint32(data[i]) | (quint32(data[i + 1]) << 8);
        wsumBE += (quint32(data[i]) << 8) | quint32(data[i + 1]);
    }
    if (len & 1) {           // odd trailing byte: pad with 0x00
        wsumLE += data[len - 1];
        wsumBE += quint32(data[len - 1]) << 8;
    }

    out.append({ QStringLiteral("Sum-8"), hex(sum & 0xFF, 2), QStringLiteral("byte sum mod 2^8") });
    out.append({ QStringLiteral("Sum-8 (2's complement)"), hex((0x100 - (sum & 0xFF)) & 0xFF, 2),
                 QStringLiteral("value that makes Sum-8 = 0") });
    out.append({ QStringLiteral("Sum-16"), hex(sum & 0xFFFF, 4), QStringLiteral("byte sum mod 2^16") });
    out.append({ QStringLiteral("Sum-32"), hex(sum & 0xFFFFFFFFu, 8), QStringLiteral("byte sum mod 2^32") });
    out.append({ QStringLiteral("Word Sum-16 (LE)"), hex(wsumLE & 0xFFFF, 4), QStringLiteral("16-bit little-endian words") });
    out.append({ QStringLiteral("Word Sum-16 (BE)"), hex(wsumBE & 0xFFFF, 4), QStringLiteral("16-bit big-endian words") });
    out.append({ QStringLiteral("XOR-8"), hex(x8, 2), QString() });

    for (const CrcSpec &s : crcCatalog()) {
        const quint32 v = crcCompute(s, data, len);
        out.append({ QString::fromLatin1(s.name), hex(v, s.width / 4),
                     QStringLiteral("poly ") + hex(s.poly, s.width / 4) });
    }

    out.append({ QStringLiteral("Adler-32"), hex(adler32(data, len), 8), QString() });

    auto hashOf = [&](QCryptographicHash::Algorithm a) {
        QCryptographicHash h(a);
        // feed in chunks so >2 GiB would also work
        qint64 done = 0;
        while (done < len) {
            const qint64 n = qMin<qint64>(len - done, 64 * 1024 * 1024);
            h.addData(QByteArrayView(reinterpret_cast<const char *>(data + done), n));
            done += n;
        }
        return QString::fromLatin1(h.result().toHex()).toUpper();
    };
    out.append({ QStringLiteral("MD5"), hashOf(QCryptographicHash::Md5), QString() });
    out.append({ QStringLiteral("SHA-1"), hashOf(QCryptographicHash::Sha1), QString() });
    out.append({ QStringLiteral("SHA-256"), hashOf(QCryptographicHash::Sha256), QString() });
    return out;
}

} // namespace Checksums
