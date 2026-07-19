# JJFB v47 — startup progress driver probe + writer/timer logs
param(
  [ValidateSet("natural","force_slogo_once","force_slogo_once_then_release","force_loading","pulse")]
  [string]$Ac8Mode = "natural",
  [ValidateSet("off","linear","step")]
  [string]$Driver = "linear",
  [string]$Mode = "45",
  [string]$Ac8PulseTicks = "0",
  [string]$Ret = "1",
  [string]$Tag = "",
  [int]$Seconds = 18
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"
$src = Join-Path $root "runtime\vmrp_src_build_v27\vmrp-master"
if (-not $Tag) { $Tag = "drv${Driver}_ac8${Ac8Mode}" }

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
$env:JJFB_PROGRESS_DRIVER = $Driver
$env:JJFB_2EC6B0_RET = $Ret
$env:JJFB_FORCE_SPLASH_NUDGE = $Mode
$env:JJFB_FORCE_UI_MODE = $Mode
$env:JJFB_AC8_PULSE_TICKS = $Ac8PulseTicks
$env:JJFB_PROGRESS_SCAN = "0"
$env:JJFB_PROGRESS_NUDGE = "0"
Remove-Item Env:JJFB_SLOGO_NUDGE -ErrorAction SilentlyContinue

Write-Host "=== JJFB v47 tag=$Tag driver=$Driver ac8=$Ac8Mode ==="

Push-Location $src
try {
  Remove-Item bridge.o -Force -ErrorAction SilentlyContinue
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
Start-Sleep -Milliseconds 500

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$out = Join-Path $logDir ("v47_" + $Tag + "_stdout.txt")
Copy-Item -Force $stdoutPath $out
Write-Host "saved $out"

$pat = "JJFB_ERW_WRITE|JJFB_XREF_LIT|JJFB_10140_REG|JJFB_TIMER_DISPATCH|JJFB_PROGRESS_DRIVER|JJFB_PROGRESS_DRAW|JJFB_STARTUP_STR\] #|AC8_WRITE|PROGRESS_WRITE|delta_prog"
Select-String -Path $out -Pattern $pat | Select-Object -First 70 | ForEach-Object { $_.Line }
Write-Host "--- counts ---"
@("ERW_WRITE.*progress_count","ERW_WRITE.*AC8","PROGRESS_DRIVER","PROGRESS_DRAW","STARTUP_STR\] #","XREF_LIT","10140_REG","delta_prog=[1-9]") | ForEach-Object {
  $c = @(Select-String -Path $out -Pattern $_).Count
  Write-Host "$_ = $c"
}
