# JJFB v44 — game state / progress writer trace (not UI polish)
param(
  [ValidateSet("natural","force_slogo_once","force_loading")]
  [string]$Ac8Mode = "natural",
  [string]$Mode = "45",          # FORCE_SPLASH_NUDGE hex; "0" = no force
  [string]$Ret = "1",            # JJFB_2EC6B0_RET: 0|1|obj|pixels
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
if (-not $Tag) { $Tag = "m${Mode}_r${Ret}_$Ac8Mode" }

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
$env:JJFB_SPLASH_AC8_MODE = $Ac8Mode
$env:JJFB_2EC6B0_RET = $Ret
$env:JJFB_FORCE_SPLASH_NUDGE = $Mode
if ($Mode -eq "0") {
  $env:JJFB_FORCE_UI_MODE = "0"
} else {
  Remove-Item Env:JJFB_FORCE_UI_MODE -ErrorAction SilentlyContinue
}
Remove-Item Env:JJFB_SLOGO_NUDGE -ErrorAction SilentlyContinue

Write-Host "=== JJFB v44 state-trace tag=$Tag ac8=$Ac8Mode mode=$Mode ret=$Ret ==="

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

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$out = Join-Path $logDir ("v44_" + $Tag + "_stdout.txt")
Copy-Item -Force $stdoutPath $out
Write-Host "saved $out"

$pat = "JJFB_UIMODE_WRITE|JJFB_PROGRESS_COUNT|JJFB_AC8_WRITE|JJFB_UI_DISPATCH|JJFB_2EC6B0_SKIP|JJFB_2EC6B0_GUARD|JJFB_NET|initNetwork|login|server|NO FORCE|FORCE state|SPLASH_ENTER|GAME_STATE"
Select-String -Path $out -Pattern $pat |
  Select-Object -First 60 | ForEach-Object { $_.Line }
