# Double-click / one-shot JJFB product launcher.
# Builds JJFB_Launcher.exe if needed, ensures Gwy main.exe exists, then starts the status window.
param(
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild,
  [switch]$Debug,
  [switch]$Diagnostic,
  [switch]$TestPattern,
  [string]$ResourceRoot = "",
  [int]$HoldSeconds = 0,
  [switch]$RequireCatalog
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root
. (Join-Path $Root 'tools\JjfbLayer1Gate.ps1')

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$Launcher = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
if (-not $ResourceRoot) {
  $ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
}
$Mrp = Join-Path $ResourceRoot 'gwy\jjfb.mrp'

if (-not (Test-Path $Mrp)) { throw "missing $Mrp" }

if (-not $SkipBuild) {
  Write-Host '== build JJFB_Launcher =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }
}
if (-not $SkipVmrpBuild) {
  Write-Host '== build Gwy runtime =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'vmrp build failed' }
}
if (-not (Test-Path $Launcher)) { throw "missing $Launcher" }
if (-not (Test-Path $MainExe)) { throw "missing $MainExe — run RUN_BUILD_VMRP.ps1 -Mode Gwy" }

# Sync resources into run dir (same as visual path).
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch -ResourceRoot $ResourceRoot
if ($LASTEXITCODE -ne 0) { throw 'resource prepare failed' }

$launchArgs = @()
if ($Debug) { $launchArgs += '--debug' }
if ($Diagnostic) { $launchArgs += '--diagnostic' }
if ($TestPattern) { $launchArgs += '--test-pattern' }
if ($ResourceRoot) {
  $launchArgs += '--root'
  $launchArgs += $ResourceRoot
}

Write-Host '== start JJFB Launcher =='
Write-Host "exe=$Launcher"
Write-Host "args=$($launchArgs -join ' ')"
Write-Host 'Close the JJFB Launcher status window to stop the runtime.'

if ($HoldSeconds -gt 0) {
  Remove-Item -Force (Join-Path $RunDir 'runtime_progress.jsonl'),
    (Join-Path $RunDir 'runtime_process.json'),
    (Join-Path $RunDir 'screenshots\launcher_first_frame.bmp') -ErrorAction SilentlyContinue
  # Single-string ArgumentList avoids PowerShell array join quirks.
  $argLine = ($launchArgs | ForEach-Object {
      if ($_ -match '\s') { '"{0}"' -f $_ } else { $_ }
    }) -join ' '
  if ($argLine) {
    $p = Start-Process -FilePath $Launcher -ArgumentList $argLine -WorkingDirectory $Root -PassThru
  } else {
    $p = Start-Process -FilePath $Launcher -WorkingDirectory $Root -PassThru
  }
  Write-Host "launcher_pid=$($p.Id) hold=${HoldSeconds}s args=$argLine"
  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  $progress = Join-Path $RunDir 'runtime_progress.jsonl'
  $sawFrame = $false
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if (-not $sawFrame -and (Test-Path $progress)) {
      $txt = Get-Content $progress -Raw -ErrorAction SilentlyContinue
      if ($txt -match 'drawfp_first_drawn|FIRST_REAL_FRAME') {
        $sawFrame = $true
        Write-Host "first_frame_seen; remaining hold until deadline"
      }
    }
  }

  $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
  $archive = Join-Path $Root "out\visual_baseline\gate_$stamp"
  $gateArgs = @{ RunDir = $RunDir; ArchiveDir = $archive; Require240 = $true }
  if ($RequireCatalog) { $gateArgs.RequireCatalog = $true }
  $gate = Test-JjfbLayer1Gate @gateArgs

  $runtimePid = Get-JjfbRuntimePid -RunDir $RunDir
  if ($null -ne $runtimePid) {
    Stop-Process -Id $runtimePid -Force -ErrorAction SilentlyContinue
  }
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Write-Host 'hold_done'
  if (-not $gate.pass) {
    throw "LAYER1_GATE_FAIL: $($gate.fail_reasons -join ',')"
  }
  Write-Host '[OK] Layer-1 first-frame gate PASS'
} else {
  if ($launchArgs.Count -gt 0) {
    Start-Process -FilePath $Launcher -ArgumentList $launchArgs -WorkingDirectory $Root
  } else {
    Start-Process -FilePath $Launcher -WorkingDirectory $Root
  }
}
