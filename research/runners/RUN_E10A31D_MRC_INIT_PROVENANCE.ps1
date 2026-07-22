# Stage E10A-3.1d: mrc_init failure provenance
# Modes: history | method0_trace | appinfo | platform_table | control_compare | validate
# Deterministic env wipe; whitelist-only; unique overlay per run_id.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('history','method0_trace','appinfo','platform_table','control_compare','validate')]
  [string]$Mode = 'history',
  [int]$TickN = 12,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
while ($Root -and -not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $parent = Split-Path -Parent $Root
  if (-not $parent -or $parent -eq $Root) { break }
  $Root = $parent
}
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  throw "cannot locate repo root from $PSScriptRoot"
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$outDir = Join-Path $Root 'out\e10a31d'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31d_mrc_init_provenance_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31d\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir ("e10a31d_{0}_stdout.txt" -f $Mode)
$stderrLog = Join-Path $logDir ("e10a31d_{0}_stderr.txt" -f $Mode)

$histCsv = Join-Path $reportDir 'e10a31d_helper_call_history.csv'
$insnCsv = Join-Path $reportDir 'e10a31d_method0_instruction_trace.csv'
$callCsv = Join-Path $reportDir 'e10a31d_method0_call_tree.csv'
$provCsv = Join-Path $reportDir 'e10a31d_method0_return_provenance.csv'
$appinfoCsv = Join-Path $reportDir 'e10a31d_appinfo_contract.csv'
$abiCsv = Join-Path $reportDir 'e10a31d_helper_abi_compare.csv'
$timerCsv = Join-Path $reportDir 'e10a31_timer_binding_trace.csv'
$initCsv = Join-Path $reportDir 'e10a31c_init_sequence_trace.csv'

function Stop-E10A31DChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31D_|E10A31C_|E10A31_')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-AllInheritedCaseEnv {
  $names = @()
  Get-ChildItem Env: | ForEach-Object {
    if ($_.Name -match '^(JJFB_|GWY_|VMRP_)') { $names += $_.Name }
  }
  foreach ($n in $names) {
    Remove-Item -Path ("Env:{0}" -f $n) -ErrorAction SilentlyContinue
  }
  return $names
}

