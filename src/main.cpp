// qBinEdit - firmware binary viewer / editor
#include "appsettings.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("qBinEdit"));
    QCoreApplication::setApplicationName(QStringLiteral("qBinEdit"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QBE_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/qbinedit.png")));

    // ---- language: "auto" follows the system; Traditional Chinese for zh_TW / zh_HK / zh_Hant
    const QString lang = AppSettings::settings().value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString();
    bool chinese = false;
    if (lang == QLatin1String("zh_TW")) {
        chinese = true;
    } else if (lang == QLatin1String("auto")) {
        for (const QString &name : QLocale::system().uiLanguages()) {
            const QString n = name.toLower();
            if (n.startsWith(QLatin1String("zh-tw")) || n.startsWith(QLatin1String("zh-hant"))
                || n.startsWith(QLatin1String("zh-hk")) || n.startsWith(QLatin1String("zh-mo"))) {
                chinese = true;
                break;
            }
            if (n.startsWith(QLatin1String("en")) || n.startsWith(QLatin1String("zh")))
                break;   // first preferred language decides
        }
    }
    QTranslator appTr, qtTr;
    if (chinese) {
        if (appTr.load(QStringLiteral(":/i18n/qbinedit_zh_TW.qm")))
            QCoreApplication::installTranslator(&appTr);
        if (qtTr.load(QStringLiteral(":/i18n/qtbase_zh_TW.qm")))
            QCoreApplication::installTranslator(&qtTr);
        QLocale::setDefault(QLocale(QLocale::Chinese, QLocale::Taiwan));
    }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("qBinEdit - firmware binary viewer / editor"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption radixOpt({ QStringLiteral("r"), QStringLiteral("radix") },
                                QStringLiteral("Display base: 2, 8 or 16."), QStringLiteral("base"));
    QCommandLineOption widthOpt({ QStringLiteral("w"), QStringLiteral("width") },
                                QStringLiteral("Data width in bits: 8, 16 or 32."), QStringLiteral("bits"));
    QCommandLineOption bigOpt(QStringLiteral("big-endian"), QStringLiteral("Show multi-byte units as big endian."));
    QCommandLineOption rawOpt(QStringLiteral("raw"), QStringLiteral("Open files as raw binary (no HEX/S-Record parsing)."));
    QCommandLineOption cmpOpt({ QStringLiteral("c"), QStringLiteral("compare") },
                              QStringLiteral("Compare the two given files side by side."));
    parser.addOptions({ radixOpt, widthOpt, bigOpt, rawOpt, cmpOpt });
    parser.addPositionalArgument(QStringLiteral("files"), QStringLiteral("Files to open."), QStringLiteral("[files...]"));
    parser.process(app);

    MainWindow w;
    w.setInitialViewOptions(parser.isSet(radixOpt) ? parser.value(radixOpt).toInt() : -1,
                            parser.isSet(widthOpt) ? parser.value(widthOpt).toInt() / 8 : -1,
                            parser.isSet(bigOpt) ? 1 : -1);
    w.show();
    const QStringList files = parser.positionalArguments();
    if (parser.isSet(cmpOpt) && files.size() == 2) {
        w.compareFiles(files.at(0), files.at(1));
    } else {
        for (const QString &f : files)
            w.openFile(f, parser.isSet(rawOpt) ? int(FileFormat::Binary) : -1);
    }
    return app.exec();
}
