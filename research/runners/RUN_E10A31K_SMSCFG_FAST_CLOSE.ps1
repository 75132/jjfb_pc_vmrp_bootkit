# Stage E10A-3.1k: SMSCFG fast causal close + DSM compatibility bootstrap
# Modes: diagnostic_ab | bootstrap | method0_validate | cfg_validate
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('diagnostic_ab','bootstrap','method0_validate','cfg_validate')]
  [string]$Mode = 'diagnostic_ab',
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
$outDir = Join-Path $Root 'out\e10a31k'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31k_smscfg_fast_close_verdict.md'

function Stop-E10A31KChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[CDEFGHIJK]_')
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
    OverlayRoot = Join-Path $RunDir ("overlay\e10a31k\{0}_{1}" -f $tag, $runId)
    stdoutLog = Join-Path $logDir ("e10a31k_{0}_stdout.txt" -f $tag)
    stderrLog = Join-Path $logDir ("e10a31k_{0}_stderr.txt" -f $tag)
    timerCsv = Join-Path $reportDir ("e10a31k_{0}_timer_binding_trace.csv" -f $tag)
    initCsv = Join-Path $reportDir ("e10a31k_{0}_init_sequence_trace.csv" -f $tag)
    histCsv = Join-Path $reportDir ("e10a31k_{0}_helper_call_history.csv" -f $tag)
    insnCsv = Join-Path $reportDir ("e10a31k_{0}_method0_instruction_trace.csv" -f $tag)
    callCsv = Join-Path $reportDir ("e10a31k_{0}_method0_call_tree.csv" -f $tag)
    provCsv = Join-Path $reportDir ("e10a31k_{0}_method0_return_provenance.csv" -f $tag)
    appinfoCsv = Join-Path $reportDir ("e10a31k_{0}_appinfo_contract.csv" -f $tag)
    metaCsv = Join-Path $reportDir ("e10a31k_{0}_package_metadata_trace.csv" -f $tag)
    bindCsv = Join-Path $reportDir ("e10a31k_{0}_appinfo_binding_trace.csv" -f $tag)
    branchCsv = Join-Path $reportDir ("e10a31k_{0}_fail_branch_trace.csv" -f $tag)
    denseCsv = Join-Path $reportDir ("e10a31k_{0}_failsite_dense_trace.csv" -f $tag)
    gCsv = Join-Path $reportDir ("e10a31k_{0}_strcmp_arg_trace.csv" -f $tag)
    hCsv = Join-Path $reportDir ("e10a31k_{0}_smscfg_trace.csv" -f $tag)
    cfgGateCsv = Join-Path $reportDir ("e10a31k_{0}_cfg_gate_trace.csv" -f $tag)
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
    JJFB_E10A31F_BRANCH_CSV = $arts.branchCsv
    JJFB_E10A31F_DENSE_CSV = $arts.denseCsv
    JJFB_E10A31G_MODE = '1'
    JJFB_E10A31G_CSV = $arts.gCsv
    JJFB_E10A31H_MODE = '1'
    JJFB_E10A31H_CSV = $arts.hCsv
  }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  if (Test-LogHit $log 'GAMELIST_INIT_SEQUENCE_COMPLETE|MRC_INIT_ACCEPTS_PLATFORM_SMSCFG') {
    return 'OBSERVE_STOP_METHOD0_OK'
  }
  if (Test-LogHit $log 'GAMELIST_CFG_GATE_REACHED|JJFB_GAMELIST_CFG_GATE|cfg\.bin') {
    return 'OBSERVE_STOP_CFG_GATE'
  }
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
  Stop-E10A31KChildren
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
    Stop-E10A31KChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Get-Ret0([string]$log) {
  if (-not (Test-Path $log)) { return $null }
  $m = Select-String -Path $log -Pattern 'delivered ret6=(-?\d+) ret8=(-?\d+) ret0=(-?\d+)' |
    Select-Object -Last 1
  if ($m) {
    return [pscustomobject]@{
      ret6 = [int]$m.Matches[0].Groups[1].Value
      ret8 = [int]$m.Matches[0].Groups[2].Value
      ret0 = [int]$m.Matches[0].Groups[3].Value
    }
  }
  $m0 = Select-String -Path $log -Pattern 'method0_return ret=(-?\d+)' | Select-Object -Last 1
  $r6 = Select-String -Path $log -Pattern 'init_method.*method=6.*ret=(-?\d+)|ret6=(-?\d+)' | Select-Object -Last 1
  $r8 = Select-String -Path $log -Pattern 'init_method.*method=8.*ret=(-?\d+)|ret8=(-?\d+)' | Select-Object -Last 1
  if (-not $m0) { return $null }
  return [pscustomobject]@{
    ret6 = if ($r6) { [int]($r6.Matches[0].Groups | Where-Object { $_.Success -and $_.Value -match '^-?\d+$' } | Select-Object -Last 1).Value } else { $null }
    ret8 = if ($r8) { [int]($r8.Matches[0].Groups | Where-Object { $_.Success -and $_.Value -match '^-?\d+$' } | Select-Object -Last 1).Value } else { $null }
    ret0 = [int]$m0.Matches[0].Groups[1].Value
  }
}

