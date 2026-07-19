$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB-only v10: _mr_param / gwyblink probe =="
python .\scripts\pc_jjfb_param_probe_v10.py run

Write-Host ""
Write-Host "Done. Send newest logs\jjfb_param_probe_feedback_*.zip"
