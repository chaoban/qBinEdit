// qBinEdit - application settings
#include "appsettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <memory>

namespace AppSettings {

namespace {
QString portableIniPath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("qBinEdit.ini"));
}
} // namespace

bool isPortable()
{
    static const bool portable = QFileInfo::exists(portableIniPath());
    return portable;
}

QSettings &settings()
{
    static std::unique_ptr<QSettings> s;
    if (!s) {
        if (isPortable())
            s = std::make_unique<QSettings>(portableIniPath(), QSettings::IniFormat);
        else
            s = std::make_unique<QSettings>();   // HKCU\Software\<org>\<app>
    }
    return *s;
}

} // namespace AppSettings
