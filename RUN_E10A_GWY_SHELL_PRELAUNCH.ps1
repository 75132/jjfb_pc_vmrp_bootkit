# Stage E10A-Fix: GWY shell launch-chain reconstruction (not jjfb AC8 event guessing)
# Modes: parse | shell_trace | no_update | compare | classify
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('parse','shell_trace','no_update','compare','classify','direct','shell','badpath','cfg','abi')]
  [string]$Mode = 'shell_trace',
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
$shotDir = Join-Path $Root 'screenshots'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $shotDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a_gwy_shell_prelaunch_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()

function Stop-E10Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      # Do not kill the current PowerShell process (self-termination).
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A_GWY|stage_e_product')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-E10Env {
  @(
    'JJFB_E10A_MODE','JJFB_E10A_SHELL_TRACE','JJFB_E10A_GWY_SHELL_PRELAUNCH',
    'GWY_SHELL_OFFLINE_NO_UPDATE','GWY_PLATFORM_RESOURCE_READY_EVENT',
    'JJFB_FAST_GWY_RESOURCE_READY_EVENT','JJFB_DEBUG_AC8_FORCE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Reset-E10AArtifacts {
  @(
    'reports\e10a_shell_vfs_trace.csv',
    'reports\e10a_shell_vfs_badpath_trace.csv',
    'reports\e10a_gamelist_cfg_runtime_trace.csv',
    'reports\e10a_shell_phase_trace.csv',
    'reports\e10a_shell_vs_direct_ac8_compare.csv',
    'reports\e10a_shell_event_loop_trace.csv',
    'reports\e10a_shell_update_contract_trace.csv',
    'reports\e10a_shell_ac8_trace.csv',
    'logs\e10a_shell_trace_stdout.txt',
    'logs\e10a_shell_badpath_stdout.txt'
  ) | ForEach-Object {
    $p = Join-Path $Root $_
    if (Test-Path $p) { Clear-Content -Path $p -ErrorAction SilentlyContinue }
  }
}

function Set-E10ATraceEnv {
  $env:JJFB_E10A_RUN_ID = "$RunId"
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
  $env:JJFB_E10A_VFS_CSV = (Join-Path $reportDir 'e10a_shell_vfs_trace.csv')
  $env:JJFB_E10A_VFS_BADPATH_CSV = (Join-Path $reportDir 'e10a_shell_vfs_badpath_trace.csv')
  $env:JJFB_E10A_CFG_RUNTIME_CSV = (Join-Path $reportDir 'e10a_gamelist_cfg_runtime_trace.csv')
  $env:JJFB_E10A_EVENT_CSV = (Join-Path $reportDir 'e10a_shell_event_loop_trace.csv')
  $env:JJFB_E10A_UPDATE_CSV = (Join-Path $reportDir 'e10a_shell_update_contract_trace.csv')
  $env:JJFB_E10A_CSV = (Join-Path $reportDir 'e10a_shell_ac8_trace.csv')
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
  Remove-Item Env:GWY_PLATFORM_RESOURCE_READY_EVENT -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_GWY_RESOURCE_READY_EVENT -EA SilentlyContinue
}

function Set-E10ADirectEnv {
  Set-E10ATraceEnv
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
}

function Set-E10AShellEnv([switch]$OfflineNoUpdate) {
  Set-E10ATraceEnv
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
  if ($OfflineNoUpdate) { $env:GWY_SHELL_OFFLINE_NO_UPDATE = '1' }
}

