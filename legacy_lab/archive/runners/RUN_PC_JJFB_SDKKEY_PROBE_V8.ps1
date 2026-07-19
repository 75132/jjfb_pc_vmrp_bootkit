$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB-only v8: sdk_key probe =="
Write-Host "This does NOT use gwy route and does NOT use portproxy."

python .\scripts\pc_jjfb_sdkkey_probe_v8.py run

Write-Host ""
Write-Host "Done. Send newest logs\sdkkey_probe_feedback_*.zip"
