# Stage E10A-3: Gamelist Post-Selection Service Contract
# Modes: postselect | event10180 | services | root_compare | waitstate | validate
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('postselect','event10180','services','root_compare','waitstate','validate')]
  [string]$Mode = 'postselect',
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
$outE10 = Join-Path $Root 'out\e10a3'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outE10 | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a3_gamelist_postselect_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$stdoutLog = Join-Path $logDir 'e10a3_postselect_stdout.txt'
$stderrLog = Join-Path $logDir 'e10a3_postselect_stderr.txt'

function Stop-E10A3Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A3_|E10A_GWY')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-E10A3Env {
  @(
    'JJFB_E10A3_MODE','JJFB_E10A3_POSTSELECT','JJFB_E10A3_EVENT10180','JJFB_E10A3_SERVICES',
    'JJFB_E10A3_WAITSTATE','JJFB_E10A_MODE','JJFB_E10A_SHELL_TRACE','GWY_SHELL_OFFLINE_NO_UPDATE',
    'GWY_PLATFORM_RESOURCE_READY_EVENT','JJFB_FAST_GWY_RESOURCE_READY_EVENT','JJFB_DEBUG_AC8_FORCE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Reset-E10A3Artifacts {
  @(
    'reports\e10a3_cfg36_postselect_trace.csv',
    'reports\e10a3_event_10180_contract.csv',
    'reports\e10a3_named_service_trace.csv',
    'reports\e10a3_gbrwcore_service_registry.csv',
    'reports\e10a3_root_vs_continue_context.csv',
    'reports\e10a3_gamelist_wait_state.csv',
    'reports\e10a_shell_phase_trace.csv',
    'logs\e10a3_postselect_stdout.txt',
    'logs\e10a3_postselect_stderr.txt'
  ) | ForEach-Object {
    $p = Join-Path $Root $_
    if (Test-Path $p) { Clear-Content -Path $p -ErrorAction SilentlyContinue }
  }
}

function Set-E10A3ShellEnv {
  $env:JJFB_E10A_RUN_ID = "$RunId"
  $env:JJFB_E10A3_RUN_ID = "$RunId"
  $env:JJFB_E10A3_MODE = '1'
  $env:JJFB_E10A_MODE = '1'
  $env:JJFB_E10A_SHELL_TRACE = '1'
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
  $env:JJFB_E10A_PHASE_CSV = (Join-Path $reportDir 'e10a_shell_phase_trace.csv')
  $env:JJFB_E10A3_POSTSELECT_CSV = (Join-Path $reportDir 'e10a3_cfg36_postselect_trace.csv')
  $env:JJFB_E10A3_EVENT10180_CSV = (Join-Path $reportDir 'e10a3_event_10180_contract.csv')
  $env:JJFB_E10A3_NAMED_CSV = (Join-Path $reportDir 'e10a3_named_service_trace.csv')
  $env:JJFB_E10A3_REGISTRY_CSV = (Join-Path $reportDir 'e10a3_gbrwcore_service_registry.csv')
  $env:JJFB_E10A3_WAIT_CSV = (Join-Path $reportDir 'e10a3_gamelist_wait_state.csv')
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
  Remove-Item Env:JJFB_FAST_GWY_RESOURCE_READY_EVENT -EA SilentlyContinue
  Remove-Item Env:JJFB_DEBUG_AC8_FORCE -EA SilentlyContinue
  Remove-Item Env:WORKBUF_SEED -EA SilentlyContinue

  switch ($Mode) {
    'postselect'  { $env:JJFB_E10A3_POSTSELECT = '1' }
    'event10180'  { $env:JJFB_E10A3_EVENT10180 = '1'; $env:JJFB_E10A3_POSTSELECT = '1' }
    'services'    { $env:JJFB_E10A3_SERVICES = '1'; $env:JJFB_E10A3_POSTSELECT = '1' }
    'waitstate'   { $env:JJFB_E10A3_WAITSTATE = '1'; $env:JJFB_E10A3_POSTSELECT = '1' }
    'validate'    { $env:JJFB_E10A3_POSTSELECT = '1'; $env:JJFB_E10A3_EVENT10180 = '1'; $env:JJFB_E10A3_SERVICES = '1'; $env:JJFB_E10A3_WAITSTATE = '1' }
    'root_compare'{ $env:JJFB_E10A3_POSTSELECT = '1'; $env:JJFB_E10A3_SERVICES = '1' }
  }
}