function Get-FileSha256([string]$path) {
  if (-not (Test-Path $path)) { return 'MISSING' }
  return (Get-FileHash -Path $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Reset-E10A31DArtifacts {
  @(
    $histCsv, $insnCsv, $callCsv, $provCsv, $appinfoCsv, $abiCsv,
    $stdoutLog, $stderrLog
  ) | ForEach-Object {
    if (Test-Path $_) { Remove-Item -Path $_ -Force -ErrorAction SilentlyContinue }
  }
}

function Write-EnvManifest([string[]]$cleared, $whitelist) {
  $csv = Join-Path $reportDir 'e10a31d_environment_manifest.csv'
  $lines = @('run_id,variable,value,source,whitelisted')
  foreach ($n in ($cleared | Sort-Object -Unique)) {
    $lines += ('{0},"{1}","","cleared_inherited",0' -f $RunId, $n)
  }
  foreach ($k in @($whitelist.Keys | Sort-Object)) {
    $v = ([string]$whitelist[$k]) -replace '"', '""'
    $lines += ('{0},"{1}","{2}","whitelist_set",1' -f $RunId, $k, $v)
  }
  Set-Content -Path $csv -Value $lines -Encoding UTF8
}

function Write-RuntimeManifest {
  $csv = Join-Path $reportDir 'e10a31d_runtime_manifest.csv'
  $profile = Join-Path $Root 'profiles\jjfb.json'
  $buildTs = if (Test-Path $exe) { (Get-Item $exe).LastWriteTimeUtc.ToString('o') } else { 'MISSING' }
  $rows = @(
    'run_id,key,value',
    ("{0},main_exe_sha256,{1}" -f $RunId, (Get-FileSha256 $exe)),
    ("{0},gamelist_mrp_sha256,{1}" -f $RunId, (Get-FileSha256 (Join-Path $ResourceRoot 'gwy\gamelist.mrp'))),
    ("{0},overlay_root,{1}" -f $RunId, $OverlayRoot),
    ("{0},build_timestamp_utc,{1}" -f $RunId, $buildTs),
    ("{0},mode,{1}" -f $RunId, $Mode)
  )
  Set-Content -Path $csv -Value $rows -Encoding UTF8
}

function Set-E10A31DWhitelist {
  New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
  $wl = [ordered]@{
    JJFB_E10A_RUN_ID = "$RunId"
    JJFB_E10A31_RUN_ID = "$RunId"
    JJFB_E10A31C_RUN_ID = "$RunId"
    JJFB_E10A31D_RUN_ID = "$RunId"
    JJFB_E10A_MODE = '1'
    JJFB_E10A_SHELL_TRACE = '1'
    JJFB_E9Y_MODE = '1'
    JJFB_E9Y_NO_DEBUG_AC8 = '1'
    JJFB_E9Y_NO_WORKBUF_SEED = '1'
    JJFB_PLATFORM_WORKBUF_ALLOC = '1'
    JJFB_GWY_PACK_REGISTRY = '1'
    JJFB_E9W_MODE = '1'
    JJFB_E9W_ARCHIVE_EXACT = '1'
    JJFB_DISPLAY_FIRST = '1'
    JJFB_E9B_MODE = '1'
    JJFB_VISIBLE_WINDOW = '1'
    JJFB_WINDOW_ZOOM = "$Zoom"
    JJFB_E9B_HOLD_SEC = "$HoldSec"
    JJFB_REAL_MRP_PATH = $mrpPath
    JJFB_FAST_BD0_INIT_CALL = '1'
    JJFB_FAST_PROGRESS_TICK_CALL = '1'
    JJFB_E9U_TICK_N = "$TickN"
    JJFB_TIMER_DELIVER_TRACE = '1'
    JJFB_TIMER_ARM_TRACE = '1'
    JJFB_E10A31_WAIT_MS = "$([Math]::Max(5000, $Seconds * 1000))"
    JJFB_E10A31_TIMER_CONTEXT = '1'
    JJFB_E10A31_WAIT_FOR_TIMER = '1'
    JJFB_E10A31_WAIT_FIRE_N = '3'
    JJFB_E10A31_TIMER_CSV = $timerCsv
    JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
    GWY_RESOURCE_ROOT = $ResourceRoot
    GWY_OVERLAY_ROOT = $OverlayRoot
    GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
    GWY_LAUNCH = '1'
    GWY_LAUNCH_PARAM = $param
    # Diagnostic: keep E10A31C-like env so appinfo lane can prove mismatch vs MRP header.
    # gamelist.mrp header = appid 400101 / appver 1006; do NOT hardcode fix here.
    GWY_PACKAGE_APPID = '400101'
    GWY_PACKAGE_APPVER = '12'
    GWY_MODULE_R9_SWITCH = '1'
    GWY_CALLBACK_FRAME = '1'
    JJFB_GAME_SELF_PATCH = '0'
    JJFB_LAUNCH_PATH = 'gwy_shell_core_continue'
    JJFB_LAUNCH_SOURCE = 'gwy_shell'
    JJFB_GWY_LAUNCHER_MODE = '1'
    JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
    JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
    JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
    JJFB_GWY_UPDATE_STUB = 'no_update_native_branch'
    JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
    JJFB_EXTCHUNK_PROVIDER = 'shell_core'
    JJFB_ER_RW_BIND_RESTORE = 'shell_core'
    JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'shell'
    JJFB_PUBLICATION_AUDIT = '1'
    JJFB_PACKAGE_SCOPED_CLOAD = '1'
    GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
    JJFB_E10A31C_MODE = '1'
    JJFB_E10A31C_INIT_ATOMICITY = '1'
    JJFB_E10A31C_INIT_CSV = $initCsv
    JJFB_E10A31D_MODE = '1'
    JJFB_E10A31D_HIST_CSV = $histCsv
    JJFB_E10A31D_INSN_CSV = $insnCsv
    JJFB_E10A31D_CALL_CSV = $callCsv
    JJFB_E10A31D_PROV_CSV = $provCsv
    JJFB_E10A31D_APPINFO_CSV = $appinfoCsv
    JJFB_E10A31D_ABI_CSV = $abiCsv
  }

  switch ($Mode) {
    'history' {
      $wl['JJFB_E10A31D_HISTORY'] = '1'
      $wl['JJFB_E10A31D_APPINFO'] = '1'
    }
    'method0_trace' {
      $wl['JJFB_E10A31D_HISTORY'] = '1'
      $wl['JJFB_E10A31D_METHOD0_TRACE'] = '1'
      $wl['JJFB_E10A31D_APPINFO'] = '1'
    }
    'appinfo' {
      $wl['JJFB_E10A31D_HISTORY'] = '1'
      $wl['JJFB_E10A31D_APPINFO'] = '1'
    }
    'platform_table' {
      $wl['JJFB_E10A31D_HISTORY'] = '1'
      $wl['JJFB_E10A31D_METHOD0_TRACE'] = '1'
    }
    'control_compare' {
      $wl['JJFB_E10A31D_HISTORY'] = '1'
      $wl['JJFB_E10A31D_APPINFO'] = '1'
    }
    'validate' {
      # Forbidden until method0 returns 0 — still arm provenance only.
      $wl['JJFB_E10A31D_HISTORY'] = '1'
      $wl['JJFB_E10A31D_METHOD0_TRACE'] = '1'
      $wl['JJFB_E10A31D_APPINFO'] = '1'
    }
  }

  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  return $wl
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  if (Test-LogHit $log 'MRC_INIT_RETURN_PROVENANCE_COMPLETE') {
    return 'OBSERVE_STOP_METHOD0_PROVENANCE'
  }
  if (Test-LogHit $log 'GAMELIST_INIT_METHOD0_FAILED|delivered ret6=0 ret8=0 ret0=-1') {
    if (Test-LogHit $log 'DEFERRED_TIMER_FIRED_AFTER_INIT|GWY_TIMER_DEFERRED_FIRE') {
      return 'OBSERVE_STOP_METHOD0_FAIL_DEFERRED_FIRE'
    }
    return 'OBSERVE_STOP_METHOD0_FAILED'
  }
  if (Test-LogHit $log 'GAMELIST_METHOD0_RETURN_ZERO|GAMELIST_INIT_SEQUENCE_COMPLETE') {
    return 'OBSERVE_STOP_METHOD0_OK'
  }
  if (Test-LogHit $log 'UC_ERR|mem_fault|MEM_FAULT|FETCH_UNMAPPED') {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Analyze-E10A31D([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $flags = @{
    first_pc = $t -match 'GAMELIST_EXT_FIRST_PC'
    history_armed = $t -match 'E10A31D_HISTORY_ARMED'
    method0_first = $t -match 'GAMELIST_METHOD0_FIRST_CALL'
    method0_dup = $t -match 'GAMELIST_METHOD0_DUPLICATE_CALL'
    no_natural_m0 = $t -match 'GAMELIST_NO_NATURAL_METHOD0_OBSERVED'
    partial_before_fast = $t -match 'GAMELIST_PARTIAL_INIT_BEFORE_FAST_SEQUENCE'
    appinfo_zero = $t -match 'APPINFO_ID_VERSION_ZERO'
    appinfo_mismatch = $t -match 'APPINFO_ID_VERSION_MISMATCH'
    appinfo_match = $t -match 'APPINFO_PACKAGE_METADATA_MATCH'
    sid_null = $t -match 'APPINFO_SIDNAME_NULL'
    ram_zero = $t -match 'APPINFO_RAM_ZERO'
    fail_found = $t -match 'MRC_INIT_FIRST_FAILURE_FOUND'
    prov_complete = $t -match 'MRC_INIT_RETURN_PROVENANCE_COMPLETE'
    plat_fail = $t -match 'MRC_INIT_PLATFORM_CALL_FAILED'
    precond_fail = $t -match 'MRC_INIT_INTERNAL_PRECONDITION_FAILED'
    method0_ret0 = $t -match 'GAMELIST_METHOD0_RETURN_ZERO|delivered ret6=0 ret8=0 ret0=0'
    method0_ret_neg = $t -match 'delivered ret6=0 ret8=0 ret0=-1|GAMELIST_INIT_METHOD0_FAILED'
    fast_real = $t -match 'FAST_REAL_GAMELIST_INIT_SEQUENCE'
  }

  $verdicts = @()
  if ($flags.method0_dup) { $verdicts += 'GAMELIST_METHOD0_DUPLICATE_CALL' }
  if ($flags.method0_first) { $verdicts += 'GAMELIST_METHOD0_FIRST_CALL' }
  if ($flags.no_natural_m0) { $verdicts += 'GAMELIST_NO_NATURAL_METHOD0_OBSERVED' }
  if ($flags.partial_before_fast) { $verdicts += 'GAMELIST_PARTIAL_INIT_BEFORE_FAST_SEQUENCE' }
  if ($flags.appinfo_zero) { $verdicts += 'APPINFO_ID_VERSION_ZERO' }
  if ($flags.appinfo_mismatch) { $verdicts += 'APPINFO_ID_VERSION_MISMATCH' }
  if ($flags.appinfo_match) { $verdicts += 'APPINFO_PACKAGE_METADATA_MATCH' }
  if ($flags.sid_null) { $verdicts += 'APPINFO_SIDNAME_NULL' }
  if ($flags.ram_zero) { $verdicts += 'APPINFO_RAM_ZERO' }
  if ($flags.fail_found) { $verdicts += 'MRC_INIT_FIRST_FAILURE_FOUND' }
  if ($flags.prov_complete) { $verdicts += 'MRC_INIT_RETURN_PROVENANCE_COMPLETE' }
  if ($flags.plat_fail) { $verdicts += 'MRC_INIT_PLATFORM_CALL_FAILED' }
  if ($flags.precond_fail) { $verdicts += 'MRC_INIT_INTERNAL_PRECONDITION_FAILED' }
  if ($flags.fast_real) { $verdicts += 'FAST_REAL_GAMELIST_INIT_SEQUENCE_NOT_PRODUCT' }
  if ($flags.method0_ret0) { $verdicts += 'GAMELIST_METHOD0_RETURN_ZERO' }

  $primary = 'E10A31D_INSUFFICIENT_EVIDENCE'
  if ($flags.method0_dup) { $primary = 'GAMELIST_METHOD0_DUPLICATE_CALL' }
  elseif ($flags.appinfo_zero) { $primary = 'APPINFO_ID_VERSION_ZERO' }
  elseif ($flags.appinfo_mismatch) { $primary = 'APPINFO_ID_VERSION_MISMATCH' }
  elseif ($flags.plat_fail) { $primary = 'MRC_INIT_PLATFORM_CALL_FAILED' }
  elseif ($flags.precond_fail) { $primary = 'MRC_INIT_INTERNAL_PRECONDITION_FAILED' }
  elseif ($flags.fail_found) { $primary = 'MRC_INIT_FIRST_FAILURE_FOUND' }
  elseif ($flags.method0_ret_neg -and $flags.no_natural_m0) { $primary = 'GAMELIST_METHOD0_FIRST_CALL' }
  elseif ($flags.method0_ret0) { $primary = 'GAMELIST_METHOD0_RETURN_ZERO' }
  elseif ($flags.method0_ret_neg) { $primary = 'GAMELIST_INIT_METHOD0_FAILED' }

  return [pscustomobject]@{ primary = $primary; verdicts = $verdicts; flags = $flags }
}

# ---- main ----
if ($Mode -eq 'validate') {
  Write-Host 'validate mode deferred: do not run cfg_validate until method0 returns 0'
}

if (-not $SkipBuild) {
  Write-Host '=== E10A-3.1d build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

# Lane B static artifacts (always, cheap)
try {
  Write-Host '=== E10A-3.1d Lane B helper dispatch disasm ==='
  python (Join-Path $Root 'tools\e10a31d_helper_dispatch_disasm.py') `
    --mrp (Join-Path $ResourceRoot 'gwy\gamelist.mrp') `
    --helper 0x2E3089 `
    --out-dir $outDir
} catch {
  Write-Host "Lane B disasm warning: $_"
}

Stop-E10A31DChildren
$cleared = Clear-AllInheritedCaseEnv
Reset-E10A31DArtifacts
$wl = Set-E10A31DWhitelist
Write-EnvManifest $cleared $wl
Write-RuntimeManifest

Write-Host "=== E10A-3.1d mode=$Mode run_id=$RunId seconds=$Seconds overlay=$OverlayRoot ==="
Write-Host "main.exe sha256=$(Get-FileSha256 $exe)"

$t0 = Get-Date
$killedByRunner = $false
$observeStop = $null
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog

$deadline = $t0.AddSeconds($OUTER_KILL_SEC)
while (-not $p.HasExited) {
  if ((Get-Date) -ge $deadline) {
    $killedByRunner = $true
    break
  }
  $obs = Get-ObserveStopReason $stdoutLog
  if ($obs) {
    $observeStop = $obs
    $killedByRunner = $true
    Write-Host "=== early observe stop: $observeStop ==="
    break
  }
  Start-Sleep -Milliseconds 400
  try { $p.Refresh() } catch {}
}

if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E10A31DChildren
  Start-Sleep -Milliseconds 500
  try { $p.Refresh() } catch {}
}

$elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
try { $p.Refresh() } catch {}
$hasExited = [bool]$p.HasExited
$exitCode = if ($hasExited) { [int]$p.ExitCode } else { -1 }
$an = Analyze-E10A31D $stdoutLog

$verdictList = ($an.verdicts | ForEach-Object { "- ``$_``" }) -join "`n"
$md = @"
# Stage E10A-3.1d mrc_init Failure Provenance Verdict

- **Mode**: ``$Mode``
- **run_id**: ``$RunId``
- **overlay**: ``$OverlayRoot``
- **Elapsed**: ${elapsed}s
- **Process exited**: $hasExited (code=$exitCode)
- **Killed by runner**: $killedByRunner
- **Observe stop**: ``$observeStop``
- **Primary**: ``$($an.primary)``
- **Label**: FAST_REAL_GAMELIST_INIT_SEQUENCE / NOT_PRODUCT

## Decision order
1. GAMELIST_METHOD0_DUPLICATE_CALL
2. METHOD0_FAST_CALL_ABI_WRONG
3. APPINFO_ID_VERSION_ZERO / MISMATCH
4. METHOD0_MR_TABLE_MISSING_SLOT
5. MRC_INIT_INTERNAL_PRECONDITION_FAILED / PLATFORM_CALL_FAILED
6. MRC_INIT_FIRST_FAILURE_FOUND

## Flags
| Flag | Value |
|------|-------|
| first_pc / history_armed | $($an.flags.first_pc) / $($an.flags.history_armed) |
| method0 first / dup / no_natural | $($an.flags.method0_first) / $($an.flags.method0_dup) / $($an.flags.no_natural_m0) |
| appinfo zero / mismatch / match | $($an.flags.appinfo_zero) / $($an.flags.appinfo_mismatch) / $($an.flags.appinfo_match) |
| sid_null / ram_zero | $($an.flags.sid_null) / $($an.flags.ram_zero) |
| fail_found / prov_complete | $($an.flags.fail_found) / $($an.flags.prov_complete) |
| method0 ret0 / ret_neg | $($an.flags.method0_ret0) / $($an.flags.method0_ret_neg) |

## Verdicts
$verdictList

## Artifacts
| Kind | Path |
|------|------|
| helper history | ``reports/e10a31d_helper_call_history.csv`` |
| method0 insn | ``reports/e10a31d_method0_instruction_trace.csv`` |
| method0 call tree | ``reports/e10a31d_method0_call_tree.csv`` |
| return provenance | ``reports/e10a31d_method0_return_provenance.csv`` |
| appinfo contract | ``reports/e10a31d_appinfo_contract.csv`` |
| helper ABI | ``reports/e10a31d_helper_abi_compare.csv`` |
| dispatch annotated | ``out/e10a31d/gamelist_helper_dispatch_annotated.txt`` |
| method0 cfg | ``out/e10a31d/gamelist_method0_cfg.dot`` |
| log | ``logs/e10a31d_${Mode}_stdout.txt`` |
"@
[System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

Write-Host "E10A-3.1d done mode=$Mode primary=$($an.primary) exit=$exitCode observe=$observeStop elapsed=$elapsed"
Write-Host "verdict: $verdictMd"
