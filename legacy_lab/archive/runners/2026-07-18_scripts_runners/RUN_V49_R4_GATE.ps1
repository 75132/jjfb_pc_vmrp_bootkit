# JJFB v49 鈥?r4 / B6C / 134D / AC8 gate probes
# See docs/HANDOFF.md and docs/reports/v49_splash_gates.md
param(
  [string]$ForceR4 = "0",
  [string]$ForceB6C = "0",
  [string]$Force134D = "0",
  [string]$ForceAc8Gate = "0",
  [string]$ForceTail = "0",
  [string]$Mode = "45",
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
  $parts = @()
  if ($ForceB6C -ne "0") { $parts += "b6c$ForceB6C" }
  if ($Force134D -ne "0") { $parts += "134d$Force134D" }
  if ($ForceAc8Gate -ne "0") { $parts += "ac8g$ForceAc8Gate" }
  if ($ForceR4 -ne "0") { $parts += "r4$ForceR4" }
  if ($parts.Count -eq 0) { $Tag = "natural" } else { $Tag = ($parts -join "_") }
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
$env:JJFB_FORCE_R4 = $ForceR4
$env:JJFB_FORCE_B6C = $ForceB6C
$env:JJFB_FORCE_134D = $Force134D
$env:JJFB_FORCE_AC8_GATE = $ForceAc8Gate
Remove-Item Env:JJFB_EVENT_CODE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_SLOGO_NUDGE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PROGRESS_SCAN -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PROGRESS_NUDGE -ErrorAction SilentlyContinue

Write-Host "=== JJFB v49 tag=$Tag B6C=$ForceB6C 134D=$Force134D AC8G=$ForceAc8Gate ==="

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
$out = Join-Path $logDir ("v49_" + $Tag + "_stdout.txt")
Copy-Item -Force $stdoutPath $out
Write-Host "saved $out"

$pat = "JJFB_FORCE_|JJFB_B6C_GATE|JJFB_AC8TAIL\]|JJFB_2EF86C_COV\] summary|JJFB_2EFC_GATE|ERW_WRITE.*progress|ERW_WRITE.*AC8|ERW_WRITE.*B6C|STARTUP_STR\] #|tag=progress_count|tag=AC8 |first pc=0x2EFC0[2-9A-F]|first pc=0x2EFC[1-3]|first pc=0x2EFBA[A-F]|first pc=0x2EFBB|first pc=0x2EFBC|first pc=0x2EFBD|first pc=0x2EFBE"
Select-String -Path $out -Pattern $pat | Select-Object -First 120 | ForEach-Object { $_.Line }
Write-Host "---"
Write-Host "max summary:"; Select-String -Path $out -Pattern "summary hit_halfwords" | Select-Object -Last 1 | ForEach-Object { $_.Line }
Write-Host "success-path firsts (BAA-BEE): $((@(Select-String -Path $out -Pattern 'first pc=0x2EFBA[A-F]|first pc=0x2EFBB|first pc=0x2EFBC|first pc=0x2EFBD|first pc=0x2EFBE').Count))"
Write-Host "2EFC02+ firsts: $((@(Select-String -Path $out -Pattern 'first pc=0x2EFC0[2-9A-F]|first pc=0x2EFC[1-3]').Count))"
Write-Host "progress_count writes: $((@(Select-String -Path $out -Pattern 'tag=progress_count').Count))"
Write-Host "AC8 writes: $((@(Select-String -Path $out -Pattern 'tag=AC8 ').Count))"
Write-Host "STARTUP_STR: $((@(Select-String -Path $out -Pattern 'STARTUP_STR\] #').Count))"
