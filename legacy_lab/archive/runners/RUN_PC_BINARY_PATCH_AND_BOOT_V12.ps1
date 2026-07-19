$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v12: binary patch main.exe and boot =="
python .\scripts\binary_patch_vmrp_param_v12.py patch
python .\scripts\binary_patch_vmrp_param_v12.py boot
python .\scripts\binary_patch_vmrp_param_v12.py collect

Write-Host "Done. Send newest logs\binary_patch_feedback_*.zip"
