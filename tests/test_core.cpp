// qBinEdit - console unit tests for checksums and firmware formats
#include "checksums.h"
#include "formats.h"

#include <QCoreApplication>
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

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    testCrc();
    testIntelHex();
    testSRecord();
    testDetect();
    std::printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
