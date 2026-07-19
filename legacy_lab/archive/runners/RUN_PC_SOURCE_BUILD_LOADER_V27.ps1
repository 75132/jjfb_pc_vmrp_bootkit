$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB v27: source build vmrp loader route =="
Write-Host "This will download vmrp source, patch, build if mingw32-make exists, then boot jjfb."
python .\scripts\source_build_loader_v27.py run

Write-Host ""
Write-Host "Done. Send newest logs\source_build_loader_v27_feedback_*.zip"
