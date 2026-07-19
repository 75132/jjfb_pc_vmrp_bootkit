# JJFB v57 - trace lifecycle source boundaries for family C0 and callback registration (NO FORCE)
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
$rt = Join-Path $root "runtime\vmrp_win32\vmrp_win32_20220102"
$src = Join-Path $root "runtime\vmrp_src_build_v27\vmrp-master"
$resourceSrc = Join-Path $root "game_files\mythroad\240x320"
$mythroadRoot = Join-Path $rt "mythroad\240x320"
$gwyRoot = Join-Path $mythroadRoot "gwy"
$logDir = Join-Path $root "logs"
$reportDir = Join-Path $root "reports"
$stdoutPath = Join-Path $rt "jjfb_loader_stdout.txt"
$stderrPath = Join-Path $rt "jjfb_loader_stderr.txt"
$out = Join-Path $logDir "v57_lifecycle_source_stdout.txt"
$errOut = Join-Path $logDir "v57_lifecycle_source_stderr.txt"
$keyManifest = Join-Path $logDir "v57_sdk_key_manifest.json"
$auditJson = Join-Path $logDir "v57_start_handoff_audit.json"
$auditReport = Join-Path $reportDir "v57_start_handoff_static_audit.md"
$reportOut = Join-Path $reportDir "v57_lifecycle_source_run_result.md"
$keyCanonical = Join-Path $mythroadRoot "sdk_key.dat"
$keyHexFallback = "d6aaa1b23878829303d1b9bcca42e1839f9b63153641f4c4e9743434427125b29b31f52a08537f4bdd1b71ab70686d35"

New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $mythroadRoot, $gwyRoot | Out-Null

Write-Host "=== JJFB v57 Lifecycle Source Coverage (natural, no FORCE) ==="
Write-Host "project=$root"
Write-Host "target: source of family app=0xC0 + robotol method1/cmd10002 or method5 registration paths"

$bridgeSource = Join-Path $src "bridge.c"
$vmrpSource = Join-Path $src "vmrp.c"
foreach ($required in @($bridgeSource, $vmrpSource)) {
  if (-not (Test-Path $required)) { throw "v57 source missing: $required" }
}
$bridgeText = [IO.File]::ReadAllText($bridgeSource)
$vmrpText = [IO.File]::ReadAllText($vmrpSource)
if (-not $bridgeText.Contains("jjfb_install_uimode_writer_hooks") -or
    -not $bridgeText.Contains("[JJFB_V57_SOURCE_COVERAGE] coverage installed") -or
    -not $bridgeText.Contains("[JJFB_GAME_SELF] natural_mode=1") -or
    -not $vmrpText.Contains("JJFB_ACCEPT_START_IGNORE_AFTER_ROBOTOL")) {
  throw "v57 source overlay missing (need v57 lifecycle-source hooks + v54 natural_mode)."
}

$canonicalSourceMrp = Join-Path $resourceSrc "gwy\jjfb.mrp"
if (-not (Test-Path $canonicalSourceMrp)) {
  throw "canonical source target missing: $canonicalSourceMrp"
}

if (-not $SkipResourceCopy) {
  Write-Host "[1/6] Copy full canonical mythroad/240x320 tree"
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
  throw "runtime jjfb.mrp differs from canonical source: source=$sourceHash runtime=$runtimeHash"
}
Write-Host "[JJFB_MRP_ORIGINAL] sha256=$runtimeHash"

$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $python) { throw "Python 3 is required for v57 preflight and analysis" }

Write-Host "[2/6] Audit canonical MRP"
$auditScript = Join-Path $root "scripts\v53_audit_start_handoff.py"
if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
  & $python.Source -3 $auditScript $runtimeMrp --source-root $src --strict --output $auditReport --json $auditJson
} else {
  & $python.Source $auditScript $runtimeMrp --source-root $src --strict --output $auditReport --json $auditJson
}
if ($LASTEXITCODE -ne 0) { throw "v57 static audit failed" }

