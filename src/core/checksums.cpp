// qBinEdit - checksum / CRC / hash algorithms
#include "checksums.h"

#include <QCryptographicHash>
#include <algorithm>
#include <memory>
#include <thread>
#include <vector>

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

inline quint32 loadLE(const uchar *p)
{
    return quint32(p[0]) | (quint32(p[1]) << 8) | (quint32(p[2]) << 16) | (quint32(p[3]) << 24);
}

inline quint32 loadBE(const uchar *p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | quint32(p[3]);
}

// ---- computeAll building blocks -------------------------------------------------

class Job
{
public:
    explicit Job(int relativeCost) : cost(relativeCost) {}
    virtual ~Job() = default;
    virtual void update(const uchar *p, qint64 n) = 0;
    virtual void finish(QVector<Result> *out) = 0;
    const int cost;         // relative CPU cost per byte, for spreading jobs over threads
};

class SumsJob : public Job
{
public:
    SumsJob() : Job(2) {}
    void update(const uchar *p, qint64 n) override
    {
        quint64 sum = 0;
        quint8 x = 0;
        for (qint64 i = 0; i < n; ++i) {
            sum += p[i];
            x ^= p[i];
        }
        m_sum += sum;
        m_xor ^= x;
        qint64 i = 0;
        if (m_odd && n > 0) {           // finish the word started by the previous block
            m_le += quint32(m_pending) | (quint32(p[0]) << 8);
            m_be += (quint32(m_pending) << 8) | quint32(p[0]);
            m_odd = false;
            i = 1;
        }
        quint32 le = 0, be = 0;
        for (; i + 1 < n; i += 2) {
            le += quint32(p[i]) | (quint32(p[i + 1]) << 8);
            be += (quint32(p[i]) << 8) | quint32(p[i + 1]);
        }
        m_le += le;
        m_be += be;
        if (i < n) {
            m_pending = p[i];
            m_odd = true;
        }
    }
    void finish(QVector<Result> *out) override
    {
        if (m_odd) {                    // odd trailing byte: pad with 0x00
            m_le += m_pending;
            m_be += quint32(m_pending) << 8;
        }
        out->append({ QStringLiteral("Sum-8"), hex(m_sum & 0xFF, 2), QStringLiteral("byte sum mod 2^8") });
        out->append({ QStringLiteral("Sum-8 (2's complement)"), hex((0x100 - (m_sum & 0xFF)) & 0xFF, 2),
                      QStringLiteral("value that makes Sum-8 = 0") });
        out->append({ QStringLiteral("Sum-16"), hex(m_sum & 0xFFFF, 4), QStringLiteral("byte sum mod 2^16") });
        out->append({ QStringLiteral("Sum-32"), hex(m_sum & 0xFFFFFFFFu, 8), QStringLiteral("byte sum mod 2^32") });
        out->append({ QStringLiteral("Word Sum-16 (LE)"), hex(m_le & 0xFFFF, 4), QStringLiteral("16-bit little-endian words") });
        out->append({ QStringLiteral("Word Sum-16 (BE)"), hex(m_be & 0xFFFF, 4), QStringLiteral("16-bit big-endian words") });
        out->append({ QStringLiteral("XOR-8"), hex(m_xor, 2), QString() });
    }

private:
    quint64 m_sum = 0;
    quint8 m_xor = 0;
    quint32 m_le = 0, m_be = 0;
    bool m_odd = false;
    uchar m_pending = 0;
};

class CrcJob : public Job
{
public:
    explicit CrcJob(const CrcSpec &s) : Job(2), m_spec(s), m_crc(s) {}
    void update(const uchar *p, qint64 n) override { m_crc.update(p, n); }
    void finish(QVector<Result> *out) override
    {
        out->append({ QString::fromLatin1(m_spec.name), hex(m_crc.value(), m_spec.width / 4),
                      QStringLiteral("poly ") + hex(m_spec.poly, m_spec.width / 4) });
    }

private:
    CrcSpec m_spec;
    Crc m_crc;
};

