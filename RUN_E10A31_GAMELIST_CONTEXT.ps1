# Stage E10A-3.1: Gamelist package-context and launch-param handoff
# Modes: timer_context | param_trace | cfg_gate | root_compare | validate
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('timer_context','param_trace','cfg_gate','root_compare','validate')]
  [string]$Mode = 'timer_context',
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
$stdoutLog = Join-Path $logDir 'e10a31_gamelist_context_stdout.txt'
$stderrLog = Join-Path $logDir 'e10a31_gamelist_context_stderr.txt'

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

function Clear-E10A31Env {
  @(
    'JJFB_E10A31_MODE','JJFB_E10A31_TIMER_CONTEXT','JJFB_E10A31_PARAM_TRACE',
    'JJFB_E10A31_CFG_GATE','JJFB_E10A31_START_DSM_ABI','JJFB_E10A31_WAIT_FOR_TIMER',
    'JJFB_E10A31_WAIT_MS','JJFB_E10A3_MODE','JJFB_E10A3_POSTSELECT',
    'GWY_SHELL_OFFLINE_NO_UPDATE','JJFB_DEBUG_AC8_FORCE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Reset-E10A31Artifacts {
  @(
    'reports\e10a31_timer_binding_trace.csv',
    'reports\e10a31_launch_param_read_trace.csv',
    'reports\e10a31_start_dsm_param_abi.csv',
    'reports\e10a31_cfg_gate_predicates.csv',
    'reports\e10a31_root_vs_continue_gamelist_context.csv',
    'reports\e10a31_process_exit_trace.csv',
    'logs\e10a31_gamelist_context_stdout.txt',
    'logs\e10a31_gamelist_context_stderr.txt'
  ) | ForEach-Object {
    $p = Join-Path $Root $_
    if (Test-Path $p) { Clear-Content -Path $p -ErrorAction SilentlyContinue }
  }
}

function Set-E10A31ShellEnv {
  $env:JJFB_E10A_RUN_ID = "$RunId"
  $env:JJFB_E10A31_RUN_ID = "$RunId"
  $env:JJFB_E10A_MODE = '1'
  $env:JJFB_E10A_SHELL_TRACE = '1'
  $env:JJFB_E10A3_POSTSELECT = '1'
  $env:JJFB_E10A3_WAITSTATE = '1'
  $env:JJFB_E9Y_MODE = '1'
  $env:JJFB_E9Y_NO_DEBUG_AC8 = '1'
  $env:JJFB_E9Y_NO_WORKBUF_SEED = '1'
  $env:JJFB_PLATFORM_WORKBUF_ALLOC = '1'
  $env:JJFB_GWY_PACK_REGISTRY = '1'
  $env:JJFB_E9W_MODE = '1'
  $env:JJFB_E9W_ARCHIVE_EXACT = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
  $env:JJFB_WINDOW_ZOOM = "$Zoom"
  $env:JJFB_E9B_HOLD_SEC = "$HoldSec"
  $env:JJFB_REAL_MRP_PATH = $mrpPath
  $env:JJFB_FAST_BD0_INIT_CALL = '1'
  $env:JJFB_FAST_PROGRESS_TICK_CALL = '1'
  $env:JJFB_E9U_TICK_N = "$TickN"
  $env:JJFB_TIMER_DELIVER_TRACE = '1'
  $env:JJFB_TIMER_ARM_TRACE = '1'
  $env:JJFB_E10A31_TIMER_CSV = (Join-Path $reportDir 'e10a31_timer_binding_trace.csv')
  $env:JJFB_E10A31_PARAM_CSV = (Join-Path $reportDir 'e10a31_launch_param_read_trace.csv')
  $env:JJFB_E10A31_START_DSM_CSV = (Join-Path $reportDir 'e10a31_start_dsm_param_abi.csv')
  $env:JJFB_E10A31_CFG_GATE_CSV = (Join-Path $reportDir 'e10a31_cfg_gate_predicates.csv')
  $env:JJFB_E10A3_POSTSELECT_CSV = (Join-Path $reportDir 'e10a3_cfg36_postselect_trace.csv')
  $env:JJFB_E10A3_WAIT_CSV = (Join-Path $reportDir 'e10a3_gamelist_wait_state.csv')
  # -Seconds is runner outer kill; WAIT_MS tells POST_CONT_PUMP how long host may hold for timer.
  $env:JJFB_E10A31_WAIT_MS = "$([Math]::Max(5000, $Seconds * 1000))"
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = Join-Path $RunDir 'overlay'
  $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_PARAM = $param
  $env:GWY_PACKAGE_APPID = '400101'
  $env:GWY_PACKAGE_APPVER = '12'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_LAUNCH_PATH = 'gwy_shell_core_continue'
  $env:JJFB_LAUNCH_SOURCE = 'gwy_shell'
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
  $env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update_native_branch'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
  $env:JJFB_EXTCHUNK_PROVIDER = 'shell_core'
  $env:JJFB_ER_RW_BIND_RESTORE = 'shell_core'
  $env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'shell'
  $env:JJFB_PUBLICATION_AUDIT = '1'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  Remove-Item Env:JJFB_PRODUCT_DESCRIPTOR_DIRECT -EA SilentlyContinue
  Remove-Item Env:GWY_PLATFORM_RESOURCE_READY_EVENT -EA SilentlyContinue
  Remove-Item Env:JJFB_DEBUG_AC8_FORCE -EA SilentlyContinue

  switch ($Mode) {
    'timer_context' {
      $env:JJFB_E10A31_MODE = '1'
      $env:JJFB_E10A31_TIMER_CONTEXT = '1'
      $env:JJFB_E10A31_WAIT_FOR_TIMER = '1'
    }
    'param_trace'   { $env:JJFB_E10A31_MODE = '1'; $env:JJFB_E10A31_PARAM_TRACE = '1'; $env:JJFB_E10A31_START_DSM_ABI = '1' }
    'cfg_gate'      { $env:JJFB_E10A31_MODE = '1'; $env:JJFB_E10A31_CFG_GATE = '1' }
    'root_compare'  { $env:JJFB_E10A31_MODE = '1'; $env:JJFB_E10A31_TIMER_CONTEXT = '1'; $env:JJFB_E10A31_PARAM_TRACE = '1' }
    'validate'      {
      $env:JJFB_E10A31_MODE = '1'
      $env:JJFB_E10A31_TIMER_CONTEXT = '1'
      $env:JJFB_E10A31_PARAM_TRACE = '1'
      $env:JJFB_E10A31_CFG_GATE = '1'
      $env:JJFB_E10A31_START_DSM_ABI = '1'
      $env:JJFB_E10A31_WAIT_FOR_TIMER = '1'
    }
  }
}

function Get-CsvRows([string]$path) {
  if (-not (Test-Path $path)) { return @() }
  try {
    $rows = @(Import-Csv $path -ErrorAction SilentlyContinue)
    return $rows
  } catch { return @() }
}

function Analyze-E10A31([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $timerCsv = Join-Path $reportDir 'e10a31_timer_binding_trace.csv'
  $paramCsv = Join-Path $reportDir 'e10a31_launch_param_read_trace.csv'
  $startCsv = Join-Path $reportDir 'e10a31_start_dsm_param_abi.csv'
  $gateCsv = Join-Path $reportDir 'e10a31_cfg_gate_predicates.csv'
  $timerRows = Get-CsvRows $timerCsv
  $paramN = @(Get-CsvRows $paramCsv).Count
  $startN = @(Get-CsvRows $startCsv).Count
  $gateN = @(Get-CsvRows $gateCsv).Count
  $timerN = $timerRows.Count

  $armRows = @($timerRows | Where-Object { $_.event -eq 'TIMER_ARM' })
  $fireRows = @($timerRows | Where-Object { $_.event -eq 'TIMER_FIRE' -or $_.event -eq 'TIMER_FIRE_RET' })
  $classVals = @($timerRows | ForEach-Object { $_.classification }) -join '|'

  $fromCsv = @{
    rebound = ($classVals -match 'GAMELIST_TIMER_CONTEXT_REBOUND')
    coherent = ($classVals -match 'TIMER_CONTEXT_COHERENT') -and -not ($classVals -match 'GAMELIST_TIMER_CONTEXT_REBOUND')
    mixed_multi = ($classVals -match 'TIMER_CONTEXT_MIXED_MULTI_OWNER')
    mixed = ($classVals -match 'TIMER_CONTEXT_MIXED|TIMER_ERW_OWNER_MISMATCH|TIMER_CHUNK_OWNER_MISMATCH|TIMER_P_OWNER_MISMATCH')
  }

  $flags = @{
    ext_first_pc = $t -match 'GAMELIST_EXT_FIRST_PC|mark=GAMELIST_EXT_FIRST_PC'
    timer_arm_csv = ($armRows.Count -gt 0)
    timer_fire_csv = ($fireRows.Count -gt 0)
    timer_mixed = [bool]$fromCsv.mixed -or [bool]$fromCsv.mixed_multi
    timer_mixed_multi = [bool]$fromCsv.mixed_multi
    timer_coherent = [bool]$fromCsv.coherent
    timer_rebound = [bool]$fromCsv.rebound
    param_mem_read = $t -match 'JJFB_E10A31_PARAM_READ'
    param_reg_read = $t -match 'JJFB_PARAM_READ'
    start_dsm_abi = $t -match 'JJFB_E10A31_START_DSM'
    cfg_gate = $t -match 'GAMELIST_CFG_GATE_REACHED|JJFB_GAMELIST_CFG_GATE'
    cfg_open = $t -match 'GAMELIST_EXTERNAL_CFG_OPEN'
    cfg_bin = $t -match 'cfg\.bin'
    timer_fire_log = $t -match 'FIRE_EXT.*helper=0x'
    timer_evidence = ($armRows.Count -gt 0 -and $fireRows.Count -gt 0)
  }

  $verdicts = @()
  if ($flags.ext_first_pc) { $verdicts += 'GAMELIST_EXT_FIRST_PC' }

  if (-not $flags.timer_evidence) {
    $verdicts += 'E10A31_INSUFFICIENT_TIMER_EVIDENCE'
  } elseif ($flags.timer_rebound) {
    $verdicts += 'GAMELIST_TIMER_CONTEXT_REBOUND'
  } elseif ($flags.timer_coherent) {
    $verdicts += 'GAMELIST_TIMER_CONTEXT_COHERENT'
  } elseif ($flags.timer_mixed_multi) {
    $verdicts += 'TIMER_CONTEXT_MIXED_MULTI_OWNER'
    $verdicts += 'GAMELIST_PACKAGE_CONTEXT_MIXED'
  } elseif ($flags.timer_mixed) {
    $verdicts += 'GAMELIST_PACKAGE_CONTEXT_MIXED'
    $verdicts += 'LAUNCH_PARAM_HANDOFF_NOT_PROVEN'
  }

  if ($paramN -gt 0 -or $flags.param_mem_read) { $verdicts += 'START_DSM_PARAM_REACHED_GAMELIST' }
  elseif ($startN -gt 0) { $verdicts += 'START_DSM_PARAM_ABI_CONFIRMED' }
  elseif (-not $flags.param_reg_read) { $verdicts += 'SHELL_PARAM_NOT_CONSUMED' }
  if ($gateN -gt 0 -or $flags.cfg_gate) { $verdicts += 'GAMELIST_CFG_GATE_REACHED' }
  if ($flags.cfg_open) { $verdicts += 'GAMELIST_CFG_OPEN_REACHED' }
  if (-not $flags.cfg_bin) { $verdicts += 'NO_REAL_CFG_BIN_OPEN' }

  # Priority: rebound > coherent > mixed; never default mixed without CSV proof.
  if (-not $flags.timer_evidence -and -not $flags.timer_mixed -and -not $flags.timer_coherent -and -not $flags.timer_rebound) {
    $primary = 'E10A31_INSUFFICIENT_TIMER_EVIDENCE'
  } elseif ($flags.timer_rebound) {
    $primary = 'GAMELIST_TIMER_CONTEXT_REBOUND'
  } elseif ($flags.timer_coherent) {
    $primary = 'GAMELIST_TIMER_CONTEXT_COHERENT'
  } elseif ($flags.timer_mixed_multi) {
    $primary = 'TIMER_CONTEXT_MIXED_MULTI_OWNER'
  } elseif ($flags.timer_mixed) {
    $primary = 'GAMELIST_PACKAGE_CONTEXT_MIXED'
  } elseif ($paramN -gt 0) {
    $primary = 'START_DSM_PARAM_REACHED_GAMELIST'
  } elseif ($gateN -gt 0) {
    $primary = 'GAMELIST_CFG_GATE_REACHED'
  } else {
    $primary = 'E10A31_INSUFFICIENT_TIMER_EVIDENCE'
  }

  return [pscustomobject]@{
    primary = $primary
    verdicts = $verdicts
    flags = $flags
    counts = @{ timer=$timerN; arm=$armRows.Count; fire=$fireRows.Count; param=$paramN; start=$startN; gate=$gateN }
  }
}

function Write-E10A31ExitTrace(
  [string]$log,
  [double]$elapsed,
  [bool]$hasExited,
  [object]$exitCode,
  [bool]$killedByRunner,
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
  } elseif ($t -match 'api=(\S+)') {
    $ms = [regex]::Matches($t, 'api=(\S+)')
    if ($ms.Count -gt 0) { $lastApi = $ms[$ms.Count - 1].Groups[1].Value }
  }

  $stopReason = 'UNKNOWN'
  if ($killedByRunner) { $stopReason = 'RUNNER_KILL_TIMEOUT' }
  elseif ($t -match 'UC_ERR|mem_fault|MEM_FAULT|VM_FAULT') { $stopReason = 'VM_FAULT' }
  elseif ($t -match 'mythroad exit|gwy_ext_obs_mr_exit|GUEST_MR_EXIT') { $stopReason = 'GUEST_MR_EXIT' }
  elseif ($t -match 'JJFB_E10A_EXIT_PARK') { $stopReason = 'HOST_DIAGNOSTIC_EXIT' }
  elseif ($t -match 'VISIBLE_WINDOW_HOLD_DONE') { $stopReason = 'WINDOW_CLOSE' }
  elseif ($t -match 'POST_CONT_PUMP end' -and $t -match 'FIRE_EXT') { $stopReason = 'IDLE_LOOP_STOP' }
  elseif ($t -match 'shell_core_continue' -and $t -notmatch 'GAMELIST_EXT_FIRST_PC|FIRE_EXT') {
    $stopReason = 'NO_CONTINUATION'
  } elseif ($hasExited) { $stopReason = 'GUEST_NORMAL_EXIT' }

  $exitVerdict = 'E10A31_HOST_DIAGNOSTIC_EXIT_EARLY'
  if ($killedByRunner) { $exitVerdict = 'E10A31_HOST_TIMEOUT' }
  elseif ($an.flags.timer_evidence) { $exitVerdict = 'E10A31_REACHED_GAMELIST_TIMER' }
  elseif ($stopReason -eq 'VM_FAULT') { $exitVerdict = 'E10A31_VM_FAULT_BEFORE_GAMELIST' }
  elseif ($stopReason -match 'GUEST_' -and -not ($t -match 'GAMELIST_EXT_FIRST_PC')) {
    $exitVerdict = 'E10A31_GUEST_EXIT_BEFORE_GAMELIST'
  } elseif ($elapsed -lt ($Seconds * 0.5) -and -not $an.flags.timer_evidence) {
    $exitVerdict = 'E10A31_HOST_DIAGNOSTIC_EXIT_EARLY'
  } elseif ($an.flags.timer_evidence) {
    $exitVerdict = 'E10A31_REACHED_GAMELIST_TIMER'
  }

  $exitCsv = Join-Path $reportDir 'e10a31_process_exit_trace.csv'
  $hdr = 'run_id,mode,elapsed,has_exited,exit_code,killed_by_runner,last_phase,last_module,last_pc,last_lr,last_helper,last_api,stop_reason,note'
  $note = "exit_verdict=$exitVerdict;timer_evidence=$($an.flags.timer_evidence);requested_seconds=$Seconds"
  $ec = if ($null -eq $exitCode) { '' } else { "$exitCode" }
  $line = "$RunId,$Mode,$elapsed,$([int]$hasExited),$ec,$([int]$killedByRunner),`"$lastPhase`",`"$lastModule`",$lastPc,$lastLr,$lastHelper,`"$lastApi`",$stopReason,`"$note`""
  Set-Content -Path $exitCsv -Value @($hdr, $line) -Encoding utf8
  return [pscustomobject]@{
    stop_reason = $stopReason
    exit_verdict = $exitVerdict
    last_phase = $lastPhase
    last_module = $lastModule
    last_pc = $lastPc
    last_helper = $lastHelper
    last_api = $lastApi
  }
}

if (-not $SkipBuild) {
  Write-Host '=== E10A-3.1 build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

Clear-E10A31Env
Reset-E10A31Artifacts
Stop-E10A31Children
Set-E10A31ShellEnv
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

Write-Host "=== E10A-3.1 mode=$Mode run_id=$RunId requested_seconds=$Seconds outer_kill=${OUTER_KILL_SEC}s wait_ms=$($env:JJFB_E10A31_WAIT_MS) wait_for_timer=$($env:JJFB_E10A31_WAIT_FOR_TIMER) ==="
$t0 = Get-Date
$killedByRunner = $false
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
  $killedByRunner = $true
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E10A31Children
  Start-Sleep -Milliseconds 500
  try { if (-not $p.HasExited) { $p.Refresh() } } catch {}
}
$elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
try { $p.Refresh() } catch {}
$hasExited = [bool]$p.HasExited
$exitCode = if ($hasExited) { $p.ExitCode } else { $null }
$an = Analyze-E10A31 $stdoutLog
$exitInfo = Write-E10A31ExitTrace -log $stdoutLog -elapsed $elapsed -hasExited $hasExited `
  -exitCode $exitCode -killedByRunner $killedByRunner -an $an
if ($an.verdicts -notcontains $exitInfo.exit_verdict) {
  $an.verdicts = @($exitInfo.exit_verdict) + @($an.verdicts)
}

$gateAnnot = Join-Path $outE31 'gamelist_cfg_gate_annotated.txt'
@"
# gamelist cfg-open predecessor graph (E10A-3.1)

Candidates (gamelist.ext base + offset):
- cfg_open: base + 0x1AF8  (live ~0x2D5E4C)
- cfg_gate: base + 0x392C   (live ~0x2D7C80)
- cfg_wrap: base + 0xF670  (live ~0x2E39C4)
- cmd_disp: base + 0xF6C4  (live ~0x2E3A18)

Runtime observe:
- GAMELIST_EXT_FIRST_PC != init complete
- timer method=2 is helper code, not gamelist state id
- mixed timer tuple (gamelist helper + gbrwcore ERW) blocks cfg/param progression
- auto ERW heal only when helper/chunk/P owners agree

Success order:
1. GAMELIST_TIMER_CONTEXT_COHERENT or REBOUND
2. START_DSM_PARAM_REACHED_GAMELIST or ABI mismatch verdict
3. GAMELIST_CFG_GATE_REACHED / cfg.bin open
"@ | Set-Content -Path $gateAnnot -Encoding utf8

$verdictList = ($an.verdicts | ForEach-Object { "- ``$_``" }) -join "`n"
$md = @"
# Stage E10A-3.1 Gamelist Context Verdict

- **Mode**: ``$Mode``
- **run_id**: ``$RunId``
- **Requested seconds**: $Seconds
- **Elapsed**: ${elapsed}s
- **Process exited**: $hasExited (code=$exitCode)
- **Killed by runner**: $killedByRunner
- **Stop reason**: ``$($exitInfo.stop_reason)``
- **Exit verdict**: ``$($exitInfo.exit_verdict)``
- **Primary**: ``$($an.primary)``
- **timer_evidence**: $($an.flags.timer_evidence)

## Focus
Package-scoped timer P/ERW/R9, launch-param handoff, cfg-open gate — not post-cfg update yet.

## Process exit
| Field | Value |
|------|-------|
| last_phase | $($exitInfo.last_phase) |
| last_module | $($exitInfo.last_module) |
| last_pc | $($exitInfo.last_pc) |
| last_helper | $($exitInfo.last_helper) |
| last_api | $($exitInfo.last_api) |
| stop_reason | $($exitInfo.stop_reason) |

Note: ``-Seconds`` is outer kill deadline only; do not claim a ${Seconds}s run if elapsed=$elapsed.

## Flags
| Flag | Value |
|------|-------|
| ext_first_pc | $($an.flags.ext_first_pc) |
| timer_arm_csv | $($an.flags.timer_arm_csv) |
| timer_fire_csv | $($an.flags.timer_fire_csv) |
| timer_evidence | $($an.flags.timer_evidence) |
| timer_mixed | $($an.flags.timer_mixed) |
| timer_mixed_multi | $($an.flags.timer_mixed_multi) |
| timer_coherent | $($an.flags.timer_coherent) |
| timer_rebound | $($an.flags.timer_rebound) |
| param_mem_read | $($an.flags.param_mem_read) |
| param_reg_read | $($an.flags.param_reg_read) |
| start_dsm_abi | $($an.flags.start_dsm_abi) |
| cfg_gate | $($an.flags.cfg_gate) |
| cfg_open | $($an.flags.cfg_open) |
| cfg_bin | $($an.flags.cfg_bin) |

## CSV counts
timer=$($an.counts.timer) arm=$($an.counts.arm) fire=$($an.counts.fire) param=$($an.counts.param) start_dsm=$($an.counts.start) cfg_gate=$($an.counts.gate)

## Verdicts
$verdictList

## Artifacts
| Kind | Path |
|------|------|
| process exit | ``reports/e10a31_process_exit_trace.csv`` |
| timer binding | ``reports/e10a31_timer_binding_trace.csv`` |
| param read | ``reports/e10a31_launch_param_read_trace.csv`` |
| start_dsm ABI | ``reports/e10a31_start_dsm_param_abi.csv`` |
| cfg gate | ``reports/e10a31_cfg_gate_predicates.csv`` |
| cfg annotate | ``out/e10a31/gamelist_cfg_gate_annotated.txt`` |
| log | ``logs/e10a31_gamelist_context_stdout.txt`` |
"@
[System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

Write-Host "E10A-3.1 done mode=$Mode primary=$($an.primary) exit=$($exitInfo.exit_verdict) elapsed=$elapsed (requested=$Seconds)"
Write-Host "verdict: $verdictMd"
