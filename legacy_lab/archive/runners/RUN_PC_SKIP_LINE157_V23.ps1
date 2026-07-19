$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v23: skip sdk error + skip line157 _mr_c_load =="
Write-Host "Watch the vmrp window and screenshot it."
python .\scripts\skip_line157_v23.py live

Write-Host ""
Write-Host "Done. Send newest logs\skip_line157_v23_feedback_*.zip and vmrp screenshot."
