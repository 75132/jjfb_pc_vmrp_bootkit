# Stage E10A-3.1j: SMSCFG boot-to-method0 writer provenance (long window)
# Modes: full_window | gamelist_window | dsmcfg | writer_api | compare
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('full_window','gamelist_window','dsmcfg','writer_api','compare')]
  [string]$Mode = 'full_window',
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
$outDir = Join-Path $Root 'out\e10a31j'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(180, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31j_smscfg_long_window_verdict.md'
$compareCsv = Join-Path $reportDir 'e10a31j_window_compare.csv'

function Stop-E10A31JChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[DEFGHIJ]_')
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
    OverlayRoot = Join-Path $RunDir ("overlay\e10a31j\{0}_{1}" -f $tag, $runId)
    stdoutLog = Join-Path $logDir ("e10a31j_{0}_stdout.txt" -f $tag)
    stderrLog = Join-Path $logDir ("e10a31j_{0}_stderr.txt" -f $tag)
    ptrCsv = Join-Path $reportDir ("e10a31j_{0}_smscfg_pointer_lifetime.csv" -f $tag)
    writeCsv = Join-Path $reportDir ("e10a31j_{0}_smscfg_long_write_trace.csv" -f $tag)
    apiCsv = Join-Path $reportDir ("e10a31j_{0}_smscfg_writer_api_trace.csv" -f $tag)
    ioCsv = Join-Path $reportDir ("e10a31j_{0}_dsm_cfg_io_trace.csv" -f $tag)
    ckptCsv = Join-Path $reportDir ("e10a31j_{0}_smscfg_checkpoints.csv" -f $tag)
    timerCsv = Join-Path $reportDir ("e10a31j_{0}_timer_binding_trace.csv" -f $tag)
    initCsv = Join-Path $reportDir ("e10a31j_{0}_init_sequence_trace.csv" -f $tag)
    histCsv = Join-Path $reportDir ("e10a31j_{0}_helper_call_history.csv" -f $tag)
    insnCsv = Join-Path $reportDir ("e10a31j_{0}_method0_instruction_trace.csv" -f $tag)
    callCsv = Join-Path $reportDir ("e10a31j_{0}_method0_call_tree.csv" -f $tag)
    provCsv = Join-Path $reportDir ("e10a31j_{0}_method0_return_provenance.csv" -f $tag)
    appinfoCsv = Join-Path $reportDir ("e10a31j_{0}_appinfo_contract.csv" -f $tag)
    metaCsv = Join-Path $reportDir ("e10a31j_{0}_package_metadata_trace.csv" -f $tag)
    bindCsv = Join-Path $reportDir ("e10a31j_{0}_appinfo_binding_trace.csv" -f $tag)
    branchCsv = Join-Path $reportDir ("e10a31j_{0}_fail_branch_trace.csv" -f $tag)
    denseCsv = Join-Path $reportDir ("e10a31j_{0}_failsite_dense_trace.csv" -f $tag)
    gCsv = Join-Path $reportDir ("e10a31j_{0}_strcmp_arg_trace.csv" -f $tag)
    hCsv = Join-Path $reportDir ("e10a31j_{0}_smscfg_trace.csv" -f $tag)
  }
}

function Publish-CanonicalArtifacts($arts) {
  Copy-Item $arts.ptrCsv (Join-Path $reportDir 'e10a31j_smscfg_pointer_lifetime.csv') -Force -EA SilentlyContinue
  Copy-Item $arts.writeCsv (Join-Path $reportDir 'e10a31j_smscfg_long_write_trace.csv') -Force -EA SilentlyContinue
  Copy-Item $arts.apiCsv (Join-Path $reportDir 'e10a31j_smscfg_writer_api_trace.csv') -Force -EA SilentlyContinue
  Copy-Item $arts.ioCsv (Join-Path $reportDir 'e10a31j_dsm_cfg_io_trace.csv') -Force -EA SilentlyContinue
  Copy-Item $arts.ckptCsv (Join-Path $reportDir 'e10a31j_smscfg_checkpoints.csv') -Force -EA SilentlyContinue
}

