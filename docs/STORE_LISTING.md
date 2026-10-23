# qBinEdit — Microsoft Store 上架文案範本

> 用法：依 Partner Center 的頁面順序，把下列內容貼到對應欄位。`【 】` 內是要換成你自己資料的地方。
> 字數上限依 Microsoft Learn 文件（2026-10 查核），實際以 Partner Center 輸入框的提示為準。
> 因為 MSIX 宣告了 `en-US` 與 `zh-TW` 兩種語言，兩種語言的商店頁都要填（本文件兩種都已附上）。

---

## 0. 上架前準備（一次性）

| 項目 | 做法 |
|---|---|
| 開發者帳號 | 個人開發者註冊免費：<https://storedeveloper.microsoft.com/home>，用個人 Microsoft 帳號登入，並以政府核發證件＋自拍完成身分驗證。帳號的發行者顯示名稱（Publisher display name）設為 **SAISAI.Dev**，與 App「關於」視窗一致。 |
| 保留 App 名稱 | Partner Center → Apps and games → New product → MSIX or PWA app → 輸入 `qBinEdit`（被用走時的備案：`qBinEdit Firmware Viewer`）。 |
| 取得套件識別 | 產品頁 → Product management → **Product identity**，把下面三個值抄進 `packaging/msix/identity.json`，再重新執行 `build_msix.ps1`：<br>`Package/Identity/Name` → `IdentityName`<br>`Package/Identity/Publisher` → `Publisher`（格式 `CN=XXXXXXXX-XXXX-...`）<br>`Package/Properties/PublisherDisplayName` → `PublisherDisplayName` |
| 隱私權政策網址 | `https://github.com/chaoban/qBinEdit/blob/main/PRIVACY.md`（repo 推上 GitHub 後生效；也可直接貼 `PRIVACY.md` 的文字）。 |

---

## 1. Pricing and availability（價格與上市）

| 欄位 | 建議值 |
|---|---|
| Markets | All markets（預設） |
| Audience / Visibility | Public audience；Make this product available and discoverable in the Store |
| Schedule | As soon as possible（或選 Manual 自己按發佈） |
| Base price | Free（免費）—若要收費改填價格，並考慮開 Free trial |

## 2. Properties（屬性）

| 欄位 | 建議值 |
|---|---|
| Category | **Developer tools**（開發人員工具）；若有 Subcategory 可選 Utilities，或改用 Category = Utilities & tools |
| Privacy policy URL | `https://github.com/chaoban/qBinEdit/blob/main/PRIVACY.md` |
| Website | `https://github.com/chaoban/qBinEdit` |
| Support contact info | aisais.dev@gmail.com |
| Product declarations | ☐ Purchases outside Store commerce：不勾<br>☑ Customers can install this app to alternate drives or removable storage：勾<br>☑ Windows can include this app's data in automatic backups to OneDrive：可勾<br>☐ Depends on non-Microsoft drivers or NT services：不勾<br>☐ Tested to meet accessibility guidelines：自行完成無障礙測試後才勾 |
| System requirements | Minimum：Keyboard、Mouse；其餘不勾 |

## 3. Age ratings（IARC 分級問卷）

- 類別選：**Utility, productivity, communication, or other**（非遊戲）。
- 所有內容題（暴力、性、髒話、毒品、賭博、恐怖）：**No**。
- 使用者互動 / 分享內容：**No**。分享位置：**No**。數位購買：**No**。不受限制的網路存取（瀏覽器/搜尋）：**No**。
- 預期結果：IARC 3+（ESRB Everyone、PEGI 3 等，實際以問卷結果為準）。

## 4. Packages（套件）

- 上傳 `dist/qBinEdit_1.0.0.0_x64.msix`（由 `build_msix.ps1` 產生、**不需簽章**，商店會重新簽章）。
- Device family：勾 **Windows 10/11 Desktop**。
- 之後每次更新：`identity.json` 的 `Version` 往上加（第 4 碼固定為 0，例如 `1.0.1.0`）。

## 5. Store listings — 繁體中文（中文 (台灣) / zh-TW）

### Product name
```
qBinEdit
```

