// qBinEdit - widget-level tests for HexView editing (run with QT_QPA_PLATFORM=offscreen)
#include "document.h"
#include "hexview.h"

#include <QApplication>
#include <QKeyEvent>
#include <QUndoStack>
#include <cstdio>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s  (%s:%d)\n", msg, __FILE__, __LINE__); ++g_fail; } else { std::printf("ok   %s\n", msg); } } while (0)

static void key(QWidget *w, int k, Qt::KeyboardModifiers m = Qt::NoModifier, const QString &text = QString())
{
    QKeyEvent press(QEvent::KeyPress, k, m, text);
    QApplication::sendEvent(w, &press);
}

static void type(QWidget *w, const QString &s)
{
    for (QChar c : s)
        key(w, 0, Qt::NoModifier, QString(c));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    Document doc;
    doc.setContent(QByteArray::fromHex("00112233445566778899AABBCCDDEEFF"), 0, QStringLiteral("t"));
    HexView v;
    v.resize(900, 400);
    v.setDocument(&doc);
    v.show();

    // read-only by default for setContent? (setContent keeps current flag; Document defaults to read-only)
    type(&v, QStringLiteral("A"));
    CHECK(doc.data().at(0) == 0x00, "read-only blocks typing");

    doc.setReadOnly(false);
    // hex 8-bit: type "AB" at offset 0
    v.setCursorPosition(0);
    type(&v, QStringLiteral("ab"));
    CHECK(uchar(doc.data().at(0)) == 0xAB && v.cursorPosition() == 1, "hex 8-bit typing");
    CHECK(doc.undoStack()->count() == 1, "typing merged into one undo step");
    doc.undoStack()->undo();
    CHECK(uchar(doc.data().at(0)) == 0x00, "undo restores byte");

    // 16-bit little endian: unit at offset 2 is 0x3322; type "BEEF" -> bytes EF BE
    v.setUnitSize(2);
    v.setCursorPosition(2);
    type(&v, QStringLiteral("BEEF"));
    CHECK(uchar(doc.data().at(2)) == 0xEF && uchar(doc.data().at(3)) == 0xBE, "16-bit LE typing");
    CHECK(v.cursorPosition() == 4, "cursor advances to next 16-bit unit");

    // 16-bit big endian
    v.setBigEndian(true);
    v.setCursorPosition(4);
    type(&v, QStringLiteral("1234"));
    CHECK(uchar(doc.data().at(4)) == 0x12 && uchar(doc.data().at(5)) == 0x34, "16-bit BE typing");
    CHECK(v.unitValue(4) == 0x1234, "unitValue BE");
    v.setBigEndian(false);
    CHECK(v.unitValue(4) == 0x3412, "unitValue LE");

    // binary 8-bit: 8 digits
    v.setUnitSize(1);
    v.setRadix(2);
    v.setCursorPosition(6);
    type(&v, QStringLiteral("10100101"));
    CHECK(uchar(doc.data().at(6)) == 0xA5, "binary typing");
    type(&v, QStringLiteral("2"));        // invalid digit in base 2 -> ignored
    CHECK(uchar(doc.data().at(7)) == 0x77, "digit 2 ignored in binary");

    // octal 8-bit: 3 digits, top digit max 3
    v.setRadix(8);
    v.setCursorPosition(8);
    type(&v, QStringLiteral("377"));
    CHECK(uchar(doc.data().at(8)) == 0xFF, "octal typing 377");
    v.setCursorPosition(9);
    type(&v, QStringLiteral("7"));         // 7xx > 255 -> rejected
    CHECK(uchar(doc.data().at(9)) == 0x99, "octal out-of-range rejected");
    CHECK(v.formatValue(0xFF, 1, 8) == QLatin1String("377"), "octal format");
    CHECK(v.formatValue(0x5, 1, 2) == QLatin1String("00000101"), "binary format");
    CHECK(v.formatValue(0xBEEF, 2, 16) == QLatin1String("BEEF"), "hex 16 format");
    CHECK(v.formatValue(0xFFFF, 2, 8) == QLatin1String("177777"), "octal 16 format");

    // append at end of file in edit mode
    v.setRadix(16);
    v.setCursorPosition(doc.size());
    CHECK(v.cursorPosition() == 16, "cursor may sit at EOF in edit mode");
    type(&v, QStringLiteral("CD"));
    CHECK(doc.size() == 17 && uchar(doc.data().at(16)) == 0xCD, "typing at EOF appends");
    doc.undoStack()->undo();
    CHECK(doc.size() == 16, "undo append");

    // selection + navigation
    v.setCursorPosition(0);
    key(&v, Qt::Key_Right, Qt::ShiftModifier);
    key(&v, Qt::Key_Right, Qt::ShiftModifier);
    CHECK(v.hasSelection() && v.selectionStart() == 0 && v.selectionLength() == 3, "shift+right selects");
    key(&v, Qt::Key_End, Qt::ControlModifier);
    CHECK(!v.hasSelection() && v.cursorPosition() == 16, "ctrl+end goes to EOF");

    // ASCII pane typing
    v.setCursorPosition(0);
    key(&v, Qt::Key_Tab);
    CHECK(v.asciiPaneActive(), "tab switches to ASCII pane");
    type(&v, QStringLiteral("Hi"));
    CHECK(doc.data().left(2) == QByteArray("Hi"), "ASCII typing");

    // replace/insert/delete through the document
    doc.replace(0, 0, QByteArray(4, char(0xEE)), QStringLiteral("ins"));
    CHECK(doc.size() == 20 && uchar(doc.data().at(0)) == 0xEE, "insert");
    doc.replace(0, 4, QByteArray(), QStringLiteral("del"));
    CHECK(doc.size() == 16 && doc.data().left(2) == QByteArray("Hi"), "delete");
    while (doc.undoStack()->canUndo())
        doc.undoStack()->undo();
    CHECK(doc.data() == QByteArray::fromHex("00112233445566778899AABBCCDDEEFF"), "undo all restores original");
    CHECK(!doc.isModified(), "clean after undo all");

    std::printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
