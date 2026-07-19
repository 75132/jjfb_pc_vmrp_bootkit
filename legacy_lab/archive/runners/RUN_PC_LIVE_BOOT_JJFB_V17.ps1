$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v17: live boot, do not kill vmrp =="
Write-Host "Please watch the vmrp window. Screenshot what it shows."
python .\scripts\live_boot_jjfb_v17.py live

Write-Host ""
Write-Host "Done. Send newest logs\live_boot_feedback_*.zip and a screenshot of vmrp window."
