# Stage E10A-3.1m: exact provenance of method0 failure at 0x2E5404
# Modes: predicate | smscfg_field | requirement_ab | validate
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('predicate','smscfg_field','requirement_ab','apply_once','validate')]
  [string]$Mode = 'predicate',
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
$outDir = Join-Path $Root 'out\e10a31m'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31m_fail_2e5404_verdict.md'

function Stop-E10A31MChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[C-M]_')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-AllInheritedCaseEnv {
  $names = @()
  Get-ChildItem Env: | ForEach-Object {
    if ($_.Name -match '^(JJFB_|GWY_|VMRP_)') { $names += $_.Name }
  }
  foreach ($n in $names) { Remove-Item -Path ("Env:{0}" -f $n) -EA SilentlyContinue }
  return $names
}

function Get-ArtifactPaths([string]$tag, [string]$runId) {
  return [ordered]@{
    OverlayRoot = Join-Path $RunDir ("overlay\e10a31m\{0}_{1}" -f $tag, $runId)
    stdoutLog = Join-Path $logDir ("e10a31m_{0}_stdout.txt" -f $tag)
    stderrLog = Join-Path $logDir ("e10a31m_{0}_stderr.txt" -f $tag)
    timerCsv = Join-Path $reportDir ("e10a31m_{0}_timer_binding_trace.csv" -f $tag)
    initCsv = Join-Path $reportDir ("e10a31m_{0}_init_sequence_trace.csv" -f $tag)
    histCsv = Join-Path $reportDir ("e10a31m_{0}_helper_call_history.csv" -f $tag)
    insnCsv = Join-Path $reportDir ("e10a31m_{0}_method0_instruction_trace.csv" -f $tag)
    callCsv = Join-Path $reportDir ("e10a31m_{0}_method0_call_tree.csv" -f $tag)
    provCsv = Join-Path $reportDir ("e10a31m_{0}_method0_return_provenance.csv" -f $tag)
    appinfoCsv = Join-Path $reportDir ("e10a31m_{0}_appinfo_contract.csv" -f $tag)
    metaCsv = Join-Path $reportDir ("e10a31m_{0}_package_metadata_trace.csv" -f $tag)
    bindCsv = Join-Path $reportDir ("e10a31m_{0}_appinfo_binding_trace.csv" -f $tag)
    branchCsvF = Join-Path $reportDir ("e10a31m_{0}_fail_branch_trace_f.csv" -f $tag)
    denseCsv = Join-Path $reportDir ("e10a31m_{0}_failsite_dense_trace.csv" -f $tag)
    gCsv = Join-Path $reportDir ("e10a31m_{0}_strcmp_arg_trace.csv" -f $tag)
    hCsv = Join-Path $reportDir ("e10a31m_{0}_smscfg_trace.csv" -f $tag)
    branchCsv = Join-Path $reportDir 'e10a31m_fail_branch_trace.csv'
    smscfgCsv = Join-Path $reportDir 'e10a31m_smscfg_355_356_provenance.csv'
    reqJson = Join-Path $reportDir 'e10a31m_requirement.json'
    nextCsv = Join-Path $reportDir 'e10a31m_next_subsystem_provenance.csv'
    abCsv = Join-Path $reportDir 'e10a31m_requirement_ab_compare.csv'
  }
}

