# Stage E10A-3.1o: strstr@0xAC4A4 + SMSCFG@0x377 napptype haystack
# Modes: helpers | apply_ab | validate
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('helpers','apply_ab','validate')]
  [string]$Mode = 'apply_ab',
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
$outDir = Join-Path $Root 'out\e10a31o'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31o_strstr_napptype_verdict.md'
$abCsv = Join-Path $reportDir 'e10a31o_requirement_ab_compare.csv'

function Stop-Kids {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[C-O]_')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-CaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } |
    ForEach-Object { Remove-Item -Path ("Env:{0}" -f $_.Name) -EA SilentlyContinue }
}

function Get-Arts([string]$tag, [string]$runId) {
  return [ordered]@{
    OverlayRoot = Join-Path $RunDir ("overlay\e10a31o\{0}_{1}" -f $tag, $runId)
    stdoutLog = Join-Path $logDir ("e10a31o_{0}_stdout.txt" -f $tag)
    stderrLog = Join-Path $logDir ("e10a31o_{0}_stderr.txt" -f $tag)
    timerCsv = Join-Path $reportDir ("e10a31o_{0}_timer_binding_trace.csv" -f $tag)
    initCsv = Join-Path $reportDir ("e10a31o_{0}_init_sequence_trace.csv" -f $tag)
    histCsv = Join-Path $reportDir ("e10a31o_{0}_helper_call_history.csv" -f $tag)
    insnCsv = Join-Path $reportDir ("e10a31o_{0}_method0_instruction_trace.csv" -f $tag)
    callCsvD = Join-Path $reportDir ("e10a31o_{0}_method0_call_tree.csv" -f $tag)
    provCsvD = Join-Path $reportDir ("e10a31o_{0}_method0_return_provenance.csv" -f $tag)
    appinfoCsv = Join-Path $reportDir ("e10a31o_{0}_appinfo_contract.csv" -f $tag)
    metaCsv = Join-Path $reportDir ("e10a31o_{0}_package_metadata_trace.csv" -f $tag)
    bindCsv = Join-Path $reportDir ("e10a31o_{0}_appinfo_binding_trace.csv" -f $tag)
    branchCsvF = Join-Path $reportDir ("e10a31o_{0}_fail_branch_f.csv" -f $tag)
    denseCsv = Join-Path $reportDir ("e10a31o_{0}_failsite_dense.csv" -f $tag)
    gCsv = Join-Path $reportDir ("e10a31o_{0}_strcmp.csv" -f $tag)
    hCsv = Join-Path $reportDir ("e10a31o_{0}_smscfg_h.csv" -f $tag)
    callCsv = Join-Path $reportDir ("e10a31o_{0}_call_tree.csv" -f $tag)
    provCsv = Join-Path $reportDir ("e10a31o_{0}_return_provenance.csv" -f $tag)
    predCsv = Join-Path $reportDir ("e10a31o_{0}_predicate_chain.csv" -f $tag)
    smscfgCsv = Join-Path $reportDir ("e10a31o_{0}_new_smscfg_reads.csv" -f $tag)
    compareCsv = Join-Path $reportDir ("e10a31o_{0}_compare_chain.csv" -f $tag)
    manifest = Join-Path $reportDir ("e10a31o_{0}_required_platform_state.json" -f $tag)
    timingCsv = Join-Path $reportDir ("e10a31o_{0}_smscfg_application_timing.csv" -f $tag)
  }
}

