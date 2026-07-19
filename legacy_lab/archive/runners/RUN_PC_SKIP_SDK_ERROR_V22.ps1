$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v22: skip sdk error branches in start.mr =="
Write-Host "Watch the vmrp window and screenshot it."
python .\scripts\skip_sdk_error_v22.py live

Write-Host ""
Write-Host "Done. Send newest logs\skip_sdk_error_v22_feedback_*.zip and vmrp screenshot."
