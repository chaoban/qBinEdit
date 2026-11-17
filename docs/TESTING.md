# qBinEdit 本機測試與移除方式

有三種測試方式，由簡單到接近商店實際安裝：

| 方式 | 需要 | 適合 |
|---|---|---|
| A. 可攜版 ZIP | 無 | 日常使用、快速測功能 |
| B. MSIX「註冊」安裝 | 開發人員模式、Windows SDK | 測 MSIX 行為（開始功能表、檔案關聯），不需憑證 |
| C. MSIX 自簽憑證安裝 | 系統管理員權限、Windows SDK | 最接近從 Microsoft Store 安裝的情況 |

以下指令都在 **PowerShell** 中執行。`<repo>` 指 qBinEdit 資料夾，例如 `C:\Users\USER\OneDrive\qBinEdit`。

---

## A. 可攜版（ZIP）

### 安裝 / 執行
1. 將 `dist\qBinEdit-1.0.1-win64-portable.zip` 解壓縮到任意位置（例如 `D:\Tools\qBinEdit`，或隨身碟）。
2. 執行 `qBinEdit.exe`。
3. 若出現 SmartScreen「Windows 已保護您的電腦」：點「其他資訊」→「仍要執行」（未做程式碼簽章時的正常現象）。
   - 從網路或 OneDrive 下載的 ZIP 可先在檔案內容勾「解除封鎖」再解壓縮，可避免每個 DLL 都被標記。

### 移除
```powershell
# 1) 關閉 qBinEdit，刪除解壓縮的資料夾
Remove-Item "D:\Tools\qBinEdit" -Recurse -Force
# 2) 只有在刪除過 qBinEdit.ini、改用登錄檔存設定時才需要
Remove-Item "HKCU:\Software\qBinEdit" -Recurse -Force -ErrorAction SilentlyContinue
```
可攜版不寫入系統資料夾、不建立捷徑、不註冊檔案關聯。

---

## 事前準備（B、C 共用）：安裝 Windows SDK 並產生 MSIX

1. 安裝 Windows SDK（提供 MakeAppx、MakePri、SignTool、App Certification Kit）：
   ```powershell
   winget install Microsoft.WindowsSDK.10.0.28000
   ```
   或至 <https://developer.microsoft.com/windows/downloads/windows-sdk/> 下載安裝。
