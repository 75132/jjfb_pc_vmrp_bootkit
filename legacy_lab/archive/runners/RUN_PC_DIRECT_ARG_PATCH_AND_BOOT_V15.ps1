$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v15: direct patch 4th arg and boot =="
python .\scripts\direct_arg_patch_vmrp_param_v15.py patch
python .\scripts\direct_arg_patch_vmrp_param_v15.py boot
python .\scripts\direct_arg_patch_vmrp_param_v15.py collect

Write-Host ""
Write-Host "Done. Send newest logs\direct_arg_patch_feedback_*.zip"