function Analyze-E10A([string]$log, [string]$launchPath) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $phaseCsv = Join-Path $reportDir 'e10a_shell_phase_trace.csv'
  $vfsCsv = Join-Path $reportDir 'e10a_shell_vfs_trace.csv'
  $badCsv = Join-Path $reportDir 'e10a_shell_vfs_badpath_trace.csv'
  $phaseRows = @()
  $vfsRows = @()
  $badRows = @()
  if (Test-Path $phaseCsv) {
    $phaseRows = Import-Csv $phaseCsv | Where-Object { $_.run_id -eq "$RunId" }
  }
  if (Test-Path $vfsCsv) {
    $vfsRows = Import-Csv $vfsCsv | Where-Object { $_.run_id -eq "$RunId" }
  }
  if (Test-Path $badCsv) {
    $badRows = Import-Csv $badCsv | Where-Object { $_.run_id -eq "$RunId" }
  }
  $phaseText = ($phaseRows.phase -join "`n") + "`n" + $t
  $phases = @{
    gbrw = $phaseText -match 'SHELL_PHASE_GBRWCORE_START'
    cont = $phaseText -match 'SHELL_PHASE_GBRWCORE_CONTINUE'
    gl = $phaseText -match 'SHELL_PHASE_GAMELIST_LOAD'
    cfg = $phaseText -match 'SHELL_PHASE_CFG_RECORD_SELECTED'
    upd = $phaseText -match 'SHELL_PHASE_UPDATE'
    runapp = $phaseText -match 'SHELL_PHASE_RUNAPP_CALLED'
    jjfb = $phaseText -match 'SHELL_PHASE_JJFB_MR_START'
    splash = $phaseText -match 'SHELL_PHASE_SPLASH_ENTER_2EF86C'
    ac8nz = $phaseText -match 'SHELL_AC8_NONZERO|AC8=0x[1-9A-Fa-f]'
  }
  $vfsMiss = @($vfsRows | Where-Object { $_.op -eq 'miss' }).Count -gt 0
  $badpath = @($badRows).Count -gt 0
  $originFound = $false
  $argWrong = $false
  foreach ($br in $badRows) {
    if ($br.note -match 'SHELL_VFS_BADPATH_ORIGIN_FOUND') { $originFound = $true }
    if ($br.note -match 'SHELL_FILE_API_ARG_REGISTER_WRONG') { $argWrong = $true }
  }
  $cls = 'PRODUCT_STILL_NEEDS_GWY_SHELL_CHAIN'
  if ($phases.splash -and $phases.ac8nz -and $launchPath -match 'shell') { $cls = 'SHELL_AC8_NONZERO_AT_SPLASH' }
  elseif ($phases.splash -and -not $phases.ac8nz -and $launchPath -match 'shell') { $cls = 'SHELL_AC8_ZERO_AT_SPLASH' }
  elseif ($phases.splash -and -not $phases.ac8nz -and $launchPath -match 'direct') { $cls = 'DIRECT_PATH_AC8_ZERO' }
  elseif ($phases.runapp) { $cls = 'SHELL_RUNAPP_REACHED' }
  elseif ($phases.upd) { $cls = 'SHELL_POST_UPDATE_REACHED' }
  elseif ($phases.cfg) { $cls = 'SHELL_CFG_REACHED' }
  elseif ($argWrong) { $cls = 'SHELL_FILE_API_ARG_REGISTER_WRONG' }
  elseif ($originFound -and $phases.gl -and $badpath) { $cls = 'SHELL_VFS_BADPATH_ORIGIN_FOUND' }
  elseif ($phases.gl -and $badpath) { $cls = 'SHELL_CHAIN_INCOMPLETE_BADPATH' }
  elseif ($phases.gl) { $cls = 'SHELL_GAMELIST_REACHED' }
  elseif ($phases.cont) { $cls = 'SHELL_GBRWCORE_CONTINUED' }
  elseif ($phases.gbrw -and -not $phases.cont) { $cls = 'SHELL_CHAIN_INCOMPLETE' }
  elseif ($vfsMiss) { $cls = 'SHELL_BLOCKED_BY_VFS' }
  return [pscustomobject]@{ class = $cls; launch_path = $launchPath; phases = $phases; vfs_miss = [bool]$vfsMiss; badpath = [bool]$badpath; origin = [bool]$originFound }
}

function Invoke-ParseOnly {
  py -3 (Join-Path $Root 'tools\e10a_dump_gwy_shell_contract.py') | Out-Host
  if ($LASTEXITCODE -ne 0) { throw 'e10a_dump_gwy_shell_contract failed' }
  return [pscustomobject]@{ class = 'GWY_SHELL_CONTRACT_PARSED' }
}

