# 發佈到 GitHub

Repo：<https://github.com/chaoban/qBinEdit>（公開、MIT 授權）。程式「關於」視窗與商店頁的原始碼 / 隱私權政策連結都指向這裡。

工作資料夾在 OneDrive（`C:\Users\USER\OneDrive\qBinEdit`）。OneDrive 會同步並鎖住 `.git` 內的檔案，所以 git 內部資料放在 OneDrive 以外的 `C:\git\qBinEdit.git`，工作資料夾裡只留一個指向它的 `.git` 檔案。

## 一次性準備
```powershell
winget install Git.Git
winget install GitHub.cli
# 重開 PowerShell
gh auth login                                   # GitHub.com → HTTPS → 用瀏覽器登入
git config --global user.name  "SAISAI.Dev"
git config --global user.email "<GitHub 帳號登記的 email 或 noreply 地址>"
```

## 從頭建立並推上 GitHub
先暫停 OneDrive 同步（工作列 OneDrive 圖示 → 設定齒輪 → 暫停同步 → 2 小時）。
```powershell
cd C:\Users\USER\OneDrive\qBinEdit
Remove-Item .git -Recurse -Force -ErrorAction SilentlyContinue          # 清掉先前的嘗試
Remove-Item C:\git\qBinEdit.git -Recurse -Force -ErrorAction SilentlyContinue
git init -b main --separate-git-dir C:\git\qBinEdit.git
git add .
git status                                      # 確認沒有 dist\、*.pfx、*.cer、*.msix、Claude outputs\
git commit -m "qBinEdit 1.0.0"
git remote add origin https://github.com/chaoban/qBinEdit.git
git push -u origin main --force                 # 覆蓋 GitHub 上建 repo 時自動產生的檔案
git status -sb                                  # 第一行應為 ## main...origin/main
```
完成後恢復 OneDrive 同步。

## 發佈版本（Releases）
```powershell
gh release create v1.0.0 dist\qBinEdit-1.0.0-win64-portable.zip --title "qBinEdit 1.0.0" --notes-file CHANGELOG.md
```
- 只上傳可攜版 ZIP。未簽章的 `.msix` 一般使用者無法安裝，不要放到 Releases。
- `dist\` 已在 `.gitignore`，程式檔只透過 Releases 發佈，不進版本庫。
- 不用 gh 也可以：repo 頁面 → Releases → Draft a new release → Tag `v1.0.0` → 拖入 ZIP → 貼上 `CHANGELOG.md` → Publish。

## 之後每次更新
1. 改程式、改 `CMakeLists.txt` 的 `VERSION` 與 `packaging/msix/identity.json` 的 `Version`，在 `CHANGELOG.md` 加一段。
2. 重新產生 ZIP / MSIX。
3. `git add . ; git commit -m "qBinEdit x.y.z" ; git push`
4. `gh release create vx.y.z dist\qBinEdit-x.y.z-win64-portable.zip --title "qBinEdit x.y.z" --notes "..."`
