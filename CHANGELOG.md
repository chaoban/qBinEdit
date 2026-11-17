# Changelog

## 1.0.1 — 2026-11-17

效能與正確性改善 / Performance and correctness update.

- 全部取代改為一次完成、只佔一個復原步驟；長度不同的取代從數十秒縮短到毫秒級
  Replace All runs in one pass as a single undo step; size-changing replacements drop from tens of seconds to milliseconds
- 不分大小寫搜尋不再複製整個檔案（每次 F3 少用一份檔案大小的記憶體，速度約快 3.5 倍）
  Case-insensitive search no longer copies the whole file (saves one file-size of RAM per F3, about 3.5x faster)
- 檢查碼 / CRC 改在背景計算並顯示進度，大檔不再讓視窗沒有回應；CRC 改用 slicing-by-8 並分散到多個執行緒，雙核心電腦上整體約快 4 倍
  Checksums / CRCs are calculated in the background with progress, so large files no longer freeze the window; slicing-by-8 CRCs spread over several threads, about 4x faster overall on a dual-core PC
- 插入或刪除位元組後，先前修改的紅字標示會保留並跟著位移（之前會全部消失）
  Edited-byte highlighting now survives inserts and deletes instead of disappearing
- 「複製為」二進位 / 十六進位 / C 陣列：32 MiB 選取從 4–12 秒縮短到 0.5 秒內，記憶體用量大幅下降
  Copy As binary / hex / C array: 32 MiB selections take under 0.5 s instead of 4–12 s, with far less memory
- 同一個檔案用不同大小寫或路徑開啟時，切換到已開啟的分頁，不會再開第二個（避免互相覆蓋）
  Opening the same file through a different letter case or path switches to its tab instead of opening a second copy
- 最近開啟的檔案若已不存在，可直接從清單移除
  Recent Files offers to remove entries whose file no longer exists

下載 / Download: `qBinEdit-1.0.1-win64-portable.zip`（Windows 10 1809+ x64）

## 1.0.0 — 2026-10-23

第一個公開版本 / First public release.

- 2 / 8 / 16 進位顯示，8 / 16 / 32-bit 單位，小端序 / 大端序
  Binary / octal / hexadecimal display, 8 / 16 / 32-bit units, little / big endian
- Intel HEX 與 Motorola S-Record 讀寫，依位址展開並標示空隙
  Intel HEX and Motorola S-Record read / write at real load addresses, gaps shaded
- 預設唯讀，Ctrl+E 編輯：覆寫、插入、刪除、填入、貼上，無限次復原
  Read-only by default; Ctrl+E to edit (overwrite, insert, delete, fill, paste) with unlimited undo
- 檢查碼 / CRC、資料檢視器、兩檔比較、搜尋取代、跳至位址
  Checksums / CRCs, data inspector, file compare, find / replace, go to address
- 繁體中文 / 英文介面、深色模式、高 DPI
  Traditional Chinese / English UI, dark mode, high-DPI

下載 / Download: `qBinEdit-1.0.0-win64-portable.zip`（Windows 10 1809+ x64）