### Description（必填，上限 10,000 字元，純文字）
```
qBinEdit 是給韌體與嵌入式工程師用的二進位檔檢視／編輯工具。開啟韌體映像後，可隨時切換用二進位（2 進位）、八進位（8 進位）或十六進位（16 進位）顯示，並以 8-bit、16-bit 或 32-bit 為單位、搭配小端序或大端序閱讀，暫存器值、校正表、查找表一眼就能看懂。

除了原始 .bin 檔，qBinEdit 也能直接開啟 Intel HEX（.hex）與 Motorola S-Record（.s19 / .s28 / .s37 / .srec / .mot），依記錄中的位址展開成完整映像，位址欄直接顯示實際位址；檔案中沒有資料的空隙會以灰底標示。編輯完成後可存回原格式，或匯出成 .bin / HEX / S-Record。

為了避免誤改韌體，檔案預設以唯讀開啟；需要修改時按 Ctrl+E 進入編輯模式，所有修改都可以復原／取消復原，並以紅色標示改過的位元組。

主要功能
• 2 / 8 / 16 進位顯示，8 / 16 / 32-bit 單位，小端序／大端序切換
• 直接開啟並存回 Intel HEX 與 Motorola S-Record，可轉存為 .bin
• 檢查碼與 CRC：Sum-8/16/32、2 的補數、XOR、CRC-8、CRC-16（CCITT-FALSE、XMODEM、MODBUS、KERMIT、ARC）、CRC-32（zip、MPEG-2/STM32、BZIP2、CRC-32C）、Adler-32、MD5、SHA-1、SHA-256，可針對整檔或選取範圍計算
• 資料檢視器：游標位置即時解讀為 int8～int64、float、double、二進位與字元
• 兩檔比較：左右並排、差異標色、差異清單，F8 跳到下一個差異
• 搜尋與取代：十六進位位元組、ASCII 或 UTF-16 文字
• 跳至偏移或位址、選取範圍、填入、插入、刪除、複製為十六進位文字或 C 陣列
• 繁體中文／英文介面，支援深色模式與高 DPI 螢幕

qBinEdit 完全在本機運作，不需帳號、不連網、不蒐集任何資料。
```

### What's new in this version（上限 1,500 字元；第一次上架可留空）
```
1.0.0：第一版。
```

### Product features（最多 20 項，每項 ≤ 200 字元，不要自己加項目符號）
```
二進位、八進位、十六進位顯示，可隨時切換
8 / 16 / 32-bit 單位，小端序與大端序
直接開啟、編輯並存回 Intel HEX 與 Motorola S-Record
空隙以灰底標示，位址欄顯示實際載入位址
CRC-8 / CRC-16 / CRC-32（含 STM32 硬體 CRC）、Sum、XOR、MD5、SHA-256
資料檢視器：int8～int64、float、double 即時解讀
兩個韌體檔並排比較，差異標色並可逐一跳轉
十六進位、ASCII、UTF-16 搜尋與取代
預設唯讀，Ctrl+E 進入編輯，可無限次復原
複製為十六進位文字、二進位文字或 C 陣列
支援 1 GB 以內的檔案，拖放即可開啟
完全離線，不蒐集任何資料
```

### Screenshots（必填至少 1 張；PNG、至少 1366×768、最多 10 張；說明 ≤ 200 字元）

| 檔案（`packaging/store/screenshots/`） | 說明文字 |
|---|---|
| `zh_01_hex_8bit.png` | 十六進位 8-bit 顯示，右側資料檢視器即時解讀游標位置 |
| `zh_02_binary.png` | 切換成二進位顯示，逐位元檢查暫存器與旗標 |
| `zh_03_hex_32bit.png` | 32-bit 單位顯示向量表與指標，可切換大小端序 |
| `zh_04_octal_16bit.png` | 八進位 16-bit 顯示 |
| `zh_05_intel_hex.png` | 直接開啟 Intel HEX，依位址展開並標示空隙 |
| `zh_06_compare.png` | 兩個韌體版本並排比較，差異清單逐一跳轉 |
| `zh_07_checksum.png` | 一次算出 Sum、CRC-16、CRC-32、SHA-256 等檢查碼 |

> 附的截圖是在雲端環境以 Fusion 樣式產生的草稿（1366×768）。正式上架建議在 Windows 上開啟 qBinEdit、視窗調成 1366×768 以上後重新截圖，畫面會是 Windows 11 原生外觀。

