# Build upstream vmrp PE32 as plain (weak) or gwy (VmFileService linked).
param(
  [ValidateSet('Plain','Gwy')]
  [string]$Mode = 'Gwy',
  [string]$LauncherBuildDir = 'build-i686'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$Upstream = Join-Path $Root 'third_party\vmrp_upstream'
$LauncherLib = Join-Path $Root "$LauncherBuildDir\liblauncher_core.a"

if ($Mode -eq 'Gwy') {
  Write-Host '== build launcher_core first =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir $LauncherBuildDir
  if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }
  if (-not (Test-Path $LauncherLib)) { throw "missing $LauncherLib" }
}

Write-Host "== build vmrp Mode=$Mode =="
Push-Location $Upstream
try {
  & mingw32-make.exe clean 2>$null
  if ($Mode -eq 'Gwy') {
    & mingw32-make.exe gwy "LAUNCHER_LIB=$LauncherLib" "LAUNCHER_INC=$Root\include"
  } else {
    & mingw32-make.exe plain
  }
  if ($LASTEXITCODE -ne 0) { throw "vmrp $Mode build failed" }
} finally { Pop-Location }

$OutPlain = Join-Path $Root 'out\vmrp_plain'
$OutGwy = Join-Path $Root 'out\vmrp_run'
New-Item -ItemType Directory -Force -Path $OutPlain, $OutGwy | Out-Null

$SdlDll = Join-Path $Upstream 'windows\SDL2-2.0.10\i686-w64-mingw32\bin\SDL2.dll'
$UcDll = Join-Path $Upstream 'windows\unicorn-1.0.2-win32\unicorn.dll'
# DSM firmware required by loadCode() before any GWY_LAUNCH prepare.
$Cfunction = Join-Path $Upstream 'wasm\dist\fs\cfunction.ext'
if (-not (Test-Path $Cfunction)) { throw "missing DSM firmware $Cfunction" }

function Stop-RuntimeLocks([string]$Dest) {
  $mainExe = Join-Path $Dest 'main.exe'
  Get-Process -Name main -ErrorAction SilentlyContinue | ForEach-Object {
    try {
      $path = $_.Path
      if ($path -and ($path -ieq $mainExe -or $path -like '*\out\vmrp_run\*' -or $path -like '*\out\vmrp_plain\*')) {
        Write-Host "[WARN] stopping locked runtime pid=$($_.Id) path=$path"
        Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
      }
    } catch {}
  }
  Start-Sleep -Milliseconds 400
}

function Deploy-Runtime([string]$Dest, [string]$ExeSrc, [string]$AlsoAs = '') {
  Stop-RuntimeLocks $Dest
  $destMain = Join-Path $Dest 'main.exe'
  $copied = $false
  foreach ($try in 1..5) {
    try {
      Copy-Item -Force $ExeSrc $destMain
      $copied = $true
      break
    } catch {
      Write-Host "[WARN] copy main.exe locked (try $try/5); retrying..."
      Stop-RuntimeLocks $Dest
      Start-Sleep -Milliseconds (300 * $try)
    }
  }
  if (-not $copied) { throw "cannot deploy $destMain (file locked by running main.exe)" }
  Copy-Item -Force $SdlDll $Dest
  Copy-Item -Force $UcDll $Dest
  Copy-Item -Force $Cfunction (Join-Path $Dest 'cfunction.ext')
  if ($AlsoAs) { Copy-Item -Force $ExeSrc (Join-Path $Dest $AlsoAs) }
}

if ($Mode -eq 'Gwy') {
  $src = Join-Path $Upstream 'bin\main_gwy.exe'
  if (-not (Test-Path $src)) { throw "missing $src" }
  Deploy-Runtime -Dest $OutGwy -ExeSrc $src -AlsoAs 'main_gwy.exe'
  Write-Host "[OK] GWY runtime -> $OutGwy\main.exe (+ cfunction.ext)"
} else {
  $src = Join-Path $Upstream 'bin\main_plain.exe'
  if (-not (Test-Path $src)) { throw "missing $src" }
  Deploy-Runtime -Dest $OutPlain -ExeSrc $src
  Write-Host "[OK] plain baseline -> $OutPlain\main.exe (+ cfunction.ext)"
}
