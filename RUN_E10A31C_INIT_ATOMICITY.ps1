# Stage E10A-3.1c: Guest dispatch atomicity, init completion, mem_get provenance
# Modes: init_atomicity | mem_get | unknown_api | cfg_validate | compare
# Deterministic env: full JJFB_/GWY_/VMRP_ wipe, whitelist-only, unique overlay per run_id.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('init_atomicity','mem_get','unknown_api','cfg_validate','compare')]
  [string]$Mode = 'init_atomicity',
  [int]$TickN = 12,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$outDir = Join-Path $Root 'out\e10a31c'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31c_init_atomicity_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31c\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir ("e10a31c_{0}_stdout.txt" -f $Mode)
$stderrLog = Join-Path $logDir ("e10a31c_{0}_stderr.txt" -f $Mode)

$initCsv = Join-Path $reportDir 'e10a31c_init_sequence_trace.csv'
$memCsv = Join-Path $reportDir 'e10a31c_mem_get_trace.csv'
$unimplCsv = Join-Path $reportDir 'e10a31c_unknown_platform_api_trace.csv'
$exitCsv = Join-Path $reportDir 'e10a31c_process_exit_trace.csv'
$excCsv = Join-Path $reportDir 'e10a31c_native_exception_trace.csv'
$timerCsv = Join-Path $reportDir 'e10a31_timer_binding_trace.csv'
$postInitCsv = Join-Path $reportDir 'e10a31c_post_init_wait_trace.csv'

function Stop-E10A31CChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31C_|E10A31_|E10A3_')
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

function Reset-E10A31CArtifacts {
  @(
    $initCsv, $memCsv, $unimplCsv, $exitCsv, $excCsv, $postInitCsv,
    $stdoutLog, $stderrLog
  ) | ForEach-Object {
    if (Test-Path $_) { Remove-Item -Path $_ -Force -ErrorAction SilentlyContinue }
  }
}

function Write-EnvManifest([string[]]$cleared, $whitelist) {
  $csv = Join-Path $reportDir 'e10a31c_environment_manifest.csv'
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
  $csv = Join-Path $reportDir 'e10a31c_runtime_manifest.csv'
  $profile = Join-Path $Root 'profiles\jjfb.json'
  $cfn = Join-Path $ResourceRoot 'cfunction.ext'
  if (-not (Test-Path $cfn)) { $cfn = Join-Path $ResourceRoot 'gwy\cfunction.ext' }
  $buildTs = if (Test-Path $exe) { (Get-Item $exe).LastWriteTimeUtc.ToString('o') } else { 'MISSING' }
  $rows = @(
    'run_id,key,value',
    ("{0},main_exe_sha256,{1}" -f $RunId, (Get-FileSha256 $exe)),
    ("{0},gbrwcore_mrp_sha256,{1}" -f $RunId, (Get-FileSha256 (Join-Path $ResourceRoot 'gwy\gbrwcore.mrp'))),
    ("{0},gamelist_mrp_sha256,{1}" -f $RunId, (Get-FileSha256 (Join-Path $ResourceRoot 'gwy\gamelist.mrp'))),
    ("{0},gwy_mrp_sha256,{1}" -f $RunId, (Get-FileSha256 (Join-Path $ResourceRoot 'gwy\gwy.mrp'))),
    ("{0},profile_sha256,{1}" -f $RunId, (Get-FileSha256 $profile)),
    ("{0},cfunction_member_view_sha256,{1}" -f $RunId, (Get-FileSha256 $cfn)),
    ("{0},working_directory,{1}" -f $RunId, $RunDir),
    ("{0},resource_root,{1}" -f $RunId, $ResourceRoot),
    ("{0},overlay_root,{1}" -f $RunId, $OverlayRoot),
    ("{0},build_timestamp_utc,{1}" -f $RunId, $buildTs),
    ("{0},mode,{1}" -f $RunId, $Mode)
  )
  Set-Content -Path $csv -Value $rows -Encoding UTF8
}

