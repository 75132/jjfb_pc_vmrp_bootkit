# jjfb.mrp visual smoke: build Gwy runtime + launcher, hold until first DrawFP frame.
# Primary acceptance: DRAW_FP / FIRST_REAL_FRAME / nonempty screenshot under out/vmrp_run/screenshots.
param(
  [int]$HoldSeconds = 45,
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$shot = Join-Path $Root 'out\vmrp_run\screenshots\launcher_first_frame.bmp'
$progress = Join-Path $Root 'out\vmrp_run\runtime_progress.jsonl'
Remove-Item -Force $shot, $progress -ErrorAction SilentlyContinue

Write-Host "== JJFB visual smoke HoldSeconds=$HoldSeconds =="
$args = @('-HoldSeconds', "$HoldSeconds")
if ($SkipBuild) { $args += '-SkipBuild' }
if ($SkipVmrpBuild) { $args += '-SkipVmrpBuild' }

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_JJFB_LAUNCHER.ps1') @args
if ($LASTEXITCODE -ne 0) { throw "RUN_JJFB_LAUNCHER failed: $LASTEXITCODE" }

$okFrame = $false
$okProgress = $false
if (Test-Path $shot) {
  $len = (Get-Item $shot).Length
  $okFrame = $len -gt 10000
  Write-Host "screenshot=$shot bytes=$len ok=$okFrame"
} else {
  Write-Host "screenshot=MISSING path=$shot"
}
if (Test-Path $progress) {
  $txt = Get-Content $progress -Raw -ErrorAction SilentlyContinue
  $okProgress = [bool]($txt -match 'drawfp_first_drawn|drawfp_call|FIRST_REAL|first_frame')
  Write-Host "progress_hit=$okProgress"
  Get-Content $progress -Tail 20
}

if (-not $okFrame -and -not $okProgress) {
  throw 'JJFB visual smoke FAIL: no first frame screenshot and no drawfp progress'
}
Write-Host '[OK] JJFB visual smoke — jjfb.mrp frame path reached'
exit 0
