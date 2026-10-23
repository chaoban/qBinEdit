// qBinEdit - application settings (registry, or INI next to the exe in portable mode)
#pragma once

#include <QSettings>

namespace AppSettings {

// Portable mode is active when "qBinEdit.ini" exists next to the executable.
bool isPortable();
QSettings &settings();

} // namespace AppSettings
