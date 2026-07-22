# Stage E10A-3.1e: Package-scoped appInfo metadata binding
# Modes: metadata | binding | ab_compare | method0_validate | read_proof
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('metadata','binding','ab_compare','method0_validate','read_proof')]
  [string]$Mode = 'metadata',
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
$outDir = Join-Path $Root 'out\e10a31e'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31e_package_appinfo_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31e\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'e10a31e_package_appinfo_stdout.txt'
$stderrLog = Join-Path $logDir 'e10a31e_package_appinfo_stderr.txt'

$metaCsv = Join-Path $reportDir 'e10a31e_package_metadata_trace.csv'
$bindCsv = Join-Path $reportDir 'e10a31e_appinfo_binding_trace.csv'
$ownerCsv = Join-Path $reportDir 'e10a31e_appinfo_owner_validation.csv'
$globalsCsv = Join-Path $reportDir 'e10a31e_gamelist_globals.csv'
$abCsv = Join-Path $reportDir 'e10a31e_appinfo_ab_compare.csv'
$readCsv = Join-Path $reportDir 'e10a31e_appinfo_read_proof.csv'
$histCsv = Join-Path $reportDir 'e10a31d_helper_call_history.csv'
$insnCsv = Join-Path $reportDir 'e10a31d_method0_instruction_trace.csv'
$callCsv = Join-Path $reportDir 'e10a31d_method0_call_tree.csv'
$provCsv = Join-Path $reportDir 'e10a31d_method0_return_provenance.csv'
$appinfoCsv = Join-Path $reportDir 'e10a31d_appinfo_contract.csv'
$abiCsv = Join-Path $reportDir 'e10a31d_helper_abi_compare.csv'
$timerCsv = Join-Path $reportDir 'e10a31_timer_binding_trace.csv'
$initCsv = Join-Path $reportDir 'e10a31c_init_sequence_trace.csv'

function Stop-E10A31EChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31E_|E10A31D_|E10A31C_')
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

function Reset-E10A31EArtifacts {
  @(
    $metaCsv, $bindCsv, $ownerCsv, $globalsCsv, $abCsv, $readCsv,
    $stdoutLog, $stderrLog
  ) | ForEach-Object {
    if (Test-Path $_) { Remove-Item -Path $_ -Force -ErrorAction SilentlyContinue }
  }
}

