<#
.SYNOPSIS
    Remove every trace of a qBinEdit test installation from this PC.

.DESCRIPTION
    - Removes the MSIX package (Store or test install) for the current user.
    - Removes the self-signed test certificate from LocalMachine\TrustedPeople (needs Administrator)
      and from CurrentUser\My.
    - -RemoveSettings: also deletes HKCU\Software\qBinEdit (settings of the portable/ZIP version).
      MSIX settings are virtualised and are removed together with the package.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File uninstall.ps1 -RemoveSettings
#>
[CmdletBinding()]
param(
    [string]$IdentityFile,
    [switch]$RemoveSettings
)
$ErrorActionPreference = "Continue"
# $PSScriptRoot is not reliable inside param() defaults on Windows PowerShell 5.1, so resolve paths here.
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $IdentityFile) { $IdentityFile = (Join-Path $ScriptDir "identity.json") }

$publisher = $null
if (Test-Path $IdentityFile) { $publisher = (Get-Content $IdentityFile -Raw -Encoding UTF8 | ConvertFrom-Json).Publisher }

$pkgs = Get-AppxPackage | Where-Object { $_.Name -like "*qBinEdit*" }
if ($pkgs) {
    foreach ($p in $pkgs) {
        Write-Host "Removing package $($p.PackageFullName)"
        Remove-AppxPackage -Package $p.PackageFullName
    }
} else {
    Write-Host "No qBinEdit package installed."
}

$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
$isAdmin = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
$machineCerts = Get-ChildItem Cert:\LocalMachine\TrustedPeople |
                Where-Object { ($publisher -and $_.Subject -eq $publisher) -or $_.FriendlyName -eq "qBinEdit test signing" }
if ($machineCerts) {
    if ($isAdmin) {
        $machineCerts | ForEach-Object { Write-Host "Removing trusted test certificate $($_.Subject) [$($_.Thumbprint)]"; Remove-Item $_.PSPath }
    } else {
        Write-Host "Test certificate is still trusted in LocalMachine\TrustedPeople - re-run as Administrator to remove it." -ForegroundColor Yellow
    }
}
Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.FriendlyName -eq "qBinEdit test signing" } |
    ForEach-Object { Write-Host "Removing signing certificate $($_.Subject) from CurrentUser\My"; Remove-Item $_.PSPath }

if ($RemoveSettings -and (Test-Path "HKCU:\Software\qBinEdit")) {
    Write-Host "Removing HKCU\Software\qBinEdit"
    Remove-Item "HKCU:\Software\qBinEdit" -Recurse -Force
}
Write-Host "Done." -ForegroundColor Green