function Set-E10A31CWhitelist {
  New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
  $wl = [ordered]@{
    JJFB_E10A_RUN_ID = "$RunId"
    JJFB_E10A31_RUN_ID = "$RunId"
    JJFB_E10A31C_RUN_ID = "$RunId"
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
    # Enable diagnostic 6→8→0 after gamelist ERW bind (shell_core path).
    JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
    GWY_RESOURCE_ROOT = $ResourceRoot
    GWY_OVERLAY_ROOT = $OverlayRoot
    GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
    GWY_LAUNCH = '1'
    GWY_LAUNCH_PARAM = $param
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
    JJFB_E10A31C_INIT_CSV = $initCsv
    JJFB_E10A31C_MEM_CSV = $memCsv
    JJFB_E10A31C_UNIMPL_CSV = $unimplCsv
    JJFB_E10A31C_EXIT_CSV = $exitCsv
    JJFB_E10A31C_EXC_CSV = $excCsv
  }

  switch ($Mode) {
    'init_atomicity' {
      $wl['JJFB_E10A31C_MODE'] = '1'
      $wl['JJFB_E10A31C_INIT_ATOMICITY'] = '1'
    }
    'mem_get' {
      $wl['JJFB_E10A31C_MODE'] = '1'
      $wl['JJFB_E10A31C_INIT_ATOMICITY'] = '1'
      $wl['JJFB_E10A31C_MEM_GET'] = '1'
    }
    'unknown_api' {
      $wl['JJFB_E10A31C_MODE'] = '1'
      $wl['JJFB_E10A31C_INIT_ATOMICITY'] = '1'
      $wl['JJFB_E10A31C_UNKNOWN_API'] = '1'
    }
    'cfg_validate' {
      $wl['JJFB_E10A31C_MODE'] = '1'
      $wl['JJFB_E10A31C_INIT_ATOMICITY'] = '1'
      $wl['JJFB_E10A31_CFG_GATE'] = '1'
      $wl['JJFB_E10A31_PARAM_TRACE'] = '1'
    }
    'compare' {
      $wl['JJFB_E10A31C_MODE'] = '1'
      $wl['JJFB_E10A31C_INIT_ATOMICITY'] = '1'
      $wl['JJFB_E10A31C_MEM_GET'] = '1'
      $wl['JJFB_E10A31C_UNKNOWN_API'] = '1'
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
  $deferredFired = Test-LogHit $log 'DEFERRED_TIMER_FIRED_AFTER_INIT'
  $fireDone = Test-LogHit $log 'FIRE_EXT_DONE|FIRE_DONE via=deferred'
  if ($deferredFired -and $fireDone) {
    if (Test-LogHit $log 'GAMELIST_INIT_SEQUENCE_COMPLETE') {
      if ($Mode -eq 'cfg_validate' -and (Test-LogHit $log 'GAMELIST_CFG_GATE_REACHED|JJFB_GAMELIST_CFG_GATE|cfg\.bin')) {
        return 'OBSERVE_STOP_CFG_AFTER_INIT'
      }
      return 'OBSERVE_STOP_INIT_COMPLETE_DEFERRED_FIRE'
    }
    if (Test-LogHit $log 'GAMELIST_INIT_METHOD0_FAILED|init_tx=end') {
      return 'OBSERVE_STOP_DEFERRED_FIRE_AFTER_INIT_TX'
    }
  }
  if (Test-LogHit $log 'UC_ERR|mem_fault|MEM_FAULT|FETCH_UNMAPPED') {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Analyze-E10A31C([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $flags = @{
    reentrancy_prevented = $t -match 'GUEST_TIMER_REENTRANCY_PREVENTED|GWY_TIMER_DEFER'
    timer_deferred_init = $t -match 'TIMER_DEFERRED_DURING_INIT'
    init6_enter = $t -match 'GAMELIST_INIT6_ENTER'
    init6_return = $t -match 'GAMELIST_INIT6_RETURN'
    init8_enter = $t -match 'GAMELIST_INIT8_ENTER'
    init8_return = $t -match 'GAMELIST_INIT8_RETURN'
    init0_enter = $t -match 'GAMELIST_INIT0_ENTER'
    init0_return = $t -match 'GAMELIST_INIT0_RETURN'
    init_complete = $t -match 'GAMELIST_INIT_SEQUENCE_COMPLETE'
    init_interrupted = $t -match 'GAMELIST_INIT_INTERRUPTED_BY_TIMER'
    deferred_fire = $t -match 'DEFERRED_TIMER_FIRED_AFTER_INIT|GWY_TIMER_DEFERRED_FIRE'
    nested_fire_during_init = $t -match 'GAMELIST_INIT_INTERRUPTED_BY_TIMER'
    mem_get_enter = $t -match 'MEM_GET_ENTER'
    mem_get_return = $t -match 'MEM_GET_RETURN'
    mem_get_no_return = $t -match 'MEM_GET_NO_RETURN'
    printf_null = $t -match 'guest-printf|\(null\)'
    cfg_gate = $t -match 'GAMELIST_CFG_GATE_REACHED|JJFB_GAMELIST_CFG_GATE'
    cfg_bin = $t -match 'cfg\.bin'
    fast_real = $t -match 'FAST_REAL_GAMELIST_INIT_SEQUENCE'
    ret6 = $t -match 'delivered ret6='
  }

  $verdicts = @()
  if ($flags.reentrancy_prevented) { $verdicts += 'GUEST_TIMER_REENTRANCY_PREVENTED' }
  if ($flags.timer_deferred_init) { $verdicts += 'TIMER_DEFERRED_DURING_INIT' }
  if ($flags.init_complete) { $verdicts += 'GAMELIST_INIT_SEQUENCE_COMPLETE' }
  if ($flags.deferred_fire) { $verdicts += 'DEFERRED_TIMER_FIRED_AFTER_INIT' }
  if ($flags.init_interrupted) { $verdicts += 'GAMELIST_INIT_INTERRUPTED_BY_TIMER' }
  if ($flags.mem_get_return) { $verdicts += 'MEM_GET_RETURN_PROVEN' }
  if ($flags.cfg_gate) { $verdicts += 'GAMELIST_CFG_GATE_REACHED' }
  if ($flags.cfg_bin) { $verdicts += 'REAL_CFG_BIN_OPEN' }
  if ($flags.fast_real) { $verdicts += 'FAST_REAL_GAMELIST_INIT_SEQUENCE_NOT_PRODUCT' }

  $primary = 'E10A31C_INSUFFICIENT_EVIDENCE'
  if ($flags.init_interrupted -or $flags.nested_fire_during_init) {
    $primary = 'GAMELIST_INIT_INTERRUPTED_BY_TIMER'
  } elseif ($t -match 'GAMELIST_INIT_METHOD0_FAILED') {
    $primary = 'GAMELIST_INIT_METHOD0_FAILED'
    $verdicts = @('GAMELIST_INIT_METHOD0_FAILED') + $verdicts
  } elseif ($flags.init_complete -and $flags.deferred_fire -and $flags.cfg_bin) {
    $primary = 'GAMELIST_INIT_COMPLETE_TO_CFG_OPEN'
  } elseif ($flags.init_complete -and $flags.deferred_fire -and $flags.cfg_gate) {
    $primary = 'GAMELIST_CFG_GATE_REACHED'
  } elseif ($flags.init_complete -and $flags.deferred_fire) {
    $primary = 'DEFERRED_TIMER_FIRED_AFTER_INIT'
  } elseif ($flags.init_complete) {
    $primary = 'GAMELIST_INIT_SEQUENCE_COMPLETE'
  } elseif ($flags.reentrancy_prevented -and $flags.deferred_fire) {
    $primary = 'DEFERRED_TIMER_FIRED_AFTER_INIT'
  } elseif ($flags.reentrancy_prevented -and -not $flags.init_complete) {
    $primary = 'GUEST_TIMER_REENTRANCY_PREVENTED'
  } elseif ($flags.init6_enter -and -not $flags.init6_return) {
    $primary = 'GAMELIST_INIT_GUEST_EXIT'
  }

  return [pscustomobject]@{
    primary = $primary
    verdicts = $verdicts
    flags = $flags
  }
}

# ---- main ----
if (-not $SkipBuild) {
  Write-Host '=== E10A-3.1c build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

Stop-E10A31CChildren
$cleared = Clear-AllInheritedCaseEnv
Reset-E10A31CArtifacts
$wl = Set-E10A31CWhitelist
Write-EnvManifest $cleared $wl
Write-RuntimeManifest

Write-Host "=== E10A-3.1c mode=$Mode run_id=$RunId seconds=$Seconds outer_kill=${OUTER_KILL_SEC}s overlay=$OverlayRoot ==="
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
  Stop-E10A31CChildren
  Start-Sleep -Milliseconds 500
  try { $p.Refresh() } catch {}
}

$elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
try { $p.Refresh() } catch {}
$hasExited = [bool]$p.HasExited
$exitCode = if ($hasExited) { [int]$p.ExitCode } else { -1 }
$an = Analyze-E10A31C $stdoutLog

# Runner-side process exit row (always, even if runtime CSV missing).
$exitClass = switch ($exitCode) {
  -1073741819 { 'ACCESS_VIOLATION' }   # 0xC0000005
  -1073741571 { 'STACK_OVERFLOW' }     # 0xC00000FD
  -1073741795 { 'ILLEGAL_INSTRUCTION' }# 0xC000001D
  -1073740940 { 'HEAP_CORRUPTION' }    # 0xC0000374
  0 { 'NORMAL_EXIT' }
  default { if ($killedByRunner) { 'RUNNER_KILL' } else { 'OTHER' } }
}
$runnerExitHdr = 'run_id,mode,elapsed,has_exited,exit_code,exit_class,killed_by_runner,observe_stop,primary,note'
$runnerExitLine = "$RunId,$Mode,$elapsed,$([int]$hasExited),$exitCode,$exitClass,$([int]$killedByRunner),`"$observeStop`",`"$($an.primary)`",runner_observed"
if (-not (Test-Path $exitCsv)) {
  Set-Content -Path $exitCsv -Value @($runnerExitHdr, $runnerExitLine) -Encoding utf8
} else {
  Add-Content -Path $exitCsv -Value $runnerExitLine -Encoding utf8
}

# Post-init wait predicate (only when init complete + deferred fire, still no cfg).
if ($an.flags.init_complete -and $an.flags.deferred_fire -and -not $an.flags.cfg_bin) {
  $t = Get-Content $stdoutLog -Raw -EA SilentlyContinue
  $pred = 'unknown'
  if ($t -match 'POLL_WAIT') { $pred = 'timer_poll_wait' }
  elseif ($t -match 'POST_START_LOOP') { $pred = 'post_start_loop' }
  elseif ($t -match 'VISIBLE_WINDOW_HOLD') { $pred = 'window_hold' }
  $hdr = 'run_id,predicate,init_complete,deferred_fire,cfg_bin,note'
  $line = "$RunId,$pred,1,1,0,post_init_still_no_cfg"
  Set-Content -Path $postInitCsv -Value @($hdr, $line) -Encoding utf8
}

$verdictList = ($an.verdicts | ForEach-Object { "- ``$_``" }) -join "`n"
$md = @"
# Stage E10A-3.1c Init Atomicity Verdict

- **Mode**: ``$Mode``
- **run_id**: ``$RunId``
- **overlay**: ``$OverlayRoot``
- **Requested seconds**: $Seconds
- **Elapsed**: ${elapsed}s
- **Process exited**: $hasExited (code=$exitCode class=$exitClass)
- **Killed by runner**: $killedByRunner
- **Observe stop**: ``$observeStop``
- **Primary**: ``$($an.primary)``
- **Label**: FAST_REAL_GAMELIST_INIT_SEQUENCE / NOT_PRODUCT (diagnostic reconstruction)

## Success order
1. GUEST_TIMER_REENTRANCY_PREVENTED
2. GAMELIST_INIT_SEQUENCE_COMPLETE
3. DEFERRED_TIMER_FIRED_AFTER_INIT
4. (later) MEM_GET_RETURN_PROVEN / cfg gate

## Flags
| Flag | Value |
|------|-------|
| reentrancy_prevented | $($an.flags.reentrancy_prevented) |
| timer_deferred_init | $($an.flags.timer_deferred_init) |
| init6 enter/return | $($an.flags.init6_enter) / $($an.flags.init6_return) |
| init8 enter/return | $($an.flags.init8_enter) / $($an.flags.init8_return) |
| init0 enter/return | $($an.flags.init0_enter) / $($an.flags.init0_return) |
| init_complete | $($an.flags.init_complete) |
| init_interrupted | $($an.flags.init_interrupted) |
| deferred_fire | $($an.flags.deferred_fire) |
| mem_get enter/return | $($an.flags.mem_get_enter) / $($an.flags.mem_get_return) |
| cfg_gate / cfg_bin | $($an.flags.cfg_gate) / $($an.flags.cfg_bin) |

## Verdicts
$verdictList

## Artifacts
| Kind | Path |
|------|------|
| init sequence | ``reports/e10a31c_init_sequence_trace.csv`` |
| mem_get | ``reports/e10a31c_mem_get_trace.csv`` |
| unknown API | ``reports/e10a31c_unknown_platform_api_trace.csv`` |
| process exit | ``reports/e10a31c_process_exit_trace.csv`` |
| native exception | ``reports/e10a31c_native_exception_trace.csv`` |
| env manifest | ``reports/e10a31c_environment_manifest.csv`` |
| runtime manifest | ``reports/e10a31c_runtime_manifest.csv`` |
| log | ``logs/e10a31c_${Mode}_stdout.txt`` |
"@
[System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

Write-Host "E10A-3.1c done mode=$Mode primary=$($an.primary) exit=$exitCode/$exitClass observe=$observeStop elapsed=$elapsed"
Write-Host "verdict: $verdictMd"
