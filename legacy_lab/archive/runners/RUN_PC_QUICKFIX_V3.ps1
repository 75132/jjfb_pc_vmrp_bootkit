$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB PC vmrp quickfix v3 =="
python .\scripts\pc_quickfix_v3.py prepare

Write-Host "Starting mock..."
$mock = Start-Process powershell -PassThru -ArgumentList "-NoExit", "-ExecutionPolicy", "Bypass", "-Command", "cd `"$ROOT`"; python .\mock\jjfb_mock_server.py"
Start-Sleep -Seconds 2

python .\scripts\pc_quickfix_v3.py launch
python .\scripts\pc_quickfix_v3.py collect
Write-Host "Done. Send logs\quickfix_feedback_*.zip"
