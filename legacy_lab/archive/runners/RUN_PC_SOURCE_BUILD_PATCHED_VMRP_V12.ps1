$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v12: source build patched vmrp =="
python .\scripts\source_build_vmrp_param_v12.py

Write-Host "Done. Send newest logs\source_build_feedback_*.zip"
