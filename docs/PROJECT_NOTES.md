# qBinEdit 專案筆記（給後續工作階段）

- 開發者 / 發行者：SAISAI.Dev（aisais.dev@gmail.com）。關於視窗的開發者、信箱、選填的原始碼 / 隱私權網址集中在 `src/appinfo.h`；Logo 原檔 `resources/brand/saisai_logo_src.png`，以 `tools/make_brand.py` 產生淺色 / 深色兩版。
- 版本：1.0.0（2026-10-22）。Qt 6.8.3（qtbase，Core/Gui/Widgets），C++17，CMake。
- 交付位置：使用者電腦 `C:\Users\USER\OneDrive\qBinEdit`（原始碼 + dist + packaging + docs）。
- Windows 二進位檔以 Linux 交叉編譯：Ubuntu 24.04 MinGW-w64 g++ 13（posix threads），Qt 6.8.3 原始碼取自 Ubuntu pool（dfsg 版，缺 wintab → `-DFEATURE_tabletevent=OFF`），關閉 network/sql/dbus/opengl/printsupport/icu/freetype。
- 可攜版：`qBinEdit.ini` 放在 exe 旁 = 可攜模式（設定寫 INI）；否則寫 `HKCU\Software\qBinEdit\qBinEdit`。MSIX 打包時會移除 ini。
- MSIX：`packaging/msix/identity.json` 目前是佔位值（SAISAIDev.qBinEdit / CN=SAISAIDev，顯示名稱 SAISAI.Dev），需換成 Partner Center Product identity。`build_msix.ps1` 需 Windows SDK（makeappx、makepri、signtool），尚未在實機 Windows 執行驗證。
- 測試：`qbe_tests`（CRC 目錄驗證值、Intel HEX / S-Record 來回轉換）、`qbe_view_tests`（2/8/16 進位、8/16/32-bit、大小端序輸入與復原）在 Linux 與 Windows 版（Wine）皆通過。
- 翻譯：英文為原文，`tools/fill_zh_tw.py` 對照表產生 zh_TW；新增 UI 字串時要同步補對照表。
- 待辦 / 未驗證：在實機 Windows 跑 build_msix.ps1 + WACK；以 Windows 原生外觀重拍商店截圖；LICENSE 目前為「All rights reserved」，若要開源需改。
