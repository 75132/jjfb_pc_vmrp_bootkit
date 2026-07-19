# JJFB clean baseline: GWY launcher (guest 240x320, window 320x480 stretch)
# Entry alias: RUN_JJFB.ps1
# v72: plat 0x10110 no-alloc; @-path RDWR create; no auto-kill
# Layout: y828_zero + textbar_dedup ON (kill duplicate bottom wood strip)
# Keep OFF: begin-tick frame clear (fake wipe look)
param(
  [switch]$SkipBuild,
  [switch]$SkipResourceCopy,
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
$reportDir = Join-Path $root "reports"
$stdoutPath = Join-Path $rt "jjfb_loader_stdout.txt"
$stderrPath = Join-Path $rt "jjfb_loader_stderr.txt"
$out = Join-Path $logDir "jjfb_gwy_clean_stdout.txt"
$errOut = Join-Path $logDir "jjfb_gwy_clean_stderr.txt"
$keyManifest = Join-Path $logDir "v56_sdk_key_manifest.json"
$auditJson = Join-Path $logDir "v56_start_handoff_audit.json"
$auditReport = Join-Path $reportDir "v56_start_handoff_static_audit.md"
$keyCanonical = Join-Path $mythroadRoot "sdk_key.dat"

New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $mythroadRoot, $gwyRoot | Out-Null

Write-Host "=== JJFB GWY clean baseline ==="
Write-Host "project=$root"
Write-Host "resource_src=$resourceSrc"
Write-Host "mythroad_root=$mythroadRoot"

$bridgeSource = Join-Path $src "bridge.c"
$vmrpSource = Join-Path $src "vmrp.c"
$vmrpHeader = Join-Path $src "header\vmrp.h"
$mainSource = Join-Path $src "main.c"
$fileLibSource = Join-Path $src "fileLib.c"
foreach ($required in @($bridgeSource, $vmrpSource, $vmrpHeader, $mainSource, $fileLibSource)) {
  if (-not (Test-Path $required)) { throw "source missing: $required" }
}
$bridgeText = [IO.File]::ReadAllText($bridgeSource)
$vmrpText = [IO.File]::ReadAllText($vmrpSource)
$hdrText = [IO.File]::ReadAllText($vmrpHeader)
$mainText = [IO.File]::ReadAllText($mainSource)
$fileLibText = [IO.File]::ReadAllText($fileLibSource)
if (-not $hdrText.Contains("#define SCREEN_WIDTH 240") -or
    -not $hdrText.Contains("#define WINDOW_WIDTH 320") -or
    -not $mainText.Contains("[JJFB_V84_SCREEN]") -or
    -not $fileLibText.Contains("mythroad/320x480/") -or
    -not $fileLibText.Contains("0@s0.map") -or
    -not $bridgeText.Contains("[JJFB_V72_10110]") -or
    -not $bridgeText.Contains("[JJFB_V86_11F00]") -or
    -not $bridgeText.Contains("jjfb_host_draw_sky16") -or
    -not $bridgeText.Contains("JJFB_11F00_GLYPH_BLOCK") -or
    -not $bridgeText.Contains("jjfb_keep_y828_zero") -or
    -not $bridgeText.Contains("jjfb_textbar_overlap_skip") -or
    -not $bridgeText.Contains("opt-in JJFB_FRAME_CLEAR=1") -or
    $bridgeText.Contains("jjfb_v86_flush_layout") -or
    -not $vmrpText.Contains("JJFB_FAMILY_APP2_AFTER_INIT")) {
  throw "clean JJFB overlay missing or leftover diagnostics present."
}

$canonicalSourceMrp = Join-Path $resourceSrc "gwy\jjfb.mrp"
if (-not (Test-Path $canonicalSourceMrp)) {
  throw "canonical source target missing: $canonicalSourceMrp"
}

if (-not $SkipResourceCopy) {
  Write-Host "[1/6] Copy mythroad tree -> runtime mythroad/320x480"
  & robocopy $resourceSrc $mythroadRoot /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /NFL /NDL /NJH /NJS /NP
  $rc = $LASTEXITCODE
  if ($rc -ge 8) { throw "robocopy failed with exit code $rc" }
} else {
  Write-Host "[1/6] Resource copy skipped"
}

$runtimeMrp = Join-Path $gwyRoot "jjfb.mrp"
if (-not (Test-Path $runtimeMrp)) { throw "runtime canonical target missing: $runtimeMrp" }
$sourceHash = (Get-FileHash -Algorithm SHA256 $canonicalSourceMrp).Hash.ToLowerInvariant()
$runtimeHash = (Get-FileHash -Algorithm SHA256 $runtimeMrp).Hash.ToLowerInvariant()
if ($sourceHash -ne $runtimeHash) {
  throw "runtime jjfb.mrp differs from canonical source"
}
Write-Host "[JJFB_MRP_ORIGINAL] sha256=$runtimeHash"

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $python) { throw "Python 3 is required" }