2. 產生 MSIX（未簽章，可直接上傳商店）：
   ```powershell
   cd <repo>\packaging\msix
   powershell -ExecutionPolicy Bypass -File .\build_msix.ps1 -AppDir ..\..\dist\qBinEdit
   ```
   輸出：`dist\qBinEdit_1.0.1.0_x64.msix` 與 `dist\msix-layout\`（展開後的套件內容）。

---

## B. MSIX 註冊安裝（開發人員模式，免憑證）

1. 開啟開發人員模式：**設定 → 系統 → 開發人員專區（For developers）→ 開發人員模式：開啟**。
2. 註冊：
   ```powershell
   cd <repo>\packaging\msix
   powershell -ExecutionPolicy Bypass -File .\install_test.ps1 -Mode Register
   ```
3. 從開始功能表開啟「qBinEdit」，或在命令提示字元輸入 `qbinedit`。
4. 在 .bin / .hex 檔按右鍵 →「開啟檔案」→ 應出現 qBinEdit。

**移除**
```powershell
powershell -ExecutionPolicy Bypass -File .\uninstall.ps1
```
（或：設定 → 應用程式 → 已安裝的應用程式 → qBinEdit → 解除安裝）

---

## C. MSIX 自簽憑證安裝（最接近商店）

1. 產生並以測試憑證簽章（第一次會在「目前使用者\個人」建立自簽憑證，主體 = `identity.json` 的 `Publisher`）：
   ```powershell
   cd <repo>\packaging\msix
   powershell -ExecutionPolicy Bypass -File .\build_msix.ps1 -AppDir ..\..\dist\qBinEdit -Sign
   ```
   會多出 `dist\qBinEdit_TestCert.cer` / `.pfx`。
2. 以**系統管理員身分**開啟 PowerShell，安裝（會把測試憑證加入「本機電腦\受信任的人」）：
   ```powershell
   cd <repo>\packaging\msix
   powershell -ExecutionPolicy Bypass -File .\install_test.ps1 -Msix ..\..\dist\qBinEdit_1.0.1.0_x64.msix -Cert ..\..\dist\qBinEdit_TestCert.cer
   ```
   也可以直接在檔案總管雙擊 `.msix`，用「應用程式安裝程式」安裝（憑證須先信任）。

**移除（含測試憑證）**：以系統管理員身分執行
```powershell
powershell -ExecutionPolicy Bypass -File .\uninstall.ps1 -RemoveSettings
```
腳本會：移除 qBinEdit 套件 → 從「本機電腦\受信任的人」與「目前使用者\個人」刪除測試憑證 → 刪除 `HKCU\Software\qBinEdit`。
手動確認是否清乾淨：
```powershell
Get-AppxPackage *qBinEdit*                                          # 應無輸出
Get-ChildItem Cert:\LocalMachine\TrustedPeople | ? Subject -like "*SAISAIDev*"   # 應無輸出
```

> 測試憑證只能用於自己的電腦。正式上架時上傳**未簽章**的 `.msix`，由 Microsoft Store 簽章。
> 若 `identity.json` 已改成 Partner Center 的 Publisher（`CN=XXXXXXXX-...`），再跑一次 `-Sign` 會建立新的測試憑證；舊的請用 `uninstall.ps1` 清除。

---

## D. 商店認證預檢（WACK）

以系統管理員身分：
```powershell
cd <repo>\packaging\msix
powershell -ExecutionPolicy Bypass -File .\run_wack.ps1 -Msix ..\..\dist\qBinEdit_1.0.1.0_x64.msix
```
報告：`dist\wack-report.xml`，`OVERALL_RESULT="PASS"` 即可送審。

---

## E. 功能測試清單

範例檔在 `tests\samples\`：`demo_fw.bin`、`demo_fw_v2.bin`、`demo_fw.hex`。

| # | 操作 | 預期 |
|---|---|---|
| 1 | 開啟 `demo_fw.bin` | 十六進位 8-bit 顯示，狀態列顯示「唯讀」 |
| 2 | Ctrl+1 / Ctrl+2 / Ctrl+3 | 切換二進位 / 八進位 / 十六進位；二進位時每列自動改 8 位元組 |
| 3 | Alt+2、工具列「順序」改大端序 | 以 16-bit 顯示，數值依大小端序改變 |
| 4 | 開啟 `demo_fw.hex` | 位址欄從 0x08000000 開始；0x08000200～0x080007FF 為灰底空隙 |
| 5 | 工具 → 與檔案比較，選 `demo_fw_v2.bin`；按 F8 | 4 個差異區塊，F8 逐一跳轉 |
| 6 | Ctrl+K | Sum-8 = 0x33、CRC-16/CCITT-FALSE = 0x003A（`demo_fw.bin` 整檔） |
| 7 | 在檔案上按 Ctrl+E，輸入 `AB` | 位元組變紅色粗體、標題出現 `*`；Ctrl+Z 可復原 |
| 8 | Ctrl+F 搜尋 `HY17P3030`（ASCII 文字） | 選取到偏移 0x804 |
| 9 | 另存新檔為 `.hex`，再開啟 | 內容與位址一致 |
| 10 | 關閉時有未儲存修改 | 詢問是否儲存 |
| 11 | （1.0.1）Ctrl+E，在偏移 0x10 輸入 `AB`，再把游標移到 0、按 Insert 插入 4 個 `FF` | 插入的 4 個位元組與移到 0x14 的 `AB` 都是紅字；Ctrl+Z 一次後只剩 0x10 的 `AB` 是紅字 |
| 12 | （1.0.1）Ctrl+H，十六進位搜尋 `FF`、取代為 `00 00`，按「全部取代」並確認大小改變 | 立即完成；復原清單只多一步，Ctrl+Z 一次全部還原 |
| 13 | （1.0.1）`fsutil file createnew big.bin 268435456` 建 256 MiB 檔，開啟後按 Ctrl+K | 視窗可正常操作、出現進度列；計算中切換範圍或關閉對話框都不會卡住 |
| 14 | （1.0.1）開啟 `demo_fw.bin` 後，在「開啟」對話框輸入 `DEMO_FW.BIN` 再開一次 | 切到原本的分頁，不會開第二個 |
| 15 | （1.0.1）把某個最近開啟的檔案改名，再從「最近開啟的檔案」點它 | 詢問是否從清單移除，選「是」後清單更新 |
| 16 | （1.0.1）在大檔 Ctrl+A，編輯 → 複製為 → 二進位文字 | 超過 32 MiB 時提示上限；32 MiB 以內約 1 秒內完成 |

---

## F. 常見錯誤

| 訊息 | 原因 / 解法 |
|---|---|
| `makeappx.exe not found` | 未安裝 Windows SDK，見「事前準備」。 |
| `0x800B0109`（憑證鏈結不受信任） | 測試憑證沒加入「本機電腦\受信任的人」：用系統管理員執行 `install_test.ps1`。 |
| `0x800B0100`（找不到簽章） | 直接安裝了未簽章的 `.msix`：改用 `-Sign` 重建，或用 B 方式註冊。 |
| SignTool 錯誤 `0x8007000B` | `identity.json` 的 `Publisher` 與憑證主體不一致：刪除舊測試憑證後重跑 `-Sign`。 |
| `0x80073CFB`（已安裝相同版本） | 先執行 `uninstall.ps1`，或把 `Version` 加 1。 |
| 註冊時提示需要開發人員模式 | 依 B-1 開啟開發人員模式。 |
| 執行 .ps1 被擋 | 一律用 `powershell -ExecutionPolicy Bypass -File ...` 執行。 |
