# Stage E10A-3.1l: batch method0 platform-config precondition map
# Modes: gwy_provenance | config_map | static_chain | profile_ab | validate
# Observe-first: do NOT guess gwy offsets or write unproven tags.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('gwy_provenance','config_map','static_chain','profile_ab','validate')]
  [string]$Mode = 'config_map',
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
$outDir = Join-Path $Root 'out\e10a31l'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31l_method0_config_map_verdict.md'

function Stop-E10A31LChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[C-L]_')
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
    OverlayRoot = Join-Path $RunDir ("overlay\e10a31l\{0}_{1}" -f $tag, $runId)
    stdoutLog = Join-Path $logDir ("e10a31l_{0}_stdout.txt" -f $tag)
    stderrLog = Join-Path $logDir ("e10a31l_{0}_stderr.txt" -f $tag)
    timerCsv = Join-Path $reportDir ("e10a31l_{0}_timer_binding_trace.csv" -f $tag)
    initCsv = Join-Path $reportDir ("e10a31l_{0}_init_sequence_trace.csv" -f $tag)
    histCsv = Join-Path $reportDir ("e10a31l_{0}_helper_call_history.csv" -f $tag)
    insnCsv = Join-Path $reportDir ("e10a31l_{0}_method0_instruction_trace.csv" -f $tag)
    callCsv = Join-Path $reportDir ("e10a31l_{0}_method0_call_tree.csv" -f $tag)
    provCsv = Join-Path $reportDir ("e10a31l_{0}_method0_return_provenance.csv" -f $tag)
    appinfoCsv = Join-Path $reportDir ("e10a31l_{0}_appinfo_contract.csv" -f $tag)
    metaCsv = Join-Path $reportDir ("e10a31l_{0}_package_metadata_trace.csv" -f $tag)
    bindCsv = Join-Path $reportDir ("e10a31l_{0}_appinfo_binding_trace.csv" -f $tag)
    branchCsv = Join-Path $reportDir ("e10a31l_{0}_fail_branch_trace.csv" -f $tag)
    denseCsv = Join-Path $reportDir ("e10a31l_{0}_failsite_dense_trace.csv" -f $tag)
    gCsv = Join-Path $reportDir ("e10a31l_{0}_strcmp_arg_trace.csv" -f $tag)
    hCsv = Join-Path $reportDir ("e10a31l_{0}_smscfg_trace.csv" -f $tag)
    readCsv = Join-Path $reportDir 'e10a31l_method0_smscfg_read_map.csv'
    cmpCsv = Join-Path $reportDir 'e10a31l_method0_compare_chain.csv'
    gwyCsv = Join-Path $reportDir 'e10a31l_gwy_strcmp_provenance.csv'
    manifest = Join-Path $reportDir 'e10a31l_required_platform_tags.json'
    annotated = Join-Path $outDir 'method0_config_gate_chain_annotated.txt'
  }
}