function Get-FailPc([string]$log) {
  if (-not (Test-Path $log)) { return $null }
  $m = Select-String -Path $log -Pattern 'fail_pc=0x([0-9A-Fa-f]+)|TRUE_FAIL_PC=0x([0-9A-Fa-f]+)|first_fail_pc=0x([0-9A-Fa-f]+)|pc=0x2E1C24' |
    Select-Object -First 1
  if (-not $m) {
    if (Test-LogHit $log '0x2E1C24') { return '0x2E1C24' }
    return $null
  }
  foreach ($g in 1..3) {
    if ($m.Matches[0].Groups[$g].Success -and $m.Matches[0].Groups[$g].Value) {
      return ('0x{0}' -f $m.Matches[0].Groups[$g].Value.ToUpperInvariant())
    }
  }
  return '0x2E1C24'
}

function Get-CaseFlags([string]$log) {
  $rets = Get-Ret0 $log
  return [ordered]@{
    diag_set = Test-LogHit $log '\[SMSCFG_DIAG_SET\]'
    bootstrap = Test-LogHit $log '\[SMSCFG_BOOTSTRAP\]'
    bootstrap_complete = Test-LogHit $log 'SMSCFG_BOOTSTRAP_COMPLETE'
    compat = Test-LogHit $log 'source=COMPAT_PROFILE'
    file_src = Test-LogHit $log 'source=FILE'
    gpt_tag = Test-LogHit $log 'SMS_CFG_349_IS_GPT|new=475054|gpt=475054'
    empty_gpt = Test-LogHit $log 'SMS_CFG_349_IS_NUL|METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG'
    ret6 = if ($rets) { $rets.ret6 } else { $null }
    ret8 = if ($rets) { $rets.ret8 } else { $null }
    ret0 = if ($rets) { $rets.ret0 } else { $null }
    fail_pc = Get-FailPc $log
    init_complete = Test-LogHit $log 'GAMELIST_INIT_SEQUENCE_COMPLETE|init_sequence_complete|INIT_TX.*complete'
    cfg_gate = Test-LogHit $log 'GAMELIST_CFG_GATE_REACHED|JJFB_GAMELIST_CFG_GATE'
    cfg_open = Test-LogHit $log 'cfg\.bin'
    external_cfg = Test-LogHit $log 'gwy/cfg\.bin|GAMELIST_EXTERNAL_CFG_OPEN'
    embedded_cfg = Test-LogHit $log 'gamelist\.mrp::cfg\.bin|GAMELIST_EMBEDDED_CFG_OPEN'
  }
}

function Invoke-OneCase([string]$tag, [hashtable]$extraEnv) {
  $runId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $arts = Get-ArtifactPaths $tag $runId
  @($arts.stdoutLog, $arts.stderrLog) | ForEach-Object {
    if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue }
  }
  Clear-AllInheritedCaseEnv | Out-Null
  $wl = Get-BaseWhitelist $runId $arts
  foreach ($k in $extraEnv.Keys) { $wl[$k] = [string]$extraEnv[$k] }
  Write-Host "=== E10A31K case tag=$tag run_id=$runId ==="
  $r = Invoke-CaseRun $wl $arts.stdoutLog $arts.stderrLog
  $flags = Get-CaseFlags $arts.stdoutLog
  return [pscustomobject]@{
    tag = $tag; runId = $runId; arts = $arts; result = $r; flags = $flags
  }
}

