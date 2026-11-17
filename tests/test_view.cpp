// qBinEdit - widget-level tests for HexView editing (run with QT_QPA_PLATFORM=offscreen)
#include "dialogs.h"
#include "document.h"
#include "hexview.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QRandomGenerator>
#include <QTemporaryDir>
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

    // ---- change highlighting survives inserts / deletes
    {
        Document d;
        d.setContent(QByteArray::fromHex("00112233445566778899"), 0, QStringLiteral("c"));
        d.setReadOnly(false);
        d.replace(2, 1, QByteArray(1, char(0xAA)), QStringLiteral("ow"));
        CHECK(d.isChangedByte(2) && !d.isChangedByte(3), "overwrite highlighted");
        d.replace(0, 0, QByteArray(3, char(0xEE)), QStringLiteral("ins"));
        CHECK(d.isChangedByte(0) && d.isChangedByte(2) && !d.isChangedByte(3), "inserted bytes highlighted");
        CHECK(d.isChangedByte(5) && !d.isChangedByte(4) && !d.isChangedByte(6), "earlier overwrite still highlighted after insert");
        d.replace(7, 2, QByteArray(), QStringLiteral("del"));
        CHECK(d.isChangedByte(5) && !d.isChangedByte(7), "delete keeps other highlights");
        d.undoStack()->undo();
        d.undoStack()->undo();
        CHECK(d.isChangedByte(2) && !d.isChangedByte(0) && !d.isChangedByte(5), "undo insert restores highlighting");
        d.undoStack()->undo();
        bool none = true;
        for (qint64 i = 0; i < d.size(); ++i)
            none = none && !d.isChangedByte(i);
        CHECK(none, "undo all: nothing highlighted");
        d.undoStack()->redo();
        d.undoStack()->redo();
        CHECK(d.isChangedByte(5) && d.isChangedByte(0), "redo restores highlighting");
    }

    // ---- change tracking against a byte-by-byte model (random edits, undo, redo)
    {
        QRandomGenerator rng(2024);
        bool ok = true;
        for (int round = 0; round < 40 && ok; ++round) {
            Document d;
            d.setContent(QByteArray(int(rng.bounded(20, 80)), char(0x55)), 0, QStringLiteral("m"));
            d.setReadOnly(false);
            struct State { QByteArray data; QVector<bool> marks; };
            QVector<State> undo, redo;
            State cur{ d.data(), QVector<bool>(d.size(), false) };
            auto apply = [&](const QVector<qint64> &pos, qint64 rem, const QByteArray &ins) {
                // model: same edit, inserted bytes marked
                State n = cur;
                for (int i = pos.size() - 1; i >= 0; --i) {
                    n.data.replace(pos[i], rem, ins);
                    n.marks.remove(pos[i], rem);
                    for (int k = 0; k < ins.size(); ++k)
                        n.marks.insert(pos[i], true);
                }
                undo.append(cur);
                redo.clear();
                cur = n;
            };
            // start with an insert so offsets are shifted and ranges are what the view uses
            d.replace(0, 0, QByteArray(1, char(0x01)), QStringLiteral("i"));
            apply({ 0 }, 0, QByteArray(1, char(0x01)));
            undo.clear();
            for (int step = 0; step < 25 && ok; ++step) {
                const int op = int(rng.bounded(5));
                const qint64 size = d.size();
                if (op == 3 && !undo.isEmpty()) {
                    d.undoStack()->undo();
                    redo.append(cur);
                    cur = undo.takeLast();
                } else if (op == 4 && !redo.isEmpty()) {
                    d.undoStack()->redo();
                    undo.append(cur);
                    cur = redo.takeLast();
                } else if (op == 2 && size > 8) {
                    const qint64 rem = rng.bounded(1, 3);
                    const QByteArray ins(int(rng.bounded(0, 4)), char(0xA0 + step));
                    QVector<qint64> pos;
                    for (qint64 p = rng.bounded(0, 4); p + rem <= size; p += rem + rng.bounded(0, 9))
                        pos.append(p);
                    if (pos.isEmpty())
                        continue;
                    if (d.replaceAll(pos, rem, ins, QStringLiteral("all")) > 0)
                        apply(pos, rem, ins);
                } else {
                    const qint64 p = rng.bounded(0, int(size) + 1);
                    const qint64 rem = qMin<qint64>(rng.bounded(0, 4), size - p);
                    const QByteArray ins(int(rng.bounded(rem == 0 ? 1 : 0, 4)), char(0x10 + step));
                    if (d.data().mid(p, rem) == ins)
                        continue;
                    d.replace(p, rem, ins, QStringLiteral("r"));
                    apply({ p }, rem, ins);
                }
                ok = d.data() == cur.data;
                if (ok && d.changeState().shifted)
                    for (qint64 i = 0; i < d.size() && ok; ++i)
                        ok = d.isChangedByte(i) == cur.marks[i];
                if (!ok)
                    std::printf("  model mismatch round %d step %d op %d\n", round, step, op);
            }
        }
        CHECK(ok, "change tracking matches byte model through edits, undo and redo");
    }

    // ---- Replace All: one undo step, in-place and size-changing, exact undo
    {
        QRandomGenerator rng(99);
        bool ok = true;
        for (int round = 0; round < 60 && ok; ++round) {
            QByteArray base(int(rng.bounded(1, 300)), Qt::Uninitialized);
            for (char &c : base)
                c = "AB\x01"[rng.bounded(3)];
            const QByteArray pat = round % 2 ? QByteArray("AB") : QByteArray("A");
            const QByteArray repl = QByteArray("XYZ").left(int(rng.bounded(0, 4)));
            Document d;
            d.setContent(base, 0, QStringLiteral("r"));
            d.setReadOnly(false);
            QVector<qint64> hits;
            for (qint64 p = base.indexOf(pat); p >= 0; p = base.indexOf(pat, p + pat.size()))
                hits.append(p);
            QByteArray expect = base;
            for (int i = hits.size() - 1; i >= 0; --i)
                expect.replace(hits[i], pat.size(), repl);
            const int before = d.undoStack()->count();
            d.replaceAll(hits, pat.size(), repl, QStringLiteral("all"));
            ok = ok && d.data() == expect && d.undoStack()->count() == before + (hits.isEmpty() ? 0 : 1);
            if (!hits.isEmpty())
                d.undoStack()->undo();
            ok = ok && d.data() == base;
        }
        CHECK(ok, "replaceAll == sequential replace; single undo restores (random)");
    }

    // ---- Replace All on an Intel HEX image keeps the gap mask on undo; save resets highlights
    {
        QTemporaryDir tmp;
        const QString hexPath = QDir(tmp.path()).filePath(QStringLiteral("g.hex"));
        QFile f(hexPath);
        f.open(QIODevice::WriteOnly);
        f.write(":04000000AABBAABB32\n:04001000AABBAABB22\n:00000001FF\n");
        f.close();
        Document d;
        QString err;
        CHECK(d.load(hexPath, -1, &err, nullptr) && d.size() == 0x14 && d.hasGaps(), "load HEX with gap");
        const QByteArray mask0 = d.gapMask(), data0 = d.data();
        d.setReadOnly(false);
        QVector<qint64> hits = { 0, 2, 0x10, 0x12 };
        d.replaceAll(hits, 2, QByteArray(1, char(0xCC)), QStringLiteral("shrink"));
        CHECK(d.size() == 0x10 && d.isGap(4) && !d.isGap(0), "gap mask follows size change");
        d.undoStack()->undo();
        CHECK(d.data() == data0 && d.gapMask() == mask0, "undo restores data and gap mask");
        d.replace(0, 1, QByteArray(1, char(0x11)), QStringLiteral("ow"));
        const QString outPath = QDir(tmp.path()).filePath(QStringLiteral("o.bin"));
        CHECK(d.save(outPath, FileFormat::Binary, &err), "save");
        CHECK(!d.isChangedByte(0), "no highlight right after save");
        d.undoStack()->undo();
        CHECK(d.isChangedByte(0), "undo after save highlights the byte that now differs from disk");
    }

    // ---- copy-as text helpers
    {
        const QByteArray b = QByteArray::fromHex("00FF10A5") + QByteArray(14, char(0x7E));
        CHECK(toHexText(QByteArray::fromHex("DEADBEEF"), 16) == QLatin1String("DE AD BE EF"), "toHexText");
        CHECK(toHexText(b, 16).count(QLatin1Char('\n')) == 1 && toHexText(QByteArray(), 16).isEmpty(), "toHexText lines");
        CHECK(toBinaryText(QByteArray::fromHex("A501")) == QLatin1String("10100101 00000001"), "toBinaryText");
        CHECK(toCArray(QByteArray::fromHex("0102")) == QLatin1String("const unsigned char data[2] = {\n    0x01, 0x02\n};\n"),
              "toCArray short");
        const QString c = toCArray(b);
        CHECK(c.startsWith(QLatin1String("const unsigned char data[18] = {\n    0x00, 0xFF, 0x10, 0xA5,"))
                  && c.endsWith(QLatin1String("0x7E,\n    0x7E, 0x7E\n};\n")),
              "toCArray two lines");
    }

    std::printf("\n%s (%d failure%s)\n", g_fail ? "FAILED" : "ALL PASSED", g_fail, g_fail == 1 ? "" : "s");
    return g_fail ? 1 : 0;
}
