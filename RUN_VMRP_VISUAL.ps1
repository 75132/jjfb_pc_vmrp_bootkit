# Launch clean upstream vmrp with an SDL window (visual smoke).
# This is NOT the old 385KB JJFB bridge — it uses third_party/vmrp_upstream.
param(
  [switch]$SkipBuild,
  [switch]$SkipResourceCopy,
  [switch]$NoLaunch,
  [string]$ResourceRoot = ""
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$Upstream = Join-Path $Root 'third_party\vmrp_upstream'
$RunDir = Join-Path $Root 'out\vmrp_run'
$Mythroad = Join-Path $RunDir 'mythroad'
if (-not $ResourceRoot) {
  $ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
}

$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }

# Preflight: format validate (no GUI)
$validate = Join-Path $Root 'build-i686\gwy_launcher.exe'
if (Test-Path $validate) {
  Write-Host '== gwy_launcher validate =='
  & $validate validate --root $ResourceRoot
  if ($LASTEXITCODE -ne 0) { throw 'validate failed' }
}

if (-not $SkipBuild) {
  Write-Host '== build GWY runtime (VmFileService linked) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP Gwy failed' }
}

$exeSrc = Join-Path $RunDir 'main.exe'
if (-not (Test-Path $exeSrc)) { throw "missing $exeSrc (run RUN_BUILD_VMRP -Mode Gwy)" }

New-Item -ItemType Directory -Force -Path $RunDir, $Mythroad | Out-Null

if (-not $SkipResourceCopy) {
  Write-Host "== sync resources $ResourceRoot -> $Mythroad =="
  & robocopy $ResourceRoot $Mythroad /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy failed: $LASTEXITCODE" }
}

# Phase 4B: sdk_key lives in overlay/generated — do NOT write cwd/mythroad/sdk_key.dat
$legacyKey = Join-Path $Mythroad 'sdk_key.dat'
if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
$env:GWY_RESOURCE_ROOT = $ResourceRoot
$env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'vmrp_visual_stdout.txt'
$stderr = Join-Path $logDir 'vmrp_visual_stderr.txt'

Write-Host '== launch SDL window (close window to end) =='
Write-Host "run_dir=$RunDir"
Write-Host "target=gwy/jjfb.mrp (cfg36 param)"

if ($NoLaunch) {
  Write-Host '[OK] prepared only (-NoLaunch)'
  exit 0
}

$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'

$p = Start-Process -FilePath (Join-Path $RunDir 'main.exe') `
  -WorkingDirectory $RunDir `
  -RedirectStandardOutput $stdout `
  -RedirectStandardError $stderr `
  -PassThru

Write-Host "pid=$($p.Id)  logs: $stdout"
Write-Host 'SDL window should be open. Close it when done.'
