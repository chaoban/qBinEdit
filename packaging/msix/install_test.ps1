<#
.SYNOPSIS
    Install qBinEdit on this PC for testing.

.DESCRIPTION
    Mode "Signed"   (default): trusts the self-signed test certificate (LocalMachine\TrustedPeople)
                    and installs the signed .msix. Run PowerShell as Administrator.
    Mode "Register": registers the unpacked layout folder directly (no signing, no admin).
                    Requires Developer Mode: Settings > System > For developers > Developer Mode = On.

.EXAMPLE
    # Administrator PowerShell
    powershell -ExecutionPolicy Bypass -File install_test.ps1 -Msix ..\..\dist\qBinEdit_1.0.1.0_x64.msix -Cert ..\..\dist\qBinEdit_TestCert.cer
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File install_test.ps1 -Mode Register
#>
[CmdletBinding()]
param(
    [ValidateSet("Signed", "Register")]
    [string]$Mode = "Signed",
    [string]$Msix,
    [string]$Cert,
    [string]$Layout
)
$ErrorActionPreference = "Stop"
# $PSScriptRoot is not reliable inside param() defaults on Windows PowerShell 5.1, so resolve paths here.
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $Cert) { $Cert = (Join-Path $ScriptDir "..\..\dist\qBinEdit_TestCert.cer") }
if (-not $Layout) { $Layout = (Join-Path $ScriptDir "..\..\dist\msix-layout") }

if ($Mode -eq "Register") {
    $manifest = Join-Path (Resolve-Path $Layout).Path "AppxManifest.xml"
    if (-not (Test-Path $manifest)) { throw "Run build_msix.ps1 first ($manifest not found)." }
    Write-Host "Registering $manifest (Developer Mode must be ON)..."
    Add-AppxPackage -Register $manifest -ForceApplicationShutdown
} else {
    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    $isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) { throw "Run this script in an Administrator PowerShell (needed to trust the test certificate)." }
    if (-not $Msix) {
        $Msix = Get-ChildItem (Join-Path $ScriptDir "..\..\dist") -Filter "qBinEdit_*_x64.msix" |
                Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    }
    if (-not $Msix -or -not (Test-Path $Msix)) { throw "MSIX file not found. Build it with build_msix.ps1 -Sign." }
    if (-not (Test-Path $Cert)) { throw "Test certificate not found: $Cert" }
    Write-Host "Trusting test certificate $Cert ..."
    Import-Certificate -FilePath $Cert -CertStoreLocation Cert:\LocalMachine\TrustedPeople | Out-Null
    Write-Host "Installing $Msix ..."
    Add-AppxPackage -Path $Msix -ForceApplicationShutdown
}

$pkg = Get-AppxPackage | Where-Object { $_.Name -like "*qBinEdit*" } | Select-Object -First 1
if ($pkg) {
    Write-Host "`nInstalled: $($pkg.PackageFullName)" -ForegroundColor Green
    Write-Host "Start it from the Start menu (qBinEdit) or run:  qbinedit  (app execution alias)"
}
