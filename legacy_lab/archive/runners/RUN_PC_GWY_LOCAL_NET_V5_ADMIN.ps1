$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
New-Item -ItemType Directory -Force -Path logs | Out-Null

function Test-Admin {
    $current = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($current)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (!(Test-Admin)) {
    Write-Host "ERROR: 这个脚本必须用管理员 PowerShell 运行。"
    Write-Host "右键 PowerShell -> 以管理员身份运行，然后 cd 到本目录再运行。"
    exit 1
}

Write-Host "== JJFB PC local network v5 =="
Write-Host "Step 1: prepare direct gwy boot"
python .\scripts\pc_localnet_v5.py prepare

Write-Host "Step 2: stop old mock processes on ports"
powershell -ExecutionPolicy Bypass -File .\scripts\stop_mock_ports_v5.ps1

Write-Host "Step 3: add local IP aliases"
powershell -ExecutionPolicy Bypass -File .\scripts\add_ip_aliases_v5.ps1

Write-Host "Step 4: start local mock"
$mock = Start-Process powershell -PassThru -ArgumentList "-NoExit", "-ExecutionPolicy", "Bypass", "-Command", "cd `"$ROOT`"; python .\mock\jjfb_mock_server.py"
Start-Sleep -Seconds 2

Write-Host "Step 5: quick local route test"
powershell -ExecutionPolicy Bypass -File .\scripts\test_local_alias_v5.ps1

Write-Host "Step 6: launch vmrp with gwy as dsm_gm"
python .\scripts\pc_localnet_v5.py launch

Write-Host "Step 7: collect feedback"
python .\scripts\pc_localnet_v5.py collect

Write-Host ""
Write-Host "Done. Send newest logs\localnet_feedback_*.zip"
