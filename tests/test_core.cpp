// qBinEdit - console unit tests for checksums and firmware formats
#include "checksums.h"
#include "formats.h"
#include "search.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <cstdio>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); ++g_fail; } else { std::printf("ok   %s\n", msg); } } while (0)

static void testCrc()
{
    const QByteArray s("123456789");
    const auto *d = reinterpret_cast<const uchar *>(s.constData());
    for (const auto &spec : Checksums::crcCatalog()) {
        const quint32 v = Checksums::crcCompute(spec, d, s.size());
        CHECK(v == spec.check, spec.name);
    }
    CHECK(Checksums::adler32(d, s.size()) == 0x091E01DE, "Adler-32");
    const auto all = Checksums::computeAll(d, s.size());
    bool shaOk = false;
    for (const auto &r : all)
        if (r.name == QLatin1String("SHA-256"))
            shaOk = r.value == QLatin1String("15E2B0D3C33891EBB0F1EF609EC419420C20E320CE94C65FBC8C3312448EB225");
    CHECK(shaOk, "SHA-256");
    bool sumOk = false;
    for (const auto &r : all)
        if (r.name == QLatin1String("Sum-16"))
            sumOk = r.value == QLatin1String("0x01DD");   // 0x31+..+0x39 = 477
    CHECK(sumOk, "Sum-16");
}

static void testIntelHex()
{
    // Classic example with an extended linear address record and a start address.
    const QByteArray hex =
        ":020000040800F2\r\n"
        ":10000000214601360121470136007EFE09D2190141\r\n"
        ":100010002146017E17C20001FF5F16002148011929\r\n"
        ":0400300001020304C2\r\n"
        ":0400000508000101ED\r\n"
        ":00000001FF\r\n";
    Formats::ParseResult pr;
    QString err;
    CHECK(Formats::parseIntelHex(hex, &pr, &err), "parse Intel HEX");
    CHECK(pr.warnings.isEmpty(), "Intel HEX no warnings");
    CHECK(pr.chunks.size() == 2, "Intel HEX two chunks (gap at 0x20)");
    CHECK(pr.chunks.value(0).address == 0x08000000, "Intel HEX base 0x08000000");
    CHECK(pr.hasStart && pr.startAddress == 0x08000101, "Intel HEX start address");

    QByteArray img, gap;
    quint64 base = 0;
    CHECK(Formats::flatten(pr, 0xFF, 1 << 20, &img, &base, &gap, &err), "flatten");
    CHECK(img.size() == 0x34 && base == 0x08000000, "image size 0x34");
    CHECK(uchar(img.at(0x20)) == 0xFF && gap.at(0x20) == 1, "gap filled with 0xFF");
    CHECK(uchar(img.at(0x30)) == 0x01 && gap.at(0x30) == 0, "data after gap");

    Formats::WriteOptions wo;
    wo.hasStart = true;
    wo.startAddress = pr.startAddress;
    QByteArray outHex;
    CHECK(Formats::writeIntelHex(img, base, gap, 0xFF, wo, &outHex, &err), "write Intel HEX");
    CHECK(outHex == hex, "Intel HEX round-trip identical");

    // checksum error is a warning, not a failure
    QByteArray bad = hex;
    bad.replace(":0400300001020304C2", ":040030000102030400");
    CHECK(Formats::parseIntelHex(bad, &pr, &err) && !pr.warnings.isEmpty(), "Intel HEX checksum warning");

    // 64 KiB boundary handling on write
    QByteArray big(0x30, char(0xAA));
    QByteArray out2;
    CHECK(Formats::writeIntelHex(big, 0x0000FFF0, QByteArray(), 0xFF, Formats::WriteOptions(), &out2, &err), "write across 64K");
    Formats::ParseResult pr2;
    CHECK(Formats::parseIntelHex(out2, &pr2, &err) && pr2.chunks.size() == 1
          && pr2.chunks[0].address == 0xFFF0 && pr2.chunks[0].data == big, "64K boundary round-trip");
}