class AdlerJob : public Job
{
public:
    AdlerJob() : Job(1) {}
    void update(const uchar *p, qint64 n) override
    {
        const quint32 MOD = 65521;
        while (n > 0) {
            qint64 k = n < 5552 ? n : 5552;     // largest block without 32-bit overflow
            n -= k;
            while (k--) {
                m_a += *p++;
                m_b += m_a;
            }
            m_a %= MOD;
            m_b %= MOD;
        }
    }
    quint32 value() const { return (m_b << 16) | m_a; }
    void finish(QVector<Result> *out) override
    {
        out->append({ QStringLiteral("Adler-32"), hex(value(), 8), QString() });
    }

private:
    quint32 m_a = 1, m_b = 0;
};

class HashJob : public Job
{
public:
    HashJob(QCryptographicHash::Algorithm a, const char *name, int relativeCost)
        : Job(relativeCost), m_hash(a), m_name(QString::fromLatin1(name)) {}
    void update(const uchar *p, qint64 n) override
    {
        m_hash.addData(QByteArrayView(reinterpret_cast<const char *>(p), n));
    }
    void finish(QVector<Result> *out) override
    {
        out->append({ m_name, QString::fromLatin1(m_hash.result().toHex()).toUpper(), QString() });
    }

private:
    QCryptographicHash m_hash;
    QString m_name;
};

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

// ---- Crc -------------------------------------------------------------------------

Crc::Crc(const CrcSpec &spec)
    : m_spec(spec)
{
    const int w = spec.width;
    m_mask = (w == 32) ? 0xFFFFFFFFu : ((1u << w) - 1u);
    if (spec.refIn) {
        // reflected register, right-aligned
        const quint32 rpoly = reflect(spec.poly, w);
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (c >> 1) ^ rpoly : (c >> 1);
            m_t[0][i] = c;
        }
        for (int k = 1; k < 8; ++k)
            for (int i = 0; i < 256; ++i)
                m_t[k][i] = (m_t[k - 1][i] >> 8) ^ m_t[0][m_t[k - 1][i] & 0xFFu];
        m_crc = reflect(spec.init, w);
    } else {
        // MSB-first register, left-aligned in 32 bits
        m_shift = 32 - w;
        const quint32 tpoly = spec.poly << m_shift;
        for (quint32 i = 0; i < 256; ++i) {
            quint32 c = i << 24;
            for (int k = 0; k < 8; ++k)
                c = (c & 0x80000000u) ? (c << 1) ^ tpoly : (c << 1);
            m_t[0][i] = c;
        }
        for (int k = 1; k < 8; ++k)
            for (int i = 0; i < 256; ++i)
                m_t[k][i] = (m_t[k - 1][i] << 8) ^ m_t[0][m_t[k - 1][i] >> 24];
        m_crc = spec.init << m_shift;
    }
}

void Crc::update(const uchar *p, qint64 len)
{
    quint32 crc = m_crc;
    const auto &t = m_t;
    if (m_spec.refIn) {
        for (; len >= 8; p += 8, len -= 8) {
            const quint32 one = loadLE(p) ^ crc;
            const quint32 two = loadLE(p + 4);
            crc = t[7][one & 0xFF] ^ t[6][(one >> 8) & 0xFF] ^ t[5][(one >> 16) & 0xFF] ^ t[4][one >> 24]
                ^ t[3][two & 0xFF] ^ t[2][(two >> 8) & 0xFF] ^ t[1][(two >> 16) & 0xFF] ^ t[0][two >> 24];
        }
        for (; len > 0; --len)
            crc = (crc >> 8) ^ t[0][(crc ^ *p++) & 0xFFu];
    } else {
        for (; len >= 8; p += 8, len -= 8) {
            const quint32 one = loadBE(p) ^ crc;
            const quint32 two = loadBE(p + 4);
            crc = t[7][one >> 24] ^ t[6][(one >> 16) & 0xFF] ^ t[5][(one >> 8) & 0xFF] ^ t[4][one & 0xFF]
                ^ t[3][two >> 24] ^ t[2][(two >> 16) & 0xFF] ^ t[1][(two >> 8) & 0xFF] ^ t[0][two & 0xFF];
        }
        for (; len > 0; --len)
            crc = (crc << 8) ^ t[0][((crc >> 24) ^ *p++) & 0xFFu];
    }
    m_crc = crc;
}

