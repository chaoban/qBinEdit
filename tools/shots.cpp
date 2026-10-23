// qBinEdit - developer tool: render screenshots of typical screens (offscreen).
// Usage: qbe_shots <en|zh> <output-dir> <samples-dir>
#include "aboutdialog.h"
#include "checksumdialog.h"
#include "document.h"
#include "hexview.h"
#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QTimer>
#include <QTranslator>

static HexView *visibleView(QWidget *w)
{
    const auto views = w->findChildren<HexView *>();
    for (HexView *v : views)
        if (v->isVisible())
            return v;
    return nullptr;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qBinEdit"));
    QCoreApplication::setApplicationName(QStringLiteral("qBinEdit-shots"));
    if (argc < 4)
        return 2;
    const bool zh = QString::fromLocal8Bit(argv[1]) == QLatin1String("zh");
    const QDir out(QString::fromLocal8Bit(argv[2]));
    const QDir samples(QString::fromLocal8Bit(argv[3]));
    QTranslator t1, t2;
    if (zh) {
        if (t1.load(QStringLiteral(":/i18n/qbinedit_zh_TW.qm")))
            QCoreApplication::installTranslator(&t1);
        if (t2.load(QStringLiteral(":/i18n/qtbase_zh_TW.qm")))
            QCoreApplication::installTranslator(&t2);
        QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::Taiwan));
    }
    const QString pfx = zh ? QStringLiteral("zh_") : QStringLiteral("en_");

    if (argc > 4 && QString::fromLocal8Bit(argv[4]) == QLatin1String("about")) {
        // About dialog in the light and in a dark palette
        for (int dark = 0; dark < 2; ++dark) {
            if (dark) {
                QApplication::setStyle(QStringLiteral("Fusion"));
                QPalette p;
                p.setColor(QPalette::Window, QColor(0x1C, 0x1F, 0x26));
                p.setColor(QPalette::WindowText, QColor(0xE6, 0xE6, 0xE6));
                p.setColor(QPalette::Base, QColor(0x15, 0x18, 0x1E));
                p.setColor(QPalette::Text, QColor(0xE6, 0xE6, 0xE6));
                p.setColor(QPalette::Button, QColor(0x2A, 0x2E, 0x36));
                p.setColor(QPalette::ButtonText, QColor(0xE6, 0xE6, 0xE6));
                p.setColor(QPalette::Link, QColor(0x8A, 0xB4, 0xF8));
                QApplication::setPalette(p);
            }
            AboutDialog dlg;
            dlg.show();
            for (int i = 0; i < 5; ++i)
                QApplication::processEvents();
            dlg.grab().save(out.filePath(pfx + (dark ? QStringLiteral("about_dark.png") : QStringLiteral("about_light.png"))));
        }
        LicenseDialog lic;
        lic.show();
        for (int i = 0; i < 5; ++i)
            QApplication::processEvents();
        lic.grab().save(out.filePath(pfx + QStringLiteral("licenses.png")));
        return 0;
    }

    MainWindow w;
    w.setInitialViewOptions(16, 1, 0);
    w.resize(1366, 768);
    w.show();
    auto trigger = [&](const QKeySequence &ks) {
        for (QAction *a : w.findChildren<QAction *>())
            if (a->shortcut() == ks) {
                a->trigger();
                return;
            }
    };
    auto shot = [&](QWidget *wd, const QString &name) {
        for (int i = 0; i < 5; ++i)
            QApplication::processEvents();
        wd->grab().save(out.filePath(pfx + name));
    };

    w.openFile(samples.filePath(QStringLiteral("demo_fw.bin")));
    HexView *v = visibleView(&w);
    v->setSelection(0x800, 0x29);
    v->setTopRow(0x7C);
    shot(&w, QStringLiteral("01_hex_8bit.png"));

    trigger(QKeySequence(Qt::CTRL | Qt::Key_1));         // binary
    trigger(QKeySequence(Qt::ALT | Qt::Key_1));          // 8-bit
    v->setCursorPosition(0x10);
    v->setTopRow(0);
    shot(&w, QStringLiteral("02_binary.png"));

    trigger(QKeySequence(Qt::CTRL | Qt::Key_3));         // hex
    trigger(QKeySequence(Qt::ALT | Qt::Key_3));          // 32-bit
    v->setCursorPosition(0x4);
    shot(&w, QStringLiteral("03_hex_32bit.png"));

    trigger(QKeySequence(Qt::CTRL | Qt::Key_2));         // octal
    trigger(QKeySequence(Qt::ALT | Qt::Key_2));          // 16-bit
    shot(&w, QStringLiteral("04_octal_16bit.png"));

    trigger(QKeySequence(Qt::CTRL | Qt::Key_3));
    trigger(QKeySequence(Qt::ALT | Qt::Key_1));
    w.openFile(samples.filePath(QStringLiteral("demo_fw.hex")));
    v = visibleView(&w);
    v->setTopRow(0x1E);
    v->setCursorPosition(0x1F8);
    shot(&w, QStringLiteral("05_intel_hex.png"));

    w.compareFiles(samples.filePath(QStringLiteral("demo_fw.bin")), samples.filePath(QStringLiteral("demo_fw_v2.bin")));
    trigger(QKeySequence(Qt::Key_F8));
    shot(&w, QStringLiteral("06_compare.png"));

    Document doc;
    doc.load(samples.filePath(QStringLiteral("demo_fw.bin")), -1, nullptr, nullptr);
    w.openFile(samples.filePath(QStringLiteral("demo_fw.bin")));
    ChecksumDialog dlg(&doc, 0, 0, &w);
    dlg.resize(640, 660);
    dlg.show();
    for (int i = 0; i < 5; ++i)
        QApplication::processEvents();
    // Store screenshots must be at least 1366x768: draw the dialog over the main window.
    QPixmap bg = w.grab();
    const QPixmap fg = dlg.grab();
    {
        QPainter p(&bg);
        p.fillRect(bg.rect(), QColor(0, 0, 0, 70));
        p.drawPixmap((bg.width() - fg.width()) / 2, (bg.height() - fg.height()) / 2, fg);
    }
    bg.save(out.filePath(pfx + QStringLiteral("07_checksum.png")));
    dlg.close();
    return 0;
}
