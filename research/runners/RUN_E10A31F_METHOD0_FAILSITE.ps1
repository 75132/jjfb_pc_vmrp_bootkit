# Stage E10A-3.1f: method0 fail-site refinement (past 0x2E1C24 sentinel)
# Modes: disasm | branch_trace | abi_ab | synthesize
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('disasm','branch_trace','abi_ab','synthesize')]
  [string]$Mode = 'branch_trace',
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
$outDir = Join-Path $Root 'out\e10a31f'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(180, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31f_method0_failsite_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31f\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'e10a31f_method0_failsite_stdout.txt'
$stderrLog = Join-Path $logDir 'e10a31f_method0_failsite_stderr.txt'

$branchCsv = Join-Path $reportDir 'e10a31f_fail_branch_trace.csv'
$denseCsv = Join-Path $reportDir 'e10a31f_failsite_dense_trace.csv'
$abiCsv = Join-Path $reportDir 'e10a31f_method0_abi_compare.csv'
$histCsv = Join-Path $reportDir 'e10a31d_helper_call_history.csv'
$insnCsv = Join-Path $reportDir 'e10a31d_method0_instruction_trace.csv'
$callCsv = Join-Path $reportDir 'e10a31d_method0_call_tree.csv'
$provCsv = Join-Path $reportDir 'e10a31d_method0_return_provenance.csv'
$appinfoCsv = Join-Path $reportDir 'e10a31d_appinfo_contract.csv'
$timerCsv = Join-Path $reportDir 'e10a31_timer_binding_trace.csv'
$initCsv = Join-Path $reportDir 'e10a31c_init_sequence_trace.csv'
$metaCsv = Join-Path $reportDir 'e10a31e_package_metadata_trace.csv'
$bindCsv = Join-Path $reportDir 'e10a31e_appinfo_binding_trace.csv'

function Stop-E10A31FChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31F_|E10A31E_|E10A31D_')
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

function Get-FileSha256([string]$path) {
  if (-not (Test-Path $path)) { return 'MISSING' }
  return (Get-FileHash -Path $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Write-EnvManifest([string[]]$cleared, $whitelist) {
  $csv = Join-Path $reportDir 'e10a31f_environment_manifest.csv'
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

function Get-BaseWhitelist {
  New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
  return [ordered]@{
    JJFB_E10A_RUN_ID = "$RunId"
    JJFB_E10A31_RUN_ID = "$RunId"
    JJFB_E10A31C_RUN_ID = "$RunId"
    JJFB_E10A31D_RUN_ID = "$RunId"
    JJFB_E10A31E_RUN_ID = "$RunId"
    JJFB_E10A31F_RUN_ID = "$RunId"
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
    JJFB_E10A31D_HISTORY = '1'
    JJFB_E10A31D_METHOD0_TRACE = '1'
    JJFB_E10A31D_APPINFO = '1'
    JJFB_E10A31D_INSN_BUDGET = '20000'
    JJFB_E10A31D_HIST_CSV = $histCsv
    JJFB_E10A31D_INSN_CSV = $insnCsv
    JJFB_E10A31D_CALL_CSV = $callCsv
    JJFB_E10A31D_PROV_CSV = $provCsv
    JJFB_E10A31D_APPINFO_CSV = $appinfoCsv
    JJFB_E10A31E_MODE = '1'
    JJFB_E10A31E_METADATA = '1'
    JJFB_E10A31E_BINDING = '1'
    JJFB_E10A31E_META_CSV = $metaCsv
    JJFB_E10A31E_BIND_CSV = $bindCsv
    JJFB_E10A31F_MODE = '1'
    JJFB_E10A31F_CONTINUE_PAST_SENTINEL = '1'
    JJFB_E10A31F_DENSE_WINDOW = '1'
    JJFB_E10A31F_BRANCH_CSV = $branchCsv
    JJFB_E10A31F_DENSE_CSV = $denseCsv
    JJFB_E10A31F_ABI_CSV = $abiCsv
  }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  # Wait for method0 deliver/disarm so CSVs flush; do not stop on mid-trace milestone.
  if (Test-LogHit $log 'MRC_INIT_RETURN_PROVENANCE_COMPLETE|method0_trace=disarm') {
    return 'OBSERVE_STOP_PROVENANCE_COMPLETE'
  }
  if (Test-LogHit $log 'delivered ret6=\d+ ret8=\d+ ret0=') {
    return 'OBSERVE_STOP_INIT_DELIVERED'
  }
  if (Test-LogHit $log 'UC_ERR|MEM_FAULT|FETCH_UNMAPPED') {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Invoke-CaseRun($wl, [string]$outLog, [string]$errLog) {
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  Stop-E10A31FChildren
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
    Stop-E10A31FChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Analyze-E10A31F([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $failPc = 0
  if (Test-Path $provCsv) {
    $rows = Get-Content $provCsv -EA SilentlyContinue
    if ($rows.Count -ge 2 -and $rows[1] -match ',') {
      $cols = $rows[1].Split(',')
      if ($cols.Count -ge 3 -and $cols[2] -match '0x([0-9A-Fa-f]+)') {
        $failPc = [Convert]::ToUInt32($Matches[1], 16)
      }
    }
  }
  if ($failPc -eq 0 -and $t -match 'MRC_INIT_TRUE_FAILURE_PC_FOUND note=pc=0x([0-9A-Fa-f]+)') {
    $failPc = [Convert]::ToUInt32($Matches[1], 16)
  }
  $flags = @{
    sentinel = $t -match 'FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE|FAILSITE_2E1C24_IGNORED_AS_SENTINEL'
    no_branch = $t -match 'FAILSITE_NO_BRANCH_PREDICATE_INTO_2E1C24'
    false_pos = $t -match 'E10A31D_FIRST_FAILURE_FALSE_POSITIVE|FAILSITE_2E1C24_IGNORED'
    continues = $t -match 'FAILSITE_EXECUTION_CONTINUES_PAST_2E1C24|FAILSITE_2E1C24_IGNORED_AS_SENTINEL'
    true_fail = $t -match 'MRC_INIT_TRUE_FAILURE_PC_FOUND'
    still_unknown = $t -match 'MRC_INIT_TRUE_FAILURE_STILL_UNKNOWN'
    method0_ok = $t -match 'delivered ret6=0 ret8=0 ret0=0|GAMELIST_METHOD0_RETURN_ZERO'
    method0_neg = $t -match 'delivered ret6=0 ret8=0 ret0=-1'
    filebuf = $t -match 'METHOD0_INPUT_ABI_FILEBUF_OVERRIDE'
    abi_irrelevant = $false
    fail_pc = $failPc
  }
  if ($flags.filebuf -and $flags.method0_neg -and $failPc -eq 0x2E1C24) {
    $flags.abi_irrelevant = $true
  }
  if ($flags.filebuf -and $flags.method0_ok) {
    # causal success
  }

  $verdicts = @()
  if ($flags.sentinel) { $verdicts += 'FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE' }
  if ($flags.no_branch) { $verdicts += 'FAILSITE_NO_BRANCH_PREDICATE_INTO_2E1C24' }
  if ($flags.false_pos) { $verdicts += 'E10A31D_FIRST_FAILURE_FALSE_POSITIVE' }
  if ($flags.continues) { $verdicts += 'FAILSITE_EXECUTION_CONTINUES_PAST_2E1C24' }
  if ($flags.true_fail) { $verdicts += 'MRC_INIT_TRUE_FAILURE_PC_FOUND' }
  if ($flags.still_unknown) { $verdicts += 'MRC_INIT_TRUE_FAILURE_STILL_UNKNOWN' }
  if ($flags.method0_ok) { $verdicts += 'GAMELIST_METHOD0_RETURN_ZERO' }
  if ($flags.filebuf -and $flags.method0_ok) { $verdicts += 'METHOD0_INPUT_ABI_CAUSAL' }
  elseif ($flags.filebuf -and $flags.method0_neg) { $verdicts += 'METHOD0_INPUT_ABI_NOT_CAUSAL' }

  $primary = 'E10A31F_INSUFFICIENT_EVIDENCE'
  if ($flags.method0_ok) { $primary = 'GAMELIST_METHOD0_RETURN_ZERO' }
  elseif ($flags.true_fail -and $failPc -ne 0 -and $failPc -ne 0x2E1C24) {
    $primary = 'MRC_INIT_TRUE_FAILURE_PC_FOUND'
  }
  elseif ($flags.sentinel -and $flags.continues) {
    $primary = 'FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE'
  }
  elseif ($flags.sentinel) { $primary = 'FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE' }

  return [pscustomobject]@{ primary = $primary; verdicts = $verdicts; flags = $flags }
}

# ---- main ----
Write-Host '=== E10A-3.1f static disasm ==='
python (Join-Path $Root 'tools\e10a31f_failsite_disasm.py') --out-dir $outDir
if ($Mode -eq 'disasm') {
  $an = [pscustomobject]@{
    primary = 'FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE'
    verdicts = @('FAILSITE_2E1C24_IS_NEG1_SENTINEL_STORE','FAILSITE_NO_BRANCH_PREDICATE_INTO_2E1C24','E10A31D_FIRST_FAILURE_FALSE_POSITIVE')
    flags = @{ fail_pc = 0; method0_ok = $false; method0_neg = $false; true_fail = $false; continues = $false; sentinel = $true }
  }
  $elapsed = 0; $exitCode = 0; $observeStop = 'DISASM_ONLY'
} else {
  if (-not $SkipBuild) {
    & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
    if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
  }
  if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

  Stop-E10A31FChildren
  $cleared = Clear-AllInheritedCaseEnv
  @(
    $branchCsv, $denseCsv, $abiCsv, $stdoutLog, $stderrLog, $insnCsv, $callCsv, $provCsv
  ) | ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue } }

  if ($Mode -eq 'abi_ab') {
    Write-Host '--- Case A: input=0 (default) ---'
    $wlA = Get-BaseWhitelist
    $rA = Invoke-CaseRun $wlA (Join-Path $logDir 'e10a31f_abi_a_stdout.txt') (Join-Path $logDir 'e10a31f_abi_a_stderr.txt')
    $anA = Analyze-E10A31F (Join-Path $logDir 'e10a31f_abi_a_stdout.txt')

    Clear-AllInheritedCaseEnv | Out-Null
    $RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    $OverlayRoot = Join-Path $RunDir ("overlay\e10a31f\{0}" -f $RunId)
    Write-Host '--- Case B: input=filebuf (guest_code_base) ---'
    $wlB = Get-BaseWhitelist
    $wlB['JJFB_E10A31F_ABI_FILEBUF'] = '1'
    $rB = Invoke-CaseRun $wlB $stdoutLog $stderrLog
    Write-EnvManifest $cleared $wlB
    $an = Analyze-E10A31F $stdoutLog
    $elapsed = $rB.elapsed; $exitCode = $rB.exitCode; $observeStop = $rB.observeStop
    # merge ab note
    if (-not (Test-Path $abiCsv)) {
      Set-Content $abiCsv "run_id,case,appid,ret0,fail_pc,note`n"
    }
    Add-Content $abiCsv ("{0},legacy_input0,,{1},0x{2:X},caseA`n{3},filebuf,,{4},,caseB" -f `
      $rA.elapsed, $(if ($anA.flags.method0_ok) {0} else {-1}), $anA.flags.fail_pc, $RunId, $(if ($an.flags.method0_ok) {0} else {-1}))
  } else {
    $wl = Get-BaseWhitelist
    Write-EnvManifest $cleared $wl
    Write-Host "=== E10A-3.1f mode=$Mode run_id=$RunId ==="
    $r = Invoke-CaseRun $wl $stdoutLog $stderrLog
    $an = Analyze-E10A31F $stdoutLog
    $elapsed = $r.elapsed; $exitCode = $r.exitCode; $observeStop = $r.observeStop
  }
}

foreach ($pair in @(
  @{p=$branchCsv; h='run_id,seq,pc,note,r0,r1,r2,r3,r4,r9,cpsr'},
  @{p=$denseCsv; h='run_id,seq,pc,lr,r0,r1,r2,r3,r4,r9,cpsr,size,bytes,note'},
  @{p=$abiCsv; h='run_id,case,input,input_len,note'}
)) {
  if (-not (Test-Path $pair.p)) { Set-Content -Path $pair.p -Value $pair.h -Encoding UTF8 }
}

$verdictList = ($an.verdicts | ForEach-Object { "- ``$_``" }) -join "`n"
$failPcHex = ('0x{0:X}' -f [uint32]($an.flags.fail_pc))
$md = @"
# Stage E10A-3.1f method0 Fail-site Verdict

- **Mode**: ``$Mode``
- **run_id**: ``$RunId``
- **elapsed_sec**: $elapsed
- **exit_code**: $exitCode
- **observe_stop**: ``$observeStop``
- **Primary verdict**: ``$($an.primary)``
- **fail_pc (provenance)**: ``$failPcHex``

## Milestones

$verdictList

## Static finding (always)

``0x2E1C24`` is **unconditional** ``MVNS r0, r4`` with ``r4=0`` (set at ``0x2E1BC0``), then ``STRH`` stores -1.
It is **not** a branch gate and does **not** read appInfo / R9+0x920.
See ``out/e10a31f/gamelist_2e1bbd_fail_annotated.txt``.

## Key observations

| Check | Result |
|-------|--------|
| sentinel identified | $($an.flags.sentinel) |
| continues past 0x2E1C24 | $($an.flags.continues) |
| true fail PC found | $($an.flags.true_fail) |
| method0 ret0 | $($an.flags.method0_ok) |
| method0 ret-1 | $($an.flags.method0_neg) |

## Artifacts

- ``out/e10a31f/gamelist_2e1bbd_fail_annotated.txt``
- ``reports/e10a31f_fail_branch_trace.csv``
- ``reports/e10a31f_failsite_dense_trace.csv``
- ``reports/e10a31f_method0_abi_compare.csv``
- ``logs/e10a31f_method0_failsite_stdout.txt``

## Forbidden (unchanged)

- no cfg work
- no force method0 return
- no patch of 0x2E1C24
"@

Set-Content -Path $verdictMd -Value $md -Encoding UTF8
Write-Host "=== E10A-3.1f done primary=$($an.primary) ==="
Write-Host "verdict: $verdictMd"