function Choose-AbVerdict($a, $b) {
  $a0 = $a.flags.ret0
  $b0 = $b.flags.ret0
  $aPc = $a.flags.fail_pc
  $bPc = $b.flags.fail_pc
  $bLog = $b.arts.stdoutLog
  $aLog = $a.arts.stdoutLog
  $bGptMatch = Test-LogHit $bLog 'GPT_SOURCE_BYTES_MATCH_TAG|SMS_CFG_349_IS_GPT'
  $aGptMatch = Test-LogHit $aLog 'GPT_SOURCE_BYTES_MATCH_TAG|SMS_CFG_349_IS_GPT'
  $bNextGwy = Test-LogHit $bLog 'rhs=gwy|hex=6777790047505400'
  $bPastGpt = $bGptMatch -and ($null -ne $b0) -and $b0 -ne 0 -and $bNextGwy

  if ($null -ne $b0 -and $b0 -eq 0 -and ($null -eq $a0 -or $a0 -ne 0)) {
    return 'SMSCFG_GPT_GATE_CAUSAL_CONFIRMED'
  }
  if ($bPastGpt -or ($bGptMatch -and -not $aGptMatch)) {
    return 'SMSCFG_GPT_GATE_CAUSAL_CONFIRMED'
  }
  if ($bPc -and $aPc -and ($bPc -ne $aPc) -and ($aPc -eq '0x2E1C24')) {
    return 'SMSCFG_GPT_GATE_CAUSAL_CONFIRMED'
  }
  if ($b.flags.diag_set -and ($null -ne $b0) -and $b0 -ne 0 -and ($bPc -eq '0x2E1C24' -or -not $bPc)) {
    if (-not $b.flags.gpt_tag -and -not $bGptMatch) { return 'SMSCFG_GPT_WRITE_PUBLICATION_WRONG' }
    return 'SMSCFG_GPT_NOT_CAUSAL'
  }
  if ($b.flags.diag_set -and -not $b.flags.gpt_tag -and -not $bGptMatch -and ($null -eq $b0 -or $b0 -ne 0)) {
    return 'SMSCFG_GPT_WRITE_PUBLICATION_WRONG'
  }
  return 'SMSCFG_GPT_NOT_CAUSAL'
}

function Choose-Method0Verdict($flags, [string]$log) {
  $gptOk = Test-LogHit $log 'GPT_SOURCE_BYTES_MATCH_TAG|SMS_CFG_349_IS_GPT|gpt_hex=475054'
  $nextGwy = Test-LogHit $log 'rhs=gwy|hex=6777790047505400'
  if ($null -ne $flags.ret0 -and $flags.ret0 -eq 0) {
    return 'MRC_INIT_ACCEPTS_PLATFORM_SMSCFG'
  }
  if ($gptOk -and ($null -eq $flags.ret0 -or $flags.ret0 -ne 0)) {
    if ($nextGwy -or (Test-LogHit $log 'TRUE_FAIL')) {
      return 'SMSCFG_GATE_PASSED_NEXT_PRECONDITION_FOUND'
    }
  }
  if ($flags.fail_pc -and $flags.fail_pc -ne '0x2E1C24') {
    return 'SMSCFG_GATE_PASSED_NEXT_PRECONDITION_FOUND'
  }
  if ($flags.fail_pc -eq '0x2E1C24' -and -not $gptOk) {
    return 'SMSCFG_POINTER_OR_PROFILE_PUBLICATION_WRONG'
  }
  if ($gptOk) { return 'SMSCFG_GATE_PASSED_NEXT_PRECONDITION_FOUND' }
  return 'SMSCFG_POINTER_OR_PROFILE_PUBLICATION_WRONG'
}

function Write-Verdict([string]$primary, [string]$modeName, $body) {
  $md = @"
# Stage E10A-3.1k SMSCFG fast close

- **Mode**: ``$modeName``
- **Primary verdict**: ``$primary``

$body

## Notes

- ``original_default_recovered=false``
- Compatibility profile is a Mythroad platform reconstruction, not a recovered full ``dsm.cfg``.
"@
  Set-Content -Path $verdictMd -Value $md -Encoding UTF8
  Write-Host "wrote $verdictMd"
  Write-Host "PRIMARY=$primary"
}

# ---- main ----
if (-not $SkipBuild) {
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }
$exeHash = (Get-FileHash $exe -Algorithm SHA256).Hash
Write-Host "main.exe sha256=$exeHash"

Stop-E10A31KChildren

$primary = 'INCOMPLETE'
$body = ''