$staticMap = Join-Path $root "scripts\v57_static_lifecycle_source_map.py"
if (Test-Path $staticMap) {
  $mapOut = Join-Path $reportDir "v57_lifecycle_source_static_map.md"
  if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
    & $python.Source -3 $staticMap $runtimeMrp --markdown $mapOut | Out-Null
  } else {
    & $python.Source $staticMap $runtimeMrp --markdown $mapOut | Out-Null
  }
}

Write-Host "[3/6] Generate and deploy sdk_key.dat"
$generator = Join-Path $root "scripts\v51_generate_sdk_key.py"
if (Test-Path $generator) {
  if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
    & $python.Source -3 $generator --vmver $VmVer --imei $Imei --hsman $HsMan --hstype $HsType --output $keyCanonical --report $keyManifest
  } else {
    & $python.Source $generator --vmver $VmVer --imei $Imei --hsman $HsMan --hstype $HsType --output $keyCanonical --report $keyManifest
  }
  if ($LASTEXITCODE -ne 0) { throw "sdk key generator failed" }
} else {
  [byte[]]$bytes = for ($i = 0; $i -lt $keyHexFallback.Length; $i += 2) {
    [Convert]::ToByte($keyHexFallback.Substring($i, 2), 16)
  }
  [IO.File]::WriteAllBytes($keyCanonical, $bytes)
}

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
$env:JJFB_V57_LIFECYCLE_SOURCE_COVERAGE = "1"

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

Write-Host "[4/6] Build v57 lifecycle source coverage"
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

Write-Host "[5/6] Run natural (no FORCE/no synthetic lifecycle input) with source coverage"
Remove-Item $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue
$p = Start-Process -FilePath $mainExe -WorkingDirectory $rt -PassThru
Write-Host "pid=$($p.Id)"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 500
if (-not (Test-Path $stdoutPath)) { throw "stdout log not produced: $stdoutPath" }

$afterHash = (Get-FileHash -Algorithm SHA256 $runtimeMrp).Hash.ToLowerInvariant()
if ($afterHash -ne $runtimeHash) {
  throw "jjfb.mrp changed during v57 run: before=$runtimeHash after=$afterHash"
}

$header = @(
  "[JJFB_SDK_KEY] vmver=$VmVer IMEI=$Imei hsman=$HsMan hstype=$HsType",
  "[JJFB_SDK_KEY] canonical=$keyCanonical len=48 sha256=$keyHash",
  "[JJFB_MRP_ORIGINAL] source=$canonicalSourceMrp sha256_before=$sourceHash sha256_after=$afterHash",
  "[JJFB_MRP_ALIAS] contract=cfunction.ext->robotol.ext method=br_log_ext_base_0xD4 scope=guest_memory_only",
  "[JJFB_START_HANDOFF] contract=MR_IGNORE_only_after_alias_and_robotol then host_6_8_0",
  "[JJFB_V57_SOURCE_COVERAGE] contract=trace_familyC0_ingress_and_robotol_method1_cmd10002_or_method5 no_FORCE no_injection"
) -join "`r`n"
$rawLog = [IO.File]::ReadAllText($stdoutPath)
[IO.File]::WriteAllText($out, $header + "`r`n" + $rawLog, [Text.Encoding]::UTF8)
if (Test-Path $stderrPath) { Copy-Item -Force $stderrPath $errOut }
Write-Host "saved $out"

Write-Host "[6/6] Analyze lifecycle source coverage"
$analyzer = Join-Path $root "scripts\v57_analyze_lifecycle_source_log.py"
if ($python.Name -eq "py.exe" -or $python.Name -eq "py") {
  & $python.Source -3 $analyzer $out --markdown $reportOut
} else {
  & $python.Source $analyzer $out --markdown $reportOut
}

$pattern = "JJFB_V57|JJFB_V56_FAMILY|JJFB_GAME_SELF|JJFB_START_HANDOFF|JJFB_801|robotol timer|FORCE|lifecycle_source|gate_|caller_|FILEOPEN_MISS"
Select-String -Path $out -Pattern $pattern | Select-Object -First 200 | ForEach-Object { $_.Line }
Write-Host "---"
Write-Host "run report: $reportOut"