static void testSRecord()
{
    const QByteArray srec =
        "S00F000068656C6C6F202020202000003C\n"
        "S11F00007C0802A6900100049421FFF07C6C1B787C8C23783C6000003863000026\n"
        "S11F001C4BFFFFE5398000007D83637880010014382100107C0803A64E800020E9\n"
        "S111003848656C6C6F20776F726C642E0A0042\n"
        "S5030003F9\n"
        "S9030000FC\n";
    Formats::ParseResult pr;
    QString err;
    CHECK(Formats::parseSRecord(srec, &pr, &err), "parse S-Record");
    CHECK(pr.warnings.isEmpty(), "S-Record no warnings");
    CHECK(pr.chunks.size() == 1 && pr.chunks[0].data.size() == 0x46, "S-Record one chunk of 70 bytes");
    CHECK(pr.srecHeader == QByteArray("hello     \0\0", 12), "S0 header");

    QByteArray img, gap;
    quint64 base = 0;
    CHECK(Formats::flatten(pr, 0xFF, 1 << 20, &img, &base, &gap, &err), "flatten S-Record");
    Formats::WriteOptions wo;
    wo.crlf = false;
    wo.bytesPerRecord = 28;
    wo.srecHeader = pr.srecHeader;
    wo.hasStart = true;
    wo.startAddress = 0;
    QByteArray out;
    CHECK(Formats::writeSRecord(img, base, gap, 0xFF, wo, &out, &err), "write S-Record");
    CHECK(out == srec, "S-Record round-trip identical");

    // S3 records
    QByteArray big(40, char(0x5A));
    CHECK(Formats::writeSRecord(big, 0x08000000, QByteArray(), 0xFF, Formats::WriteOptions(), &out, &err), "write S3");
    Formats::ParseResult pr3;
    CHECK(Formats::parseSRecord(out, &pr3, &err) && pr3.srecAddressBytes == 4
          && pr3.chunks.size() == 1 && pr3.chunks[0].address == 0x08000000 && pr3.chunks[0].data == big, "S3 round-trip");
}

static void testDetect()
{
    CHECK(Formats::detect("a.hex", ":020000040800F2\r\n") == FileFormat::IntelHex, "detect hex");
    CHECK(Formats::detect("a.s19", "S00F000068656C6C6F202020202000003C\n") == FileFormat::SRecord, "detect srec");
    CHECK(Formats::detect("a.hex", QByteArray("\x00\x01\x02", 3)) == FileFormat::Binary, "detect binary");
    CHECK(Formats::detect("a.bin", "hello world") == FileFormat::Binary, "detect text as binary");
}

// slicing-by-8 CRC against the bit-by-bit reference, with random block splits
static void testCrcSlicing()
{
    QRandomGenerator rng(12345);
    bool allOk = true;
    for (const auto &spec : Checksums::crcCatalog()) {
        for (int round = 0; round < 40; ++round) {
            QByteArray buf(int(rng.bounded(0, 600)), Qt::Uninitialized);
            for (char &c : buf)
                c = char(rng.bounded(256));
            const auto *d = reinterpret_cast<const uchar *>(buf.constData());
            const quint32 ref = Checksums::crcBitwise(spec, d, buf.size());
            Checksums::Crc crc(spec);
            qint64 off = 0;
            while (off < buf.size()) {
                const qint64 n = qMin<qint64>(buf.size() - off, rng.bounded(1, 40));
                crc.update(d + off, n);
                off += n;
            }
            if (crc.value() != ref || Checksums::crcCompute(spec, d, buf.size()) != ref) {
                std::printf("  mismatch %s len %d\n", spec.name, int(buf.size()));
                allOk = false;
            }
        }
    }
    CHECK(allOk, "CRC slicing-by-8 == bitwise reference (random data, random blocks)");
}

