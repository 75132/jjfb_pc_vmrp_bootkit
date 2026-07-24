# Double-click / one-shot JJFB product launcher (Task 1–2).
# Builds JJFB_Launcher.exe if needed, ensures Gwy main.exe exists, then starts the status window.
param(
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild,
  [switch]$Debug,
  [switch]$TestPattern
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$Launcher = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$Mrp = Join-Path $ResourceRoot 'gwy\jjfb.mrp'

if (-not (Test-Path $Mrp)) { throw "missing $Mrp" }

if (-not $SkipBuild) {
  Write-Host '== build JJFB_Launcher =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }
}
if (-not $SkipVmrpBuild -and -not (Test-Path $MainExe)) {
  Write-Host '== build Gwy runtime =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'vmrp build failed' }
}
if (-not (Test-Path $Launcher)) { throw "missing $Launcher" }
if (-not (Test-Path $MainExe)) { throw "missing $MainExe — run RUN_BUILD_VMRP.ps1 -Mode Gwy" }

# Sync resources into run dir (same as visual path).
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'resource prepare failed' }

$args = @()
if ($Debug) { $args += '--debug' }
if ($TestPattern) { $args += '--test-pattern' }

Write-Host '== start JJFB Launcher =='
Write-Host "exe=$Launcher"
Write-Host 'Close the JJFB Launcher status window to stop the runtime.'
Start-Process -FilePath $Launcher -ArgumentList $args -WorkingDirectory $Root