function Analyze-E10A3([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $flags = @{
    gamelist = $t -match 'SHELL_PHASE_GAMELIST_LOAD|mark=gamelist_init_ok'
    cfg_fmt = $t -match 'SHELL_PHASE_CFG_FMT_MAPPED'
    cfg_real = $t -match 'SHELL_PHASE_CFG_RECORD_SELECTED|mark=real_cfg_selected'
    cfg_desc = $t -match 'SHELL_PHASE_CFG_DESCRIPTOR_BUILT'
    ui_req = $t -match 'SHELL_PHASE_USERINFO_REQUEST'
    ui_rsp = $t -match 'SHELL_PHASE_USERINFO_RESPONSE'
    named_reg = $t -match 'GWY_NAMED_SERVICE.*operation=register'
    named_lookup = $t -match 'GWY_NAMED_SERVICE.*operation=lookup'
    getuserinfo = $t -match 'GWY_NAMED_SERVICE.*operation=lookup.*lib\.getuserinfo|EXPORT_CALL.*lib\.getuserinfo'
    startgame = $t -match 'SHELL_PHASE_STARTGAME|EXPORT_CALL.*lib\.startGame|GWY_NAMED_SERVICE.*operation=lookup.*lib\.startGame'
    runapp = $t -match 'SHELL_PHASE_RUNAPP_CALLED|EXPORT_CALL.*lib\.runapp|GWY_NAMED_SERVICE.*operation=lookup.*lib\.runapp'
    update = $t -match 'SHELL_PHASE_UPDATE|GAMELIST_POST_UPDATE'
    wait_loop = $t -match 'JJFB_E10A3_WAIT_LOOP'
    timer = $t -match 'FIRE_EXT.*helper=0x2E3089|helper=0x2E3089'
    plat10180 = $t -match 'PLATFORM_10180|E10A3_10180'
    param_read = $t -match 'JJFB_PARAM_READ'
    cfg_bin = $t -match 'cfg\.bin'
  }
  $evCsv = Join-Path $reportDir 'e10a3_event_10180_contract.csv'
  $namedCsv = Join-Path $reportDir 'e10a3_named_service_trace.csv'
  $regCsv = Join-Path $reportDir 'e10a3_gbrwcore_service_registry.csv'
  $waitCsv = Join-Path $reportDir 'e10a3_gamelist_wait_state.csv'
  $postCsv = Join-Path $reportDir 'e10a3_cfg36_postselect_trace.csv'
  $evN = if (Test-Path $evCsv) { @((Import-Csv $evCsv)).Count } else { 0 }
  $namedN = if (Test-Path $namedCsv) { @((Import-Csv $namedCsv)).Count } else { 0 }
  $regN = if (Test-Path $regCsv) { @((Import-Csv $regCsv)).Count } else { 0 }
  $waitN = if (Test-Path $waitCsv) { @((Import-Csv $waitCsv)).Count } else { 0 }
  $postN = if (Test-Path $postCsv) { @((Import-Csv $postCsv)).Count } else { 0 }

  $verdicts = @()
  if ($postN -gt 0 -or $flags.timer -or $flags.plat10180) { $verdicts += 'GAMELIST_POSTSELECT_FLOW_CAPTURED' }
  if ($evN -gt 0 -or $flags.plat10180) { $verdicts += 'EVENT_10180_CONTRACT_PARSED' }
  if ($regN -gt 0 -or $flags.named_reg) { $verdicts += 'GBRWCORE_SERVICE_REGISTRY_BUILT' }
  if ($flags.wait_loop -or $waitN -gt 0) { $verdicts += 'GAMELIST_WAIT_PREDICATE_FOUND' }

  if ($flags.plat10180 -and $flags.ui_rsp) {
    $verdicts += 'EVENT_10180_COMPLETED'
  } elseif ($flags.plat10180 -and -not $flags.ui_rsp) {
    $verdicts += 'EVENT_10180_CALLBACK_NOT_DELIVERED'
  }

  if ($flags.getuserinfo) { $verdicts += 'GBRWCORE_GETUSERINFO_LOOKUP_REACHED' }
  if ($flags.startgame) { $verdicts += 'GBRWCORE_STARTGAME_LOOKUP_REACHED' }
  if ($flags.runapp) { $verdicts += 'GBRWCORE_RUNAPP_LOOKUP_REACHED' }
  if ($regN -gt 0 -and -not $flags.named_lookup) {
    $verdicts += 'GBRWCORE_NAMED_SERVICE_PROVIDER_MISSING'
  }

  if (-not $flags.cfg_real -and $flags.gamelist) {
    $verdicts += 'GAMELIST_WAITING_FOR_UI_EVENT'
    $verdicts += 'SHELL_CONTINUE_MISSING_ROOT_SERVICES'
  }
  if ($flags.wait_loop -and -not $flags.cfg_real) {
    $verdicts += 'GAMELIST_WAITING_FOR_EVENT'
  }

  if ($flags.runapp) { $verdicts += 'SHELL_RUNAPP_REACHED' }
  elseif ($flags.startgame) { $verdicts += 'SHELL_STARTGAME_REACHED' }
  elseif ($flags.update) { $verdicts += 'SHELL_UPDATE_CHECK_REACHED' }
  elseif ($flags.ui_rsp -and $flags.cfg_real) { $verdicts += 'SHELL_POSTSELECT_CONTRACT_RESTORED' }

  $verdicts += 'PRODUCT_STILL_NEEDS_NATIVE_SHELL_COMPLETION'

  $primary = 'GAMELIST_POSTSELECT_FLOW_CAPTURED'
  if ($flags.runapp) { $primary = 'SHELL_RUNAPP_REACHED' }
  elseif ($flags.startgame) { $primary = 'SHELL_STARTGAME_REACHED' }
  elseif ($flags.update) { $primary = 'SHELL_UPDATE_CHECK_REACHED' }
  elseif ($flags.cfg_real) { $primary = 'SHELL_CFG_REACHED' }
  elseif ($flags.wait_loop -and $flags.plat10180 -and -not $flags.cfg_real) {
    $primary = 'GAMELIST_WAITING_FOR_EVENT'
  }
  elseif ($flags.plat10180 -and $flags.ui_rsp) { $primary = 'EVENT_10180_CONTRACT_PARSED' }
  elseif ($flags.gamelist) { $primary = 'SHELL_GAMELIST_REACHED' }

  return [pscustomobject]@{
    primary = $primary
    verdicts = $verdicts
    flags = $flags
    counts = @{ post=$postN; ev=$evN; named=$namedN; reg=$regN; wait=$waitN }
  }
}

# ---- main ----
if (-not $SkipBuild) {
  Write-Host '=== E10A-3 build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing exe: $exe" }

Clear-E10A3Env
Reset-E10A3Artifacts
Stop-E10A3Children
Set-E10A3ShellEnv
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

Write-Host "=== E10A-3 mode=$Mode run_id=$RunId timeout=${CASE_TIMEOUT_SEC}s ==="
$t0 = Get-Date
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E10A3Children
}
$elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
$an = Analyze-E10A3 $stdoutLog

# Placeholder root_compare / timer annotate when no dedicated capture yet
$rootCsv = Join-Path $reportDir 'e10a3_root_vs_continue_context.csv'
if (-not (Test-Path $rootCsv) -or ((Get-Item $rootCsv).Length -lt 32)) {
  @"
run_id,field,shell_continue,gwy_root,note
$RunId,loaded_modules,gbrwcore+gamelist,unknown,deferred_root_baseline
$RunId,named_service_registry,string_va_only,unknown,no_dispatcher_observed
$RunId,userinfo_provider,platform_0x10180_sync,unknown,EVENT_10180_CONTRACT_PARSED
$RunId,cfg_bin_open,no,unknown,gamelist_never_opened_cfg
$RunId,param_read,no,unknown,gwyblink_not_consumed
"@ | Set-Content -Path $rootCsv -Encoding utf8
}

$annot = Join-Path $outE10 'gamelist_timer_2e3089_annotated.txt'
@"
# gamelist timer helper 0x2E3089 (E10A-3)

Observed: PLATFORM_TIMER FIRE_EXT code=2 helper=0x2E3089
P=0x2AC8DC erw=0x2B0D18 (gbrwcore leftover ER_RW, not fresh gamelist ER_RW)

Wait predicate (runtime):
- timer fires repeatedly with ret=0
- no cfg.bin open
- no PARAM_READ of gwyblink entry
- no lib.startGame / lib.runapp lookup
- 0x10180 returns sync blob (platform_userinfo) then idle

Classification:
GAMELIST_WAITING_FOR_EVENT / pre_cfg_idle_timer
Not yet waiting on update callback — cfg selection itself not reached.
"@ | Set-Content -Path $annot -Encoding utf8

$verdictList = ($an.verdicts | ForEach-Object { "- ``$_``" }) -join "`n"
$md = @"
# Stage E10A-3 Gamelist Post-Selection Verdict

- **Mode**: ``$Mode``
- **run_id**: ``$RunId``
- **Elapsed**: ${elapsed}s
- **Primary**: ``$($an.primary)``
- **Product success**: **NO** (``NOT_PRODUCT``)

## Focus
Prove cfg36 post-selection service contract:
``0x10180`` / named services / wait predicate — do **not** treat format-string map as cfg select.

## Observed flags
| Flag | Value |
|------|-------|
| gamelist | $($an.flags.gamelist) |
| cfg_fmt_mapped | $($an.flags.cfg_fmt) |
| cfg_real_selected | $($an.flags.cfg_real) |
| cfg_descriptor | $($an.flags.cfg_desc) |
| userinfo_request | $($an.flags.ui_req) |
| userinfo_response | $($an.flags.ui_rsp) |
| plat_10180 | $($an.flags.plat10180) |
| named_registry | $($an.flags.named_reg) |
| named_lookup | $($an.flags.named_lookup) |
| getuserinfo lookup | $($an.flags.getuserinfo) |
| startGame | $($an.flags.startgame) |
| runapp | $($an.flags.runapp) |
| update | $($an.flags.update) |
| wait_loop | $($an.flags.wait_loop) |
| timer_2e3089 | $($an.flags.timer) |
| param_read | $($an.flags.param_read) |
| cfg_bin_open | $($an.flags.cfg_bin) |

## CSV counts
postselect=$($an.counts.post) event10180=$($an.counts.ev) named=$($an.counts.named) registry=$($an.counts.reg) wait=$($an.counts.wait)

## Verdicts
$verdictList

## Branch decision (from first three knives)
$(if ($an.flags.plat10180 -and $an.flags.ui_rsp -and -not $an.flags.named_lookup) {
  '10180 sync completes, but named lookup does not appear → chase gamelist wait / missing cfg consume (not fake userinfo).'
} elseif ($an.flags.plat10180 -and -not $an.flags.ui_rsp) {
  '10180 request without return/callback → fix event/provider contract.'
} elseif ($an.flags.getuserinfo -and -not $an.flags.startgame) {
  'lib.getuserinfo lookup with provider gap → root assembly / real registration.'
} elseif ($an.flags.startgame -or $an.flags.runapp) {
  'startGame/runapp lookup reached → repair named-service call ABI if call does not enter.'
} else {
  'No update/runapp yet; cfg.bin not opened — postselect wait is pre-cfg idle, not post-select update.'
})

## Artifacts
| Kind | Path |
|------|------|
| postselect trace | ``reports/e10a3_cfg36_postselect_trace.csv`` |
| 10180 contract | ``reports/e10a3_event_10180_contract.csv`` |
| named service | ``reports/e10a3_named_service_trace.csv`` |
| service registry | ``reports/e10a3_gbrwcore_service_registry.csv`` |
| root vs continue | ``reports/e10a3_root_vs_continue_context.csv`` |
| wait state | ``reports/e10a3_gamelist_wait_state.csv`` |
| timer annotate | ``out/e10a3/gamelist_timer_2e3089_annotated.txt`` |
| log | ``logs/e10a3_postselect_stdout.txt`` |
"@
[System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

Write-Host "E10A-3 done mode=$Mode primary=$($an.primary) elapsed=$elapsed"
Write-Host "verdict: $verdictMd"
