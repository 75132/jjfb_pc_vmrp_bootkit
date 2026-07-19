$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v21: sdk error branch variants =="
python .\scripts\sdk_branch_variants_v21.py run

Write-Host ""
Write-Host "Done. Send newest logs\sdk_branch_variants_feedback_*.zip and vmrp screenshot."
