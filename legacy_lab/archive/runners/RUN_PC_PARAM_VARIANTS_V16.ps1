$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v16: direct arg parameter variants =="
python .\scripts\param_variants_v16.py run

Write-Host ""
Write-Host "Done. Send newest logs\param_variants_feedback_*.zip"