Write-Host "[2/6] Audit canonical MRP"
$audit = Join-Path $root "scripts\v53_audit_start_handoff.py"
& $python.Source $audit $runtimeMrp --source-root $src --strict --output $auditReport --json $auditJson
if ($LASTEXITCODE -ne 0) { throw "static audit failed" }

Write-Host "[3/6] Generate and deploy sdk_key.dat"
$keyGen = Join-Path $root "scripts\v51_generate_sdk_key.py"
& $python.Source $keyGen --vmver $VmVer --imei $Imei --hsman $HsMan --hstype $HsType --output $keyCanonical --report $keyManifest
if ($LASTEXITCODE -ne 0) { throw "sdk key generator failed" }
$keyTargets = @(
  $keyCanonical,
  (Join-Path $rt "mythroad\sdk_key.dat"),
  (Join-Path $rt "mythroad\gwy\sdk_key.dat"),
  (Join-Path $rt "mythroad\gwy\jjfbol\sdk_key.dat")
)
foreach ($target in $keyTargets) {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target) | Out-Null
  if ($target -ne $keyCanonical) { Copy-Item -Force $keyCanonical $target }
}
$keyHash = (Get-FileHash -Algorithm SHA256 $keyCanonical).Hash.ToLowerInvariant()

$env:Path = "C:\msys64\mingw32\bin;" + $env:Path
$env:JJFB_GWY_LAUNCHER_MODE = "1"
$env:JJFB_MRP_ALIAS_CFUNCTION_ROBOTOL = "1"
$env:JJFB_ACCEPT_START_IGNORE_AFTER_ROBOTOL = "1"
$env:JJFB_MYTHROAD_ROOT = $mythroadRoot
$env:JJFB_GWY_ROOT = $gwyRoot
$env:JJFB_GWY_PARAM = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$env:JJFB_SCREEN_W = "240"
$env:JJFB_SCREEN_H = "320"
$env:JJFB_FORCE_UI_MODE = "0"
$env:JJFB_FORCE_SPLASH_NUDGE = "0"
$env:JJFB_DISABLE_MRC_EVENT0_INJECT = "1"
$env:JJFB_MRC_RESUME_AFTER_INIT = "1"
$env:JJFB_FAMILY_APP2_AFTER_INIT = "1"
$env:JJFB_PATH_A_EVENT_ONCE = "0"
$env:JJFB_V64_ENQUEUE_ONCE = "1"
$env:JJFB_IMEI = $Imei
$env:JJFB_HSMAN = $HsMan
$env:JJFB_HSTYPE = $HsType
# Platform layout: kill duplicate bottom textbar (y828 +40 / overlapping 120x30).
$env:JJFB_Y828_ZERO = "1"
$env:JJFB_TEXTBAR_DEDUP = "1"
# TEMPORARY PROBE (not official): ~2s after splash, release net-wait + arm 2EFC40.
$env:JJFB_SKIP_NET_LOGIN = "1"
$env:JJFB_SKIP_NET_LOGIN_MS = "2000"
Remove-Item Env:JJFB_GWY_TARGET -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRESENT_IMMEDIATE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRESENT_FREEZE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_11F00_GLYPH_BLOCK -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_11F00_GB16 -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_FRAME_CLEAR -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_VERBOSE -ErrorAction SilentlyContinue
# Keep begin-tick wipe OFF; keep diagnostic flood OFF (was freezing UI).

