$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v18: patch start.mr sdk check and live boot =="
Write-Host "Watch the vmrp window and screenshot it."
python .\scripts\sdk_bypass_live_v18.py live

Write-Host ""
Write-Host "Done. Send newest logs\sdk_bypass_feedback_*.zip and vmrp screenshot."
