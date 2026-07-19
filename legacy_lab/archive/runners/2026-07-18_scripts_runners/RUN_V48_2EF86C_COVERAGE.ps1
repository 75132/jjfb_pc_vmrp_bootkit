# JJFB v48 — 0x2EF86C coverage + optional single event_code
param(
  [string]$EventCode = "",          # empty = natural r1; hex/dec override
  [string]$ForceTail = "0",         # JJFB_FORCE_2EFC_TAIL
  [string]$Mode = "45",
  [string]$Tag = "",
  [int]$Seconds = 14
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
  if ($EventCode) { $Tag = "ev$EventCode" } else { $Tag = "cov_natural" }
  if ($ForceTail -eq "1") { $Tag += "_forcetail" }
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
$env:JJFB_AC8_MODE = "natural"
$env:JJFB_SPLASH_AC8_MODE = "natural"
$env:JJFB_PROGRESS_DRIVER = "off"
$env:JJFB_2EC6B0_RET = "1"
$env:JJFB_FORCE_SPLASH_NUDGE = $Mode
$env:JJFB_FORCE_UI_MODE = $Mode
$env:JJFB_FORCE_2EFC_TAIL = $ForceTail
if ($EventCode) { $env:JJFB_EVENT_CODE = $EventCode }
else { Remove-Item Env:JJFB_EVENT_CODE -ErrorAction SilentlyContinue }
Remove-Item Env:JJFB_SLOGO_NUDGE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PROGRESS_SCAN -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PROGRESS_NUDGE -ErrorAction SilentlyContinue

Write-Host "=== JJFB v48 tag=$Tag event=$EventCode forceTail=$ForceTail ==="

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
Start-Sleep -Milliseconds 400

$logDir = Join-Path $root "logs"
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$out = Join-Path $logDir ("v48_" + $Tag + "_stdout.txt")
Copy-Item -Force $stdoutPath $out
Write-Host "saved $out"

$pat = "JJFB_2EF86C_COV|JJFB_2EFC_|JJFB_HANDLER_306|JJFB_DISPATCH_306|JJFB_EVENT_CODE|JJFB_FORCE_2EFC|ERW_WRITE.*progress|ERW_WRITE.*AC8|STARTUP_STR\] #|2EFC_GATE"
Select-String -Path $out -Pattern $pat | Select-Object -First 60 | ForEach-Object { $_.Line }
Write-Host "---"
Write-Host "2EFC_TAIL hits: $((@(Select-String -Path $out -Pattern '2EFC_TAIL\] hit').Count))"
Write-Host "GATE not reached: $((@(Select-String -Path $out -Pattern '2EFC_GATE\] not reached').Count))"
Write-Host "progress_count writes: $((@(Select-String -Path $out -Pattern 'tag=progress_count').Count))"
Write-Host "AC8 writes: $((@(Select-String -Path $out -Pattern 'tag=AC8 ').Count))"
Write-Host "STARTUP_STR: $((@(Select-String -Path $out -Pattern 'STARTUP_STR\] #').Count))"