function Get-WL([string]$runId, $arts) {
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

function Test-Hit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-Stop([string]$log) {
  if (Test-Hit $log '\[JJFB_E10A31N\] method0_return|METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND|SMSCFG_APPLY_NAPPTYPE') {
    # Prefer waiting for method0_return when apply path may pass 2E3FBA
    if (Test-Hit $log '\[JJFB_E10A31N\] method0_return') { return 'OBSERVE_STOP_METHOD0_RETURN' }
    if (Test-Hit $log 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND') { return 'OBSERVE_STOP_NEG' }
  }
  if (Test-Hit $log 'GAMELIST_METHOD0_RETURN_ZERO') { return 'OBSERVE_STOP_METHOD0_ZERO' }
  if (Test-Hit $log 'MEM_GET_HOST_CRASH') {
    if (-not (Test-Hit $log 'E10A31N_ARMED|SMSCFG_APPLY')) { return 'OBSERVE_STOP_EARLY_CRASH' }
  }
  if (Test-Hit $log 'UC_ERR|FETCH_UNMAPPED') { return 'OBSERVE_STOP_VM_FAULT' }
  return $null
}

function Invoke-Case($wl, [string]$outLog, [string]$errLog) {
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  Stop-Kids
  $t0 = Get-Date
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
  $deadline = $t0.AddSeconds($OUTER_KILL_SEC)
  $observeStop = $null
  while (-not $p.HasExited) {
    if ((Get-Date) -ge $deadline) { break }
    $obs = Get-Stop $outLog
    if ($obs) { $observeStop = $obs; Write-Host "=== early stop: $obs ==="; break }
    Start-Sleep -Milliseconds 400
    try { $p.Refresh() } catch {}
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-Kids
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Invoke-One([string]$tag, [hashtable]$extra) {
  $runId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $arts = Get-Arts $tag $runId
  @($arts.stdoutLog, $arts.stderrLog) | ForEach-Object {
    if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue }
  }
  Clear-CaseEnv
  $wl = Get-WL $runId $arts
  foreach ($k in $extra.Keys) { $wl[$k] = [string]$extra[$k] }
  Write-Host "=== E10A31O case tag=$tag run_id=$runId ==="
  $r = Invoke-Case $wl $arts.stdoutLog $arts.stderrLog
  return [pscustomobject]@{ tag = $tag; runId = $runId; arts = $arts; result = $r }
}

function Get-Ret([string]$log) {
  if (-not (Test-Path $log)) { return '' }
  $m = Select-String -Path $log -Pattern '\[JJFB_E10A31N\] method0_return ret=(-?\d+)' | Select-Object -Last 1
  if ($m) { return $m.Matches[0].Groups[1].Value }
  return ''
}

function Get-FailPc([string]$log) {
  if (-not (Test-Path $log)) { return '' }
  if (Test-Hit $log 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND|0x2E3FBA') {
    # Prefer explicit next-fail markers from dense/branch if present
  }
  $m = Select-String -Path $log -Pattern 'first_negative_pc=0x([0-9A-Fa-f]+)|TRUE_FAIL.*?0x([0-9A-Fa-f]+)|fail_pc=0x([0-9A-Fa-f]+)' |
    Select-Object -Last 1
  if ($m) {
    foreach ($g in $m.Matches[0].Groups) {
      if ($g.Name -match '^\d+$' -and $g.Value) { return ('0x' + $g.Value) }
    }
  }
  if (Test-Hit $log '0x2E3FBA') { return '0x2E3FBA' }
  return ''
}

Write-Host '=== E10A-3.1o static helpers ==='
python (Join-Path $Root 'tools\e10a31o_strstr_helper_disasm.py')
if ($LASTEXITCODE -ne 0) { throw "helper disasm failed" }

if ($Mode -eq 'helpers') {
  Set-Content -Path $verdictMd -Value @"
# Stage E10A-3.1o helpers

- **Primary**: ``HELPER_0xAC4A4_IS_STRSTR``
- Also: ``HELPER_0xAC374_IS_STRLEN``
- See ``out/e10a31o/helpers_ac374_ac4a4_annotated.txt``
"@ -Encoding UTF8
  Write-Host 'PRIMARY=HELPER_0xAC4A4_IS_STRSTR'
  exit 0
}

if (-not $SkipBuild) {
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

Stop-Kids

$primary = 'INCOMPLETE'
$body = ''

if ($Mode -eq 'apply_ab') {
  # A: length=1 only (prior diagnostic) — expect strstr fail @0x2E3FBA
  $a = Invoke-One 'case_a_len1' @{
    JJFB_E10A31N_APPLY_INT16 = '0x355:1'
    JJFB_E10A31O_APPLY_NAPPTYPE = '0'
  }
  # B: paired length + launch-param haystack
  $b = Invoke-One 'case_b_napptype' @{
    JJFB_E10A31N_APPLY_INT16 = '0'
    JJFB_E10A31O_APPLY_NAPPTYPE = '1'
  }

  $aNeg = Test-Hit $a.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND|0x2E3FBA'
  $bApply = Test-Hit $b.arts.stdoutLog 'SMSCFG_APPLY_NAPPTYPE'
  $bStrstr = Test-Hit $b.arts.stdoutLog 'HELPER_0xAC4A4_IS_STRSTR'
  $bNeg = Test-Hit $b.arts.stdoutLog 'METHOD0_FIRST_CAUSAL_NEGATIVE_FOUND'
  $aRet = Get-Ret $a.arts.stdoutLog
  $bRet = Get-Ret $b.arts.stdoutLog

  $lines = @(
    'case,method0_ret,saw_2e3fba_neg,apply_napptype,note'
    ("A_len1,{0},{1},0,""expect strstr NULL -> 0x2E3FBA""" -f $aRet, [int]$aNeg)
    ("B_napptype,{0},{1},1,""paired 0x355=len + 0x377=GWY_LAUNCH_PARAM; original_default unknown""" -f $bRet, [int]$bNeg)
  )
  Set-Content -Path $abCsv -Value $lines -Encoding UTF8

  if ($bApply -and $bStrstr -and -not $bNeg -and $aNeg) {
    $primary = 'METHOD0_NAPPTYPE_STRSTR_GATE_PASSED'
  } elseif ($bApply -and $bNeg) {
    $primary = 'METHOD0_NAPPTYPE_APPLY_STILL_2E3FBA'
  } elseif ($bApply) {
    $primary = 'METHOD0_NAPPTYPE_APPLY_OBSERVED'
  } else {
    $primary = 'E10A31O_APPLY_INCOMPLETE'
  }

  $body = @"
## Helper identity (static + live)

- ``0xAC374`` = **strlen**
- ``0xAC4A4`` = **strstr(haystack, needle)**
- Failfn ``0x2E3F85``: strstr(buf, key); NULL -> MVNS -1 at ``0x2E3FBA``
- First key = ``napptype`` (caller r3)

## SMSCFG pairing

- ``@0x355`` int16_le = **copy length** for ``@0x377``
- ``@0x377`` = C-string haystack containing ``napptype=<val>``
- Prior compat ``0x355=1`` only copies 1 byte -> strstr fails

## AB

| case | ret | 2E3FBA neg | apply |
|------|-----|------------|-------|
| A len1 | $aRet | $aNeg | no |
| B napptype | $bRet | $bNeg | $bApply |

- elapsed A=$($a.result.elapsed)s B=$($b.result.elapsed)s
- ``original_default_recovered=false`` (launch-param candidate only)

## Artifacts

- ``out/e10a31o/helpers_ac374_ac4a4_annotated.txt``
- ``reports/e10a31o_requirement_ab_compare.csv``
- ``logs/e10a31o_case_a_len1_stdout.txt``
- ``logs/e10a31o_case_b_napptype_stdout.txt``
"@
}

Set-Content -Path $verdictMd -Value @"
# Stage E10A-3.1o strstr / napptype

- **Mode**: ``$Mode``
- **Primary verdict**: ``$primary``

$body

## Notes

- Do not brute-force ``@0x377``.
- Do not treat method0-enter apply as final product bootstrap.
- ``cfg_validate`` remains disabled.
"@ -Encoding UTF8

Write-Host "wrote $verdictMd"
Write-Host "PRIMARY=$primary"
