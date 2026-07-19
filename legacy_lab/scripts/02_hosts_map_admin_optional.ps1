# Optional hosts mapping. Run PowerShell as Administrator for it to take effect.
$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent $PSScriptRoot
Set-Location $ROOT
$config = Get-Content .\CONFIG.json -Raw | ConvertFrom-Json
$hosts = "$env:windir\System32\drivers\etc\hosts"

$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (!$isAdmin) {
    Write-Host "Not Administrator. Skip hosts mapping."
    Write-Host "Need mapping only if vmrp tries domains like spd.skymobiapp.com."
    exit 0
}

"`n# JJFB PC Bootkit begin" | Add-Content $hosts
foreach ($d in $config.host_domains_to_localhost) {
    "127.0.0.1 $d" | Add-Content $hosts
}
"# JJFB PC Bootkit end`n" | Add-Content $hosts
Write-Host "Hosts mapping added. Restore manually by deleting JJFB PC Bootkit block if needed."
