# Prepare clean vmrp run dir, then open the local GWY game list window.
param(
  [switch]$SkipBuild,
  [switch]$SkipResourceCopy,
  [string]$ResourceRoot = ""
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

if (-not $ResourceRoot) {
  $ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
}

# Ensure vmrp binary + mythroad tree are ready
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') `
  -SkipBuild:$SkipBuild -SkipResourceCopy:$SkipResourceCopy -NoLaunch -ResourceRoot $ResourceRoot
if ($LASTEXITCODE -ne 0) { throw 'vmrp prep failed' }

# Rebuild launcher with games UI
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }

$exe = Join-Path $Root 'build-i686\gwy_launcher.exe'
$vmrp = Join-Path $Root 'out\vmrp_run\main.exe'
$cwd = Join-Path $Root 'out\vmrp_run'

Write-Host '== scan (console) =='
& $exe scan --root $ResourceRoot

Write-Host '== games window =='
Write-Host 'Select a title and click 启动 (vmrp). Close list window when done.'
& $exe games --root $ResourceRoot --vmrp $vmrp --cwd $cwd
Write-Host "games_exit=$LASTEXITCODE"
