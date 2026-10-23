// qBinEdit - firmware file formats: Intel HEX and Motorola S-Record
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

enum class FileFormat { Binary, IntelHex, SRecord };

namespace Formats {

struct Chunk {
    quint64 address = 0;
    QByteArray data;
};

struct ParseResult {
    QVector<Chunk> chunks;          // sorted, merged, non-overlapping
    bool hasStart = false;
    quint64 startAddress = 0;
    int startRecordType = 5;        // Intel HEX: 3 or 5
    QByteArray srecHeader;          // S0 payload
    int srecAddressBytes = 0;       // 2 (S1), 3 (S2), 4 (S3)
    bool crlf = true;
    int recordCount = 0;
    QStringList warnings;
};

struct WriteOptions {
    int bytesPerRecord = 16;
    bool crlf = true;
    bool hasStart = false;
    quint64 startAddress = 0;
    int startRecordType = 5;        // Intel HEX
    QByteArray srecHeader;          // S-Record S0
    int srecAddressBytes = 0;       // 0 = automatic
};

// Guess the format from file name and first bytes.
FileFormat detect(const QString &fileName, const QByteArray &head);

bool parseIntelHex(const QByteArray &text, ParseResult *out, QString *error);
bool parseSRecord(const QByteArray &text, ParseResult *out, QString *error);

// Flatten chunks into one contiguous image starting at the lowest address.
// gapMask gets one byte per image byte: 1 = not present in the file (filled).
bool flatten(const ParseResult &pr, quint8 fill, qint64 maxSpan,
             QByteArray *image, quint64 *baseAddress, QByteArray *gapMask, QString *error);

// Writers skip bytes that are gaps (gapMask==1) *and* still equal to the fill value.
bool writeIntelHex(const QByteArray &image, quint64 baseAddress, const QByteArray &gapMask,
                   quint8 fill, const WriteOptions &opt, QByteArray *out, QString *error);
bool writeSRecord(const QByteArray &image, quint64 baseAddress, const QByteArray &gapMask,
                  quint8 fill, const WriteOptions &opt, QByteArray *out, QString *error);

QString formatName(FileFormat f);

} // namespace Formats
