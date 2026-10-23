<#
.SYNOPSIS
    Build the qBinEdit MSIX package (Microsoft Store upload and local testing).

.DESCRIPTION
    1. Copies the deployed application folder (qBinEdit.exe + Qt DLLs) into a package layout.
    2. Writes AppxManifest.xml from the template using identity.json.
    3. Generates resources.pri (MakePri) so the scaled tile/icon assets resolve.
    4. Packs the layout with MakeAppx.
    5. -Sign: creates (once) a self-signed test certificate whose subject equals the
       Publisher value and signs the package. Only for local testing - Store uploads
       do not need to be signed (Microsoft re-signs them).

    Requires the Windows 10/11 SDK (MakeAppx.exe, MakePri.exe, SignTool.exe):
        winget install Microsoft.WindowsSDK.10.0.28000
    or https://developer.microsoft.com/windows/downloads/windows-sdk/

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File build_msix.ps1 -AppDir ..\..\dist\qBinEdit
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File build_msix.ps1 -AppDir ..\..\dist\qBinEdit -Sign
#>
[CmdletBinding()]
param(
    [string]$AppDir,
    [string]$OutDir,
    [string]$IdentityFile,
    [string]$Version,
    [switch]$Sign,
    [string]$CertPassword = "qbinedit-test"
)
$ErrorActionPreference = "Stop"
# $PSScriptRoot is not reliable inside param() defaults on Windows PowerShell 5.1, so resolve paths here.
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $AppDir) { $AppDir = (Join-Path $ScriptDir "..\..\dist\qBinEdit") }
if (-not $OutDir) { $OutDir = (Join-Path $ScriptDir "..\..\dist") }
if (-not $IdentityFile) { $IdentityFile = (Join-Path $ScriptDir "identity.json") }

function Find-SdkTool([string]$Name) {
    $roots = @("${env:ProgramFiles(x86)}\Windows Kits\10\bin", "$env:ProgramFiles\Windows Kits\10\bin")
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        $dirs = Get-ChildItem -Path $root -Directory | Where-Object { $_.Name -match '^10\.\d+\.\d+\.\d+$' } |
                Sort-Object { [version]$_.Name } -Descending
        foreach ($d in $dirs) {
            $p = Join-Path $d.FullName "x64\$Name"
            if (Test-Path $p) { return $p }
        }
    }
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    throw "$Name not found. Install the Windows SDK:  winget install Microsoft.WindowsSDK.10.0.28000  (list versions: winget search Microsoft.WindowsSDK)"
}

function Invoke-Tool([string]$Exe, [string[]]$ToolArgs) {
    Write-Host "> $([IO.Path]::GetFileName($Exe)) $($ToolArgs -join ' ')" -ForegroundColor DarkGray
    & $Exe @ToolArgs
    if ($LASTEXITCODE -ne 0) { throw "$([IO.Path]::GetFileName($Exe)) failed with exit code $LASTEXITCODE" }
}

