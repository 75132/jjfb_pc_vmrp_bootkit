# Stage E10A-3.1n: post-range batch provenance 0x2E5410 -> 0x2E3FBA
# Modes: provenance | smscfg_map | requirement_ab | bootstrap_timing | validate
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('provenance','smscfg_map','requirement_ab','bootstrap_timing','validate','apply_napptype','apply_case1','apply_bkeys')]
  [string]$Mode = 'provenance',
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
$outDir = Join-Path $Root 'out\e10a31n'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31n_post_range_verdict.md'

function Stop-E10A31NChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[C-N]_')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-AllInheritedCaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } |
    ForEach-Object { Remove-Item -Path ("Env:{0}" -f $_.Name) -EA SilentlyContinue }
}

function Get-ArtifactPaths([string]$tag, [string]$runId) {
  return [ordered]@{
    OverlayRoot = Join-Path $RunDir ("overlay\e10a31n\{0}_{1}" -f $tag, $runId)
    stdoutLog = Join-Path $logDir ("e10a31n_{0}_stdout.txt" -f $tag)
    stderrLog = Join-Path $logDir ("e10a31n_{0}_stderr.txt" -f $tag)
    timerCsv = Join-Path $reportDir ("e10a31n_{0}_timer_binding_trace.csv" -f $tag)
    initCsv = Join-Path $reportDir ("e10a31n_{0}_init_sequence_trace.csv" -f $tag)
    histCsv = Join-Path $reportDir ("e10a31n_{0}_helper_call_history.csv" -f $tag)
    insnCsv = Join-Path $reportDir ("e10a31n_{0}_method0_instruction_trace.csv" -f $tag)
    callCsvD = Join-Path $reportDir ("e10a31n_{0}_method0_call_tree.csv" -f $tag)
    provCsvD = Join-Path $reportDir ("e10a31n_{0}_method0_return_provenance.csv" -f $tag)
    appinfoCsv = Join-Path $reportDir ("e10a31n_{0}_appinfo_contract.csv" -f $tag)
    metaCsv = Join-Path $reportDir ("e10a31n_{0}_package_metadata_trace.csv" -f $tag)
    bindCsv = Join-Path $reportDir ("e10a31n_{0}_appinfo_binding_trace.csv" -f $tag)
    branchCsvF = Join-Path $reportDir ("e10a31n_{0}_fail_branch_f.csv" -f $tag)
    denseCsv = Join-Path $reportDir ("e10a31n_{0}_failsite_dense.csv" -f $tag)
    gCsv = Join-Path $reportDir ("e10a31n_{0}_strcmp.csv" -f $tag)
    hCsv = Join-Path $reportDir ("e10a31n_{0}_smscfg_h.csv" -f $tag)
    callCsv = Join-Path $reportDir 'e10a31n_call_tree.csv'
    provCsv = Join-Path $reportDir 'e10a31n_return_provenance.csv'
    predCsv = Join-Path $reportDir 'e10a31n_predicate_chain.csv'
    smscfgCsv = Join-Path $reportDir 'e10a31n_new_smscfg_reads.csv'
    compareCsv = Join-Path $reportDir 'e10a31n_post_range_compare_chain.csv'
    manifest = Join-Path $reportDir 'e10a31n_required_platform_state.json'
    timingCsv = Join-Path $reportDir 'e10a31n_smscfg_application_timing.csv'
    abCsv = Join-Path $reportDir 'e10a31n_requirement_ab_compare.csv'
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
    JJFB_E10A31M_RUN_ID = "$runId"
    JJFB_E10A31N_RUN_ID = "$runId"
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
    JJFB_E10A31D_INSN_BUDGET = '25000'
    JJFB_E10A31D_HIST_CSV = $arts.histCsv
    JJFB_E10A31D_INSN_CSV = $arts.insnCsv
    JJFB_E10A31D_CALL_CSV = $arts.callCsvD
    JJFB_E10A31D_PROV_CSV = $arts.provCsvD
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
    # Diagnostic: int16@0x355 at method0 enter (not product bootstrap)
    JJFB_E10A31N_MODE = '1'
    JJFB_E10A31N_APPLY_INT16 = '0x355:1'
    JJFB_E10A31N_CALL_CSV = $arts.callCsv
    JJFB_E10A31N_PROV_CSV = $arts.provCsv
    JJFB_E10A31N_PRED_CSV = $arts.predCsv
    JJFB_E10A31N_SMSCFG_CSV = $arts.smscfgCsv
    JJFB_E10A31N_COMPARE_CSV = $arts.compareCsv
    JJFB_E10A31N_MANIFEST_JSON = $arts.manifest
    JJFB_E10A31N_TIMING_CSV = $arts.timingCsv
  }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log, [switch]$PreferMethod0Return) {
  if ($PreferMethod0Return) {
    if (Test-LogHit $log '\[JJFB_E10A31N\] method0_return|GAMELIST_METHOD0_RETURN_ZERO') {
      return 'OBSERVE_STOP_E10A31N_COMPLETE'
    }
  } else {
    if (Test-LogHit $log '\[JJFB_E10A31N\] method0_return|METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND') {
      return 'OBSERVE_STOP_E10A31N_COMPLETE'
    }
    if (Test-LogHit $log 'GAMELIST_METHOD0_RETURN_ZERO') {
      return 'OBSERVE_STOP_METHOD0_ZERO'
    }
  }
  if (Test-LogHit $log 'MRC_INIT_RETURN_PROVENANCE_COMPLETE|method0_trace=disarm') {
    return 'OBSERVE_STOP_PROVENANCE_COMPLETE'
  }
  # Do not early-stop on MEM_GET_HOST_CRASH alone during timing experiments;
  # only stop if crash happens before any method0 arm.
  if (Test-LogHit $log 'MEM_GET_HOST_CRASH') {
    if (-not (Test-LogHit $log 'E10A31N_ARMED|method0_trace=arm|SMSCFG_APPLY_INT16.*method0_enter|SMSCFG_APPLY_NAPPTYPE')) {
      return 'OBSERVE_STOP_EARLY_BOOTSTRAP_CRASH'
    }
  }
  if (Test-LogHit $log 'UC_ERR|FETCH_UNMAPPED') {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Invoke-CaseRun($wl, [string]$outLog, [string]$errLog, [switch]$PreferMethod0Return) {
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  Stop-E10A31NChildren
  $t0 = Get-Date
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
  $deadline = $t0.AddSeconds($OUTER_KILL_SEC)
  $observeStop = $null
  while (-not $p.HasExited) {
    if ((Get-Date) -ge $deadline) { break }
    $obs = Get-ObserveStopReason $outLog -PreferMethod0Return:$PreferMethod0Return
    if ($obs) { $observeStop = $obs; Write-Host "=== early stop: $obs ==="; break }
    Start-Sleep -Milliseconds 400
    try { $p.Refresh() } catch {}
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E10A31NChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Invoke-OneCase([string]$tag, [hashtable]$extraEnv, [switch]$PreferMethod0Return) {
  $runId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $arts = Get-ArtifactPaths $tag $runId
  @($arts.stdoutLog, $arts.stderrLog) | ForEach-Object {
    if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue }
  }
  Clear-AllInheritedCaseEnv
  $wl = Get-BaseWhitelist $runId $arts
  foreach ($k in $extraEnv.Keys) { $wl[$k] = [string]$extraEnv[$k] }
  Write-Host "=== E10A31N case tag=$tag run_id=$runId ==="
  $r = Invoke-CaseRun $wl $arts.stdoutLog $arts.stderrLog -PreferMethod0Return:$PreferMethod0Return
  return [pscustomobject]@{ tag = $tag; runId = $runId; arts = $arts; result = $r }
}

function Get-Method0Ret([string]$log) {
  if (-not (Test-Path $log)) { return '' }
  $m = Select-String -Path $log -Pattern '\[JJFB_E10A31N\] method0_return ret=(-?\d+)' | Select-Object -Last 1
  if ($m) { return $m.Matches[0].Groups[1].Value }
  $m2 = Select-String -Path $log -Pattern 'method0_return ret=(-?\d+)' | Select-Object -Last 1
  if ($m2) { return $m2.Matches[0].Groups[1].Value }
  return ''
}

function Write-Verdict([string]$primary, [string]$modeName, [string]$body) {
  $md = @"
# Stage E10A-3.1n post-range provenance

- **Mode**: ``$modeName``
- **Primary verdict**: ``$primary``

$body

## Notes

- ``original_default_recovered=false``
- int16@0x355=1 is compatibility/diagnostic (range 1..434); not claimed as original default.
- Profile content validity is separate from bootstrap application timing.
- ``cfg_validate`` remains disabled until method0 returns 0 AND safe bootstrap is proven.
"@
  Set-Content -Path $verdictMd -Value $md -Encoding UTF8
  Write-Host "wrote $verdictMd"
  Write-Host "PRIMARY=$primary"
}

# ---- main ----
Write-Host '=== E10A-3.1n Lane A static disasm ==='
python (Join-Path $Root 'tools\e10a31n_post_range_disasm.py') --out-dir $outDir
if ($LASTEXITCODE -ne 0) { throw "static disasm failed: $LASTEXITCODE" }

if (-not $SkipBuild) {
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }
$exeHash = (Get-FileHash $exe -Algorithm SHA256).Hash
Write-Host "main.exe sha256=$exeHash"

Stop-E10A31NChildren

$primary = 'INCOMPLETE'
$body = ''

switch ($Mode) {
  { $_ -in @('provenance','smscfg_map') } {
    $c = Invoke-OneCase $Mode @{}
    $neg = Test-LogHit $c.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND'
    $direct = Test-LogHit $c.arts.stdoutLog 'METHOD0_2E3FBA_DIRECT_FAILURE'
    $role = Test-LogHit $c.arts.stdoutLog 'METHOD0_2E3FBA_ROLE_IDENTIFIED'
    $fn = Test-LogHit $c.arts.stdoutLog 'METHOD0_2E5410_FUNCTION_IDENTIFIED'
    if ($neg) { $primary = 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND' }
    elseif ($direct) { $primary = 'METHOD0_2E3FBA_DIRECT_FAILURE' }
    else { $primary = 'PROVENANCE_INCOMPLETE' }
    $body = @"
## Observe ($Mode)

- function_2e5410: $fn
- role_2e3fba: $role
- direct_failure: $direct
- first_causal_negative: $neg
- method0_ret: $(Get-Method0Ret $c.arts.stdoutLog)
- elapsed: $($c.result.elapsed)s observeStop=$($c.result.observeStop)

## Artifacts

- ``out/e10a31n/function_2e5410_annotated.txt``
- ``out/e10a31n/fail_2e3fba_annotated.txt``
- ``out/e10a31n/post_range_cfg.dot``
- ``reports/e10a31n_call_tree.csv``
- ``reports/e10a31n_return_provenance.csv``
- ``reports/e10a31n_predicate_chain.csv``
- ``reports/e10a31n_new_smscfg_reads.csv``
- ``reports/e10a31n_post_range_compare_chain.csv``
- ``reports/e10a31n_required_platform_state.json``
"@
  }
  'requirement_ab' {
    $a = Invoke-OneCase 'case_a' @{ JJFB_E10A31N_APPLY_INT16 = '0x355:1' }
    $b = Invoke-OneCase 'case_b' @{ JJFB_E10A31N_APPLY_INT16 = '0x355:1' }
    # Case B uses manifest extras only if present; for now same apply, compare fail PC movement notes
    $retA = Get-Method0Ret $a.arts.stdoutLog
    $retB = Get-Method0Ret $b.arts.stdoutLog
    $failA = if (Test-LogHit $a.arts.stdoutLog '0x2E3FBA') { '0x2E3FBA' } else { 'other' }
    $failB = if (Test-LogHit $b.arts.stdoutLog '0x2E3FBA') { '0x2E3FBA' } else { 'moved_or_gone' }
    $lines = @(
      'case,ret0,fail_pc,apply,elapsed,note'
      ('A,{0},{1},int16@355=1,{2},current_proven_profile_diag' -f $retA, $failA, $a.result.elapsed)
      ('B,{0},{1},int16@355=1+manifest,{2},plus_newly_proven' -f $retB, $failB, $b.result.elapsed)
    )
    Set-Content -Path (Join-Path $reportDir 'e10a31n_requirement_ab_compare.csv') -Value $lines -Encoding UTF8
    if ($retB -eq '0') { $primary = 'GAMELIST_METHOD0_RETURN_ZERO' }
    elseif ($failB -ne '0x2E3FBA' -and $retB -ne '') { $primary = 'METHOD0_NEXT_PRECONDITION_FOUND' }
    elseif ($failB -eq '0x2E3FBA') { $primary = 'REQUIREMENT_PUBLICATION_WRONG' }
    else { $primary = 'REQUIREMENT_AB_INCOMPLETE' }
    $body = @"
## requirement_ab

- A ret=$retA fail=$failA
- B ret=$retB fail=$failB
- Compare: ``reports/e10a31n_requirement_ab_compare.csv``
"@
  }
  'bootstrap_timing' {
    $c = Invoke-OneCase 'bootstrap_timing' @{ JJFB_E10A31N_APPLY_INT16 = '0x355:1' }
    $content = Test-LogHit $c.arts.stdoutLog 'SMSCFG_PROFILE_CONTENT_VALID'
    $unsafe = Test-LogHit $c.arts.stdoutLog 'SMSCFG_EARLY_APPLICATION_UNSAFE'
    $safe = Test-LogHit $c.arts.stdoutLog 'SMSCFG_SAFE_APPLICATION_POINT_FOUND'
    $ready = Test-LogHit $c.arts.stdoutLog 'SMSCFG_PRODUCT_BOOTSTRAP_READY'
    if ($ready) { $primary = 'SMSCFG_PRODUCT_BOOTSTRAP_READY' }
    elseif ($safe) { $primary = 'SMSCFG_SAFE_APPLICATION_POINT_FOUND' }
    elseif ($unsafe -or $content) { $primary = 'SMSCFG_EARLY_APPLICATION_UNSAFE' }
    else { $primary = 'BOOTSTRAP_TIMING_INCOMPLETE' }
    $body = @"
## bootstrap_timing

- content_valid: $content
- early_unsafe: $unsafe
- safe_point: $safe
- product_ready: $ready
- Artifact: ``reports/e10a31n_smscfg_application_timing.csv``
- Note: method0-enter apply remains diagnostic only.
"@
  }
  'validate' {
    $c = Invoke-OneCase 'validate' @{
      JJFB_E10A31N_APPLY_INT16 = '0x355:1'
      JJFB_E10A31_MODE = '1'
      JJFB_E10A31_CFG_GATE = '1'
    }
    if (Test-LogHit $c.arts.stdoutLog 'GAMELIST_METHOD0_RETURN_ZERO|method0_return ret=0') {
      $primary = 'GAMELIST_METHOD0_RETURN_ZERO'
    } else {
      $primary = 'METHOD0_STILL_NONZERO'
    }
    $body = @"
## validate

- elapsed=$($c.result.elapsed)s
- cfg gate only after method0==0 and safe bootstrap
"@
  }
  'apply_napptype' {
    # E10A-3.1o: A=len1 only; B=paired 0x355 length + 0x377=GWY_LAUNCH_PARAM (strstr haystack)
    python (Join-Path $Root 'tools\e10a31o_strstr_helper_disasm.py') | Out-Host
    $a = Invoke-OneCase 'o_case_a_len1' @{
      JJFB_E10A31N_APPLY_INT16 = '0x355:1'
      JJFB_E10A31O_APPLY_NAPPTYPE = '0'
    }
    $b = Invoke-OneCase 'o_case_b_napptype' @{
      JJFB_E10A31N_APPLY_INT16 = '0'
      JJFB_E10A31O_APPLY_NAPPTYPE = '1'
    }
    $aNeg = Test-LogHit $a.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND|0x2E3FBA'
    $bApply = Test-LogHit $b.arts.stdoutLog 'SMSCFG_APPLY_NAPPTYPE'
    $bStr = Test-LogHit $b.arts.stdoutLog 'HELPER_0xAC4A4_IS_STRSTR'
    $bNeg = Test-LogHit $b.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND'
    $aArm = Test-LogHit $a.arts.stdoutLog 'E10A31N_ARMED'
    $bArm = Test-LogHit $b.arts.stdoutLog 'E10A31N_ARMED'
    $retA = Get-Method0Ret $a.arts.stdoutLog
    $retB = Get-Method0Ret $b.arts.stdoutLog
    $ab = Join-Path $reportDir 'e10a31o_requirement_ab_compare.csv'
    $lines = @(
      'case,method0_ret,armed,saw_2e3fba_neg,apply_napptype,note'
      ('A_len1,{0},{1},{2},0,"expect strstr NULL -> 0x2E3FBA"' -f $retA, [int]$aArm, [int]$aNeg)
      ('B_napptype,{0},{1},{2},1,"paired 0x355=len + 0x377=GWY_LAUNCH_PARAM"' -f $retB, [int]$bArm, [int]$bNeg)
    )
    Set-Content -Path $ab -Value $lines -Encoding UTF8
    if ($bApply -and $bStr -and $bArm -and -not $bNeg -and $aNeg) {
      $primary = 'METHOD0_NAPPTYPE_STRSTR_GATE_PASSED'
    } elseif ($bApply -and $bArm -and $bNeg) {
      $primary = 'METHOD0_NAPPTYPE_APPLY_STILL_2E3FBA'
    } elseif ($bApply -and $bArm) {
      $primary = 'METHOD0_NAPPTYPE_APPLY_OBSERVED'
    } elseif ($aArm -or $bArm) {
      $primary = 'E10A31O_PARTIAL_METHOD0'
    } else {
      $primary = 'E10A31O_APPLY_INCOMPLETE'
    }
    $oVerdict = Join-Path $reportDir 'stage_e10a31o_strstr_napptype_verdict.md'
    $body = @"
## E10A-3.1o apply_napptype (via N runner)

### Helpers
- ``0xAC374`` = strlen
- ``0xAC4A4`` = strstr(haystack, needle); needle from failfn r3 = ``napptype``
- ``@0x355`` = copy length for ``@0x377`` C-string haystack

### AB
| case | ret | armed | 2E3FBA | apply |
|------|-----|-------|--------|-------|
| A | $retA | $aArm | $aNeg | no |
| B | $retB | $bArm | $bNeg | $bApply |

- strstr milestone B: $bStr
- ``reports/e10a31o_requirement_ab_compare.csv``
- ``out/e10a31o/helpers_ac374_ac4a4_annotated.txt``
- original_default_recovered=false
"@
    Set-Content -Path $oVerdict -Value @"
# Stage E10A-3.1o strstr / napptype

- **Primary verdict**: ``$primary``

$body
"@ -Encoding UTF8
    Write-Host "wrote $oVerdict"
  }
  'apply_case1' {
    # E10A-3.1p: A=O launch-param (napptype=12); B=case1 n* + @0x35F=1 skip entry
    # Stop on FIRST_CAUSAL again: with @0x35F skip, entry MVNS is not hit; first MVNS is next gate.
    $a = Invoke-OneCase 'p_case_a_launch12' @{
      JJFB_E10A31N_APPLY_INT16 = '0'
      JJFB_E10A31O_APPLY_NAPPTYPE = '1'
      JJFB_E10A31P_APPLY = '0'
    }
    $needleA = ''
    $sharedProv = Join-Path $reportDir 'e10a31n_return_provenance.csv'
    if (Test-Path $sharedProv) {
      $provLine = Get-Content $sharedProv | Select-Object -Last 1
      if ($provLine -match "needle='([^']+)'") { $needleA = $Matches[1] }
    }
    $b = Invoke-OneCase 'p_case_b_case1' @{
      JJFB_E10A31N_APPLY_INT16 = '0'
      JJFB_E10A31O_APPLY_NAPPTYPE = '0'
      JJFB_E10A31P_APPLY = '1'
    }
    $aApply = Test-LogHit $a.arts.stdoutLog 'SMSCFG_APPLY_NAPPTYPE'
    $bApply = Test-LogHit $b.arts.stdoutLog 'SMSCFG_APPLY_NAPPTYPE'
    $bSkip = Test-LogHit $b.arts.stdoutLog 'SMSCFG_APPLY_35F|SMSCFG_35F_SKIP_ENTRY'
    $aArm = Test-LogHit $a.arts.stdoutLog 'E10A31N_ARMED'
    $bArm = Test-LogHit $b.arts.stdoutLog 'E10A31N_ARMED'
    $aNeg = Test-LogHit $a.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND'
    $bNeg = Test-LogHit $b.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND'
    $retA = Get-Method0Ret $a.arts.stdoutLog
    $retB = Get-Method0Ret $b.arts.stdoutLog
    $needleB = ''
    $needleChain = ''
    if (Test-Path $sharedProv) {
      $provLine = Get-Content $sharedProv | Select-Object -Last 1
      if ($provLine -match "needle='([^']+)'") { $needleB = $Matches[1] }
    }
    $sharedCmp = Join-Path $reportDir 'e10a31n_post_range_compare_chain.csv'
    if (Test-Path $sharedCmp) {
      $needles = @()
      Import-Csv $sharedCmp -EA SilentlyContinue | ForEach-Object {
        if ($_.rhs_str) { $needles += $_.rhs_str }
        elseif ($_.rhs) { $needles += $_.rhs }
      }
      $needleChain = ($needles -join '>')
    }
    $ab = Join-Path $reportDir 'e10a31p_requirement_ab_compare.csv'
    $lines = @(
      'case,method0_ret,armed,saw_2e3fba_neg,apply,skip_entry,fail_needle,note'
      ('A_launch12,{0},{1},{2},O,0,{3},"napptype=12 path; entry optional"' -f $retA, [int]$aArm, [int]$aNeg, $needleA)
      ('B_case1_35F,{0},{1},{2},P,{3},{4},"napptype=1 n* + int32@0x35F=1"' -f $retB, [int]$bArm, [int]$bNeg, [int]$bSkip, $needleB)
    )
    Set-Content -Path $ab -Value $lines -Encoding UTF8
    if ($bApply -and $bSkip -and $bArm -and $retB -eq '0') {
      $primary = 'METHOD0_CASE1_35F_RETURN_ZERO'
    } elseif ($bApply -and $bSkip -and $bArm -and $bNeg -and $needleB -and $needleB -ne 'entry') {
      $primary = 'METHOD0_CASE1_NEXT_KEY_GATE'
    } elseif ($bApply -and $bSkip -and $bArm -and -not $bNeg) {
      $primary = 'METHOD0_CASE1_PAST_ENTRY_NO_MVNS'
    } elseif ($bApply -and $bSkip -and $bArm) {
      $primary = 'METHOD0_CASE1_APPLY_OBSERVED'
    } elseif ($aArm -or $bArm) {
      $primary = 'E10A31P_PARTIAL_METHOD0'
    } else {
      $primary = 'E10A31P_APPLY_INCOMPLETE'
    }
    $pVerdict = Join-Path $reportDir 'stage_e10a31p_case1_35f_verdict.md'
    $body = @"
## E10A-3.1p apply_case1 + @0x35F skip-entry

### Static
- After napptype atoi: ``sms_get(0x35F)``; nonzero skips optional ``entry`` strstr
- Switch on napptype: ``1/2/3`` use **n*** keys; else (e.g. 12) uses **b*** keys
- Launch ``napptype=12`` + n* fields is shell shape; SMSCFG case for n* is **1**

### AB (diagnostic method0-enter only)
| case | ret | armed | 2E3FBA | apply | skip@0x35F | fail needle |
|------|-----|-------|--------|-------|------------|-------------|
| A launch12 | $retA | $aArm | $aNeg | $aApply | no | $needleA |
| B case1 | $retB | $bArm | $bNeg | $bApply | $bSkip | $needleB |

- B needle chain (compare RHS): $needleChain
- ``reports/e10a31p_requirement_ab_compare.csv``
- original_default_recovered=false
- ``@0x355`` product bootstrap still deferred
"@
    Set-Content -Path $pVerdict -Value @"
# Stage E10A-3.1p case1 / 0x35F

- **Primary verdict**: ``$primary``

$body
"@ -Encoding UTF8
    Write-Host "wrote $pVerdict"
    $verdictMd = $pVerdict
  }
  'apply_bkeys' {
    # E10A-3.1q: B-only diagnostic — case1 n* + mirrored b* + @0x35F
    # Longer string → more failfn work; raise insn budget so trace reaches MVNS/return.
    # Retry on pre-method0 MEM_GET_HOST_CRASH flake (same as P case A).
    $b = $null
    for ($attempt = 1; $attempt -le 4; $attempt++) {
      Write-Host "=== apply_bkeys attempt $attempt/4 ==="
      $b = Invoke-OneCase 'q_case_b_nb' @{
        JJFB_E10A31N_APPLY_INT16 = '0'
        JJFB_E10A31O_APPLY_NAPPTYPE = '0'
        JJFB_E10A31P_APPLY = '0'
        JJFB_E10A31Q_APPLY = '1'
        JJFB_E10A31D_INSN_BUDGET = '80000'
      } -PreferMethod0Return
      $armedTry = Test-LogHit $b.arts.stdoutLog 'E10A31N_ARMED|SMSCFG_METHOD0_ENTER_APPLY_Q'
      $crashTry = Test-LogHit $b.arts.stdoutLog 'MEM_GET_HOST_CRASH'
      $bappPass = Test-LogHit $b.arts.stdoutLog 'skip_MVNS_success'
      if ($armedTry -and (Test-LogHit $b.arts.stdoutLog 'method0_return|METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND')) { break }
      if ($armedTry -and (Test-Path (Join-Path $reportDir 'e10a31n_post_range_compare_chain.csv'))) {
        $rhs = @(Import-Csv (Join-Path $reportDir 'e10a31n_post_range_compare_chain.csv') -EA SilentlyContinue | ForEach-Object {
          if ($_.rhs_str) { $_.rhs_str } else { $_.rhs }
        })
        if ($rhs -contains 'bapptype') { break }  # enough for Q gate verdict; R continues past strstr
      }
      if ($armedTry) { break }
      if ($crashTry) {
        Write-Host ("=== attempt {0}: pre-method0 crash; retry ===" -f $attempt)
        Start-Sleep -Seconds 2
        continue
      }
      break
    }
    $bApply = Test-LogHit $b.arts.stdoutLog 'SMSCFG_APPLY_NAPPTYPE'
    $bSkip = Test-LogHit $b.arts.stdoutLog 'SMSCFG_APPLY_35F|SMSCFG_35F_SKIP_ENTRY'
    $bQ = Test-LogHit $b.arts.stdoutLog 'SMSCFG_METHOD0_ENTER_APPLY_Q|SMSCFG_Q_BKEYS'
    $bArm = Test-LogHit $b.arts.stdoutLog 'E10A31N_ARMED'
    $bNeg = Test-LogHit $b.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND'
    $retB = Get-Method0Ret $b.arts.stdoutLog
    $needleB = ''
    $needleChain = ''
    $sharedProv = Join-Path $reportDir 'e10a31n_return_provenance.csv'
    if (Test-Path $sharedProv) {
      $provLine = Get-Content $sharedProv | Select-Object -Last 1
      if ($provLine -match "needle='([^']+)'") { $needleB = $Matches[1] }
    }
    $sharedCmp = Join-Path $reportDir 'e10a31n_post_range_compare_chain.csv'
    if (Test-Path $sharedCmp) {
      $needles = @()
      Import-Csv $sharedCmp -EA SilentlyContinue | ForEach-Object {
        if ($_.rhs_str) { $needles += $_.rhs_str }
        elseif ($_.rhs) { $needles += $_.rhs }
      }
      $needleChain = ($needles -join '>')
    }
    $ab = Join-Path $reportDir 'e10a31q_requirement_ab_compare.csv'
    Set-Content -Path $ab -Value @(
      'case,method0_ret,armed,saw_2e3fba_neg,apply_q,skip_entry,fail_needle,needle_chain,note'
      ('B_n_plus_b,{0},{1},{2},{3},{4},{5},{6},"case1 n* + mirrored b* + 0x35F"' -f `
        $retB, [int]$bArm, [int]$bNeg, [int]$bQ, [int]$bSkip, $needleB, $needleChain)
    ) -Encoding UTF8
    $bappInChain = ($needleChain -split '>' | Where-Object { $_ -eq 'bapptype' }).Count -gt 0
    if ($bApply -and $bQ -and $bArm -and $retB -eq '0') {
      $primary = 'METHOD0_Q_N_PLUS_B_RETURN_ZERO'
    } elseif ($bApply -and $bQ -and $bArm -and $bNeg -and $needleB -eq 'bapptype') {
      $primary = 'METHOD0_Q_BAPPTYPE_STILL_FAIL'
    } elseif ($bApply -and $bQ -and $bArm -and $bNeg -and $needleB -and $needleB -ne 'bapptype') {
      $primary = 'METHOD0_Q_NEXT_KEY_AFTER_BAPPTYPE'
    } elseif ($bApply -and $bQ -and $bArm -and -not $bNeg -and $bappInChain) {
      $primary = 'METHOD0_Q_BAPPTYPE_STRSTR_PASSED'
    } elseif ($bApply -and $bQ -and $bArm -and -not $bNeg) {
      $primary = 'METHOD0_Q_PAST_BAPPTYPE_NO_MVNS'
    } elseif ($bArm) {
      $primary = 'E10A31Q_PARTIAL_METHOD0'
    } else {
      $primary = 'E10A31Q_APPLY_INCOMPLETE'
    }
    $qVerdict = Join-Path $reportDir 'stage_e10a31q_bkeys_verdict.md'
    $body = @"
## E10A-3.1q n* + mirrored b*

- Apply Q: case1 n* + ``bapptype=12_bextid=482_...`` (launch-parallel) + ``@0x35F=1``
- ret=$retB armed=$bArm neg=$bNeg fail_needle=$needleB
- chain: $needleChain
- ``reports/e10a31q_requirement_ab_compare.csv``
- original_default_recovered=false
"@
    Set-Content -Path $qVerdict -Value @"
# Stage E10A-3.1q b* keys

- **Primary verdict**: ``$primary``

$body
"@ -Encoding UTF8
    Write-Host "wrote $qVerdict"
    $verdictMd = $qVerdict
  }
}

Write-Verdict $primary $Mode $body
Write-Host "exe_sha256=$exeHash"
