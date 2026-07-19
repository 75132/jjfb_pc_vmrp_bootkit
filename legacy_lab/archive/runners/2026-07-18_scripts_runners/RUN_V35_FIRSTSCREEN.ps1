# JJFB v35 — First-screen bring-up (机甲风暴启动/加载首屏)
# Goal: show guest/game buffer content. No host overlay. Chrome decorations skipped.
#
# Usage:
#   .\RUN_V35_FIRSTSCREEN.ps1

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"

# A. Keep chrome guard — skip decorative 310BB4 blit body
$env:JJFB_ALLOW_CHROME = "1"
$env:JJFB_CHROME_ALLOW_CALLS = "8"
$env:JJFB_CHROME_SKIP_310BB4 = "1"
$env:JJFB_310BB4_DUMP_N = "1"

Write-Host "=== JJFB v35 FIRSTSCREEN ==="
Write-Host "ALLOW_CHROME=$($env:JJFB_ALLOW_CHROME) SKIP_310BB4=$($env:JJFB_CHROME_SKIP_310BB4)"
Write-Host "WorkingDir=$rt"
Write-Host "No host overlay. DEBUG_PRESENT = guest buffer only."

if (Test-Path (Join-Path $rt "jjfb_loader_stdout.txt")) {
    Remove-Item (Join-Path $rt "jjfb_loader_stdout.txt") -Force
}

$p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id) — close window or Ctrl+C when done"
Wait-Process -Id $p.Id

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$out = Join-Path $logDir "v35_firstscreen_stdout.txt"
Copy-Item -Force (Join-Path $rt "jjfb_loader_stdout.txt") $out
Write-Host "saved $out"
