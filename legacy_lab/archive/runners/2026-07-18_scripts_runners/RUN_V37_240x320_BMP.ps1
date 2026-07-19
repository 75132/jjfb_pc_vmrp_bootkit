# JJFB v37 — fixed 240x320 + 0x10134 RGB565 construct + 310BB4 host blit
# Goal: show original loadingbar/bar/textbar in guest screen buffer (DEBUG_PRESENT only).

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
$env:JJFB_310BB4_DUMP_N = "1"
$env:JJFB_10134_EAGER_BLIT = "1"

Write-Host "=== JJFB v37 240x320 BMP ==="
Write-Host "HOST_BLIT=$($env:JJFB_310BB4_HOST_BLIT) EAGER=$($env:JJFB_10134_EAGER_BLIT) SKIP_310BB4=$($env:JJFB_CHROME_SKIP_310BB4)"
Write-Host "WorkingDir=$rt"

if (Test-Path (Join-Path $rt "jjfb_loader_stdout.txt")) {
    Remove-Item (Join-Path $rt "jjfb_loader_stdout.txt") -Force
}

$p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id) — close window or Ctrl+C when done"
Wait-Process -Id $p.Id

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$out = Join-Path $logDir "v37_240x320_bmp_stdout.txt"
Copy-Item -Force (Join-Path $rt "jjfb_loader_stdout.txt") $out
Write-Host "saved $out"