quint32 Crc::value() const
{
    quint32 c = m_crc;
    if (m_spec.refIn) {
        if (!m_spec.refOut)
            c = reflect(c, m_spec.width);
    } else {
        c >>= m_shift;
        if (m_spec.refOut)
            c = reflect(c, m_spec.width);
    }
    return (c ^ m_spec.xorOut) & m_mask;
}

quint32 crcCompute(const CrcSpec &spec, const uchar *data, qint64 len)
{
    Crc c(spec);
    c.update(data, len);
    return c.value();
}

quint32 crcBitwise(const CrcSpec &spec, const uchar *data, qint64 len)
{
    const int w = spec.width;
    const quint32 top = 1u << (w - 1);
    const quint32 mask = (w == 32) ? 0xFFFFFFFFu : ((1u << w) - 1u);
    quint32 crc = spec.init & mask;
    for (qint64 i = 0; i < len; ++i) {
        const quint32 b = spec.refIn ? reflect(data[i], 8) : data[i];
        crc ^= b << (w - 8);
        for (int k = 0; k < 8; ++k)
            crc = (crc & top) ? ((crc << 1) ^ spec.poly) & mask : (crc << 1) & mask;
    }
    if (spec.refOut)
        crc = reflect(crc, w);
    return (crc ^ spec.xorOut) & mask;
}

quint32 adler32(const uchar *data, qint64 len)
{
    AdlerJob j;
    j.update(data, len);
    return j.value();
}

QVector<Result> computeAll(const uchar *data, qint64 len, const std::atomic<bool> *cancel,
                           std::atomic<qint64> *progress)
{
    std::vector<std::unique_ptr<Job>> jobs;
    jobs.push_back(std::make_unique<SumsJob>());
    for (const CrcSpec &s : crcCatalog())
        jobs.push_back(std::make_unique<CrcJob>(s));
    jobs.push_back(std::make_unique<AdlerJob>());
    jobs.push_back(std::make_unique<HashJob>(QCryptographicHash::Md5, "MD5", 4));
    jobs.push_back(std::make_unique<HashJob>(QCryptographicHash::Sha1, "SHA-1", 5));
    jobs.push_back(std::make_unique<HashJob>(QCryptographicHash::Sha256, "SHA-256", 12));

    // Spread jobs over up to 3 threads (longest job first onto the least loaded lane).
    constexpr qint64 kParallelThreshold = qint64(4) * 1024 * 1024;
    const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    const int lanes = len >= kParallelThreshold ? int(std::min(3u, hw)) : 1;
    std::vector<std::vector<Job *>> lane(lanes);
    {
        std::vector<Job *> order;
        for (auto &j : jobs)
            order.push_back(j.get());
        std::stable_sort(order.begin(), order.end(), [](Job *a, Job *b) { return a->cost > b->cost; });
        std::vector<int> load(lanes, 0);
        for (Job *j : order) {
            const int k = int(std::min_element(load.begin(), load.end()) - load.begin());
            lane[k].push_back(j);
            load[k] += j->cost;
        }
    }

    // Each lane walks the data in blocks that stay in cache while all of its jobs run.
    constexpr qint64 kBlock = 1024 * 1024;     // even, so 16-bit word sums never split a word mid-buffer
    std::vector<std::atomic<qint64>> done(lanes);
    for (auto &d : done)
        d.store(0);
    auto runLane = [&](int k) {
        for (qint64 off = 0; off < len; off += kBlock) {
            if (cancel && cancel->load(std::memory_order_relaxed))
                return;
            const qint64 n = std::min(kBlock, len - off);
            for (Job *j : lane[k])
                j->update(data + off, n);
            done[k].store(off + n, std::memory_order_relaxed);
            if (progress) {
                qint64 slowest = len;
                for (const auto &d : done)
                    slowest = std::min(slowest, d.load(std::memory_order_relaxed));
                progress->store(slowest, std::memory_order_relaxed);
            }
        }
    };
    if (lanes == 1) {
        runLane(0);
    } else {
        std::vector<std::thread> threads;
        for (int k = 1; k < lanes; ++k)
            threads.emplace_back(runLane, k);
        runLane(0);
        for (auto &t : threads)
            t.join();
    }
    if (cancel && cancel->load())
        return {};
    if (progress)
        progress->store(len);

    QVector<Result> out;
    for (auto &j : jobs)
        j->finish(&out);
    return out;
}

} // namespace Checksums
