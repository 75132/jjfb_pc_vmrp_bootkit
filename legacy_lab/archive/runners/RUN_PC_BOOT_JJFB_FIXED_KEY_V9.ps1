$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB-only v9: fixed sdk_key g:u2 =="
Write-Host "No admin. No gwy route. Direct jjfb boot."

python .\scripts\pc_jjfb_fixed_key_v9.py prepare
python .\scripts\pc_jjfb_fixed_key_v9.py launch
python .\scripts\pc_jjfb_fixed_key_v9.py collect

Write-Host ""
Write-Host "Done. Send newest logs\jjfb_fixed_key_feedback_*.zip"
