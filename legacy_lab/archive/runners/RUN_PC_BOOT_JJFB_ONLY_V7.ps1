$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB-only v7: direct boot jjfb.mrp =="
Write-Host "No admin, no portproxy, no gwy community route."

python .\scripts\pc_jjfb_only_v7.py prepare
python .\scripts\pc_jjfb_only_v7.py launch
python .\scripts\pc_jjfb_only_v7.py collect

Write-Host ""
Write-Host "Done. Send newest logs\jjfb_only_feedback_*.zip"
