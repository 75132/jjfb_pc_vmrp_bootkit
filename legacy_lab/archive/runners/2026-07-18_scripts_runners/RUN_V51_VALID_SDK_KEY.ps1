# JJFB v51 - original start.mr + valid host sdk_key.dat
param(
  [int]$Seconds = 25,
  [switch]$SkipBuild,
  [switch]$SkipResourceCopy,
  [string]$VmVer = "1968",
  [string]$Imei = "864086040622841",
  [string]$HsMan = "vmrp",
  [string]$HsType = "vmrp"
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
$out = Join-Path $logDir "v51_valid_sdk_key_stdout.txt"
$errOut = Join-Path $logDir "v51_valid_sdk_key_stderr.txt"
$manifest = Join-Path $logDir "v51_sdk_key_manifest.json"
$reportOut = Join-Path $reportDir "v51_valid_sdk_key_run_result.md"
$keyCanonical = Join-Path $mythroadRoot "sdk_key.dat"
$keyHexFallback = "d6aaa1b23878829303d1b9bcca42e1839f9b63153641f4c4e9743434427125b29b31f52a08537f4bdd1b71ab70686d35"

New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $mythroadRoot, $gwyRoot | Out-Null

Write-Host "=== JJFB v51 Valid SDK Key ==="
Write-Host "project=$root"
Write-Host "runtime=$rt"
Write-Host "identity: vmver=$VmVer IMEI=$Imei hsman=$HsMan hstype=$HsType"

$canonicalSourceMrp = Join-Path $resourceSrc "gwy\jjfb.mrp"
if (-not (Test-Path $canonicalSourceMrp)) {
  throw "canonical source target missing: $canonicalSourceMrp"
}

if (-not $SkipResourceCopy) {
  Write-Host "[1/5] Copy full canonical mythroad/240x320 tree"
  & robocopy $resourceSrc $mythroadRoot /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /NFL /NDL /NJH /NJS /NP
  $rc = $LASTEXITCODE
  if ($rc -ge 8) { throw "robocopy failed with exit code $rc" }
} else {
  Write-Host "[1/5] Resource copy skipped"
}

$runtimeMrp = Join-Path $gwyRoot "jjfb.mrp"
if (-not (Test-Path $runtimeMrp)) {
  throw "runtime canonical target missing: $runtimeMrp"
}
$sourceHash = (Get-FileHash -Algorithm SHA256 $canonicalSourceMrp).Hash.ToLowerInvariant()
$runtimeHash = (Get-FileHash -Algorithm SHA256 $runtimeMrp).Hash.ToLowerInvariant()
if ($sourceHash -ne $runtimeHash) {
  throw "runtime jjfb.mrp differs from canonical source: source=$sourceHash runtime=$runtimeHash"
}
Write-Host "[JJFB_MRP_ORIGINAL] sha256=$runtimeHash"

Write-Host "[2/5] Generate 48-byte sdk_key.dat from actual VMRP identity"
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
$generator = Join-Path $root "scripts\v51_generate_sdk_key.py"
if ($python -and (Test-Path $generator)) {
  if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
    & $python.Source -3 $generator --vmver $VmVer --imei $Imei --hsman $HsMan --hstype $HsType --output $keyCanonical --report $manifest
  } else {
    & $python.Source $generator --vmver $VmVer --imei $Imei --hsman $HsMan --hstype $HsType --output $keyCanonical --report $manifest
  }
  if ($LASTEXITCODE -ne 0) { throw "sdk key generator failed" }
} else {
  if ($VmVer -ne "1968" -or $Imei -ne "864086040622841" -or $HsMan -ne "vmrp" -or $HsType -ne "vmrp") {
    throw "Python is required when using a non-default VMRP identity"
  }
  [byte[]]$bytes = for ($i = 0; $i -lt $keyHexFallback.Length; $i += 2) {
    [Convert]::ToByte($keyHexFallback.Substring($i, 2), 16)
  }
  [IO.File]::WriteAllBytes($keyCanonical, $bytes)
  $fallbackReport = [ordered]@{
    vmver = $VmVer; imei = $Imei; hsman = $HsMan; hstype = $HsType
    key_len = $bytes.Length; key_hex = $keyHexFallback
    key_sha256 = (Get-FileHash -Algorithm SHA256 $keyCanonical).Hash.ToLowerInvariant()
    output = $keyCanonical; generator = "PowerShell embedded fallback"
  }
  $fallbackReport | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 $manifest
}