function Get-BaseWhitelist([string]$runId, $arts) {
  New-Item -ItemType Directory -Force -Path $arts.OverlayRoot | Out-Null
  New-Item -ItemType Directory -Force -Path (Join-Path $arts.OverlayRoot 'system') | Out-Null
  # NOTE: do NOT set GWY_PACKAGE_APPID/APPVER here — package metadata must come from
  # the active MRP header (diagnostic env override is opt-in only).
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
    JJFB_E10A31F_BRANCH_CSV = $arts.branchCsv
    JJFB_E10A31F_DENSE_CSV = $arts.denseCsv
    JJFB_E10A31G_MODE = '1'
    JJFB_E10A31G_CSV = $arts.gCsv
    JJFB_E10A31H_MODE = '1'
    JJFB_E10A31H_CSV = $arts.hCsv
    # SMSCFG: GPT bootstrap for observe past first gate; fingerprint pinned via cwd cfunction.ext
    GWY_SMSCFG_BOOTSTRAP = '1'
    GWY_DIAG_SMSCFG_GPT_MINIMAL = '0'
    JJFB_E10A31L_MODE = '1'
    JJFB_E10A31L_MEM_READ = '1'
    JJFB_E10A31L_COMPARE = '1'
    JJFB_E10A31L_READ_CSV = $arts.readCsv
    JJFB_E10A31L_COMPARE_CSV = $arts.cmpCsv
    JJFB_E10A31L_GWY_CSV = $arts.gwyCsv
    JJFB_E10A31L_MANIFEST_JSON = $arts.manifest
    JJFB_E10A31L_ANNOTATED = $arts.annotated
  }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  if (Test-LogHit $log 'METHOD0_CONFIG_MAP_COMPLETE|REQUIRED_PLATFORM_TAGS_MANIFEST_WRITTEN') {
    return 'OBSERVE_STOP_CONFIG_MAP_COMPLETE'
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
  Stop-E10A31LChildren
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
    Stop-E10A31LChildren
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
  @($arts.stdoutLog, $arts.stderrLog, $arts.readCsv, $arts.cmpCsv, $arts.gwyCsv, $arts.manifest) |
    ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue } }
  Clear-AllInheritedCaseEnv | Out-Null
  $wl = Get-BaseWhitelist $runId $arts
  foreach ($k in $extraEnv.Keys) { $wl[$k] = [string]$extraEnv[$k] }
  Write-Host "=== E10A31L case tag=$tag run_id=$runId ==="
  $r = Invoke-CaseRun $wl $arts.stdoutLog $arts.stderrLog
  return [pscustomobject]@{ tag = $tag; runId = $runId; arts = $arts; result = $r }
}

function Get-GwyVerdict([string]$log) {
  if (Test-LogHit $log 'GWY_COMPARE_SOURCE_SMSCFG') { return 'GWY_COMPARE_SOURCE_SMSCFG' }
  if (Test-LogHit $log 'GWY_COMPARE_SOURCE_LAUNCH_PARAM') { return 'GWY_COMPARE_SOURCE_LAUNCH_PARAM' }
  if (Test-LogHit $log 'GWY_COMPARE_SOURCE_DESCRIPTOR') { return 'GWY_COMPARE_SOURCE_DESCRIPTOR' }
  if (Test-LogHit $log 'GWY_COMPARE_SOURCE_PLATFORM_STATE') { return 'GWY_COMPARE_SOURCE_PLATFORM_STATE' }
  if (Test-LogHit $log 'GWY_COMPARE_SOURCE_UNKNOWN') { return 'GWY_COMPARE_SOURCE_UNKNOWN' }
  return 'GWY_COMPARE_SOURCE_UNKNOWN'
}