# ---------------------------------------------------------------- inputs
$AppDir = (Resolve-Path $AppDir).Path
if (-not (Test-Path (Join-Path $AppDir "qBinEdit.exe"))) { throw "qBinEdit.exe not found in $AppDir" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path

$id = Get-Content $IdentityFile -Raw -Encoding UTF8 | ConvertFrom-Json
if ($Version) { $id.Version = $Version }
if ($id.Version -notmatch '^\d+\.\d+\.\d+\.0$') { throw "Version must look like 1.2.3.0 (the 4th part must be 0 for the Store)." }

$makeappx = Find-SdkTool "makeappx.exe"
$makepri  = Find-SdkTool "makepri.exe"
Write-Host "Windows SDK tools: $(Split-Path $makeappx)"

# ---------------------------------------------------------------- layout
$layout = Join-Path $OutDir "msix-layout"
$priDir = Join-Path $OutDir "msix-pri"
foreach ($d in @($layout, $priDir)) {
    if (Test-Path $d) { Remove-Item $d -Recurse -Force }
    New-Item -ItemType Directory -Path $d | Out-Null
}
Copy-Item -Path (Join-Path $AppDir "*") -Destination $layout -Recurse
# The portable marker must not be packaged: the MSIX install folder is read-only,
# settings go to the (virtualised) registry instead.
Remove-Item (Join-Path $layout "qBinEdit.ini") -ErrorAction SilentlyContinue
Copy-Item -Path (Join-Path $ScriptDir "Assets") -Destination (Join-Path $layout "Assets") -Recurse
Copy-Item -Path (Join-Path $ScriptDir "Assets") -Destination (Join-Path $priDir "Assets") -Recurse

function Escape-Xml([string]$s) { return [System.Security.SecurityElement]::Escape($s) }
$manifest = Get-Content (Join-Path $ScriptDir "AppxManifest.xml") -Raw -Encoding UTF8
$manifest = $manifest.Replace("__IDENTITY_NAME__", (Escape-Xml $id.IdentityName))
$manifest = $manifest.Replace("__PUBLISHER_DISPLAY_NAME__", (Escape-Xml $id.PublisherDisplayName))
$manifest = $manifest.Replace("__PUBLISHER__", (Escape-Xml $id.Publisher))
$manifest = $manifest.Replace("__VERSION__", $id.Version)
$manifestPath = Join-Path $layout "AppxManifest.xml"
[IO.File]::WriteAllText($manifestPath, $manifest, (New-Object System.Text.UTF8Encoding $false))

# ---------------------------------------------------------------- resources.pri
$priConfig = Join-Path $priDir "priconfig.xml"
Invoke-Tool $makepri @("createconfig", "/cf", $priConfig, "/dq", "en-US", "/pv", "10.0.0", "/o")
# Single package (no bundle / resource packs): drop <packaging> so every scale stays in resources.pri.
[xml]$cfg = Get-Content $priConfig -Raw
$packaging = $cfg.DocumentElement.SelectSingleNode("packaging")
if ($packaging) { [void]$cfg.DocumentElement.RemoveChild($packaging) }
$cfg.Save($priConfig)
Invoke-Tool $makepri @("new", "/pr", $priDir, "/cf", $priConfig,
                       "/mn", $manifestPath, "/of", (Join-Path $layout "resources.pri"), "/o")

# ---------------------------------------------------------------- pack
$msix = Join-Path $OutDir ("qBinEdit_{0}_x64.msix" -f $id.Version)
Invoke-Tool $makeappx @("pack", "/d", $layout, "/p", $msix, "/o")
Write-Host "`nPackage: $msix" -ForegroundColor Green

# ---------------------------------------------------------------- optional test signing
if ($Sign) {
    $signtool = Find-SdkTool "signtool.exe"
    $cert = Get-ChildItem Cert:\CurrentUser\My |
            Where-Object { $_.Subject -eq $id.Publisher -and $_.HasPrivateKey -and $_.NotAfter -gt (Get-Date) } |
            Select-Object -First 1
    if (-not $cert) {
        Write-Host "Creating self-signed test certificate '$($id.Publisher)'..."
        $cert = New-SelfSignedCertificate -Type Custom -Subject $id.Publisher -KeyUsage DigitalSignature `
                -FriendlyName "qBinEdit test signing" -CertStoreLocation "Cert:\CurrentUser\My" `
                -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}") `
                -NotAfter (Get-Date).AddYears(2)
    }
    $pfx = Join-Path $OutDir "qBinEdit_TestCert.pfx"
    $cer = Join-Path $OutDir "qBinEdit_TestCert.cer"
    $securePwd = ConvertTo-SecureString -String $CertPassword -Force -AsPlainText
    Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $securePwd | Out-Null
    Export-Certificate -Cert $cert -FilePath $cer | Out-Null
    Invoke-Tool $signtool @("sign", "/fd", "SHA256", "/f", $pfx, "/p", $CertPassword, $msix)
    Write-Host "Signed with test certificate: $cer" -ForegroundColor Green
    Write-Host "Install it with:  install_test.ps1 -Msix `"$msix`" -Cert `"$cer`"   (run as Administrator)"
} else {
    Write-Host "Unsigned package - upload this file to Partner Center, or rebuild with -Sign to install it locally."
}
