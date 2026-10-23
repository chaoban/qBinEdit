qBinEdit - Firmware Binary Viewer / Editor (portable version)
==============================================================

[繁體中文]
免安裝版：解壓縮到任何資料夾（可放在隨身碟），直接執行 qBinEdit.exe。
- 設定檔存放在同一資料夾的 qBinEdit.ini（可攜模式）。
  刪除 qBinEdit.ini 則改用登錄檔 HKCU\Software\qBinEdit。
- 移除：關閉程式後直接刪除整個資料夾即可，不會留下系統檔案。
- 第一次執行若出現 Windows SmartScreen「已保護您的電腦」，
  點「其他資訊」>「仍要執行」（此版本未經程式碼簽章）。
- 命令列：qBinEdit.exe [--radix 2|8|16] [--width 8|16|32] [--big-endian]
          [--raw] [--compare 檔案A 檔案B] [檔案...]

[English]
Unzip anywhere (a USB stick works) and run qBinEdit.exe - no installation.
- Settings are stored in qBinEdit.ini next to the exe (portable mode).
  Delete qBinEdit.ini to use the registry (HKCU\Software\qBinEdit) instead.
- To remove: close the app and delete the folder.
- If Windows SmartScreen shows "Windows protected your PC", choose
  "More info" > "Run anyway" (this build is not code-signed).
- Command line: qBinEdit.exe [--radix 2|8|16] [--width 8|16|32] [--big-endian]
                [--raw] [--compare fileA fileB] [files...]

Keyboard
  Ctrl+O open        Ctrl+S save         Ctrl+E edit mode on/off
  Ctrl+1/2/3  binary / octal / hex       Alt+1/2/3  8 / 16 / 32-bit
  Ctrl+F find        Ctrl+H replace      F3 / Shift+F3 next / previous
  Ctrl+G go to       Ctrl+K checksum/CRC Ctrl+D compare, F8 next difference
  Tab  switch between data and ASCII column     Ctrl+wheel  zoom

License: MIT (LICENSE.txt). Third-party components: THIRD-PARTY-NOTICES.txt.
Source code / 原始碼: https://github.com/chaoban/qBinEdit
Contact / 聯絡: aisais.dev@gmail.com (SAISAI.Dev)
