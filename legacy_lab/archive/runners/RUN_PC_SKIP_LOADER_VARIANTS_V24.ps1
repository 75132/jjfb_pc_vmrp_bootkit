$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v24: skip loader object call variants =="
python .\scripts\skip_loader_variants_v24.py run

Write-Host ""
Write-Host "Done. Send newest logs\skip_loader_variants_v24_feedback_*.zip and vmrp screenshot."