function Get-BaseWhitelist([string]$runId, $arts) {
  New-Item -ItemType Directory -Force -Path $arts.OverlayRoot | Out-Null
  New-Item -ItemType Directory -Force -Path (Join-Path $arts.OverlayRoot 'system') | Out-Null
  return [ordered]@{
    JJFB_E10A_RUN_ID = "$runId"
    JJFB_E10A31_RUN_ID = "$runId"
    JJFB_E10A31C_RUN_ID = "$runId"
    JJFB_E10A31D_RUN_ID = "$runId"
    JJFB_E10A31E_RUN_ID = "$runId"
    JJFB_E10A31F_RUN_ID = "$runId"
    JJFB_E10A31G_RUN_ID = "$runId"
    JJFB_E10A31H_RUN_ID = "$runId"
    JJFB_E10A31K_RUN_ID = "$runId"
    JJFB_E10A31L_RUN_ID = "$runId"
    JJFB_E10A31M_RUN_ID = "$runId"
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
    JJFB_E10A31_TIMER_CSV = $arts.timerCsv
    JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
    GWY_RESOURCE_ROOT = $ResourceRoot
    GWY_OVERLAY_ROOT = $arts.OverlayRoot
    GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
    GWY_LAUNCH = '1'
    GWY_LAUNCH_PARAM = $param
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
    JJFB_E10A31C_INIT_CSV = $arts.initCsv
    JJFB_E10A31D_MODE = '1'
    JJFB_E10A31D_HISTORY = '1'
    JJFB_E10A31D_METHOD0_TRACE = '1'
    JJFB_E10A31D_APPINFO = '1'
    JJFB_E10A31D_INSN_BUDGET = '20000'
    JJFB_E10A31D_HIST_CSV = $arts.histCsv
    JJFB_E10A31D_INSN_CSV = $arts.insnCsv
    JJFB_E10A31D_CALL_CSV = $arts.callCsv
    JJFB_E10A31D_PROV_CSV = $arts.provCsv
    JJFB_E10A31D_APPINFO_CSV = $arts.appinfoCsv
    JJFB_E10A31E_MODE = '1'
    JJFB_E10A31E_METADATA = '1'
    JJFB_E10A31E_BINDING = '1'
    JJFB_E10A31E_META_CSV = $arts.metaCsv
    JJFB_E10A31E_BIND_CSV = $arts.bindCsv
    JJFB_E10A31F_MODE = '1'
    JJFB_E10A31F_CONTINUE_PAST_SENTINEL = '1'
    JJFB_E10A31F_DENSE_WINDOW = '1'
    JJFB_E10A31F_BRANCH_CSV = $arts.branchCsvF
    JJFB_E10A31F_DENSE_CSV = $arts.denseCsv
    JJFB_E10A31G_MODE = '1'
    JJFB_E10A31G_CSV = $arts.gCsv
    JJFB_E10A31H_MODE = '1'
    JJFB_E10A31H_CSV = $arts.hCsv
    GWY_SMSCFG_BOOTSTRAP = '1'
    GWY_DIAG_SMSCFG_GPT_MINIMAL = '0'
    JJFB_E10A31M_MODE = '1'
    JJFB_E10A31M_BRANCH_CSV = $arts.branchCsv
    JJFB_E10A31M_SMSCFG_CSV = $arts.smscfgCsv
    JJFB_E10A31M_REQ_JSON = $arts.reqJson
  }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  if (Test-LogHit $log '\[JJFB_E10A31M\] method0_return') {
    return 'OBSERVE_STOP_E10A31M_COMPLETE'
  }
  if (Test-LogHit $log 'GAMELIST_METHOD0_RETURN_ZERO|method0_return ret=0') {
    return 'OBSERVE_STOP_METHOD0_ZERO'
  }
  if (Test-LogHit $log 'MRC_INIT_RETURN_PROVENANCE_COMPLETE|method0_trace=disarm') {
    return 'OBSERVE_STOP_PROVENANCE_COMPLETE'
  }
  if (Test-LogHit $log 'UC_ERR|MEM_FAULT|FETCH_UNMAPPED') {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Invoke-CaseRun($wl, [string]$outLog, [string]$errLog) {
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  Stop-E10A31MChildren
  $t0 = Get-Date
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
  $deadline = $t0.AddSeconds($OUTER_KILL_SEC)
  $observeStop = $null
  while (-not $p.HasExited) {
    if ((Get-Date) -ge $deadline) { break }
    $obs = Get-ObserveStopReason $outLog
    if ($obs) { $observeStop = $obs; Write-Host "=== early stop: $obs ==="; break }
    Start-Sleep -Milliseconds 400
    try { $p.Refresh() } catch {}
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E10A31MChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Invoke-OneCase([string]$tag, [hashtable]$extraEnv) {
  $runId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $arts = Get-ArtifactPaths $tag $runId
  @($arts.stdoutLog, $arts.stderrLog) |
    ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue } }
  # Shared reports (branch/smscfg/req) are overwritten by the live run, not pre-deleted
  # across AB cases so requirement.json survives between predicate_pre and case_b.
  Clear-AllInheritedCaseEnv | Out-Null
  $wl = Get-BaseWhitelist $runId $arts
  foreach ($k in $extraEnv.Keys) { $wl[$k] = [string]$extraEnv[$k] }
  Write-Host "=== E10A31M case tag=$tag run_id=$runId ==="
  $r = Invoke-CaseRun $wl $arts.stdoutLog $arts.stderrLog
  return [pscustomobject]@{ tag = $tag; runId = $runId; arts = $arts; result = $r }
}

function Get-Method0Ret([string]$log) {
  if (-not (Test-Path $log)) { return '' }
  $m = Select-String -Path $log -Pattern 'method0_return ret=(-?\d+)' | Select-Object -Last 1
  if ($m) { return $m.Matches[0].Groups[1].Value }
  return ''
}

function Get-FailPc([string]$log) {
  if (-not (Test-Path $log)) { return '' }
  $m = Select-String -Path $log -Pattern 'TRUE_FAILURE_PC_FOUND.*?pc=0x([0-9A-Fa-f]+)' | Select-Object -Last 1
  if ($m) { return ('0x' + $m.Matches[0].Groups[1].Value) }
  $m2 = Select-String -Path $log -Pattern 'fail=RETURN_NEG1_IMMEDIATE|first_fail.*?0x([0-9A-Fa-f]+)' | Select-Object -Last 1
  if (Test-LogHit $log '0x2E5404') { return '0x2E5404' }
  return ''
}

function Write-Verdict([string]$primary, [string]$modeName, [string]$body) {
  $md = @"
# Stage E10A-3.1m fail 0x2E5404 provenance

- **Mode**: ``$modeName``
- **Primary verdict**: ``$primary``

$body

## Notes

- ``original_default_recovered=false``
- Do not brute-force 0x355/0x356; bounds come from immediates ``#0`` and ``0xFF+0xB3=0x1B2``.
- ``cfg_validate`` remains disabled until method0 returns 0.
"@
  Set-Content -Path $verdictMd -Value $md -Encoding UTF8
  Write-Host "wrote $verdictMd"
  Write-Host "PRIMARY=$primary"
}

# ---- main ----
Write-Host '=== E10A-3.1m Lane A static disasm ==='
python (Join-Path $Root 'tools\e10a31m_fail_2e5404_disasm.py') `
  --out-dir $outDir
if ($LASTEXITCODE -ne 0) { throw "static disasm failed: $LASTEXITCODE" }

if (-not $SkipBuild) {
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }
$exeHash = (Get-FileHash $exe -Algorithm SHA256).Hash
Write-Host "main.exe sha256=$exeHash"

Stop-E10A31MChildren

$primary = 'INCOMPLETE'
$body = ''

switch ($Mode) {
  { $_ -in @('predicate','smscfg_field') } {
    $c = Invoke-OneCase $Mode @{}
    $pred = Test-LogHit $c.arts.stdoutLog 'METHOD0_2E5404_FAIL_PREDICATE_FOUND'
    $epi = Test-LogHit $c.arts.stdoutLog 'METHOD0_2E5404_COMMON_FAILURE_EPILOGUE'
    $src = Test-LogHit $c.arts.stdoutLog 'METHOD0_FAIL_SOURCE_SMSCFG_355_356'
    $ftype = Test-LogHit $c.arts.stdoutLog 'SMSCFG_355_356_FIELD_TYPE_IDENTIFIED'
    if ($pred) { $primary = 'METHOD0_2E5404_FAIL_PREDICATE_FOUND' }
    elseif ($epi) { $primary = 'METHOD0_2E5404_COMMON_FAILURE_EPILOGUE' }
    else { $primary = 'PREDICATE_INCOMPLETE' }
    $body = @"
## Observe ($Mode)

- predecessor: ``METHOD0_2E5404_PREDECESSOR_IDENTIFIED`` (static)
- predicate_found: $pred
- common_epilogue: $epi
- sms_cfg_causal: $src
- field_type: $ftype
- method0_ret: $(Get-Method0Ret $c.arts.stdoutLog)
- elapsed: $($c.result.elapsed)s observeStop=$($c.result.observeStop)

## Artifacts

- ``out/e10a31m/fail_2e5404_annotated.txt``
- ``out/e10a31m/fail_2e5404_cfg.dot``
- ``reports/e10a31m_fail_branch_trace.csv``
- ``reports/e10a31m_smscfg_355_356_provenance.csv``
- ``reports/e10a31m_requirement.json``
- ``reports/e10a31m_next_subsystem_provenance.csv``
"@
  }
  'requirement_ab' {
    if (-not (Test-Path (Join-Path $reportDir 'e10a31m_requirement.json'))) {
      Write-Host 'requirement.json missing; running predicate first'
      $null = Invoke-OneCase 'predicate_pre' @{}
    }
    $a = Invoke-OneCase 'case_a' @{}
    $b = Invoke-OneCase 'case_b' @{
      JJFB_E10A31M_APPLY_INT16 = '0x355:1'
    }
    $retA = Get-Method0Ret $a.arts.stdoutLog
    $retB = Get-Method0Ret $b.arts.stdoutLog
    $failA = if (Test-LogHit $a.arts.stdoutLog '0x2E5404') { '0x2E5404' } else { 'other' }
    $failB = if (Test-LogHit $b.arts.stdoutLog '0x2E5404') { '0x2E5404' } else { 'moved_or_gone' }
    $zeroB = ($retB -eq '0') -or (Test-LogHit $b.arts.stdoutLog 'GAMELIST_METHOD0_RETURN_ZERO')
    $lines = @(
      'case,ret0,fail_pc,apply_int16,elapsed,note'
      ('A,{0},{1},none,{2},current_GPT_gwy_profile' -f $retA, $failA, $a.result.elapsed)
      ('B,{0},{1},0x355:1,{2},plus_proven_int16_min' -f $retB, $failB, $b.result.elapsed)
    )
    Set-Content -Path (Join-Path $reportDir 'e10a31m_requirement_ab_compare.csv') -Value $lines -Encoding UTF8
    if ($zeroB) {
      $primary = 'GAMELIST_METHOD0_RETURN_ZERO'
    } elseif ($failB -ne '0x2E5404' -and $retB -ne '') {
      $primary = 'METHOD0_NEXT_PRECONDITION_FOUND'
    } elseif ($failB -eq '0x2E5404') {
      $primary = 'REQUIREMENT_VALUE_OR_PUBLICATION_WRONG'
    } else {
      $primary = 'REQUIREMENT_AB_INCOMPLETE'
    }
    $body = @"
## requirement_ab

- Case A: GPT+gwy only, ret=$retA fail=$failA
- Case B: + int16_le@0x355=1 (minimal within proven 1..0x1B2), ret=$retB fail=$failB
- Compare: ``reports/e10a31m_requirement_ab_compare.csv``
- Profile not permanently extended unless Case B proves causality.
"@
  }
  'apply_once' {
    $c = Invoke-OneCase 'apply_once' @{
      JJFB_E10A31M_APPLY_INT16 = '0x355:1'
    }
    $ret = Get-Method0Ret $c.arts.stdoutLog
    $crash = Test-LogHit $c.arts.stdoutLog 'MEM_GET_HOST_CRASH|ACCESS_VIOLATION'
    $fail5404 = Test-LogHit $c.arts.stdoutLog 'pc=0x2E5404_RETURN_NEG1|FAIL_PREDICATE_FOUND|fail=1'
    $predPass = (Test-LogHit $c.arts.stdoutLog 'copy355=1') -and (-not $fail5404)
    # Also detect from E10A31M return line
    $mLine = ''
    if (Test-Path $c.arts.stdoutLog) {
      $m = Select-String -Path $c.arts.stdoutLog -Pattern '\[JJFB_E10A31M\] method0_return.*' | Select-Object -Last 1
      if ($m) { $mLine = $m.Line }
    }
    if ($ret -eq '0') { $primary = 'GAMELIST_METHOD0_RETURN_ZERO' }
    elseif ($mLine -match 'fail=0' -and $mLine -match 'pred_a=1') { $primary = 'METHOD0_NEXT_PRECONDITION_FOUND' }
    elseif ($fail5404) { $primary = 'REQUIREMENT_VALUE_OR_PUBLICATION_WRONG' }
    elseif ($crash) { $primary = 'APPLY_HOST_CRASH_BEFORE_METHOD0' }
    else { $primary = 'APPLY_ONCE_INCOMPLETE' }
    $body = @"
## apply_once

- APPLY int16_le@0x355=1
- method0_ret: $ret
- crash: $crash
- fail_2e5404: $fail5404
- e10a31m_line: $mLine
- elapsed: $($c.result.elapsed)s observeStop=$($c.result.observeStop)
"@
  }
  'validate' {
    $c = Invoke-OneCase 'validate' @{
      JJFB_E10A31M_APPLY_INT16 = '0x355:1'
      JJFB_E10A31_MODE = '1'
      JJFB_E10A31_CFG_GATE = '1'
    }
    if (Test-LogHit $c.arts.stdoutLog 'ret0=0|GAMELIST_METHOD0_RETURN_ZERO|method0_return ret=0') {
      $primary = 'GAMELIST_METHOD0_RETURN_ZERO'
    } elseif (Test-LogHit $c.arts.stdoutLog 'GAMELIST_CFG_GATE_REACHED') {
      $primary = 'GAMELIST_CFG_GATE_REACHED'
    } else {
      $primary = 'METHOD0_STILL_NONZERO'
    }
    $body = @"
## validate

- elapsed=$($c.result.elapsed)s
- apply int16@0x355=1 for gate pass attempt only
- cfg gate only meaningful after method0==0
"@
  }
}

Write-Verdict $primary $Mode $body
Write-Host "exe_sha256=$exeHash"