function Invoke-E10ACase([string]$CaseName, [scriptblock]$EnvFn) {
  $caseLog = Join-Path $logDir "e10a_${CaseName}_stdout.txt"
  $caseErr = Join-Path $logDir "e10a_${CaseName}_stderr.txt"
  Clear-E10Env
  Reset-E10AArtifacts
  Stop-E10Children
  & $EnvFn
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  Write-Host "=== E10A case=$CaseName launch_path=$($env:JJFB_LAUNCH_PATH) timeout=${CASE_TIMEOUT_SEC}s ==="
  $t0 = Get-Date
  if ($CaseName -eq 'direct') {
    $argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
      '-Target','gwy/jjfb.mrp','-Seconds',"$CASE_TIMEOUT_SEC")
    if ($SkipBuild) { $argList += '-SkipBuild' }
    $p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -WorkingDirectory $Root -PassThru `
      -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
  } else {
    $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
      -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
  }
  if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E10Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ($CaseName -eq 'direct' -and (Test-Path $src) -and
      (Select-String -Path $src -Pattern 'JJFB_E10A_' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }
  Copy-Item -Force $caseLog (Join-Path $logDir 'e10a_shell_trace_stdout.txt') -EA SilentlyContinue
  if ($CaseName -match 'shell|badpath|cfg|abi') {
    Copy-Item -Force $caseLog (Join-Path $logDir 'e10a_shell_badpath_stdout.txt') -EA SilentlyContinue
  }
  $an = Analyze-E10A $caseLog $env:JJFB_LAUNCH_PATH
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
  Write-Host "== E10A_CASE_DONE name=$CaseName verdict=$($an.class) elapsed=$elapsed"
  return [pscustomobject]@{ case = $CaseName; elapsed = $elapsed; analysis = $an; log = $caseLog }
}

# ---- main ----
if ($Mode -ne 'parse' -and -not $SkipBuild) {
  Write-Host '=== E10A build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}

$results = @()
switch ($Mode) {
  'parse' {
    $results += Invoke-ParseOnly
  }
  'shell_trace' { $results += Invoke-E10ACase 'shell' { Set-E10AShellEnv } }
  'badpath'     { $results += Invoke-E10ACase 'badpath' { Set-E10AShellEnv } }
  'cfg'         { $results += Invoke-E10ACase 'cfg' { Set-E10AShellEnv } }
  'abi'         { $results += Invoke-E10ACase 'abi' { Set-E10AShellEnv } }
  'no_update'   { $results += Invoke-E10ACase 'no_update' { Set-E10AShellEnv -OfflineNoUpdate } }
  'direct'      { $results += Invoke-E10ACase 'direct' { Set-E10ADirectEnv } }
  'shell'       { $results += Invoke-E10ACase 'shell' { Set-E10AShellEnv } }
  'compare' {
    if (-not (Test-Path (Join-Path $reportDir 'e10a_gwy_shell_inventory.csv'))) { Invoke-ParseOnly | Out-Null }
    $results += Invoke-E10ACase 'direct' { Set-E10ADirectEnv }
    $results += Invoke-E10ACase 'shell' { Set-E10AShellEnv }
  }
  'classify' {
    if (-not (Test-Path (Join-Path $reportDir 'e10a_gwy_shell_inventory.csv'))) { Invoke-ParseOnly | Out-Null }
    $results += Invoke-E10ACase 'shell' { Set-E10AShellEnv -OfflineNoUpdate }
  }
}

$primary = 'GWY_SHELL_CONTRACT_PARSED'
if ($results.Count -gt 0) {
  $last = $results[-1]
  if ($last.analysis) { $primary = $last.analysis.class }
  elseif ($last.class) { $primary = $last.class }
}
$md = @"
# Stage E10A-Fix GWY Shell Prelaunch Verdict

- **Mode**: $Mode
- **Primary**: ``$primary``
- **Product success**: **NO** (``NOT_PRODUCT``)

## Focus
Reconstruct **gbrwcore -> gamelist -> cfg36 -> update/no-update -> runapp -> jjfb -> splash**.
Do **not** treat robotol resource-ready evt as shell contract.

## Cases
$(if ($results.analysis) {
  ($results | ForEach-Object {
    if ($_.analysis) {
      "- **$($_.case)** ($($_.elapsed)s): ``$($_.analysis.class)`` gbrw=$($_.analysis.phases.gbrw) continue=$($_.analysis.phases.cont) gamelist=$($_.analysis.phases.gl) runapp=$($_.analysis.phases.runapp) splash=$($_.analysis.phases.splash) ac8_nz=$($_.analysis.phases.ac8nz)"
    } else { "- **parse**: ``$($_.class)``" }
  }) -join "`n"
} else { "- parse only" })

## Artifacts
| Kind | Path |
|------|------|
| Shell inventory | ``reports/e10a_gwy_shell_inventory.csv`` |
| cfg records | ``reports/e10a_gwy_cfg_records.csv`` |
| strings | ``reports/e10a_shell_strings.csv`` |
| dependencies | ``reports/e10a_shell_file_dependencies.csv`` |
| transition graph | ``out/e10a_shell/launch_transition_graph.md`` |
| phase trace | ``reports/e10a_shell_phase_trace.csv`` |
| vfs trace | ``reports/e10a_shell_vfs_trace.csv`` |
| vfs badpath | ``reports/e10a_shell_vfs_badpath_trace.csv`` |
| file API ABI | ``reports/e10a_shell_file_api_abi.md`` |
| event loop | ``reports/e10a_shell_event_loop_trace.csv`` |
| update contract | ``reports/e10a_shell_update_contract_trace.csv`` |
| ac8 trace | ``reports/e10a_shell_ac8_trace.csv`` |
| log | ``logs/e10a_shell_trace_stdout.txt`` |
"@
[System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

Write-Host "E10A-Fix done mode=$Mode primary=$primary"
