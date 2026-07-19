$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v14: direct call hook bridge_dsm_mr_start_dsm and boot =="
python .\scripts\direct_call_hook_vmrp_param_v14.py patch
python .\scripts\direct_call_hook_vmrp_param_v14.py boot
python .\scripts\direct_call_hook_vmrp_param_v14.py collect

Write-Host ""
Write-Host "Done. Send newest logs\direct_call_hook_feedback_*.zip"
