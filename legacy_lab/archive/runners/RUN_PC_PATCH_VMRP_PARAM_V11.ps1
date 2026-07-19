$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null
New-Item -ItemType Directory -Force -Path runtime | Out-Null

Write-Host "== JJFB v11: patch vmrp start param =="
python .\scripts\patch_vmrp_param_v11.py

Write-Host ""
Write-Host "Done. Send newest logs\vmrp_param_patch_feedback_*.zip"
