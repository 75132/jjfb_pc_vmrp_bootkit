# Stage E10A-3.1: Gamelist package-context and launch-param handoff
# Deterministic runner (ported from E10A-3.1a): full JJFB_/GWY_/VMRP_ wipe,
# whitelist-only env, unique overlay per run_id, SHA256 manifests, run_id-scoped CSV.
# Modes: timer_context | param_trace | cfg_gate | root_compare | validate
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('timer_context','param_trace','cfg_gate','root_compare','validate')]
  [string]$Mode = 'timer_context',
  [int]$TickN = 12,
  [int]$EarlyStopFireN = 3,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$outE31 = Join-Path $Root 'out\e10a31'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outE31 | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31_gamelist_context_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'e10a31_gamelist_context_stdout.txt'
$stderrLog = Join-Path $logDir 'e10a31_gamelist_context_stderr.txt'
$timerCsvPath = Join-Path $reportDir 'e10a31_timer_binding_trace.csv'
$paramCsvPath = Join-Path $reportDir 'e10a31_launch_param_read_trace.csv'
$startCsvPath = Join-Path $reportDir 'e10a31_start_dsm_param_abi.csv'
$gateCsvPath = Join-Path $reportDir 'e10a31_cfg_gate_predicates.csv'

function Stop-E10A31Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31_|E10A3_|E10A_GWY')
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

function Reset-E10A31Artifacts {
  @(
    $timerCsvPath, $paramCsvPath, $startCsvPath, $gateCsvPath,
    (Join-Path $reportDir 'e10a31_root_vs_continue_gamelist_context.csv'),
    (Join-Path $reportDir 'e10a31_process_exit_trace.csv'),
    (Join-Path $reportDir 'e10a31_environment_manifest.csv'),
    (Join-Path $reportDir 'e10a31_runtime_manifest.csv'),
    (Join-Path $reportDir 'e10a31b_publication_path.csv'),
    $stdoutLog, $stderrLog
  ) | ForEach-Object {
    if (Test-Path $_) {
      Remove-Item -Path $_ -Force -ErrorAction SilentlyContinue
    }
  }
}

