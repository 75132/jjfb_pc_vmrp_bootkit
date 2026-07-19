# JJFB v50 - GWY Launcher Shim / canonical resource-root run
param(
  [int]$Seconds = 25,
  [switch]$SkipBuild,
  [switch]$SkipResourceCopy
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"
$src = Join-Path $root "runtime\vmrp_src_build_v27\vmrp-master"
$resourceSrc = Join-Path $root "game_files\mythroad\240x320"
$mythroadRoot = Join-Path $rt "mythroad\240x320"
$gwyRoot = Join-Path $mythroadRoot "gwy"
$logDir = Join-Path $root "logs"
$reportDir = Join-Path $root "reports"
$stdoutPath = Join-Path $rt "jjfb_loader_stdout.txt"
$stderrPath = Join-Path $rt "jjfb_loader_stderr.txt"
$out = Join-Path $logDir "v50_gwy_launcher_mode_stdout.txt"
$errOut = Join-Path $logDir "v50_gwy_launcher_mode_stderr.txt"
$reportOut = Join-Path $reportDir "v50_gwy_launcher_run_result.md"

New-Item -ItemType Directory -Force -Path $logDir, $reportDir | Out-Null

Write-Host "=== JJFB v50 GWY Launcher Mode ==="
Write-Host "project=$root"
Write-Host "runtime=$rt"

if (-not (Test-Path (Join-Path $resourceSrc "gwy\jjfb.mrp"))) {
  throw "canonical source target missing: $resourceSrc\gwy\jjfb.mrp"
}

if (-not $SkipResourceCopy) {
  New-Item -ItemType Directory -Force -Path $mythroadRoot | Out-Null
  Write-Host "[1/4] Copy full mythroad/240x320 tree (structure preserved)"
  & robocopy $resourceSrc $mythroadRoot /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /NFL /NDL /NJH /NJS /NP
  $rc = $LASTEXITCODE
  if ($rc -ge 8) { throw "robocopy failed with exit code $rc" }
}

$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
$env:JJFB_GWY_LAUNCHER_MODE = "1"
$env:JJFB_MYTHROAD_ROOT = $mythroadRoot
$env:JJFB_GWY_ROOT = $gwyRoot
$env:JJFB_GWY_PARAM = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$env:JJFB_SCREEN_W = "240"
$env:JJFB_SCREEN_H = "320"

# v49 and older UI/state-force probes are explicitly not part of launcher mode.
$legacyVars = @(
  "JJFB_FORCE_UI_MODE", "JJFB_FORCE_SPLASH_NUDGE", "JJFB_FORCE_2EFC_TAIL",
  "JJFB_FORCE_R4", "JJFB_FORCE_B6C", "JJFB_FORCE_134D", "JJFB_FORCE_AC8_GATE",
  "JJFB_PROGRESS_DRIVER", "JJFB_PROGRESS_SCAN", "JJFB_PROGRESS_NUDGE",
  "JJFB_EVENT_CODE", "JJFB_SLOGO_NUDGE", "JJFB_SPLASH_HOST_BLIT",
  "JJFB_310BB4_HOST_BLIT", "JJFB_CHROME_SKIP_310BB4", "JJFB_ALLOW_CHROME",
  "JJFB_CHROME_ALLOW_CALLS", "JJFB_AC8_MODE", "JJFB_SPLASH_AC8_MODE"
)
foreach ($name in $legacyVars) {
  Remove-Item ("Env:" + $name) -ErrorAction SilentlyContinue
}

if (-not $SkipBuild) {
  Write-Host "[2/4] Fresh 32-bit build (all objects)"
  Push-Location $src
  try {
    $sources = @("network.c", "fileLib.c", "vmrp.c", "utils.c", "rbtree.c", "bridge.c", "memory.c", "main.c")
    foreach ($source in $sources) {
      $obj = [System.IO.Path]::ChangeExtension($source, ".o")
      Remove-Item $obj -Force -ErrorAction SilentlyContinue
      & gcc -g -Wall -DNETWORK_SUPPORT -DVMRP -m32 -c $source -o $obj
      if ($LASTEXITCODE -ne 0) { throw "compile failed: $source" }
    }
    New-Item -ItemType Directory -Force -Path "bin" | Out-Null
    & gcc -g -Wall -DNETWORK_SUPPORT -DVMRP -m32 -o ./bin/main `
      network.o fileLib.o vmrp.o utils.o rbtree.o bridge.o memory.o main.o `
      ./windows/unicorn-1.0.2-win32/unicorn.lib `
      -lpthread -lm -lws2_32 -lz -lmingw32 -mconsole `
      -L./windows/SDL2-2.0.10/i686-w64-mingw32/lib/ -lSDL2main -lSDL2
    if ($LASTEXITCODE -ne 0) { throw "link failed" }
    Copy-Item -Force "bin\main.exe" (Join-Path $rt "main.exe")
  } finally {
    Pop-Location
  }
} else {
  Write-Host "[2/4] Build skipped"
}

if (-not (Test-Path (Join-Path $gwyRoot "jjfb.mrp"))) {
  throw "runtime canonical target missing: $gwyRoot\jjfb.mrp"
}

Write-Host "[3/4] Run canonical target"
Write-Host "JJFB_MYTHROAD_ROOT=$($env:JJFB_MYTHROAD_ROOT)"
Write-Host "JJFB_GWY_ROOT=$($env:JJFB_GWY_ROOT)"
Write-Host "JJFB_GWY_PARAM=$($env:JJFB_GWY_PARAM)"
Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
$p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id)"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 500

if (Test-Path $stdoutPath) { Copy-Item -Force $stdoutPath $out } else { throw "stdout log not produced: $stdoutPath" }
if (Test-Path $stderrPath) { Copy-Item -Force $stderrPath $errOut }
Write-Host "saved $out"

Write-Host "[4/4] Analyze launcher evidence"
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
if ($python) {
  if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
    & $python.Source -3 (Join-Path $root "scripts\v50_analyze_launcher_log.py") $out --output $reportOut
  } else {
    & $python.Source (Join-Path $root "scripts\v50_analyze_launcher_log.py") $out --output $reportOut
  }
  if ($LASTEXITCODE -ne 0) { Write-Warning "log analyzer failed" }
} else {
  Write-Warning "Python not found; raw log is still saved"
}

$pattern = "JJFB_GWY_LAUNCH|JJFB_GWY_ROOT|JJFB_CFG36|JJFB_STARTGAME|JJFB_FILEOPEN|JJFB_FILEOPEN_MISS|JJFB_LOADER|JJFB_801|robotol|connect|21002|21003"
Select-String -Path $out -Pattern $pattern | Select-Object -First 180 | ForEach-Object { $_.Line }
Write-Host "---"
Write-Host "FILEOPEN success: $((@(Select-String -Path $out -Pattern '\[JJFB_FILEOPEN\]').Count))"
Write-Host "FILEOPEN miss:    $((@(Select-String -Path $out -Pattern '\[JJFB_FILEOPEN_MISS\]').Count))"
Write-Host "report: $reportOut"
