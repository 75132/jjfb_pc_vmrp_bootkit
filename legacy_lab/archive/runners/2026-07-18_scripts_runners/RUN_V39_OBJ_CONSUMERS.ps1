# JJFB v39 — bitmap object consumers + slogo gate
# Default: no eager blit. Watch object/parent readers. Optional ui_mode nudge / flag A/B.

param(
  [string]$Mode = "45",
  [string]$Flag = "",
  [int]$Seconds = 0
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"

$env:JJFB_SCREEN_W = "240"
$env:JJFB_SCREEN_H = "320"
$env:JJFB_ALLOW_CHROME = "1"
$env:JJFB_CHROME_ALLOW_CALLS = "8"
$env:JJFB_CHROME_SKIP_310BB4 = "1"
$env:JJFB_310BB4_HOST_BLIT = "1"
$env:JJFB_10134_EAGER_BLIT = "0"
$env:JJFB_10134_RET = "pixels"
$env:JJFB_FORCE_SPLASH_NUDGE = $Mode
if ($Flag) {
  $env:JJFB_OBJ_SET_FLAG = $Flag
  $env:JJFB_OBJ_SET_FLAG_VAL = "1"
} else {
  Remove-Item Env:JJFB_OBJ_SET_FLAG -ErrorAction SilentlyContinue
}

Write-Host "=== JJFB v39 consumers mode=$Mode flag=$Flag ==="

if (Test-Path (Join-Path $rt "jjfb_loader_stdout.txt")) {
  Remove-Item (Join-Path $rt "jjfb_loader_stdout.txt") -Force
}

$p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id)"
if ($Seconds -gt 0) {
  Start-Sleep -Seconds $Seconds
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
} else {
  Wait-Process -Id $p.Id
}

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$tag = "v39_mode$Mode"
if ($Flag) { $tag += "_flag$Flag" }
$out = Join-Path $logDir ($tag + "_stdout.txt")
Copy-Item -Force (Join-Path $rt "jjfb_loader_stdout.txt") $out
Write-Host "saved $out"
