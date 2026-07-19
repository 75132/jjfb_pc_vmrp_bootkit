# JJFB v38 — real coords / 10134 return ABI / slogo xref
# Default: NO eager blit. Probe guest draw path (30680C) + pixel/slogo watches.

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"

$mode = $args[0]
if (-not $mode) { $mode = "pixels" }

$env:JJFB_SCREEN_W = "240"
$env:JJFB_SCREEN_H = "320"
$env:JJFB_ALLOW_CHROME = "1"
$env:JJFB_CHROME_ALLOW_CALLS = "8"
$env:JJFB_CHROME_SKIP_310BB4 = "1"
$env:JJFB_310BB4_HOST_BLIT = "1"
$env:JJFB_10134_EAGER_BLIT = "0"
$env:JJFB_10134_RET = $mode

Write-Host "=== JJFB v38 real-coords mode=$mode ==="
Write-Host "EAGER=$($env:JJFB_10134_EAGER_BLIT) RET=$($env:JJFB_10134_RET)"

if (Test-Path (Join-Path $rt "jjfb_loader_stdout.txt")) {
    Remove-Item (Join-Path $rt "jjfb_loader_stdout.txt") -Force
}

$p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id)"
Wait-Process -Id $p.Id

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$out = Join-Path $logDir "v38_${mode}_stdout.txt"
Copy-Item -Force (Join-Path $rt "jjfb_loader_stdout.txt") $out
Write-Host "saved $out"
