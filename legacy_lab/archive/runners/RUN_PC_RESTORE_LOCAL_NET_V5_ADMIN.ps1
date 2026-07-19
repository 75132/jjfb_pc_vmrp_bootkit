$ErrorActionPreference = "Continue"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT

function Test-Admin {
    $current = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($current)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if (!(Test-Admin)) {
    Write-Host "ERROR: 这个脚本必须用管理员 PowerShell 运行。"
    exit 1
}

powershell -ExecutionPolicy Bypass -File .\scripts\remove_ip_aliases_v5.ps1
python .\scripts\pc_localnet_v5.py restore
Write-Host "Restored local aliases and dsm_gm if backup exists."