function Write-EnvManifest([string[]]$cleared, $whitelist) {
  $csv = Join-Path $reportDir 'e10a31e_environment_manifest.csv'
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
  $csv = Join-Path $reportDir 'e10a31e_runtime_manifest.csv'
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

function Get-BaseWhitelist {
  New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
  $wl = [ordered]@{
    JJFB_E10A_RUN_ID = "$RunId"
    JJFB_E10A31_RUN_ID = "$RunId"
    JJFB_E10A31C_RUN_ID = "$RunId"
    JJFB_E10A31D_RUN_ID = "$RunId"
    JJFB_E10A31E_RUN_ID = "$RunId"
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
    # Present but NOT used for product appInfo (diagnostic compare only).
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
    JJFB_E10A31D_APPINFO = '1'
    JJFB_E10A31D_HIST_CSV = $histCsv
    JJFB_E10A31D_INSN_CSV = $insnCsv
    JJFB_E10A31D_CALL_CSV = $callCsv
    JJFB_E10A31D_PROV_CSV = $provCsv
    JJFB_E10A31D_APPINFO_CSV = $appinfoCsv
    JJFB_E10A31D_ABI_CSV = $abiCsv
    JJFB_E10A31E_MODE = '1'
    JJFB_E10A31E_META_CSV = $metaCsv
    JJFB_E10A31E_BIND_CSV = $bindCsv
    JJFB_E10A31E_OWNER_CSV = $ownerCsv
    JJFB_E10A31E_GLOBALS_CSV = $globalsCsv
    JJFB_E10A31E_AB_CSV = $abCsv
    JJFB_E10A31E_READ_CSV = $readCsv
  }
  return $wl
}

function Apply-Whitelist($wl) {
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  if (Test-LogHit $log 'MRC_INIT_RETURN_PROVENANCE_COMPLETE') {
    return 'OBSERVE_STOP_METHOD0_PROVENANCE'
  }
  if (Test-LogHit $log 'GAMELIST_METHOD0_RETURN_ZERO|delivered ret6=0 ret8=0 ret0=0') {
    return 'OBSERVE_STOP_METHOD0_OK'
  }
  if (Test-LogHit $log 'GAMELIST_INIT_METHOD0_FAILED|delivered ret6=0 ret8=0 ret0=-1') {
    return 'OBSERVE_STOP_METHOD0_FAILED'
  }
  if (Test-LogHit $log 'UC_ERR|mem_fault|MEM_FAULT|FETCH_UNMAPPED') {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Invoke-CaseRun([hashtable]$wl, [string]$caseStdout, [string]$caseStderr) {
  Apply-Whitelist $wl
  Stop-E10A31EChildren
  $t0 = Get-Date
  $killedByRunner = $false
  $observeStop = $null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $caseStdout -RedirectStandardError $caseStderr
  $deadline = $t0.AddSeconds($OUTER_KILL_SEC)
  while (-not $p.HasExited) {
    if ((Get-Date) -ge $deadline) { $killedByRunner = $true; break }
    $obs = Get-ObserveStopReason $caseStdout
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
    Stop-E10A31EChildren
    Start-Sleep -Milliseconds 500
  }
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = $elapsed
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
    killed = $killedByRunner
  }
}

function Analyze-E10A31E([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $failPc = 0
  $provPath = Join-Path $reportDir 'e10a31d_method0_return_provenance.csv'
  if (Test-Path $provPath) {
    $rows = Get-Content $provPath -EA SilentlyContinue
    if ($rows -and $rows.Count -ge 2) {
      $cols = $rows[1].Split(',')
      if ($cols.Count -ge 3 -and $cols[2] -match '0x([0-9A-Fa-f]+)') {
        $failPc = [Convert]::ToUInt32($Matches[1], 16)
      }
    }
  }
  if ($failPc -eq 0 -and $t -match 'failure_pc=0x([0-9A-Fa-f]+)') {
    $failPc = [Convert]::ToUInt32($Matches[1], 16)
  }
  $stillSame = ($failPc -eq 0x2E1C24) -or ($failPc -eq 0 -and $t -match '0x2E1C24')
  $newFail = ($failPc -ne 0 -and $failPc -ne 0x2E1C24)

  $flags = @{
    meta_ok = $t -match 'ACTIVE_PACKAGE_METADATA_RESOLVED|GWY_PKG_META\] activate=OK[^\n]*gamelist'
    bind_ok = $t -match 'APPINFO_BOUND_TO_ACTIVE_PACKAGE_METADATA|\[GWY_APPINFO_BIND\][^\n]*source=MRP_HEADER[^\n]*appver=1006|\[GWY_APPINFO_BIND\][^\n]*gamelist[^\n]*appver='
    owner_match = $t -match 'APPINFO_OWNER_MATCH'
    owner_wrong = $t -match 'APPINFO_OWNER_WRONG_PACKAGE'
    owner_stale = $t -match 'APPINFO_OWNER_STALE_GENERATION'
    owner_missing = $t -match 'APPINFO_METADATA_NOT_AVAILABLE|ACTIVE_PACKAGE_METADATA_BINDING_BROKEN'
    mr_ver = $t -match 'GAMELIST_MR_VERSION_PUBLISHED'
    appinfo_pub = $t -match 'GAMELIST_APPINFO_PUBLISHED'
    meta_match = $t -match 'APPINFO_PACKAGE_METADATA_MATCH'
    method0_ok = $t -match 'GAMELIST_METHOD0_RETURN_ZERO|delivered ret6=0 ret8=0 ret0=0'
    method0_neg = $t -match 'delivered ret6=0 ret8=0 ret0=-1|GAMELIST_INIT_METHOD0_FAILED'
    gate_passed = $newFail -and ($t -match 'delivered ret6=0 ret8=0 ret0=-1')
    still_2e1c24 = $stillSame -and ($t -match 'delivered ret6=0 ret8=0 ret0=-1')
    diag_override = $t -match 'DIAGNOSTIC_OVERRIDE'
    read_id = $t -match 'METHOD0_READS_APPINFO_ID'
    read_ver = $t -match 'METHOD0_READS_APPINFO_VERSION'
    read_none = $t -match 'METHOD0_DOES_NOT_READ_APPINFO_BEFORE_FAILURE'
    appinfo_ver_1006 = $t -match '\[GWY_APPINFO_BIND\][^\n]*appver=1006|after_code8[^\n]*ver=1006|phase=code8[^\n]*ver=1006'
    appinfo_ver_12 = $t -match '\[GWY_APPINFO_BIND\][^\n]*appver=12|DIAGNOSTIC_OVERRIDE[^\n]*appver=12'
    fail_pc = $failPc
  }

  # Require gamelist owner for true metadata success
  $gamelistBound = $t -match '\[GWY_APPINFO_BIND\][^\n]*package=gwy/gamelist\.mrp[^\n]*appver=1006'
  if ($gamelistBound) { $flags.bind_ok = $true; $flags.appinfo_ver_1006 = $true }

  $verdicts = @()
  if ($flags.meta_ok) { $verdicts += 'ACTIVE_PACKAGE_METADATA_RESOLVED' }
  if ($flags.bind_ok -and $gamelistBound) { $verdicts += 'APPINFO_BOUND_TO_ACTIVE_PACKAGE_METADATA' }
  if ($flags.owner_match) { $verdicts += 'APPINFO_OWNER_MATCH' }
  if ($flags.owner_wrong) { $verdicts += 'APPINFO_OWNER_WRONG_PACKAGE' }
  if ($flags.owner_stale) { $verdicts += 'APPINFO_OWNER_STALE_GENERATION' }
  if ($flags.owner_missing) { $verdicts += 'ACTIVE_PACKAGE_METADATA_BINDING_BROKEN' }
  if ($flags.mr_ver) { $verdicts += 'GAMELIST_MR_VERSION_PUBLISHED' }
  if ($flags.appinfo_pub) { $verdicts += 'GAMELIST_APPINFO_PUBLISHED' }
  if ($flags.meta_match -and $flags.appinfo_ver_1006) { $verdicts += 'APPINFO_PACKAGE_METADATA_MATCH' }
  if ($flags.method0_ok) {
    $verdicts += 'GAMELIST_METHOD0_RETURN_ZERO'
    $verdicts += 'MRC_INIT_ACCEPTS_PACKAGE_METADATA'
    $verdicts += 'GAMELIST_INIT_SEQUENCE_COMPLETE'
  }
  if ($flags.gate_passed) {
    $verdicts += 'APPINFO_VERSION_GATE_PASSED'
    $verdicts += 'MRC_INIT_NEXT_PRECONDITION_FOUND'
  }
  if ($flags.method0_neg -and $flags.still_2e1c24 -and -not $flags.gate_passed -and $flags.appinfo_ver_1006) {
    $verdicts += 'APPINFO_VERSION_MISMATCH_NOT_CAUSAL'
    $verdicts += 'MRC_INIT_INTERNAL_PRECONDITION_STILL_AT_2E1C24'
  }
  if ($flags.read_id) { $verdicts += 'METHOD0_READS_APPINFO_ID' }
  if ($flags.read_ver) { $verdicts += 'METHOD0_READS_APPINFO_VERSION' }
  if ($flags.read_none) { $verdicts += 'METHOD0_DOES_NOT_READ_APPINFO_BEFORE_FAILURE' }

  $primary = 'E10A31E_INSUFFICIENT_EVIDENCE'
  if ($flags.owner_wrong -or ($flags.owner_missing -and -not $gamelistBound)) {
    $primary = 'ACTIVE_PACKAGE_METADATA_BINDING_BROKEN'
  }
  elseif ($flags.method0_ok) { $primary = 'GAMELIST_METHOD0_RETURN_ZERO' }
  elseif ($flags.gate_passed) { $primary = 'APPINFO_VERSION_GATE_PASSED' }
  elseif ($flags.appinfo_ver_1006 -and $flags.method0_neg -and $flags.still_2e1c24) {
    $primary = 'APPINFO_VERSION_MISMATCH_NOT_CAUSAL'
  }
  elseif ($flags.appinfo_ver_1006 -and $flags.owner_match) { $primary = 'APPINFO_PACKAGE_METADATA_MATCH' }
  elseif ($gamelistBound) { $primary = 'APPINFO_BOUND_TO_ACTIVE_PACKAGE_METADATA' }

  return [pscustomobject]@{ primary = $primary; verdicts = $verdicts; flags = $flags }
}

# ---- main ----
if (-not $SkipBuild) {
  Write-Host '=== E10A-3.1e build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

Stop-E10A31EChildren
$cleared = Clear-AllInheritedCaseEnv
Reset-E10A31EArtifacts

$allResults = @()

if ($Mode -eq 'ab_compare') {
  Write-Host "=== E10A-3.1e A/B compare run_id=$RunId ==="

  # Case A — legacy env ver=12 (diagnostic force)
  $wlA = Get-BaseWhitelist
  $wlA['JJFB_E10A31E_FORCE_ENV_APPINFO'] = '1'
  $wlA['JJFB_E10A31E_AB_COMPARE'] = '1'
  $wlA['JJFB_E10A31E_AB_CASE'] = 'legacy_env_ver12'
  $wlA['JJFB_E10A31E_BINDING'] = '1'
  $wlA['JJFB_E10A31E_METADATA'] = '1'
  $stdoutA = Join-Path $logDir 'e10a31e_ab_case_a_stdout.txt'
  $stderrA = Join-Path $logDir 'e10a31e_ab_case_a_stderr.txt'
  Write-Host '--- Case A: FORCE_ENV appver=12 ---'
  $rA = Invoke-CaseRun $wlA $stdoutA $stderrA
  $anA = Analyze-E10A31E $stdoutA
  $allResults += [pscustomobject]@{ case = 'A_legacy'; result = $rA; analysis = $anA }

  # Case B — package metadata (product path)
  Clear-AllInheritedCaseEnv | Out-Null
  $RunIdB = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $OverlayRoot = Join-Path $RunDir ("overlay\e10a31e\{0}" -f $RunIdB)
  $wlB = Get-BaseWhitelist
  $wlB['JJFB_E10A_RUN_ID'] = "$RunIdB"
  $wlB['JJFB_E10A31E_RUN_ID'] = "$RunIdB"
  $wlB['JJFB_E10A31D_RUN_ID'] = "$RunIdB"
  $wlB['JJFB_E10A31E_AB_COMPARE'] = '1'
  $wlB['JJFB_E10A31E_AB_CASE'] = 'package_metadata'
  $wlB['JJFB_E10A31E_BINDING'] = '1'
  $wlB['JJFB_E10A31E_METADATA'] = '1'
  $wlB['JJFB_E10A31D_METHOD0_TRACE'] = '1'
  # Do NOT set FORCE_ENV — product MRP_HEADER path
  $stdoutB = Join-Path $logDir 'e10a31e_ab_case_b_stdout.txt'
  $stderrB = Join-Path $logDir 'e10a31e_ab_case_b_stderr.txt'
  Write-Host '--- Case B: package metadata appver from MRP header ---'
  $rB = Invoke-CaseRun $wlB $stdoutB $stderrB
  $anB = Analyze-E10A31E $stdoutB
  $allResults += [pscustomobject]@{ case = 'B_metadata'; result = $rB; analysis = $anB }

  # Merge primary stdout from Case B (product path)
  Copy-Item -Path $stdoutB -Destination $stdoutLog -Force
  Copy-Item -Path $stderrB -Destination $stderrLog -Force -EA SilentlyContinue
  Write-EnvManifest $cleared $wlB
  Write-RuntimeManifest
  $an = $anB
  $elapsed = $rB.elapsed
  $exitCode = $rB.exitCode
  $observeStop = $rB.observeStop
} else {
  $wl = Get-BaseWhitelist
  switch ($Mode) {
    'metadata' {
      $wl['JJFB_E10A31E_METADATA'] = '1'
      $wl['JJFB_E10A31E_BINDING'] = '1'
    }
    'binding' {
      $wl['JJFB_E10A31E_METADATA'] = '1'
      $wl['JJFB_E10A31E_BINDING'] = '1'
    }
    'method0_validate' {
      $wl['JJFB_E10A31E_METADATA'] = '1'
      $wl['JJFB_E10A31E_BINDING'] = '1'
      $wl['JJFB_E10A31E_METHOD0'] = '1'
      $wl['JJFB_E10A31D_METHOD0_TRACE'] = '1'
    }
    'read_proof' {
      $wl['JJFB_E10A31E_METADATA'] = '1'
      $wl['JJFB_E10A31E_BINDING'] = '1'
      $wl['JJFB_E10A31E_METHOD0'] = '1'
      $wl['JJFB_E10A31E_READ_PROOF'] = '1'
      $wl['JJFB_E10A31D_METHOD0_TRACE'] = '1'
    }
  }
  Write-EnvManifest $cleared $wl
  Write-RuntimeManifest
  Write-Host "=== E10A-3.1e mode=$Mode run_id=$RunId seconds=$Seconds ==="
  Write-Host "main.exe sha256=$(Get-FileSha256 $exe)"
  $r = Invoke-CaseRun $wl $stdoutLog $stderrLog
  $an = Analyze-E10A31E $stdoutLog
  $elapsed = $r.elapsed
  $exitCode = $r.exitCode
  $observeStop = $r.observeStop
}

# Ensure CSV stubs exist even if process died early
foreach ($pair in @(
  @{p=$metaCsv; h='run_id,phase,package,package_id,generation,archive,source,appid,appver,internal_name,valid'},
  @{p=$bindCsv; h='run_id,package,package_id,generation,archive,source,appid,appver,guest_ptr,sidName,ram,binding_valid'},
  @{p=$ownerCsv; h='run_id,verdict,active_package,active_primary,meta_package,meta_archive,helper,helper_module_id,appinfo,meta_package_id,meta_generation'},
  @{p=$globalsCsv; h='run_id,phase,erw,slot,value,expected,appinfo,id,ver,sid,ram,ret,note'},
  @{p=$abCsv; h='run_id,case,appid,appver,ret6,ret8,ret0,fail_pc,fail_class,platform_api_count,cfg_gate'},
  @{p=$readCsv; h='run_id,seq,pc,addr,value,field,enclosing_function,branch_relation'}
)) {
  if (-not (Test-Path $pair.p)) {
    Set-Content -Path $pair.p -Value $pair.h -Encoding UTF8
  }
}

$verdictList = ($an.verdicts | ForEach-Object { "- ``$_``" }) -join "`n"
$abSection = ''
if ($Mode -eq 'ab_compare' -and $allResults.Count -ge 2) {
  $abSection = @"

## A/B compare

| Case | appInfo source | primary | method0 |
|------|----------------|---------|---------|
| A legacy | env ver=12 (FORCE) | $($allResults[0].analysis.primary) | neg=$($allResults[0].analysis.flags.method0_neg) ok=$($allResults[0].analysis.flags.method0_ok) |
| B metadata | MRP header | $($allResults[1].analysis.primary) | neg=$($allResults[1].analysis.flags.method0_neg) ok=$($allResults[1].analysis.flags.method0_ok) |

"@
}

$md = @"
# Stage E10A-3.1e Package-scoped appInfo Metadata Binding Verdict

- **Mode**: ``$Mode``
- **run_id**: ``$RunId``
- **elapsed_sec**: $elapsed
- **exit_code**: $exitCode
- **observe_stop**: ``$observeStop``
- **Primary verdict**: ``$($an.primary)``

## Milestones / verdicts

$verdictList
$abSection
## Key observations

| Check | Result |
|-------|--------|
| metadata resolved | $($an.flags.meta_ok) |
| binding MRP_HEADER | $($an.flags.bind_ok) |
| owner match | $($an.flags.owner_match) |
| appInfo ver=1006 | $($an.flags.appinfo_ver_1006) |
| APPINFO_PACKAGE_METADATA_MATCH | $($an.flags.meta_match) |
| method0 ret0 | $($an.flags.method0_ok) |
| still @ 0x2E1C24 | $($an.flags.still_2e1c24) |
| version gate passed | $($an.flags.gate_passed) |

## Artifacts

- ``reports/e10a31e_package_metadata_trace.csv``
- ``reports/e10a31e_appinfo_binding_trace.csv``
- ``reports/e10a31e_appinfo_owner_validation.csv``
- ``reports/e10a31e_gamelist_globals.csv``
- ``reports/e10a31e_appinfo_ab_compare.csv``
- ``reports/e10a31e_appinfo_read_proof.csv``
- ``logs/e10a31e_package_appinfo_stdout.txt``

## Notes

- Product appInfo id/ver come from the **active package MRP header**, not ``GWY_PACKAGE_APPVER``.
- Env override requires ``JJFB_E10A31E_FORCE_ENV_APPINFO=1`` and prints ``DIAGNOSTIC_OVERRIDE``.
- sidName/ram left 0; no method0 force; no cfg gate in this stage.
"@

Set-Content -Path $verdictMd -Value $md -Encoding UTF8
Write-Host "=== E10A-3.1e done primary=$($an.primary) log=$stdoutLog ==="
Write-Host "verdict: $verdictMd"