function Write-E10A31EnvManifest([string[]]$cleared, $whitelist) {
  $csv = Join-Path $reportDir 'e10a31_environment_manifest.csv'
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

function Write-E10A31RuntimeManifest {
  $csv = Join-Path $reportDir 'e10a31_runtime_manifest.csv'
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

function Set-E10A31Whitelist {
  New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
  $wl = [ordered]@{
    JJFB_E10A_RUN_ID = "$RunId"
    JJFB_E10A31_RUN_ID = "$RunId"
    JJFB_E10A_MODE = '1'
    JJFB_E10A_SHELL_TRACE = '1'
    # NOTE: do not enable E10A3 POSTSELECT/WAITSTATE on timer_context — proven
    # E10A-3.1a A/B success path omitted them; they reintroduce pre-continue noise.
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
  }

  switch ($Mode) {
    'timer_context' {
      # Match proven E10A-3.1a case B lean path: TIMER_CONTEXT only.
      # Do NOT set JJFB_E10A31_MODE=1 — that also arms param/cfg observe hooks.
      $wl['JJFB_E10A31_TIMER_CONTEXT'] = '1'
      $wl['JJFB_E10A31_WAIT_FOR_TIMER'] = '1'
      $wl['JJFB_E10A31_WAIT_FIRE_N'] = '3'
      $wl['JJFB_E10A31_TIMER_CSV'] = $timerCsvPath
      $wl['JJFB_E10A31B_MODE'] = '1'
      $wl['JJFB_E10A31B_PUB_CSV'] = (Join-Path $reportDir 'e10a31b_publication_path.csv')
    }
    'param_trace' {
      # Keep proven timer + 3.1b publication; add param/start_dsm observe only.
      $wl['JJFB_E10A31_TIMER_CONTEXT'] = '1'
      $wl['JJFB_E10A31_WAIT_FOR_TIMER'] = '1'
      $wl['JJFB_E10A31_WAIT_FIRE_N'] = '3'
      $wl['JJFB_E10A31_TIMER_CSV'] = $timerCsvPath
      $wl['JJFB_E10A31B_MODE'] = '1'
      $wl['JJFB_E10A31B_PUB_CSV'] = (Join-Path $reportDir 'e10a31b_publication_path.csv')
      $wl['JJFB_E10A31_PARAM_TRACE'] = '1'
      $wl['JJFB_E10A31_START_DSM_ABI'] = '1'
      $wl['JJFB_E10A31_PARAM_CSV'] = $paramCsvPath
      $wl['JJFB_E10A31_START_DSM_CSV'] = $startCsvPath
    }
    'cfg_gate' {
      # Keep proven timer + 3.1b; add cfg gate. Avoid E10A3 POSTSELECT/WAITSTATE
      # (historically exits ~11s at font load before gamelist timer).
      $wl['JJFB_E10A31_TIMER_CONTEXT'] = '1'
      $wl['JJFB_E10A31_WAIT_FOR_TIMER'] = '1'
      $wl['JJFB_E10A31_WAIT_FIRE_N'] = '3'
      $wl['JJFB_E10A31_TIMER_CSV'] = $timerCsvPath
      $wl['JJFB_E10A31B_MODE'] = '1'
      $wl['JJFB_E10A31B_PUB_CSV'] = (Join-Path $reportDir 'e10a31b_publication_path.csv')
      $wl['JJFB_E10A31_PARAM_TRACE'] = '1'
      $wl['JJFB_E10A31_CFG_GATE'] = '1'
      $wl['JJFB_E10A31_PARAM_CSV'] = $paramCsvPath
      $wl['JJFB_E10A31_CFG_GATE_CSV'] = $gateCsvPath
      # Allow DOCUMENTED 6→8→0 after gamelist ERW bind (shell_core path).
      $wl['JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE'] = '1'
    }
    'root_compare' {
      $wl['JJFB_E10A31_MODE'] = '1'
      $wl['JJFB_E10A31_TIMER_CONTEXT'] = '1'
      $wl['JJFB_E10A31_PARAM_TRACE'] = '1'
      $wl['JJFB_E10A31_TIMER_CSV'] = $timerCsvPath
      $wl['JJFB_E10A31_PARAM_CSV'] = $paramCsvPath
    }
    'validate' {
      $wl['JJFB_E10A31_MODE'] = '1'
      $wl['JJFB_E10A31_TIMER_CONTEXT'] = '1'
      $wl['JJFB_E10A31_PARAM_TRACE'] = '1'
      $wl['JJFB_E10A31_CFG_GATE'] = '1'
      $wl['JJFB_E10A31_START_DSM_ABI'] = '1'
      $wl['JJFB_E10A31_WAIT_FOR_TIMER'] = '1'
      $wl['JJFB_E10A31_TIMER_CSV'] = $timerCsvPath
      $wl['JJFB_E10A31_PARAM_CSV'] = $paramCsvPath
      $wl['JJFB_E10A31_START_DSM_CSV'] = $startCsvPath
      $wl['JJFB_E10A31_CFG_GATE_CSV'] = $gateCsvPath
    }
  }

  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  return $wl
}

function Get-CsvRows([string]$path) {
  if (-not (Test-Path $path)) { return @() }
  try { return @(Import-Csv $path -ErrorAction SilentlyContinue) } catch { return @() }
}

function Get-RunIdRows([string]$path) {
  $rid = "$RunId"
  return @(Get-CsvRows $path | Where-Object {
    (-not $_.run_id) -or ("$($_.run_id)" -eq $rid)
  })
}

function Test-HexNonZero([string]$v) {
  if (-not $v) { return $false }
  $s = "$v".Trim()
  if ($s -match '^0x0+$' -or $s -eq '0' -or $s -eq '0x0') { return $false }
  if ($s -match '^0x[0-9A-Fa-f]+$' -or $s -match '^[0-9]+$') { return $true }
  return $false
}

function Test-OwnerResolved([string]$v) {
  if (-not $v) { return $false }
  $s = "$v".Trim()
  if ($s -eq '' -or $s -eq '0' -or $s -eq '0x0' -or $s -match '^0x0+$') { return $false }
  return $true
}

function Get-ObserveStopReason {
  $timerRows = Get-RunIdRows $timerCsvPath
  $armRows = @($timerRows | Where-Object { $_.event -eq 'TIMER_ARM' })
  $fireRows = @($timerRows | Where-Object { $_.event -eq 'TIMER_FIRE' })
  $classVals = @($timerRows | ForEach-Object { $_.classification }) -join '|'

  if ($classVals -match 'TIMER_CONTEXT_MIXED_MULTI_OWNER') {
    return 'OBSERVE_STOP_MIXED_MULTI_OWNER'
  }
  if ((Test-Path $stdoutLog) -and
      (Select-String -Path $stdoutLog -Pattern 'GAMELIST_CFG_GATE_REACHED|JJFB_GAMELIST_CFG_GATE' -Quiet -EA SilentlyContinue)) {
    return 'OBSERVE_STOP_CFG_GATE'
  }
  # param_trace: guest may parse entry during start_dsm before any TIMER_ARM.
  if ($Mode -eq 'param_trace' -and (Test-Path $stdoutLog) -and
      (Select-String -Path $stdoutLog -Pattern 'SHELL_PARAM_GWYBLINK_PARSED|JJFB_E10A31_PARAM_READ|JJFB_PARAM_READ' -Quiet -EA SilentlyContinue)) {
    return 'OBSERVE_STOP_PARAM_READ'
  }
  if ($armRows.Count -gt 0 -and $fireRows.Count -ge $EarlyStopFireN) {
    return "OBSERVE_STOP_TIMER_FIRE_$($fireRows.Count)"
  }
  if ((Test-Path $stdoutLog) -and
      (Select-String -Path $stdoutLog -Pattern 'UC_ERR|mem_fault|MEM_FAULT|VM_FAULT|FETCH_UNMAPPED' -Quiet -EA SilentlyContinue)) {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Analyze-E10A31([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $timerRows = Get-RunIdRows $timerCsvPath
  $paramN = @(Get-RunIdRows $paramCsvPath).Count
  $startN = @(Get-RunIdRows $startCsvPath).Count
  $gateN = @(Get-RunIdRows $gateCsvPath).Count
  $timerN = $timerRows.Count

  $armRows = @($timerRows | Where-Object { $_.event -eq 'TIMER_ARM' })
  $fireRows = @($timerRows | Where-Object { $_.event -eq 'TIMER_FIRE' })
  $fireRetRows = @($timerRows | Where-Object { $_.event -eq 'TIMER_FIRE_RET' })
  $classVals = @($timerRows | ForEach-Object { $_.classification }) -join '|'

  $sample = $null
  if ($fireRows.Count -gt 0) { $sample = $fireRows[0] }
  elseif ($armRows.Count -gt 0) { $sample = $armRows[0] }

  $ownersOk = $false
  $bindingOk = $false
  if ($null -ne $sample) {
    $helperV = $sample.helper
    $pV = if ($sample.p_guest) { $sample.p_guest } else { $sample.P }
    $chunkV = if ($sample.chunk_guest) { $sample.chunk_guest } else { $sample.chunk }
    $erwV = if ($sample.erw) { $sample.erw } elseif ($sample.er_rw) { $sample.er_rw } else { $sample.ERW }
    $bindingOk = (Test-HexNonZero $helperV) -and (Test-HexNonZero $pV) -and `
                 (Test-HexNonZero $chunkV) -and (Test-HexNonZero $erwV)
    $ho = if ($sample.helper_module) { $sample.helper_module } else { $sample.helper_module_id }
    $co = if ($sample.chunk_module) { $sample.chunk_module } else { $sample.chunk_module_id }
    $po = if ($sample.p_module) { $sample.p_module } else { $sample.p_module_id }
    $eo = if ($sample.erw_module) { $sample.erw_module } else { $sample.erw_module_id }
    $ownersOk = (Test-OwnerResolved $ho) -and (Test-OwnerResolved $co) -and `
                (Test-OwnerResolved $po) -and (Test-OwnerResolved $eo)
  }

  $fromCsv = @{
    rebound = ($classVals -match 'GAMELIST_TIMER_CONTEXT_REBOUND')
    coherent = ($classVals -match 'TIMER_CONTEXT_COHERENT')
    mixed_multi = ($classVals -match 'TIMER_CONTEXT_MIXED_MULTI_OWNER')
    chunk_mm = ($classVals -match 'TIMER_CHUNK_OWNER_MISMATCH')
    p_mm = ($classVals -match 'TIMER_P_OWNER_MISMATCH')
    erw_mm = ($classVals -match 'TIMER_ERW_OWNER_MISMATCH')
    erw_unpublished = ($classVals -match 'GAMELIST_ERW_NOT_PUBLISHED')
    mixed = ($classVals -match 'TIMER_CONTEXT_MIXED')
  }

  $flags = @{
    ext_first_pc = $t -match 'GAMELIST_EXT_FIRST_PC|mark=GAMELIST_EXT_FIRST_PC'
    post_cont = $t -match 'POST_CONT_PUMP'
    continue_apply = $t -match 'GWY_CONTINUE_APPLY|JJFB_SHELL_CORE_CONTINUE\] from=gbrwcore'
    timer_arm_csv = ($armRows.Count -gt 0)
    timer_fire_csv = ($fireRows.Count -gt 0)
    timer_mixed = [bool]$fromCsv.mixed -or [bool]$fromCsv.mixed_multi -or `
                  [bool]$fromCsv.chunk_mm -or [bool]$fromCsv.p_mm -or [bool]$fromCsv.erw_mm -or `
                  [bool]$fromCsv.erw_unpublished
    timer_mixed_multi = [bool]$fromCsv.mixed_multi
    timer_coherent = [bool]$fromCsv.coherent
    timer_rebound = [bool]$fromCsv.rebound
    chunk_mm = [bool]$fromCsv.chunk_mm
    p_mm = [bool]$fromCsv.p_mm
    erw_mm = [bool]$fromCsv.erw_mm
    erw_unpublished = [bool]$fromCsv.erw_unpublished
    chunk_retarget = $t -match 'GAMELIST_CHUNK_RETARGETED'
    chunk_reuse_refused = $t -match 'GAMELIST_CHUNK_REUSE_REFUSED'
    chunk_created = $t -match 'GAMELIST_CHUNK_CREATED'
    erw_published = $t -match 'GAMELIST_ERW_PUBLISHED'
    foreign_erw = $t -match 'GAMELIST_P_REUSES_FOREIGN_ERW|FOREIGN_ERW_REFUSED'
    timer_own = $t -match 'GAMELIST_TIMER_ARMED_WITH_OWN_CHUNK'
    binding_ok = $bindingOk
    owners_ok = $ownersOk
    param_mem_read = $t -match 'JJFB_E10A31_PARAM_READ'
    param_reg_read = $t -match 'JJFB_PARAM_READ'
    start_dsm_abi = $t -match 'JJFB_E10A31_START_DSM'
    cfg_gate = $t -match 'GAMELIST_CFG_GATE_REACHED|JJFB_GAMELIST_CFG_GATE'
    cfg_open = $t -match 'GAMELIST_EXTERNAL_CFG_OPEN'
    cfg_bin = $t -match 'cfg\.bin'
    timer_fire_log = $t -match 'FIRE_EXT.*helper=0x'
    timer_evidence = ($armRows.Count -gt 0 -and $fireRows.Count -gt 0 -and $bindingOk -and $ownersOk)
  }

  $verdicts = @()
  if ($flags.ext_first_pc) { $verdicts += 'GAMELIST_EXT_FIRST_PC' }
  if ($flags.timer_rebound) { $verdicts += 'GAMELIST_TIMER_CONTEXT_REBOUND' }
  if ($flags.timer_coherent -and -not $flags.timer_rebound) { $verdicts += 'GAMELIST_TIMER_CONTEXT_COHERENT' }
  if ($flags.timer_mixed_multi) { $verdicts += 'TIMER_CONTEXT_MIXED_MULTI_OWNER' }
  if ($flags.chunk_mm) { $verdicts += 'TIMER_CHUNK_OWNER_MISMATCH' }
  if ($flags.p_mm) { $verdicts += 'TIMER_P_OWNER_MISMATCH' }
  if ($flags.erw_unpublished) { $verdicts += 'GAMELIST_ERW_NOT_PUBLISHED' }
  if ($flags.erw_mm) { $verdicts += 'TIMER_ERW_OWNER_MISMATCH' }
  if ($flags.chunk_retarget) { $verdicts += 'GAMELIST_CHUNK_RETARGETED' }
  if ($flags.chunk_reuse_refused) { $verdicts += 'GAMELIST_CHUNK_REUSE_REFUSED' }
  if ($flags.chunk_created) { $verdicts += 'GAMELIST_CHUNK_CREATED' }
  if ($flags.erw_published) { $verdicts += 'GAMELIST_ERW_PUBLISHED' }
  if ($flags.timer_own) { $verdicts += 'GAMELIST_TIMER_ARMED_WITH_OWN_CHUNK' }
  if ($flags.foreign_erw) { $verdicts += 'GAMELIST_P_REUSES_FOREIGN_ERW' }

  if ($paramN -gt 0 -or $flags.param_mem_read) { $verdicts += 'START_DSM_PARAM_REACHED_GAMELIST' }
  elseif ($startN -gt 0) { $verdicts += 'START_DSM_PARAM_ABI_CONFIRMED' }
  elseif (-not $flags.param_reg_read) { $verdicts += 'SHELL_PARAM_NOT_CONSUMED' }
  if ($gateN -gt 0 -or $flags.cfg_gate) { $verdicts += 'GAMELIST_CFG_GATE_REACHED' }
  if ($flags.cfg_open) { $verdicts += 'GAMELIST_CFG_OPEN_REACHED' }
  if (-not $flags.cfg_bin) { $verdicts += 'NO_REAL_CFG_BIN_OPEN' }

  # Classification order: rebound > coherent/own > multi > chunk > p > erw_unpublished > erw
  # Mode overlays: param_trace / cfg_gate promote their stage milestone when timer is healthy.
  if ($flags.timer_rebound) {
    $primary = 'GAMELIST_TIMER_CONTEXT_REBOUND'
  } elseif ($flags.timer_own -or ($flags.timer_evidence -and $flags.timer_coherent -and -not $flags.timer_mixed_multi)) {
    $primary = 'GAMELIST_TIMER_CONTEXT_COHERENT'
  } elseif ($flags.timer_mixed_multi) {
    $primary = 'TIMER_CONTEXT_MIXED_MULTI_OWNER'
  } elseif ($flags.chunk_mm) {
    $primary = 'TIMER_CHUNK_OWNER_MISMATCH'
  } elseif ($flags.p_mm) {
    $primary = 'TIMER_P_OWNER_MISMATCH'
  } elseif ($flags.erw_unpublished -or ($flags.chunk_retarget -and $flags.erw_mm)) {
    $primary = 'GAMELIST_ERW_NOT_PUBLISHED'
  } elseif ($flags.erw_mm) {
    $primary = 'TIMER_ERW_OWNER_MISMATCH'
  } elseif ($flags.ext_first_pc -and -not $flags.timer_arm_csv) {
    $primary = 'GAMELIST_REACHED_TIMER_NOT_REGISTERED'
    $verdicts += 'GAMELIST_REACHED_TIMER_NOT_REGISTERED'
  } elseif (-not $flags.timer_evidence) {
    $primary = 'E10A31_INSUFFICIENT_TIMER_EVIDENCE'
    $verdicts += 'E10A31_INSUFFICIENT_TIMER_EVIDENCE'
  } elseif ($flags.timer_coherent) {
    $primary = 'GAMELIST_TIMER_CONTEXT_COHERENT'
  } else {
    $primary = 'E10A31_INSUFFICIENT_TIMER_EVIDENCE'
    $verdicts += 'E10A31_INSUFFICIENT_TIMER_EVIDENCE'
  }

  if ($Mode -eq 'param_trace' -and ($paramN -gt 0 -or $flags.param_mem_read) -and
      ($primary -eq 'GAMELIST_TIMER_CONTEXT_COHERENT' -or $primary -eq 'GAMELIST_TIMER_CONTEXT_REBOUND')) {
    $primary = 'START_DSM_PARAM_REACHED_GAMELIST'
  }
  if ($Mode -eq 'cfg_gate' -and ($gateN -gt 0 -or $flags.cfg_gate) -and
      ($primary -eq 'GAMELIST_TIMER_CONTEXT_COHERENT' -or $primary -eq 'GAMELIST_TIMER_CONTEXT_REBOUND' -or
       $primary -eq 'START_DSM_PARAM_REACHED_GAMELIST')) {
    $primary = 'GAMELIST_CFG_GATE_REACHED'
  }

  return [pscustomobject]@{
    primary = $primary
    verdicts = $verdicts
    flags = $flags
    counts = @{
      timer = $timerN; arm = $armRows.Count; fire = $fireRows.Count
      fire_ret = $fireRetRows.Count; param = $paramN; start = $startN; gate = $gateN
    }
    sample = $sample
  }
}

function Write-E10A31ExitTrace(
  [string]$log,
  [double]$elapsed,
  [bool]$hasExited,
  [object]$exitCode,
  [bool]$killedByRunner,
  [string]$observeStop,
  [object]$an
) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $lastPhase = '?'
  if ($t -match 'phase=(\S+)') {
    $ms = [regex]::Matches($t, 'phase=(\S+)')
    if ($ms.Count -gt 0) { $lastPhase = $ms[$ms.Count - 1].Groups[1].Value }
  }
  $lastModule = '?'
  if ($t -match 'module=([A-Za-z0-9_./]+)') {
    $ms = [regex]::Matches($t, 'module=([A-Za-z0-9_./]+)')
    if ($ms.Count -gt 0) { $lastModule = $ms[$ms.Count - 1].Groups[1].Value }
  }
  $lastPc = '0x0'; $lastLr = '0x0'
  if ($t -match 'pc=(0x[0-9A-Fa-f]+)') {
    $ms = [regex]::Matches($t, 'pc=(0x[0-9A-Fa-f]+)')
    if ($ms.Count -gt 0) { $lastPc = $ms[$ms.Count - 1].Groups[1].Value }
  }
  if ($t -match 'lr=(0x[0-9A-Fa-f]+)') {
    $ms = [regex]::Matches($t, 'lr=(0x[0-9A-Fa-f]+)')
    if ($ms.Count -gt 0) { $lastLr = $ms[$ms.Count - 1].Groups[1].Value }
  }
  $lastHelper = '0x0'
  if ($t -match 'helper=(0x[0-9A-Fa-f]+)') {
    $ms = [regex]::Matches($t, 'helper=(0x[0-9A-Fa-f]+)')
    if ($ms.Count -gt 0) { $lastHelper = $ms[$ms.Count - 1].Groups[1].Value }
  }
  $lastApi = '?'
  if ($t -match '\[PLATFORM_TIMER\] op=(\S+)') {
    $ms = [regex]::Matches($t, '\[PLATFORM_TIMER\] op=(\S+)')
    if ($ms.Count -gt 0) { $lastApi = 'PLATFORM_TIMER/' + $ms[$ms.Count - 1].Groups[1].Value }
  }

  $stopReason = 'UNKNOWN'
  if ($observeStop) { $stopReason = $observeStop }
  elseif ($killedByRunner) { $stopReason = 'RUNNER_KILL_TIMEOUT' }
  elseif ($t -match 'UC_ERR|mem_fault|MEM_FAULT|VM_FAULT') { $stopReason = 'VM_FAULT' }
  elseif ($t -match 'mythroad exit|GUEST_MR_EXIT') { $stopReason = 'GUEST_MR_EXIT' }
  elseif ($t -match 'JJFB_E10A_EXIT_PARK') { $stopReason = 'HOST_DIAGNOSTIC_EXIT' }
  elseif ($t -match 'VISIBLE_WINDOW_HOLD_DONE') { $stopReason = 'WINDOW_CLOSE' }
  elseif ($hasExited) { $stopReason = 'GUEST_NORMAL_EXIT' }

  $exitVerdict = 'E10A31_HOST_DIAGNOSTIC_EXIT_EARLY'
  if ($observeStop -match 'TIMER_FIRE') { $exitVerdict = 'E10A31_REACHED_GAMELIST_TIMER' }
  elseif ($killedByRunner -and -not $observeStop) { $exitVerdict = 'E10A31_HOST_TIMEOUT' }
  elseif ($an.flags.timer_evidence) { $exitVerdict = 'E10A31_REACHED_GAMELIST_TIMER' }
  elseif ($stopReason -eq 'VM_FAULT') { $exitVerdict = 'E10A31_VM_FAULT_BEFORE_GAMELIST' }
  elseif ($an.flags.ext_first_pc -and -not $an.flags.timer_arm_csv) {
    $exitVerdict = 'GAMELIST_REACHED_TIMER_NOT_REGISTERED'
  } elseif ($elapsed -lt ($Seconds * 0.5) -and -not $an.flags.timer_evidence) {
    $exitVerdict = 'E10A31_HOST_DIAGNOSTIC_EXIT_EARLY'
  }

  $exitCsv = Join-Path $reportDir 'e10a31_process_exit_trace.csv'
  $hdr = 'run_id,mode,elapsed,has_exited,exit_code,killed_by_runner,observe_stop,last_phase,last_module,last_pc,last_lr,last_helper,last_api,stop_reason,note'
  $note = "exit_verdict=$exitVerdict;timer_evidence=$($an.flags.timer_evidence);requested_seconds=$Seconds;overlay=$OverlayRoot"
  $ec = if ($null -eq $exitCode) { '' } else { "$exitCode" }
  $line = "$RunId,$Mode,$elapsed,$([int]$hasExited),$ec,$([int]$killedByRunner),`"$observeStop`",`"$lastPhase`",`"$lastModule`",$lastPc,$lastLr,$lastHelper,`"$lastApi`",$stopReason,`"$note`""
  Set-Content -Path $exitCsv -Value @($hdr, $line) -Encoding utf8
  return [pscustomobject]@{
    stop_reason = $stopReason
    exit_verdict = $exitVerdict
    last_phase = $lastPhase
    last_module = $lastModule
    last_pc = $lastPc
    last_helper = $lastHelper
    last_api = $lastApi
    observe_stop = $observeStop
  }
}

# ---- main ----
if (-not $SkipBuild) {
  Write-Host '=== E10A-3.1 build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

Stop-E10A31Children
$cleared = Clear-AllInheritedCaseEnv
Reset-E10A31Artifacts
$wl = Set-E10A31Whitelist
Write-E10A31EnvManifest $cleared $wl
Write-E10A31RuntimeManifest

Write-Host "=== E10A-3.1 mode=$Mode run_id=$RunId requested_seconds=$Seconds outer_kill=${OUTER_KILL_SEC}s wait_ms=$($env:JJFB_E10A31_WAIT_MS) wait_for_timer=$($env:JJFB_E10A31_WAIT_FOR_TIMER) overlay=$OverlayRoot ==="
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
  if ($Mode -eq 'timer_context' -or $Mode -eq 'validate') {
    $obs = Get-ObserveStopReason
    if ($obs) {
      $observeStop = $obs
      $killedByRunner = $true
      Write-Host "=== early observe stop: $observeStop ==="
      break
    }
  }
  Start-Sleep -Milliseconds 400
  try { $p.Refresh() } catch {}
}

if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E10A31Children
  Start-Sleep -Milliseconds 500
  try { $p.Refresh() } catch {}
}

$elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
try { $p.Refresh() } catch {}
$hasExited = [bool]$p.HasExited
$exitCode = if ($hasExited) { [int]$p.ExitCode } else { -1 }
$an = Analyze-E10A31 $stdoutLog
$exitInfo = Write-E10A31ExitTrace -log $stdoutLog -elapsed $elapsed -hasExited $hasExited `
  -exitCode $exitCode -killedByRunner $killedByRunner -observeStop $observeStop -an $an
if ($an.verdicts -notcontains $exitInfo.exit_verdict) {
  $an.verdicts = @($exitInfo.exit_verdict) + @($an.verdicts)
}

$sampleTxt = if ($an.sample) {
  $s = $an.sample
  "helper=$($s.helper) P=$($s.p_guest) chunk=$($s.chunk_guest) ERW=$($s.erw) class=$($s.classification) helper_mod=$($s.helper_module) chunk_mod=$($s.chunk_module) p_mod=$($s.p_module) erw_mod=$($s.erw_module) module=$($s.source_module)"
} else { '(no timer sample)' }

$gateAnnot = Join-Path $outE31 'gamelist_cfg_gate_annotated.txt'
@"
# gamelist cfg-open predecessor graph (E10A-3.1)
run_id=$RunId overlay=$OverlayRoot
Success order: REBOUND > COHERENT > MIXED_MULTI > chunk/P/ERW mismatch > NOT_REGISTERED > INSUFFICIENT
Safe ERW heal only when helper_owner==chunk_owner==p_owner and erw differs.
"@ | Set-Content -Path $gateAnnot -Encoding utf8

$verdictList = ($an.verdicts | ForEach-Object { "- ``$_``" }) -join "`n"
$md = @"
# Stage E10A-3.1 Gamelist Context Verdict

- **Mode**: ``$Mode``
- **run_id**: ``$RunId``
- **overlay**: ``$OverlayRoot``
- **Requested seconds**: $Seconds
- **Elapsed**: ${elapsed}s
- **Process exited**: $hasExited (code=$exitCode)
- **Killed by runner**: $killedByRunner
- **Observe stop**: ``$observeStop``
- **Stop reason**: ``$($exitInfo.stop_reason)``
- **Exit verdict**: ``$($exitInfo.exit_verdict)``
- **Primary**: ``$($an.primary)``
- **timer_evidence**: $($an.flags.timer_evidence) (binding_ok=$($an.flags.binding_ok) owners_ok=$($an.flags.owners_ok))
- **env**: ``E10A31_RUN_ENVIRONMENT_DETERMINISTIC`` (full JJFB_/GWY_/VMRP_ wipe + unique overlay)

## Timer sample
``````
$sampleTxt
``````

## Process exit
| Field | Value |
|------|-------|
| last_phase | $($exitInfo.last_phase) |
| last_module | $($exitInfo.last_module) |
| last_pc | $($exitInfo.last_pc) |
| last_helper | $($exitInfo.last_helper) |
| last_api | $($exitInfo.last_api) |
| stop_reason | $($exitInfo.stop_reason) |

## Flags
| Flag | Value |
|------|-------|
| ext_first_pc | $($an.flags.ext_first_pc) |
| post_cont | $($an.flags.post_cont) |
| continue_apply | $($an.flags.continue_apply) |
| timer_arm_csv | $($an.flags.timer_arm_csv) |
| timer_fire_csv | $($an.flags.timer_fire_csv) |
| timer_evidence | $($an.flags.timer_evidence) |
| timer_rebound | $($an.flags.timer_rebound) |
| timer_coherent | $($an.flags.timer_coherent) |
| timer_mixed_multi | $($an.flags.timer_mixed_multi) |
| erw_unpublished | $($an.flags.erw_unpublished) |
| chunk_retarget / foreign_erw | $($an.flags.chunk_retarget) / $($an.flags.foreign_erw) |
| chunk_mm / p_mm / erw_mm | $($an.flags.chunk_mm) / $($an.flags.p_mm) / $($an.flags.erw_mm) |

## CSV counts (run_id filtered)
timer=$($an.counts.timer) arm=$($an.counts.arm) fire=$($an.counts.fire) fire_ret=$($an.counts.fire_ret) param=$($an.counts.param) start_dsm=$($an.counts.start) cfg_gate=$($an.counts.gate)

## Verdicts
$verdictList

## Artifacts
| Kind | Path |
|------|------|
| env manifest | ``reports/e10a31_environment_manifest.csv`` |
| runtime manifest | ``reports/e10a31_runtime_manifest.csv`` |
| process exit | ``reports/e10a31_process_exit_trace.csv`` |
| timer binding | ``reports/e10a31_timer_binding_trace.csv`` |
| log | ``logs/e10a31_gamelist_context_stdout.txt`` |
"@
[System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

Write-Host "E10A-3.1 done mode=$Mode primary=$($an.primary) exit=$($exitInfo.exit_verdict) observe=$observeStop elapsed=$elapsed (requested=$Seconds)"
Write-Host "timer sample: $sampleTxt"
Write-Host "verdict: $verdictMd"
