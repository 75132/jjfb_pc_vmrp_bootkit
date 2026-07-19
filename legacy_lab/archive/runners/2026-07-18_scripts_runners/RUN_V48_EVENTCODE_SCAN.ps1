# JJFB v48 — scan r1/event_code 0x00..0x30 for 2EFC40 / progress / AC8 hits
param(
  [int]$From = 0,
  [int]$To = 0x30,
  [int]$Seconds = 8,
  [switch]$SkipRebuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$script = Join-Path $root "RUN_V48_2EF86C_COVERAGE.ps1"
$logDir = Join-Path $root "logs"
$summary = Join-Path $logDir "v48_eventcode_scan_summary.txt"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
"" | Set-Content $summary

$built = $false
for ($ev = $From; $ev -le $To; $ev++) {
  $hex = ("{0:X}" -f $ev)
  $tag = "scan_ev$hex"
  Write-Host "======== EVENT 0x$hex ========"
  if (-not $SkipRebuild -and -not $built) {
    & $script -EventCode $hex -Tag $tag -Seconds $Seconds
    $built = $true
  } else {
    # Reuse binary; only change env and run
    $rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"
    $env:Path = "C:\msys64\mingw32\bin;" + $env:Path
    $env:JJFB_SCREEN_W = "240"; $env:JJFB_SCREEN_H = "320"
    $env:JJFB_ALLOW_CHROME = "1"; $env:JJFB_CHROME_ALLOW_CALLS = "4"
    $env:JJFB_CHROME_SKIP_310BB4 = "1"; $env:JJFB_310BB4_HOST_BLIT = "1"
    $env:JJFB_10134_EAGER_BLIT = "0"; $env:JJFB_10134_RET = "pixels"
    $env:JJFB_SPLASH_HOST_BLIT = "1"; $env:JJFB_AXIS_FIX = "1"; $env:JJFB_COLORKEY = "auto"
    $env:JJFB_AC8_MODE = "natural"; $env:JJFB_SPLASH_AC8_MODE = "natural"
    $env:JJFB_PROGRESS_DRIVER = "off"; $env:JJFB_2EC6B0_RET = "1"
    $env:JJFB_FORCE_SPLASH_NUDGE = "45"; $env:JJFB_FORCE_UI_MODE = "45"
    $env:JJFB_FORCE_2EFC_TAIL = "0"
    $env:JJFB_EVENT_CODE = $hex
    $stdoutPath = Join-Path $rt "jjfb_loader_stdout.txt"
    if (Test-Path $stdoutPath) { Remove-Item $stdoutPath -Force }
    $p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
    Start-Sleep -Seconds $Seconds
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 300
    $out = Join-Path $logDir ("v48_" + $tag + "_stdout.txt")
    Copy-Item -Force $stdoutPath $out
  }
  $out = Join-Path $logDir ("v48_" + $tag + "_stdout.txt")
  if (-not (Test-Path $out)) { continue }
  $tail = @(Select-String -Path $out -Pattern "2EFC_TAIL\] hit").Count
  $prog = @(Select-String -Path $out -Pattern "tag=progress_count").Count
  $ac8 = @(Select-String -Path $out -Pattern "tag=AC8 ").Count
  $str = @(Select-String -Path $out -Pattern "STARTUP_STR\] #").Count
  $max = (Select-String -Path $out -Pattern "max_pc=0x([0-9A-Fa-f]+)" | Select-Object -Last 1)
  $maxpc = if ($max) { $max.Matches[0].Groups[1].Value } else { "?" }
  $line = "ev=0x$hex tail=$tail prog_w=$prog ac8_w=$ac8 str=$str max_pc=0x$maxpc"
  Add-Content $summary $line
  Write-Host $line
  if ($tail -gt 0 -or $prog -gt 0 -or $ac8 -gt 0 -or $str -gt 0) {
    Write-Host "*** HIT interesting for 0x$hex ***"
  }
  # After first build, skip rebuild for rest
  $SkipRebuild = $true
}

Write-Host "=== summary saved $summary ==="
Get-Content $summary