function Write-Verdict([string]$primary, [string]$modeName, [string]$body) {
  $md = @"
# Stage E10A-3.1l method0 config map

- **Mode**: ``$modeName``
- **Primary verdict**: ``$primary``

$body

## Notes

- ``original_default_recovered=false``
- Do not infer offsets from literal adjacency ``gwy\\0GPT\\0``.
- ``cfg_validate`` remains disabled until method0 returns 0.
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

Stop-E10A31LChildren

$primary = 'INCOMPLETE'
$body = ''

switch ($Mode) {
  { $_ -in @('gwy_provenance','config_map','static_chain') } {
    $c = Invoke-OneCase $Mode @{}
    $gwy = Get-GwyVerdict $c.arts.stdoutLog
    $fpPinned = Test-LogHit $c.arts.stdoutLog 'SMSCFG_PROFILE_FINGERPRINT_PINNED'
    $fpBroad = Test-LogHit $c.arts.stdoutLog 'SMSCFG_PROFILE_TOO_BROAD'
    $mapDone = Test-LogHit $c.arts.stdoutLog 'METHOD0_CONFIG_MAP_COMPLETE'
    $reads = 0
    if (Test-Path $c.arts.readCsv) {
      $reads = [Math]::Max(0, (Get-Content $c.arts.readCsv | Measure-Object -Line).Lines - 1)
    }
    $cmps = 0
    if (Test-Path $c.arts.cmpCsv) {
      $cmps = [Math]::Max(0, (Get-Content $c.arts.cmpCsv | Measure-Object -Line).Lines - 1)
    }
    if ($mapDone) { $primary = $gwy }
    else { $primary = 'CONFIG_MAP_INCOMPLETE' }
    $fp = if ($fpPinned) { 'SMSCFG_PROFILE_FINGERPRINT_PINNED' }
          elseif ($fpBroad) { 'SMSCFG_PROFILE_TOO_BROAD' }
          else { 'SMSCFG_PROFILE_UNKNOWN' }
    $body = @"
## Observe

- gwy_verdict: ``$gwy``
- fingerprint: ``$fp``
- sms_cfg reads logged: $reads
- compares logged: $cmps
- elapsed: $($c.result.elapsed)s observeStop=$($c.result.observeStop)

## Artifacts

- ``reports/e10a31l_gwy_strcmp_provenance.csv``
- ``reports/e10a31l_method0_smscfg_read_map.csv``
- ``reports/e10a31l_method0_compare_chain.csv``
- ``reports/e10a31l_required_platform_tags.json``
- ``out/e10a31l/method0_config_gate_chain_annotated.txt``

Observation only — no unproven SMSCFG tags written.
"@
  }
  'profile_ab' {
    # Case A: GPT-only (current pinned profile). Case B: apply manifest SMSCFG tags if any.
    $a = Invoke-OneCase 'profile_a' @{
      GWY_SMSCFG_BOOTSTRAP = '1'
    }
    $manifest = Join-Path $reportDir 'e10a31l_required_platform_tags.json'
    $bExtra = @{ GWY_SMSCFG_BOOTSTRAP = '1' }
    # Profile B uses same bootstrap unless a proven extra SMSCFG tag exists in manifest.
    # Extra tags are applied only via overlay dsm.cfg generation when source==sms_cfg.
    if (Test-Path $manifest) {
      $json = Get-Content $manifest -Raw | ConvertFrom-Json
      $smsTags = @($json.requirements | Where-Object { $_.source -eq 'sms_cfg' -and $_.required -eq $true })
      if ($smsTags.Count -gt 1) {
        $ov = (Get-ArtifactPaths 'profile_b' ([DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())).OverlayRoot
        # profile_b case will create its own overlay; mark intent in log via env
        $bExtra['JJFB_E10A31L_PROFILE_B'] = '1'
        $bExtra['JJFB_E10A31L_EXTRA_SMSCFG_TAGS'] = ($smsTags | ForEach-Object { "{0}:{1}" -f $_.offset, $_.bytes_hex }) -join ','
      }
    }
    $b = Invoke-OneCase 'profile_b' $bExtra
    $lines = @(
      'case,ret0,gwy_verdict,gpt_pass,elapsed'
      ('A,,{0},,{1}' -f (Get-GwyVerdict $a.arts.stdoutLog), $a.result.elapsed)
      ('B,,{0},,{1}' -f (Get-GwyVerdict $b.arts.stdoutLog), $b.result.elapsed)
    )
    $cmpPath = Join-Path $reportDir 'e10a31l_profile_ab_compare.csv'
    Set-Content -Path $cmpPath -Value $lines -Encoding UTF8
    $gwy = Get-GwyVerdict $b.arts.stdoutLog
    if ($gwy -ne 'GWY_COMPARE_SOURCE_SMSCFG') {
      $primary = 'SMSCFG_PROFILE_NOT_RESPONSIBLE_FOR_GWY_GATE'
    } else {
      $primary = 'GWY_REQUIREMENT_PUBLICATION_WRONG'
    }
    $body = @"
## profile_ab

- Case A/B compare: ``reports/e10a31l_profile_ab_compare.csv``
- gwy_verdict_B: ``$gwy``
- Extra SMSCFG tags are applied only when manifest proves ``source=sms_cfg``.
"@
  }
  'validate' {
    $c = Invoke-OneCase 'validate' @{
      JJFB_E10A31_MODE = '1'
      JJFB_E10A31_CFG_GATE = '1'
    }
    if (Test-LogHit $c.arts.stdoutLog 'ret0=0|method0_return ret=0') {
      $primary = 'GAMELIST_METHOD0_RETURN_ZERO'
    } else {
      $primary = 'METHOD0_STILL_NONZERO'
    }
    $body = @"
## validate

- elapsed=$($c.result.elapsed)s
- cfg gate only meaningful after method0==0
"@
  }
}

Write-Verdict $primary $Mode $body
Write-Host "exe_sha256=$exeHash"
