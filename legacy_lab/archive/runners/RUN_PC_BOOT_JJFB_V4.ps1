$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB PC direct replace v4: jjfb =="
python .\scripts\pc_direct_replace_v4.py prepare --mode jjfb

Write-Host "Starting mock..."
$mock = Start-Process powershell -PassThru -ArgumentList "-NoExit", "-ExecutionPolicy", "Bypass", "-Command", "cd `"$ROOT`"; python .\mock\jjfb_mock_server.py"
Start-Sleep -Seconds 2

python .\scripts\pc_direct_replace_v4.py launch --mode jjfb
python .\scripts\pc_direct_replace_v4.py collect --mode jjfb

Write-Host "Done. Send newest logs\direct_replace_feedback_*.zip"