switch ($Mode) {
  'diagnostic_ab' {
    $caseA = Invoke-OneCase 'diag_a' @{
      GWY_DIAG_SMSCFG_GPT_MINIMAL = '0'
      GWY_SMSCFG_BOOTSTRAP = '0'
      JJFB_E10A31K_MODE = '0'
    }
    $caseB = Invoke-OneCase 'diag_b' @{
      GWY_DIAG_SMSCFG_GPT_MINIMAL = '1'
      GWY_SMSCFG_BOOTSTRAP = '0'
      JJFB_E10A31K_MODE = '0'
    }
    $primary = Choose-AbVerdict $caseA $caseB
    $body = @"
## A/B

| Case | DIAG | ret6 | ret8 | ret0 | fail_pc | diag_set | gpt_tag | elapsed |
|------|------|------|------|------|---------|----------|---------|---------|
| A | 0 | $($caseA.flags.ret6) | $($caseA.flags.ret8) | $($caseA.flags.ret0) | $($caseA.flags.fail_pc) | $($caseA.flags.diag_set) | $($caseA.flags.gpt_tag) | $($caseA.result.elapsed)s |
| B | 1 | $($caseB.flags.ret6) | $($caseB.flags.ret8) | $($caseB.flags.ret0) | $($caseB.flags.fail_pc) | $($caseB.flags.diag_set) | $($caseB.flags.gpt_tag) | $($caseB.result.elapsed)s |

Stop writer research if method0 returns 0 or fail PC moves past ``0x2E1C24``.
"@
  }
  'bootstrap' {
    $c = Invoke-OneCase 'bootstrap' @{
      GWY_DIAG_SMSCFG_GPT_MINIMAL = '0'
      GWY_SMSCFG_BOOTSTRAP = '1'
      JJFB_E10A31K_MODE = '1'
    }
    if ($c.flags.bootstrap_complete -and ($c.flags.compat -or $c.flags.file_src)) {
      $primary = 'SMSCFG_BOOTSTRAP_COMPLETE'
    } else {
      $primary = 'SMSCFG_BOOTSTRAP_INCOMPLETE'
    }
    $body = @"
## Bootstrap

- source FILE=$($c.flags.file_src) COMPAT=$($c.flags.compat)
- ret0=$($c.flags.ret0) fail_pc=$($c.flags.fail_pc)
- elapsed=$($c.result.elapsed)s
"@
  }
  'method0_validate' {
    $c = Invoke-OneCase 'method0' @{
      GWY_DIAG_SMSCFG_GPT_MINIMAL = '0'
      GWY_SMSCFG_BOOTSTRAP = '1'
      JJFB_E10A31K_MODE = '1'
    }
    $primary = Choose-Method0Verdict $c.flags $c.arts.stdoutLog
    if ($primary -eq 'MRC_INIT_ACCEPTS_PLATFORM_SMSCFG') {
      if ($c.flags.init_complete -or ($c.flags.ret6 -eq 0 -and $c.flags.ret8 -eq 0 -and $c.flags.ret0 -eq 0)) {
        $primary = 'GAMELIST_INIT_SEQUENCE_COMPLETE'
      }
    }
    $body = @"
## method0_validate

- bootstrap=$($c.flags.bootstrap_complete) source_compat=$($c.flags.compat) gpt=$($c.flags.gpt_tag)
- ret6=$($c.flags.ret6) ret8=$($c.flags.ret8) ret0=$($c.flags.ret0)
- fail_pc=$($c.flags.fail_pc)
- elapsed=$($c.result.elapsed)s
"@
  }
  'cfg_validate' {
    $c = Invoke-OneCase 'cfg' @{
      GWY_DIAG_SMSCFG_GPT_MINIMAL = '0'
      GWY_SMSCFG_BOOTSTRAP = '1'
      JJFB_E10A31K_MODE = '1'
      JJFB_E10A31_MODE = '1'
      JJFB_E10A31_CFG_GATE = '1'
      JJFB_E10A31_CFG_GATE_CSV = (Join-Path $reportDir 'e10a31k_cfg_cfg_gate_trace.csv')
    }
    if ($c.flags.external_cfg) { $primary = 'GAMELIST_EXTERNAL_CFG_OPEN' }
    elseif ($c.flags.embedded_cfg) { $primary = 'GAMELIST_EMBEDDED_CFG_OPEN' }
    elseif ($c.flags.cfg_gate -or $c.flags.cfg_open) { $primary = 'GAMELIST_CFG_GATE_REACHED' }
    elseif ($null -ne $c.flags.ret0 -and $c.flags.ret0 -eq 0) { $primary = 'METHOD0_OK_CFG_NOT_YET' }
    else { $primary = Choose-Method0Verdict $c.flags $c.arts.stdoutLog }
    $body = @"
## cfg_validate

- ret0=$($c.flags.ret0) cfg_gate=$($c.flags.cfg_gate) cfg_open=$($c.flags.cfg_open)
- elapsed=$($c.result.elapsed)s
"@
  }
}

Write-Verdict $primary $Mode $body
Write-Host "exe_sha256=$exeHash"
