#!/usr/bin/env bash
# Cross-compile qBinEdit for Windows x64 on Linux (MinGW-w64 + Qt built for MinGW) and
# produce dist/qBinEdit (deployed app folder) and dist/qBinEdit-<ver>-win64-portable.zip.
#
#   QT_WIN=/opt/qt-win QT_HOST=/opt/qt-host tools/package_portable.sh
set -euo pipefail
SRC="$(cd "$(dirname "$0")/.." && pwd)"
QT_WIN="${QT_WIN:-/opt/qt-win}"
QT_HOST="${QT_HOST:-/opt/qt-host}"
BUILD="${BUILD:-$SRC/build-mingw}"
DIST="${DIST:-$SRC/dist}"
TRIPLE=x86_64-w64-mingw32
VERSION=$(sed -n 's/^ *VERSION \([0-9]*\.[0-9]*\.[0-9]*\)$/\1/p' "$SRC/CMakeLists.txt" | head -1)

cmake -G Ninja -S "$SRC" -B "$BUILD" \
  -DCMAKE_TOOLCHAIN_FILE="$QT_WIN/lib/cmake/Qt6/qt.toolchain.cmake" \
  -DQT_CHAINLOAD_TOOLCHAIN_FILE=/opt/mingw-toolchain.cmake \
  -DQT_HOST_PATH="$QT_HOST" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_EXE_LINKER_FLAGS="-s" -DQBE_BUILD_TESTS=ON
cmake --build "$BUILD" --parallel 2

APP="$DIST/qBinEdit"
rm -rf "$APP"
mkdir -p "$APP/platforms" "$APP/styles" "$APP/imageformats" "$APP/licenses"
cp "$BUILD/qBinEdit.exe" "$APP/"
for m in Core Gui Widgets; do cp "$QT_WIN/bin/Qt6$m.dll" "$APP/"; done
cp "$QT_WIN/plugins/platforms/qwindows.dll" "$APP/platforms/"
cp "$QT_WIN"/plugins/styles/*.dll "$APP/styles/"
cp "$QT_WIN/plugins/imageformats/qico.dll" "$APP/imageformats/" 2>/dev/null || true

# MinGW runtime DLLs
GCCDIR=$(dirname "$($TRIPLE-g++-posix -print-libgcc-file-name)")
cp "$GCCDIR/libstdc++-6.dll" "$GCCDIR/libgcc_s_seh-1.dll" "$APP/"
cp /usr/$TRIPLE/lib/libwinpthread-1.dll "$APP/"

# strip debug info from DLLs (exe was linked with -s)
for f in "$APP"/*.dll "$APP"/*/*.dll; do $TRIPLE-strip --strip-unneeded "$f"; done

# docs (CRLF for Notepad)
for f in LICENSE.txt THIRD-PARTY-NOTICES.txt; do sed 's/$/\r/' "$SRC/$f" > "$APP/$f"; done
sed 's/$/\r/' "$SRC/packaging/portable/README-portable.txt" > "$APP/README.txt"
for f in "$SRC"/licenses/*.txt; do sed 's/$/\r/' "$f" > "$APP/licenses/$(basename "$f")"; done
printf '[General]\r\n' > "$APP/qBinEdit.ini"     # portable-mode marker

# sanity: every imported DLL must be shipped or be a Windows system DLL
echo "--- imported DLLs ---"
for f in "$APP"/qBinEdit.exe "$APP"/*.dll "$APP"/*/*.dll; do
  $TRIPLE-objdump -p "$f" | sed -n 's/^\s*DLL Name: //p'
done | sort -u | tr '\n' ' '; echo

ZIP="$DIST/qBinEdit-$VERSION-win64-portable.zip"
rm -f "$ZIP"
(cd "$DIST" && zip -qr9 "$ZIP" qBinEdit)
echo "Portable ZIP: $ZIP ($(du -h "$ZIP" | cut -f1))"
