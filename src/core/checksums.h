// qBinEdit - checksum / CRC / hash algorithms
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <atomic>

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

const QVector<CrcSpec> &crcCatalog();

// Incremental table-driven CRC (slicing-by-8). update() may be called any number of times.
class Crc
{
public:
    explicit Crc(const CrcSpec &spec);
    void update(const uchar *data, qint64 len);
    quint32 value() const;

private:
    CrcSpec m_spec;
    int m_shift = 0;        // MSB-first: CRC kept left-aligned in a 32-bit register
    quint32 m_mask = 0;
    quint32 m_crc = 0;
    quint32 m_t[8][256];
};

// One-shot convenience wrapper around Crc.
quint32 crcCompute(const CrcSpec &spec, const uchar *data, qint64 len);

// Reference implementation (bit by bit), used by the self-tests.
quint32 crcBitwise(const CrcSpec &spec, const uchar *data, qint64 len);

quint32 adler32(const uchar *data, qint64 len);

struct Result {
    QString name;
    QString value;      // formatted (hex, upper case)
    QString note;
};

// Compute every supported algorithm over [data, data+len). Large inputs are split over
// worker threads. 'cancel' and 'progress' are optional and may be used from another
// thread: progress receives the bytes processed so far (0..len) by the slowest worker.
// Returns an empty vector when cancelled.
QVector<Result> computeAll(const uchar *data, qint64 len,
                           const std::atomic<bool> *cancel = nullptr,
                           std::atomic<qint64> *progress = nullptr);

} // namespace Checksums