# Mirror the same valid key to legacy locations.  The new fileLib mapping makes
# the 240x320 copy canonical; these mirrors keep -SkipBuild safe with a v50 EXE.
$keyTargets = @(
  $keyCanonical,
  (Join-Path $gwyRoot "sdk_key.dat"),
  (Join-Path $gwyRoot "jjfbol\sdk_key.dat"),
  (Join-Path $rt "mythroad\sdk_key.dat"),
  (Join-Path $rt "mythroad\gwy\sdk_key.dat"),
  (Join-Path $rt "mythroad\gwy\jjfbol\sdk_key.dat")
)
foreach ($target in $keyTargets) {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
  if ($target -ne $keyCanonical) { Copy-Item -Force $keyCanonical $target }
  $len = (Get-Item $target).Length
  $hash = (Get-FileHash -Algorithm SHA256 $target).Hash.ToLowerInvariant()
  if ($len -ne 48) { throw "invalid sdk key length at $target : $len" }
  Write-Host "[JJFB_SDK_KEY] host=$target len=$len sha256=$hash"
}
$keyHash = (Get-FileHash -Algorithm SHA256 $keyCanonical).Hash.ToLowerInvariant()

$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
$env:JJFB_GWY_LAUNCHER_MODE = "1"
$env:JJFB_MYTHROAD_ROOT = $mythroadRoot
$env:JJFB_GWY_ROOT = $gwyRoot
$env:JJFB_GWY_PARAM = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$env:JJFB_SCREEN_W = "240"
$env:JJFB_SCREEN_H = "320"

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
  Write-Host "[3/5] Fresh 32-bit build with v51 canonical mythroad path mapping"
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
  Write-Host "[3/5] Build skipped; valid key mirrors retain compatibility with v50 main.exe"
}

Write-Host "[4/5] Run original gwy/jjfb.mrp"
Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
$p = Start-Process -FilePath (Join-Path $rt "main.exe") -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id)"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 500
if (-not (Test-Path $stdoutPath)) { throw "stdout log not produced: $stdoutPath" }

$header = @(
  "[JJFB_SDK_KEY] vmver=$VmVer IMEI=$Imei hsman=$HsMan hstype=$HsType",
  "[JJFB_SDK_KEY] canonical=$keyCanonical len=48 sha256=$keyHash",
  "[JJFB_MRP_ORIGINAL] source=$canonicalSourceMrp sha256=$sourceHash"
) -join "`r`n"
$rawLog = [IO.File]::ReadAllText($stdoutPath)
[IO.File]::WriteAllText($out, $header + "`r`n" + $rawLog, [Text.Encoding]::UTF8)
if (Test-Path $stderrPath) { Copy-Item -Force $stderrPath $errOut }
Write-Host "saved $out"

Write-Host "[5/5] Analyze sdk acceptance and loader chain"
$analyzer = Join-Path $root "scripts\v51_analyze_sdk_key_log.py"
if ($python -and (Test-Path $analyzer)) {
  if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
    & $python.Source -3 $analyzer $out --output $reportOut --manifest $manifest
  } else {
    & $python.Source $analyzer $out --output $reportOut --manifest $manifest
  }
  if ($LASTEXITCODE -ne 0) { Write-Warning "v51 log analyzer failed" }
} else {
  Write-Warning "Python/analyzer unavailable; raw log is still saved"
}

$pattern = "JJFB_SDK_KEY|JJFB_MRP_ORIGINAL|sdk key|mr_get_method|_mr_c_function_new|JJFB_FILEOPEN|JJFB_FILEOPEN_MISS|JJFB_LOADER|JJFB_801|robotol|bridge_dsm_mr_start_dsm ret"
Select-String -Path $out -Pattern $pattern | Select-Object -First 220 | ForEach-Object { $_.Line }
Write-Host "---"
Write-Host "report: $reportOut"
