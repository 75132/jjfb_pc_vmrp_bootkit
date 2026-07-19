$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB PC vmrp flatten fix v2 =="
Write-Host "Step 1: flatten 240x320 into vmrp mythroad root"
python .\scripts\pc_flatten_240x320.py flatten

Write-Host "Step 2: start mock server"
$mock = Start-Process powershell -PassThru -ArgumentList "-NoExit", "-ExecutionPolicy", "Bypass", "-Command", "cd `"$ROOT`"; python .\mock\jjfb_mock_server.py"
Start-Sleep -Seconds 2

Write-Host "Step 3: launch vmrp normally"
python .\scripts\pc_flatten_240x320.py launch

Write-Host "Step 4: collect logs"
python .\scripts\pc_flatten_240x320.py collect

Write-Host ""
Write-Host "Done. Send newest logs\flatten_feedback_*.zip"
