$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v19: fixed MRP size after SDK bypass =="
Write-Host "Watch the vmrp window and screenshot it."
python .\scripts\sdk_bypass_v19.py live

Write-Host ""
Write-Host "Done. Send newest logs\sdk_bypass_v19_feedback_*.zip and vmrp screenshot."