function Get-BaseWhitelist([string]$runId, [string]$window, $arts) {
  New-Item -ItemType Directory -Force -Path $arts.OverlayRoot | Out-Null
  return [ordered]@{
    JJFB_E10A_RUN_ID = "$runId"
    JJFB_E10A31_RUN_ID = "$runId"
    JJFB_E10A31C_RUN_ID = "$runId"
    JJFB_E10A31D_RUN_ID = "$runId"
    JJFB_E10A31E_RUN_ID = "$runId"
    JJFB_E10A31F_RUN_ID = "$runId"
    JJFB_E10A31G_RUN_ID = "$runId"
    JJFB_E10A31H_RUN_ID = "$runId"
    JJFB_E10A31J_RUN_ID = "$runId"
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
    JJFB_E10A31J_MODE = '1'
    JJFB_E10A31J_WINDOW = $window
    JJFB_E10A31J_PTR_CSV = $arts.ptrCsv
    JJFB_E10A31J_WRITE_CSV = $arts.writeCsv
    JJFB_E10A31J_API_CSV = $arts.apiCsv
    JJFB_E10A31J_IO_CSV = $arts.ioCsv
    JJFB_E10A31J_CKPT_CSV = $arts.ckptCsv
  }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  if (Test-LogHit $log 'E10A31J_STOP|SMSCFG_GPT_WRITE|METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG') {
    return 'OBSERVE_STOP_E10A31J'
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
  Stop-E10A31JChildren
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
    Stop-E10A31JChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Get-FlagsFromLog([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  return [ordered]@{
    armed = [bool]($t -match 'E10A31J_ARMED')
    any_write = [bool]($t -match 'SMSCFG_ANY_WRITE')
    write_before = [bool]($t -match 'SMSCFG_WRITE_BEFORE_GAMELIST|SMSCFG_WRITER_PRECEDES_GAMELIST')
    write_during = [bool]($t -match 'SMSCFG_WRITE_DURING_GAMELIST')
    write_349 = [bool]($t -match 'SMSCFG_349_WRITE')
    write_gpt = [bool]($t -match 'SMSCFG_GPT_WRITE')
    setbytes_349 = [bool]($t -match 'SMSCFG_SETBYTES_349_CALLED')
    testcom_502 = [bool]($t -match 'SMSCFG_TESTCOM_502_CALLED')
    indication6 = [bool]($t -match 'SMSCFG_INDICATION_CODE6_CALLED')
    direct_copy = [bool]($t -match 'SMSCFG_DIRECT_COPY_WRITER_FOUND')
    load_called = [bool]($t -match 'SMSCFG_LOAD_CALLED')
    dsm_open = [bool]($t -match 'DSM_CFG_OPEN_ATTEMPTED')
    dsm_missing = [bool]($t -match 'DSM_CFG_MISSING')
    dsm_loaded = [bool]($t -match 'DSM_CFG_LOADED_TO_SMSCFG')
    load_not = [bool]($t -match 'DSM_CFG_LOAD_NOT_CALLED')
    never_init = [bool]($t -match 'SMSCFG_NEVER_INITIALIZED')
    upstream = [bool]($t -match 'SMSCFG_INITIALIZED_UPSTREAM')
    no_bootstrap = [bool]($t -match 'SMSCFG_NO_BOOTSTRAP_WRITER_OBSERVED')
    empty_gpt_fail = [bool]($t -match 'METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG')
    ptr_missing = [bool]($t -match 'SMSCFG_POINTER_PUBLICATION_MISSING')
    ptr_stable = [bool]($t -match 'SMSCFG_POINTER_STABLE')
    hook_armed = [bool]($t -match 'SMSCFG_WRITE_HOOK_ARMED')
  }
}

function Choose-Primary($flags) {
  if ($flags.write_gpt -or $flags.setbytes_349) { return 'SMSCFG_SETBYTES_349_CALLED' }
  if ($flags.write_before) { return 'SMSCFG_WRITER_PRECEDES_GAMELIST' }
  if ($flags.direct_copy) { return 'SMSCFG_DIRECT_COPY_WRITER_FOUND' }
  if ($flags.dsm_loaded) { return 'DSM_CFG_LOADED_TO_SMSCFG' }
  if ($flags.no_bootstrap -or (-not $flags.any_write)) {
    if ($flags.dsm_missing -and $flags.load_not) { return 'DSM_CFG_MISSING_NO_DEFAULT_INIT' }
    if ($flags.load_not) { return 'DSM_CFG_LOAD_NOT_CALLED' }
    return 'SMSCFG_NO_BOOTSTRAP_WRITER_OBSERVED'
  }
  if ($flags.empty_gpt_fail) { return 'METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG' }
  return 'SMSCFG_NO_BOOTSTRAP_WRITER_OBSERVED'
}

function Write-Verdict([string]$primary, $flags, [string]$modeName, $elapsed, $exitCode, $observeStop, [string]$window) {
  $md = @"
# Stage E10A-3.1j SMSCFG long-window writer provenance

- **Mode**: ``$modeName``
- **Window**: ``$window``
- **Primary verdict**: ``$primary``

## Headline

Long-window observation from DSM/cfunction boot to method0 fail (not gamelist-first alone).

## Key flags

| Flag | Value |
|------|-------|
| armed | $($flags.armed) |
| write_hook_armed | $($flags.hook_armed) |
| any_write | $($flags.any_write) |
| write_before_gamelist | $($flags.write_before) |
| write_during_gamelist | $($flags.write_during) |
| write_349 / GPT | $($flags.write_349) / $($flags.write_gpt) |
| setbytes_349 | $($flags.setbytes_349) |
| testcom_502 | $($flags.testcom_502) |
| indication_code6 | $($flags.indication6) |
| direct_copy | $($flags.direct_copy) |
| load_sms_cfg | $($flags.load_called) |
| dsm.cfg open/missing/loaded | $($flags.dsm_open) / $($flags.dsm_missing) / $($flags.dsm_loaded) |
| never_initialized | $($flags.never_init) |
| empty GPT fail | $($flags.empty_gpt_fail) |

## Artifacts

- ``reports/e10a31j_smscfg_pointer_lifetime.csv``
- ``reports/e10a31j_smscfg_long_write_trace.csv``
- ``reports/e10a31j_smscfg_writer_api_trace.csv``
- ``reports/e10a31j_dsm_cfg_io_trace.csv``
- ``reports/e10a31j_smscfg_checkpoints.csv``
- ``reports/e10a31j_window_compare.csv`` (compare mode)

elapsed=${elapsed}s exit=$exitCode observeStop=$observeStop
"@
  Set-Content -Path $verdictMd -Value $md -Encoding UTF8
  Write-Host "wrote $verdictMd"
}

function Invoke-OneWindow([string]$tag, [string]$window) {
  $runId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $arts = Get-ArtifactPaths $tag $runId
  @($arts.stdoutLog, $arts.stderrLog, $arts.ptrCsv, $arts.writeCsv, $arts.apiCsv, $arts.ioCsv, $arts.ckptCsv) |
    ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue } }
  Clear-AllInheritedCaseEnv | Out-Null
  $wl = Get-BaseWhitelist $runId $window $arts
  Write-Host "=== E10A31J case tag=$tag window=$window run_id=$runId ==="
  $r = Invoke-CaseRun $wl $arts.stdoutLog $arts.stderrLog
  $flags = Get-FlagsFromLog $arts.stdoutLog
  Publish-CanonicalArtifacts $arts
  return [pscustomobject]@{
    tag = $tag; window = $window; runId = $runId; arts = $arts; result = $r; flags = $flags
  }
}

# ---- main ----
if (-not $SkipBuild) {
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }
$exeHash = (Get-FileHash $exe -Algorithm SHA256).Hash
Write-Host "main.exe sha256=$exeHash"

Stop-E10A31JChildren

$cases = @()
if ($Mode -eq 'compare') {
  $cases += Invoke-OneWindow 'full' 'full'
  $cases += Invoke-OneWindow 'gamelist' 'gamelist'
} elseif ($Mode -eq 'gamelist_window') {
  $cases += Invoke-OneWindow 'gamelist' 'gamelist'
} else {
  # full_window / dsmcfg / writer_api share the full observation window
  $cases += Invoke-OneWindow 'full' 'full'
}

$primaryCase = $cases[0]
$flags = $primaryCase.flags
$primary = Choose-Primary $flags

# compare CSV
$lines = @('run_id,window,any_write,write_before,write_during,write_349,setbytes_349,direct_copy,load_called,dsm_open,dsm_missing,dsm_loaded,never_init,empty_gpt_fail,observe_stop,elapsed')
foreach ($c in $cases) {
  $f = $c.flags
  $lines += ("{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15}" -f `
    $c.runId, $c.window, [int]$f.any_write, [int]$f.write_before, [int]$f.write_during, [int]$f.write_349, `
    [int]$f.setbytes_349, [int]$f.direct_copy, [int]$f.load_called, [int]$f.dsm_open, [int]$f.dsm_missing, `
    [int]$f.dsm_loaded, [int]$f.never_init, [int]$f.empty_gpt_fail, $c.result.observeStop, $c.result.elapsed)
}
if ($Mode -eq 'compare' -and $cases.Count -ge 2) {
  $a = $cases[0].flags; $b = $cases[1].flags
  if ($a.any_write -and -not $b.any_write) { $primary = 'SMSCFG_WRITER_PRECEDES_GAMELIST' }
  elseif ($a.any_write -and $b.any_write) { $primary = 'SMSCFG_WRITER_DURING_GAMELIST' }
  elseif (-not $a.any_write -and -not $b.any_write) { $primary = 'SMSCFG_NO_BOOTSTRAP_WRITER_OBSERVED' }
}
Set-Content -Path $compareCsv -Value $lines -Encoding UTF8

Write-Verdict $primary $flags $Mode $primaryCase.result.elapsed $primaryCase.result.exitCode $primaryCase.result.observeStop $primaryCase.window
Write-Host "PRIMARY=$primary"
Write-Host "exe_sha256=$exeHash"
