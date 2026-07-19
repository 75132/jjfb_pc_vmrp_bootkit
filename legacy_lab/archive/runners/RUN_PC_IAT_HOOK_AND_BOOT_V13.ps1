$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v13: IAT hook bridge_dsm_mr_start_dsm and boot =="
python .\scripts\iat_hook_vmrp_param_v13.py patch
python .\scripts\iat_hook_vmrp_param_v13.py boot
python .\scripts\iat_hook_vmrp_param_v13.py collect

Write-Host ""
Write-Host "Done. Send newest logs\iat_hook_feedback_*.zip"
