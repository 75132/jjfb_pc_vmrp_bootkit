$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v25: loader/source audit =="
Write-Host "No game launch. No MRP patch. This only generates analysis logs."
python .\scripts\loader_source_audit_v25.py

Write-Host ""
Write-Host "Done. Send newest logs\loader_source_audit_v25_feedback_*.zip"