$legacyVars = @(
  "JJFB_FORCE_2EFC_TAIL",
  "JJFB_FORCE_R4", "JJFB_FORCE_B6C", "JJFB_FORCE_134D", "JJFB_FORCE_AC8_GATE",
  "JJFB_PROGRESS_DRIVER", "JJFB_PROGRESS_SCAN", "JJFB_PROGRESS_NUDGE",
  "JJFB_EVENT_CODE", "JJFB_SLOGO_NUDGE", "JJFB_SPLASH_HOST_BLIT",
  "JJFB_310BB4_HOST_BLIT", "JJFB_CHROME_SKIP_310BB4", "JJFB_ALLOW_CHROME",
  "JJFB_CHROME_ALLOW_CALLS", "JJFB_AC8_MODE", "JJFB_SPLASH_AC8_MODE",
  "JJFB_SKIP_LUA_801_0", "JJFB_FORCE_START_DSM_SUCCESS"
)
foreach ($name in $legacyVars) {
  Remove-Item ("Env:" + $name) -ErrorAction SilentlyContinue
}

Write-Host "[4/6] Build"
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
} else {
  if (-not (Test-Path $mainExe)) { throw "-SkipBuild requested but main.exe is missing" }
}

Write-Host "[5/6] Run"
Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
Get-ChildItem $rt -Filter "jjfb_v86_frame_t*.bmp" -ErrorAction SilentlyContinue | Remove-Item -Force
$p = Start-Process -FilePath $mainExe -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id) — window stays open; close it (or Ctrl+C here) when done"
$p.WaitForExit()
Start-Sleep -Milliseconds 500
if (-not (Test-Path $stdoutPath)) { throw "stdout log not produced: $stdoutPath" }

$afterHash = (Get-FileHash -Algorithm SHA256 $runtimeMrp).Hash.ToLowerInvariant()
if ($afterHash -ne $runtimeHash) {
  throw "jjfb.mrp changed during run"
}

$header = @(
  "[JJFB_SDK_KEY] vmver=$VmVer IMEI=$Imei hsman=$HsMan hstype=$HsType",
  "[JJFB_SDK_KEY] canonical=$keyCanonical sha256=$keyHash",
  "[JJFB_MRP_ORIGINAL] source=$canonicalSourceMrp sha256=$runtimeHash",
  "[JJFB_CLEAN] removed=layout_classify,bmp_dump,host_frame_clear_default",
  "[JJFB_CLEAN] kept=y828_zero,textbar_overlap_dedup,11F00_no_glyph_block,gb16_opt,begin_tick_clear_optin",
  "[JJFB_V72] plat_10110=family_notify_no_alloc at_cache_rdwr_create=1",
  "[JJFB_V84_SCREEN] contract=guest_240x320 + window_320x480_stretch + mythroad/320x480"
) -join "`r`n"
$rawLog = [IO.File]::ReadAllText($stdoutPath)
[IO.File]::WriteAllText($out, $header + "`r`n" + $rawLog, [Text.Encoding]::UTF8)
if (Test-Path $stderrPath) { Copy-Item -Force $stderrPath $errOut }
Write-Host "saved $out"

Write-Host "[6/6] Key lines"
$pattern = "JJFB_GWY_LAUNCH|JJFB_GWY_ROOT|JJFB_LOADER|JJFB_ROBOTOL|JJFB_PROBE_SKIP_NET|JJFB_FORCE_2EFC_TAIL|JJFB_V72_10110|JJFB_FILEOPEN|FILEOPEN_MISS|FILEOPEN_AT|BMP_LOAD.*FAIL|my_malloc no memory|UC_ERR|mr_exit"
Select-String -Path $out -Pattern $pattern | Select-Object -First 50 | ForEach-Object { $_.Line }

$launch = (Select-String -Path $out -Pattern "JJFB_GWY_LAUNCH\] cfg_index=36" | Measure-Object).Count
$v72 = (Select-String -Path $out -Pattern "JJFB_V72_10110" | Measure-Object).Count
$probe = (Select-String -Path $out -Pattern "JJFB_PROBE_SKIP_NET\] FIRE" | Measure-Object).Count
$tail = (Select-String -Path $out -Pattern "JJFB_FORCE_2EFC_TAIL" | Measure-Object).Count
$oom = (Select-String -Path $out -Pattern "my_malloc no memory" | Measure-Object).Count
$exc = (Select-String -Path $out -Pattern "UC_ERR_EXCEPTION" | Measure-Object).Count
$fail = (Select-String -Path $out -Pattern "BMP_LOAD.*FAIL|FILEOPEN_MISS" | Measure-Object).Count
Write-Host "---"
Write-Host "launch=$launch v72_10110=$v72 skip_net_fire=$probe 2efc_tail=$tail oom=$oom uc_exc=$exc miss_or_fail=$fail exit=$($p.ExitCode)"
Write-Host "log: $out"
