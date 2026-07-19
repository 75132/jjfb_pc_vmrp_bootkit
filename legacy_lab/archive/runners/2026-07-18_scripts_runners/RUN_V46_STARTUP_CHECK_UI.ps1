# JJFB v46 — startup check UI / animation (stay in 0x45; do not skip)
param(
  [ValidateSet("natural","force_slogo_once","force_slogo_once_then_release","force_loading","pulse")]
  [string]$Ac8Mode = "natural",
  [string]$Mode = "45",
  [string]$Ret = "1",
  [string]$ProgressNudge = "0",
  [string]$ProgressScan = "0",
  [string]$Ac8PulseTicks = "0",
  [string]$Tag = "",
  [int]$Seconds = 16
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"
$src = Join-Path $root "runtime\vmrp_src_build_v27\vmrp-master"
if (-not $Tag) {
  $Tag = "ac8${Ac8Mode}_p${ProgressNudge}_s${ProgressScan}_r${Ret}"
}

$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
$env:JJFB_SCREEN_W = "240"
$env:JJFB_SCREEN_H = "320"
$env:JJFB_ALLOW_CHROME = "1"
$env:JJFB_CHROME_ALLOW_CALLS = "4"
$env:JJFB_CHROME_SKIP_310BB4 = "1"
$env:JJFB_310BB4_HOST_BLIT = "1"
$env:JJFB_10134_EAGER_BLIT = "0"
$env:JJFB_10134_RET = "pixels"
$env:JJFB_SPLASH_HOST_BLIT = "1"
$env:JJFB_AXIS_FIX = "1"
$env:JJFB_COLORKEY = "auto"
$env:JJFB_AC8_MODE = $Ac8Mode
$env:JJFB_SPLASH_AC8_MODE = $Ac8Mode
$env:JJFB_2EC6B0_RET = $Ret
$env:JJFB_FORCE_SPLASH_NUDGE = $Mode
$env:JJFB_FORCE_UI_MODE = $Mode
$env:JJFB_PROGRESS_NUDGE = $ProgressNudge
$env:JJFB_PROGRESS_SCAN = $ProgressScan
$env:JJFB_AC8_PULSE_TICKS = $Ac8PulseTicks
Remove-Item Env:JJFB_SLOGO_NUDGE -ErrorAction SilentlyContinue

Write-Host "=== JJFB v46 startup-check tag=$Tag ac8=$Ac8Mode mode=$Mode progNudge=$ProgressNudge scan=$ProgressScan ==="

Push-Location $src
try {
  gcc -g -Wall -DNETWORK_SUPPORT -DVMRP -m32 -c bridge.c -o bridge.o
  if ($LASTEXITCODE -ne 0) { throw "compile failed" }
  gcc -g -Wall -DNETWORK_SUPPORT -DVMRP -m32 -o ./bin/main `
    network.o fileLib.o vmrp.o utils.o rbtree.o bridge.o memory.o main.o `
    ./windows/unicorn-1.0.2-win32/unicorn.lib `
    -lpthread -lm -lws2_32 -lz -lmingw32 -mconsole `
    -L./windows/SDL2-2.0.10/i686-w64-mingw32/lib/ -lSDL2main -lSDL2
  if ($LASTEXITCODE -ne 0) { throw "link failed" }
  Copy-Item -Force bin\main.exe (Join-Path $rt "main.exe")
} finally {
  Pop-Location
}

$stdoutPath = Join-Path $rt "jjfb_loader_stdout.txt"
if (Test-Path $stdoutPath) { Remove-Item $stdoutPath -Force }

$p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id)"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 400

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$out = Join-Path $logDir ("v46_" + $Tag + "_stdout.txt")
Copy-Item -Force $stdoutPath $out
Write-Host "saved $out"

$pat = "JJFB_STARTUP_PHASE|JJFB_STARTUP_STR|JJFB_PROGRESS_DRAW|JJFB_PROGRESS_PHASE|JJFB_PROGRESS_WRITE|JJFB_AC8_WRITE|JJFB_AC8\] pulse|SPLASH_PHASE|slogo|loadingbar|PROGRESS_DRAW|check_update|connecting"
Select-String -Path $out -Pattern $pat |
  Select-Object -First 55 | ForEach-Object { $_.Line }
