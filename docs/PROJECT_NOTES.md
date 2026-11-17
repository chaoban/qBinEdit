# qBinEdit 專案筆記（給後續工作階段）

- 開發者 / 發行者：SAISAI.Dev（aisais.dev@gmail.com）。關於視窗的開發者、信箱、原始碼 / 隱私權網址集中在 `src/appinfo.h`；Logo 原檔 `resources/brand/saisai_logo_src.png`，以 `tools/make_brand.py` 產生淺色 / 深色兩版。
- 版本：1.0.1（2026-11-17）。唯一版本來源是 `CMakeLists.txt` 的 `project(VERSION)`（exe 版本資源與應用程式 manifest 都由它產生）；MSIX 另在 `packaging/msix/identity.json` 的 `Version`（第 4 碼固定 0）。Qt 6.8.3（qtbase，Core/Gui/Widgets），C++17，CMake。
- 授權：MIT（`LICENSE.txt`），原始碼公開於 <https://github.com/chaoban/qBinEdit>。
- 交付位置：使用者電腦 `C:\Users\mm579\OneDrive\microsoft_store\qBinEdit`（原始碼 + dist + packaging + docs）；git 內部資料在 `C:\git\qBinEdit.git`（見 `docs/GITHUB.md`）。
- Windows 二進位檔以 Linux 交叉編譯：Ubuntu 24.04 MinGW-w64 g++ 13（posix threads），Qt 6.8.3 原始碼取自 Ubuntu pool（dfsg 版，缺 wintab → `-DFEATURE_tabletevent=OFF`）。先建主機工具版 qtbase（`-DFEATURE_gui=OFF`，只為 moc/rcc/uic），再建 MinGW 版，關閉 network/sql/dbus/opengl/vulkan/printsupport/concurrent/xml/icu/freetype/jpeg/gif/testlib。之後用 `tools/package_portable.sh` 產生 dist 與 ZIP。
- 架構：`src/core`（不依賴 GUI：formats、checksums、search）→ `Document`（資料、復原、變更追蹤）→ `HexView` → `MainWindow`。搜尋 / 全部取代 / 檢查碼的邏輯都在 core 或 Document，可單元測試。
- 1.0.1 重點：搜尋不複製緩衝區（`Search::`，ASCII 不分大小寫 BMH）；全部取代是單一 `MultiReplaceCommand`（就地或單次搬移，復原含 gap mask）；變更標示在插入/刪除後改用區間追蹤（`Document::ChangeState`，undo 還原快照，存檔後以 epoch 區隔）；檢查碼在 `QThread` 背景計算、最多 3 條執行緒、CRC slicing-by-8；複製為文字直接寫入預配置 QString；重複開檔以 canonical path 不分大小寫比對。
- 可攜版：`qBinEdit.ini` 放在 exe 旁 = 可攜模式（設定寫 INI）；否則寫 `HKCU\Software\qBinEdit\qBinEdit`。MSIX 打包時會移除 ini。
- MSIX：`identity.json` 已填 Partner Center 的值（SAISAI.Dev.qBinEdit / CN=8EE2A838-…）。`build_msix.ps1` 需 Windows SDK（makeappx、makepri、signtool）。
- 測試：`qbe_tests`（CRC 目錄驗證值、slicing-by-8 對照逐位元參考、多執行緒 computeAll、搜尋對照、Intel HEX / S-Record 來回轉換）、`qbe_view_tests`（2/8/16 進位、8/16/32-bit、大小端序輸入與復原、變更標示、全部取代與 gap mask 復原、複製為文字）在 Linux 與 Windows 版（Wine）皆通過。
- 翻譯：英文為原文，`tools/fill_zh_tw.py` 對照表產生 zh_TW；新增 UI 字串時要同步補對照表（lupdate → fill_zh_tw.py → lrelease）。
- 待辦 / 未驗證：在實機 Windows 跑 build_msix.ps1 + WACK；以 Windows 原生外觀重拍商店截圖。
