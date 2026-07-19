$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

Write-Host "== JJFB PC vmrp Bootkit =="
Write-Host "Step 1: download/extract vmrp"
powershell -ExecutionPolicy Bypass -File .\scripts\01_download_vmrp.ps1

Write-Host "Step 2: prepare mythroad fs"
python .\scripts\pc_bootkit.py prepare

Write-Host "Step 3: start mock server"
$mock = Start-Process powershell -PassThru -ArgumentList "-NoExit", "-ExecutionPolicy", "Bypass", "-Command", "cd `"$ROOT`"; python .\mock\jjfb_mock_server.py"
Start-Sleep -Seconds 2

Write-Host "Step 4: try hosts mapping"
powershell -ExecutionPolicy Bypass -File .\scripts\02_hosts_map_admin_optional.ps1

Write-Host "Step 5: snapshot pre"
python .\scripts\pc_bootkit.py snapshot --phase pre

Write-Host "Step 6: launch vmrp attempts"
python .\scripts\pc_bootkit.py launch

Write-Host "Step 7: snapshot post"
python .\scripts\pc_bootkit.py snapshot --phase post

Write-Host "Step 8: collect feedback"
python .\scripts\pc_bootkit.py collect

Write-Host ""
Write-Host "Done. Send back newest logs\feedback_*.zip"
