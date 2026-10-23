<#
.SYNOPSIS
    Run the Windows App Certification Kit (the same checks the Store runs) on the MSIX package.
    Run in an Administrator PowerShell. The kit ships with the Windows SDK.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File run_wack.ps1 -Msix ..\..\dist\qBinEdit_1.0.0.0_x64.msix
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Msix,
    [string]$Report
)
$ErrorActionPreference = "Stop"
# $PSScriptRoot is not reliable inside param() defaults on Windows PowerShell 5.1, so resolve paths here.
$ScriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $Report) { $Report = (Join-Path $ScriptDir "..\..\dist\wack-report.xml") }
$appcert = "${env:ProgramFiles(x86)}\Windows Kits\10\App Certification Kit\appcert.exe"
if (-not (Test-Path $appcert)) { throw "appcert.exe not found. Install the Windows SDK (includes the App Certification Kit)." }
$Msix = (Resolve-Path $Msix).Path
& $appcert reset
& $appcert test -appxpackagepath $Msix -reportoutputpath $Report
Write-Host "Report: $Report  (look for OVERALL_RESULT=PASS)"