### Store logos
- 1:1 App tile icon 300×300：`packaging/store/StoreLogo_300x300.png`（建議上傳）

### Supplemental fields（選填）
| 欄位 | 內容 |
|---|---|
| Short title（≤ 50） | `qBinEdit` |
| Short description（≤ 1,000，建議 ≤ 270） | `給韌體工程師的二進位檔檢視／編輯工具：2／8／16 進位顯示，8／16／32-bit 與大小端序，直接開啟 Intel HEX 與 S-Record，內建 CRC／檢查碼與兩檔比較。` |

### Search terms（最多 7 個，每個 ≤ 30 字元，合計 ≤ 21 個字）
```
十六進位編輯器
韌體
二進位檢視器
Intel HEX
S-Record
CRC
hex editor
```

### Copyright and trademark info（≤ 200 字元）
```
© 2026 SAISAI.Dev
```

### Additional license terms
```
qBinEdit 為開放原始碼軟體，依 MIT 授權發布：https://github.com/chaoban/qBinEdit 。本軟體以 Qt 6（GNU LGPL v3）動態連結建置。Qt 原始碼：https://download.qt.io/archive/qt/6.8/6.8.3/ 。隨附的 Qt 函式庫依 LGPL v3 授權；即使 Microsoft 標準應用程式授權條款另有規定，您仍可依 LGPL v3 替換或修改這些函式庫，並為除錯此類修改而對本軟體進行反向工程。第三方元件授權詳見程式資料夾中的 THIRD-PARTY-NOTICES.txt，或程式內「關於 → 授權資訊」。
```

### Developed by
```
SAISAI.Dev
```

### Additional system requirements（各最多 11 項，每項 ≤ 200 字元）
- Minimum hardware：
  ```
  Windows 10 版本 1809（64 位元）或更新版本
  x64 處理器
  4 GB 記憶體
  1366×768 解析度
  ```
- Recommended hardware：
  ```
  Windows 11（64 位元）
  8 GB 記憶體（開啟 256 MB 以上檔案時）
  1920×1080 解析度
  ```

---

## 6. Store listings — English (United States / en-US)

### Product name
```
qBinEdit
```

### Description
```
qBinEdit is a binary file viewer and editor made for firmware and embedded engineers. Open a firmware image and switch the display between binary (base 2), octal (base 8) and hexadecimal (base 16) at any time, grouped as 8-bit, 16-bit or 32-bit units in little-endian or big-endian order, so register values, calibration tables and lookup tables are easy to read.

Besides raw .bin files, qBinEdit opens Intel HEX (.hex) and Motorola S-Record (.s19 / .s28 / .s37 / .srec / .mot) directly. Records are expanded into one image at their real load addresses, and address gaps are shaded. Save back to the original format, or export to .bin, Intel HEX or S-Record.

To protect firmware images, files open read-only. Press Ctrl+E to enter edit mode; every change can be undone and modified bytes are highlighted in red.

Key features
• Binary / octal / hexadecimal display, 8 / 16 / 32-bit units, little or big endian
• Open, edit and save Intel HEX and Motorola S-Record; convert to .bin
• Checksums and CRCs: Sum-8/16/32, two's complement, XOR, CRC-8, CRC-16 (CCITT-FALSE, XMODEM, MODBUS, KERMIT, ARC), CRC-32 (zip, MPEG-2/STM32, BZIP2, CRC-32C), Adler-32, MD5, SHA-1, SHA-256 over the whole file or a selection
• Data inspector: the bytes at the cursor as int8 to int64, float, double, binary and character
• Compare two files side by side with highlighted differences and a difference list; F8 jumps to the next one
• Find and replace hex bytes, ASCII or UTF-16 text
• Go to offset or address, select range, fill, insert, delete, copy as hex text or C array
• English and Traditional Chinese user interface, dark mode and high-DPI support

qBinEdit works entirely offline. No account, no network access, no data collection.
```

### What's new in this version
```
1.0.0: First release.
```

### Product features
```
Binary, octal and hexadecimal display - switch at any time
8 / 16 / 32-bit units in little or big endian
Open, edit and save Intel HEX and Motorola S-Record files
Real load addresses in the address column, with shaded gaps
CRC-8 / CRC-16 / CRC-32 (incl. STM32 hardware CRC), sums, XOR, MD5, SHA-256
Data inspector for int8 to int64, float and double
Side-by-side compare of two firmware images with a difference list
Find and replace hex bytes, ASCII or UTF-16 text
Read-only by default; Ctrl+E to edit, unlimited undo
Copy as hex text, binary text or C array
Files up to 1 GB, drag and drop to open
Fully offline - no data collection
```

