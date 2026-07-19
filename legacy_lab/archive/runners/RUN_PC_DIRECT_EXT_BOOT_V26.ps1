$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v26: direct boot mrc_loader.ext =="
Write-Host "Watch vmrp window and stdout."
python .\scripts\direct_ext_boot_v26.py live

Write-Host ""
Write-Host "Done. Send newest logs\direct_ext_boot_v26_feedback_*.zip and vmrp screenshot."
