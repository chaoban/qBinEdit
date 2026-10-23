<#
.SYNOPSIS
    Build qBinEdit from source on Windows and create the portable ZIP (optional - prebuilt binaries are provided).

.DESCRIPTION
    Needs Qt 6.4+ for Windows (Qt Online Installer, kit "MSVC 2022 64-bit" or "MinGW 64-bit"), CMake and Ninja
    (both installable from the Qt installer "Developer and Designer Tools"), and for MSVC the
    "Desktop development with C++" workload of Visual Studio / Build Tools.
    Run from a "x64 Native Tools Command Prompt for VS 2022" PowerShell when using the MSVC kit.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build_windows.ps1 -QtDir C:\Qt\6.8.3\msvc2022_64
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$QtDir,
    [string]$BuildDir,
    [string]$DistDir
)
$ErrorActionPreference = "Stop"
# $PSScriptRoot is not reliable inside param() defaults on Windows PowerShell 5.1, so resolve paths here.
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $BuildDir) { $BuildDir = (Join-Path $ScriptDir "..\build-win") }
if (-not $DistDir) { $DistDir = (Join-Path $ScriptDir "..\dist") }
$src = (Resolve-Path (Join-Path $ScriptDir "..")).Path

cmake -S $src -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$QtDir" -DQBE_BUILD_TESTS=ON
if ($LASTEXITCODE) { throw "CMake configure failed" }
cmake --build $BuildDir --config Release
if ($LASTEXITCODE) { throw "Build failed" }
& (Join-Path $BuildDir "qbe_tests.exe")
if ($LASTEXITCODE) { throw "Unit tests failed" }

$version = (Select-String -Path (Join-Path $src "CMakeLists.txt") -Pattern 'VERSION\s+(\d+\.\d+\.\d+)').Matches[0].Groups[1].Value
$app = Join-Path $DistDir "qBinEdit"
if (Test-Path $app) { Remove-Item $app -Recurse -Force }
New-Item -ItemType Directory -Force -Path $app | Out-Null
Copy-Item (Join-Path $BuildDir "qBinEdit.exe") $app

& (Join-Path $QtDir "bin\windeployqt.exe") --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
    (Join-Path $app "qBinEdit.exe")
if ($LASTEXITCODE) { throw "windeployqt failed" }

# MSVC kit: ship the VC++ runtime next to the exe (app-local), so neither the ZIP nor the MSIX needs VC_redist.
if ($env:VCToolsRedistDir) {
    $crt = Get-ChildItem (Join-Path $env:VCToolsRedistDir "x64") -Directory -Filter "Microsoft.VC*.CRT" | Select-Object -First 1
    if ($crt) { Copy-Item (Join-Path $crt.FullName "*.dll") $app }
}
Remove-Item (Join-Path $app "vc_redist*.exe") -ErrorAction SilentlyContinue

Copy-Item (Join-Path $src "LICENSE.txt"), (Join-Path $src "THIRD-PARTY-NOTICES.txt") $app
Copy-Item (Join-Path $src "licenses") (Join-Path $app "licenses") -Recurse
Copy-Item (Join-Path $src "packaging\portable\README-portable.txt") (Join-Path $app "README.txt")
Set-Content -Path (Join-Path $app "qBinEdit.ini") -Value "[General]" -Encoding ASCII   # portable-mode marker

$zip = Join-Path $DistDir "qBinEdit-$version-win64-portable.zip"
if (Test-Path $zip) { Remove-Item $zip }
Compress-Archive -Path $app -DestinationPath $zip
Write-Host "Portable ZIP: $zip" -ForegroundColor Green
Write-Host "Next: packaging\msix\build_msix.ps1 -AppDir `"$app`""