### Screenshot captions
| File | Caption |
|---|---|
| `en_01_hex_8bit.png` | Hexadecimal 8-bit view with the data inspector decoding the cursor position |
| `en_02_binary.png` | Binary view for checking registers and flags bit by bit |
| `en_03_hex_32bit.png` | 32-bit units for vector tables and pointers, little or big endian |
| `en_04_octal_16bit.png` | Octal 16-bit view |
| `en_05_intel_hex.png` | Intel HEX opened at its real addresses, with gaps shaded |
| `en_06_compare.png` | Compare two firmware versions side by side and step through the differences |
| `en_07_checksum.png` | Sum, CRC-16, CRC-32, SHA-256 and more in one click |

### Short description
```
Binary viewer/editor for firmware engineers: binary, octal or hex display with 8/16/32-bit units and either byte order, Intel HEX and S-Record support, CRC/checksums and file compare.
```

### Search terms
```
hex editor
firmware
binary viewer
Intel HEX
S-Record
CRC checksum
embedded
```

### Copyright and trademark info
```
© 2026 SAISAI.Dev
```

### Developed by
```
SAISAI.Dev
```

### Additional license terms
```
qBinEdit is open source under the MIT License: https://github.com/chaoban/qBinEdit . Built with Qt 6, dynamically linked under the GNU LGPL v3. Qt source code: https://download.qt.io/archive/qt/6.8/6.8.3/ . The included Qt libraries are licensed under the LGPL v3; notwithstanding the Microsoft Standard Application License Terms, you may replace or modify them and reverse engineer the app as far as needed to debug such modifications. See THIRD-PARTY-NOTICES.txt in the application folder, or About > License Information in the app, for all third-party licenses.
```

### Additional system requirements
- Minimum: `Windows 10 version 1809 (64-bit) or later` / `x64 processor` / `4 GB RAM` / `1366x768 display`
- Recommended: `Windows 11 (64-bit)` / `8 GB RAM for files over 256 MB` / `1920x1080 display`

---

## 7. Submission options（送審選項）

### Notes for certification（給審核人員的說明）
```
qBinEdit is an offline desktop utility (C++ / Qt 6) for viewing and editing firmware binary files. It needs no account, makes no network connections and has no in-app purchases.

How to test:
1. Start qBinEdit and choose File > Open. Any file can be opened, for example C:\Windows\System32\notepad.exe.
2. Use the "Display" box on the toolbar (or Ctrl+1 / Ctrl+2 / Ctrl+3) to switch between binary, octal and hexadecimal; use "Width" (Alt+1 / Alt+2 / Alt+3) for 8/16/32-bit units.
3. Tools > Checksum / CRC calculates checksums of the file.
4. Tools > Compare With File compares the open file with a second file.
5. Files open read-only; press Ctrl+E to enable editing, type hex digits to change bytes, Ctrl+Z to undo.
```

### Restricted capabilities — runFullTrust（必填，因為 Manifest 宣告了 runFullTrust）
```
qBinEdit is a Win32 desktop application (C++ / Qt 6) packaged as MSIX. The runFullTrust capability is required for every packaged Win32 desktop app so that its executable runs as a full-trust desktop process. qBinEdit uses it only to run its own executable and to read and write files that the user explicitly opens or saves through the standard Windows file dialogs. It installs no drivers or services and makes no network connections.
```

---

## 8. 送審前檢查清單

- [ ] `identity.json` 已換成 Partner Center 的 Product identity 值，並重新產生 `.msix`
- [ ] `Version` 第 4 碼為 0，且大於上一次上架的版本
- [ ] 已用 `run_wack.ps1` 跑過 Windows App Certification Kit，結果 PASS
- [ ] 中、英文商店頁的 Description 與至少 1 張截圖都已填好
- [ ] 已填 runFullTrust 的理由與審核說明
- [ ] 已在乾淨的電腦（或新使用者帳號）用 `install_test.ps1` 實際安裝並測過
