$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent $PSScriptRoot
Set-Location $ROOT

$config = Get-Content .\CONFIG.json -Raw | ConvertFrom-Json
$url = $config.vmrp_release_url
$zipPath = Join-Path $ROOT $config.vmrp_zip
$outDir = Join-Path $ROOT $config.vmrp_extract_dir

New-Item -ItemType Directory -Force -Path (Split-Path $zipPath -Parent) | Out-Null
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (!(Test-Path $zipPath)) {
    Write-Host "Downloading vmrp release..."
    Write-Host $url
    try {
        Invoke-WebRequest -Uri $url -OutFile $zipPath
    } catch {
        Write-Host "Invoke-WebRequest failed, trying curl.exe..."
        curl.exe -L $url -o $zipPath
    }
} else {
    Write-Host "vmrp zip already exists: $zipPath"
}

if ((Test-Path $zipPath) -and ((Get-Item $zipPath).Length -gt 1024)) {
    Write-Host "Extracting..."
    Expand-Archive -Force -Path $zipPath -DestinationPath $outDir
} else {
    Write-Host "Download failed or zip too small. You can manually place vmrp files into runtime/vmrp_win32/"
}