// multi-threaded computeAll on an odd-sized buffer against independent calculations
static void testComputeAllLarge()
{
    QByteArray buf(5 * 1024 * 1024 + 3, Qt::Uninitialized);
    QRandomGenerator rng(777);
    for (char &c : buf)
        c = char(rng.bounded(256));
    const auto *d = reinterpret_cast<const uchar *>(buf.constData());
    const qint64 n = buf.size();
    std::atomic<qint64> progress{ 0 };
    const auto all = Checksums::computeAll(d, n, nullptr, &progress);
    auto value = [&](const char *name) {
        for (const auto &r : all)
            if (r.name == QLatin1String(name))
                return r.value;
        return QString();
    };
    CHECK(progress.load() == n, "computeAll reports full progress");

    quint64 sum = 0;
    quint8 x = 0;
    quint32 le = 0, be = 0;
    for (qint64 i = 0; i < n; ++i) {
        sum += d[i];
        x ^= d[i];
    }
    for (qint64 i = 0; i + 1 < n; i += 2) {
        le += quint32(d[i]) | (quint32(d[i + 1]) << 8);
        be += (quint32(d[i]) << 8) | quint32(d[i + 1]);
    }
    le += d[n - 1];
    be += quint32(d[n - 1]) << 8;
    auto hx = [](quint64 v, int digits) {
        return QStringLiteral("0x") + QString::number(v, 16).toUpper().rightJustified(digits, QLatin1Char('0'));
    };
    CHECK(value("Sum-32") == hx(sum & 0xFFFFFFFFu, 8), "large: Sum-32");
    CHECK(value("XOR-8") == hx(x, 2), "large: XOR-8");
    CHECK(value("Word Sum-16 (LE)") == hx(le & 0xFFFF, 4) && value("Word Sum-16 (BE)") == hx(be & 0xFFFF, 4),
          "large: word sums with odd tail");
    bool crcOk = true;
    for (const auto &spec : Checksums::crcCatalog())
        crcOk = crcOk && value(spec.name) == hx(Checksums::crcBitwise(spec, d, n), spec.width / 4);
    CHECK(crcOk, "large: all CRCs");
    CHECK(value("SHA-256") == QString::fromLatin1(QCryptographicHash::hash(buf, QCryptographicHash::Sha256).toHex()).toUpper(),
          "large: SHA-256");
    CHECK(value("MD5") == QString::fromLatin1(QCryptographicHash::hash(buf, QCryptographicHash::Md5).toHex()).toUpper(),
          "large: MD5");

    std::atomic<bool> cancel{ true };
    CHECK(Checksums::computeAll(d, n, &cancel, nullptr).isEmpty(), "computeAll can be cancelled");
}

static qint64 naiveIndex(const QByteArray &h, const QByteArray &p, qint64 from)
{
    return h.toLower().indexOf(p.toLower(), from);
}

static void testSearch()
{
    QRandomGenerator rng(4242);
    const char alphabet[] = "aAbB\x00\xFF\xC1\xE1Zz";     // letters, non-letters and Latin-1 look-alikes
    bool fwd = true, back = true, all = true, cs = true;
    for (int round = 0; round < 300; ++round) {
        QByteArray h(int(rng.bounded(0, 200)), Qt::Uninitialized);
        for (char &c : h)
            c = alphabet[rng.bounded(int(sizeof(alphabet) - 1))];
        QByteArray p(int(rng.bounded(1, 5)), Qt::Uninitialized);
        for (char &c : p)
            c = alphabet[rng.bounded(int(sizeof(alphabet) - 1))];
        const qint64 from = rng.bounded(0, int(h.size()) + 2);
        if (Search::indexOf(h, p, from, false) != naiveIndex(h, p, from))
            fwd = false;
        if (Search::indexOf(h, p, from, true) != h.indexOf(p, from))
            cs = false;
        const qint64 bfrom = qint64(rng.bounded(0, int(h.size()) + 1)) - 1;
        const qint64 expBack = bfrom < 0 ? h.toLower().lastIndexOf(p.toLower())
                                         : (bfrom > h.size() - p.size() ? h.toLower().lastIndexOf(p.toLower())
                                                                         : h.toLower().lastIndexOf(p.toLower(), bfrom));
        if (Search::lastIndexOf(h, p, bfrom, false) != expBack)
            back = false;
        QVector<qint64> exp;
        const QByteArray hl = h.toLower(), pl = p.toLower();
        for (qint64 q = hl.indexOf(pl); q >= 0; q = hl.indexOf(pl, q + pl.size()))
            exp.append(q);
        if (Search::findAll(h, p, false) != exp || Search::count(h, p, false) != exp.size())
            all = false;
    }
    CHECK(fwd, "case-insensitive indexOf == lower-case copy search");
    CHECK(back, "case-insensitive lastIndexOf == lower-case copy search");
    CHECK(all, "case-insensitive findAll / count");
    CHECK(cs, "case-sensitive indexOf");
    CHECK(Search::indexOf(QByteArray("\xC1"), QByteArray("\xE1"), 0, false) == -1, "non-ASCII bytes are not case-folded");
    CHECK(Search::indexOf(QByteArray("xxHeLLo"), QByteArray("hello"), 0, false) == 2, "mixed case match");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testCrc();
    testCrcSlicing();
    testComputeAllLarge();
    testSearch();
    testIntelHex();
    testSRecord();
    testDetect();
    std::printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
