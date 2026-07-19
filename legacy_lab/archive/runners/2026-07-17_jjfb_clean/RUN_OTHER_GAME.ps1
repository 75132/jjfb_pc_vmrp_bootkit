# Smoke-test another GWY title via the same launcher shim (not JJFB-specific).
# Example:
#   .\RUN_OTHER_GAME.ps1 -Target gwy/tlbb.mrp
#   .\RUN_OTHER_GAME.ps1 -Target gwy/ssjx.mrp -Seconds 15
param(
  [Parameter(Mandatory = $true)]
  [string]$Target = "gwy/tlbb.mrp",
  [int]$Seconds = 15,
  [switch]$SkipBuild,
  [string]$VmVer = "1968",
  [string]$Imei = "864086040622841",
  [string]$HsMan = "vmrp",
  [string]$HsType = "vmrp"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"
$src = Join-Path $root "runtime\vmrp_src_build_v27\vmrp-master"
$resourceSrc = Join-Path $root "game_files\mythroad\320x480"
$mythroadRoot = Join-Path $rt "mythroad\320x480"
$gwyRoot = Join-Path $mythroadRoot "gwy"
$logDir = Join-Path $root "logs"
$safeName = ($Target -replace '[\\/]', '_').Trim('_')
$stdoutPath = Join-Path $rt "jjfb_loader_stdout.txt"
$stderrPath = Join-Path $rt "jjfb_loader_stderr.txt"
$out = Join-Path $logDir ("other_" + $safeName + "_stdout.txt")
$errOut = Join-Path $logDir ("other_" + $safeName + "_stderr.txt")
$keyCanonical = Join-Path $mythroadRoot "sdk_key.dat"

New-Item -ItemType Directory -Force -Path $logDir, $mythroadRoot, $gwyRoot | Out-Null

Write-Host "=== JJFB launcher smoke: other game ==="
Write-Host "target=$Target"
Write-Host "mythroad_root=$mythroadRoot"

if (-not (Test-Path (Join-Path $resourceSrc ($Target -replace '/', '\')))) {
  throw "missing resource: $resourceSrc\$Target"
}

Write-Host "[1/4] Copy mythroad tree"
robocopy $resourceSrc $mythroadRoot /E /NFL /NDL /NJH /NJS /nc /ns /np | Out-Null

Write-Host "[2/4] sdk_key.dat"
$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
$keyGen = Join-Path $root "scripts\v51_generate_sdk_key.py"
$keyManifest = Join-Path $logDir ("other_" + $safeName + "_sdk_key.json")
& $python.Source $keyGen --vmver $VmVer --imei $Imei --hsman $HsMan --hstype $HsType --output $keyCanonical --report $keyManifest
if ($LASTEXITCODE -ne 0) { throw "sdk key failed" }
Copy-Item -Force $keyCanonical (Join-Path $rt "mythroad\sdk_key.dat") -ErrorAction SilentlyContinue
Copy-Item -Force $keyCanonical (Join-Path $gwyRoot "sdk_key.dat") -ErrorAction SilentlyContinue

$env:JJFB_GWY_LAUNCHER_MODE = "1"
$env:JJFB_GWY_TARGET = $Target
$env:JJFB_GWY_PARAM = "napptype=12_nextid=0_ncode=0_narg=0_narg1=1_nmrpname=${Target}_gwyblink"
$env:JJFB_MYTHROAD_ROOT = $mythroadRoot
$env:JJFB_GWY_ROOT = $gwyRoot
$env:JJFB_SCREEN_W = "240"
$env:JJFB_SCREEN_H = "320"
$env:JJFB_FORCE_UI_MODE = "0"
$env:JJFB_DISABLE_MRC_EVENT0_INJECT = "1"
$env:JJFB_MRC_RESUME_AFTER_INIT = "1"
$env:JJFB_FAMILY_APP2_AFTER_INIT = "1"
$env:JJFB_IMEI = $Imei
$env:JJFB_HSMAN = $HsMan
$env:JJFB_HSTYPE = $HsType
# Other titles are NOT jjfb/robotol — do not apply JJFB member alias / handoff.
Remove-Item Env:JJFB_MRP_ALIAS_CFUNCTION_ROBOTOL -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_ACCEPT_START_IGNORE_AFTER_ROBOTOL -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRESENT_IMMEDIATE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRESENT_FREEZE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_TEXTBAR_DEDUP -ErrorAction SilentlyContinue

Write-Host "[3/4] Build (if needed)"
$mainExe = Join-Path $rt "main.exe"
if (-not $SkipBuild) {
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
    Copy-Item -Force "bin\main.exe" $mainExe
  } finally {
    Pop-Location
  }
}

Write-Host "[4/4] Run $Seconds s"
Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
$p = Start-Process -FilePath $mainExe -WorkingDirectory $rt -PassThru `
  -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
Write-Host "pid=$($p.Id)"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  try { Wait-Process -Id $p.Id -TimeoutSec 3 } catch {}
}
Copy-Item -Force $stdoutPath $out -ErrorAction SilentlyContinue
Copy-Item -Force $stderrPath $errOut -ErrorAction SilentlyContinue

Write-Host "--- key lines ---"
Select-String -Path $out -Pattern 'GWY_LAUNCH|LOADER|ROBOTOL|FILEOPEN_MISS|unhandled|ext:|start_dsm|FAIL|error|crash|alias' |
  Select-Object -First 40 | ForEach-Object { $_.Line }
Write-Host "log: $out exit=$($p.ExitCode)"
