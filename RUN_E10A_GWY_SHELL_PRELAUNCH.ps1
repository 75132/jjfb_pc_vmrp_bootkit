# Stage E10A: GWY shell prelaunch vs descriptor_direct — AC8 compare at jjfb splash
# Modes: direct | shell | compare | classify
# NOT product. No AC8 poke / synthetic resource-ready / branch patch.
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('direct','shell','compare','classify')]
  [string]$Mode = 'compare',
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

function Stop-E10Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
      ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A_GWY|stage_e_product')
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-E10Env {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE',
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_E9C_MODE','JJFB_E9D_MODE','JJFB_E9E_MODE',
    'JJFB_E9F_MODE','JJFB_E9G_MODE','JJFB_E9H_MODE','JJFB_E9I_MODE','JJFB_E9J_MODE',
    'JJFB_E9K_MODE','JJFB_E9L_MODE','JJFB_E9M_MODE','JJFB_E9N_MODE','JJFB_E9O_MODE',
    'JJFB_E9P_MODE','JJFB_E9Q_MODE','JJFB_E9R_MODE','JJFB_E9S_MODE','JJFB_E9T_MODE',
    'JJFB_E9U_MODE','JJFB_E9V_MODE','JJFB_E9W_MODE','JJFB_E9Y_MODE','JJFB_E9Y_FIX_MODE',
    'JJFB_E9Z_MODE','JJFB_E10A_MODE','JJFB_E10A_GWY_SHELL_PRELAUNCH',
    'JJFB_GWY_LAUNCHER_MODE','JJFB_NATIVE_BOOT_FULL','JJFB_SHELL_CHAIN_MODE',
    'JJFB_DISABLE_JJFB_ALIAS_DIRECT','JJFB_SHELL_NATIVE_EXEC_TRACE',
    'JJFB_PRODUCT_DESCRIPTOR_DIRECT','JJFB_GWY_UPDATE_STUB',
    'GWY_PLATFORM_RESOURCE_READY_EVENT','JJFB_FAST_GWY_RESOURCE_READY_EVENT',
    'JJFB_DEBUG_AC8_FORCE','JJFB_FAST_DOWNIMAGE_READY_EVENT'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Set-E10ATraceEnv {
  $env:JJFB_E10A_MODE = '1'
  $env:JJFB_E9Y_MODE = '1'
  $env:JJFB_E9Y_NO_DEBUG_AC8 = '1'
  $env:JJFB_E9Y_NO_WORKBUF_SEED = '1'
  $env:JJFB_PLATFORM_WORKBUF_ALLOC = '1'
  $env:JJFB_GWY_PACK_REGISTRY = '1'
  $env:JJFB_E9W_MODE = '1'
  $env:JJFB_E9W_ARCHIVE_EXACT = '1'
  $env:JJFB_E9V_MODE = '1'
  $env:JJFB_E9U_MODE = '1'
  $env:JJFB_E9T_MODE = '1'
  $env:JJFB_E9S_MODE = '1'
  $env:JJFB_E9R_MODE = '1'
  $env:JJFB_E9Q_MODE = '1'
  $env:JJFB_E9P_MODE = '1'
  $env:JJFB_E9O_MODE = '1'
  $env:JJFB_E9N_MODE = '1'
  $env:JJFB_E9M_MODE = '1'
  $env:JJFB_E9L_MODE = '1'
  $env:JJFB_E9K_MODE = '1'
  $env:JJFB_E9J_MODE = '1'
  $env:JJFB_E9I_MODE = '1'
  $env:JJFB_E9H_MODE = '1'
  $env:JJFB_E9G_MODE = '1'
  $env:JJFB_E9F_MODE = '1'
  $env:JJFB_E9E_MODE = '1'
  $env:JJFB_E9E_POSTMATCH_SHIMS = '1'
  $env:JJFB_E9D_MODE = '1'
  $env:JJFB_E9D_STRCMP_SHIM = '1'
  $env:JJFB_E9A_MODE = '1'
  $env:JJFB_E8Z_MODE = '1'
  $env:JJFB_E8Y_MODE = '1'
  $env:JJFB_E8X_MODE = '1'
  $env:JJFB_E8W_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
  $env:JJFB_WINDOW_ZOOM = "$Zoom"
  $env:JJFB_E9B_HOLD_SEC = "$HoldSec"
  $env:JJFB_REAL_MRP_PATH = $mrpPath
  $env:JJFB_E9F_MULTI_POSTMATCH = '1'
  $env:JJFB_E9H_R4_TRACE = '1'
  $env:JJFB_E9I_SKIP_SIBLING = '0'
  $env:JJFB_E9K_HOLD_AFTER_POST_R4 = '1'
  $env:JJFB_E9K_MIN_BLITS = '64'
  $env:JJFB_E9K_STOP_ON_TEXTBAR_DRAW = '0'
  $env:JJFB_E9C_DEFER_HOLD = '1'
  $env:JJFB_E9J_HOLD_AFTER_BLIT = '64'
  $env:JJFB_FAST_SPLASH_CALL = '1'
  $env:JJFB_E9G_UI_MODE_ASSIST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_PLATFORM_TEXT_API_11F00 = '1'
  $env:JJFB_PLATFORM_TEXT_MEASURE_12340 = '1'
  $env:JJFB_PLATFORM_SCREEN_DIMS = '1'
  $env:JJFB_E9P_TEXT_BLIT = 'transparent'
  $env:JJFB_COLORKEY = 'auto'
  $env:JJFB_E10A_CSV = (Join-Path $reportDir 'e10a_shell_ac8_trace.csv')
  $env:JJFB_E9U_TICK_N = "$TickN"
  $env:JJFB_FAST_BD0_INIT_CALL = '1'
  $env:JJFB_FAST_PROGRESS_TICK_CALL = '1'
  $env:JJFB_E8C_IDLE_WATCH = '1'
  $env:JJFB_E8D_EARLY_WATCH = '1'
  $env:JJFB_E8J_CLUSTER_BP = '1'
  $env:JJFB_E8I_STATE_WATCH = '1'
  $env:JJFB_PLAT_CENSUS = '1'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:GWY_PACKAGE_APPID = '400101'
  $env:GWY_PACKAGE_APPVER = '12'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = Join-Path $RunDir 'overlay'
  $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_PARAM = $param
}

function Set-E10ADirectEnv {
  Set-E10ATraceEnv
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_PRIMARY_TARGET = ($Target -replace '\\', '/')
  $env:GWY_LAUNCH_TARGET = ($Target -replace '\\', '/')
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  Remove-Item Env:JJFB_GWY_LAUNCHER_MODE -EA SilentlyContinue
  Remove-Item Env:JJFB_SHELL_CHAIN_MODE -EA SilentlyContinue
  Remove-Item Env:JJFB_DISABLE_JJFB_ALIAS_DIRECT -EA SilentlyContinue
}

function Set-E10AShellEnv {
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
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  Remove-Item Env:JJFB_PRODUCT_DESCRIPTOR_DIRECT -EA SilentlyContinue
}

function Analyze-E10A([string]$log, [string]$launchPath) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $splash = $t -match 'JJFB_E10A_AC8.*phase=splash_enter|splash_2EF86C|0x2EF86C'
  $branch = $t -match 'phase=ac8_branch.*AC8=0x([^0\s]|[^0\s][0-9A-Fa-f]+)|SPLASH_LOGO_BRANCH_REACHED'
  $ac8Nz = $t -match 'JJFB_E10A_AC8.*AC8=0x[1-9A-Fa-f]|SPLASH_LOGO_BRANCH_REACHED(?!_DEBUG)'
  $ac8Zero = $t -match 'loading_only|AC8=0x0|AC8_after=0x0'
  $shellGbrw = $t -match 'JJFB_E10A_SHELL.*gbrwcore|gbrwcore_mr_start|JJFB_SHELL_CORE_MODULE.*gbrwcore'
  $shellGl = $t -match 'JJFB_E10A_SHELL.*gamelist|gamelist_mr_start|JJFB_GAMELIST_STARTED'
  $shellPost = $t -match 'JJFB_E10A_SHELL.*post_update|JJFB_GAMELIST_POST_UPDATE'
  $shellRunapp = $t -match 'JJFB_E10A_SHELL.*runapp|JJFB_RUNAPP.*native_shell'
  $cls = 'AC8_STILL_BLOCKED_AFTER_SHELL_REPLAY'
  if ($ac8Nz -and -not ($t -match 'DEBUG_AC8_FORCE')) { $cls = 'SHELL_PATH_RAISED_AC8' }
  elseif (-not $splash -and $launchPath -match 'shell') { $cls = 'SHELL_CHAIN_INCOMPLETE' }
  elseif ($splash -and $ac8Zero -and -not $ac8Nz) {
    if ($launchPath -match 'shell') { $cls = 'SHELL_PATH_SAME_AS_DIRECT_AC8_ZERO' }
    else { $cls = 'DIRECT_PATH_AC8_ZERO' }
  }
  return [pscustomobject]@{
    class = $cls
    launch_path = $launchPath
    splash = [bool]$splash
    ac8_nz = [bool]$ac8Nz
    shell_gbrwcore = [bool]$shellGbrw
    shell_gamelist = [bool]$shellGl
    shell_post_update = [bool]$shellPost
    shell_runapp = [bool]$shellRunapp
  }
}

function Invoke-E10ACase([string]$CaseName, [scriptblock]$EnvFn) {
  $caseLog = Join-Path $logDir "e10a_${CaseName}_stdout.txt"
  $caseErr = Join-Path $logDir "e10a_${CaseName}_stderr.txt"
  $verdictMd = Join-Path $reportDir 'stage_e10a_gwy_shell_prelaunch_verdict.md'
  Clear-E10Env
  Stop-E10Children
  & $EnvFn
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  Write-Host "=== E10A case=$CaseName launch_path=$($env:JJFB_LAUNCH_PATH) timeout=${CASE_TIMEOUT_SEC}s ==="
  $t0 = Get-Date
  if ($CaseName -eq 'direct') {
    $argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
      '-Target',$Target,'-Seconds',"$CASE_TIMEOUT_SEC")
    if ($SkipBuild) { $argList += '-SkipBuild' }
    $p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -WorkingDirectory $Root -PassThru `
      -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
  } else {
    if (-not $SkipBuild) {
      & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy | Out-Host
      if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
    }
    $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
      -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
  }
  if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E10Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E10A_' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }
  $an = Analyze-E10A $caseLog $env:JJFB_LAUNCH_PATH
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
  Write-Host "== E10A_CASE_DONE name=$CaseName verdict=$($an.class) elapsed=$elapsed"
  return [pscustomobject]@{ case = $CaseName; elapsed = $elapsed; analysis = $an; log = $caseLog }
}

# ---- main ----
if (-not $SkipBuild -and $Mode -ne 'shell') {
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}

$results = @()
switch ($Mode) {
  'direct' { $results += Invoke-E10ACase 'direct' { Set-E10ADirectEnv } }
  'shell'  { $results += Invoke-E10ACase 'shell'  { Set-E10AShellEnv } }
  'compare' {
    $results += Invoke-E10ACase 'direct' { Set-E10ADirectEnv }
    $results += Invoke-E10ACase 'shell'  { Set-E10AShellEnv }
  }
  'classify' {
    $results += Invoke-E10ACase 'shell' { Set-E10AShellEnv }
  }
}

$primary = $results[-1].analysis.class
$md = @"
# Stage E10A GWY Shell Prelaunch Verdict

- **Mode**: $Mode
- **Primary**: ``$primary``
- **Product success**: **NO** (``NOT_PRODUCT``)

## Cases
$(($results | ForEach-Object {
"- **$($_.case)** ($($_.elapsed)s): ``$($_.analysis.class)`` launch=$($_.analysis.launch_path) splash=$($_.analysis.splash) ac8_nz=$($_.analysis.ac8_nz) shell_gbrw=$($_.analysis.shell_gbrwcore) shell_gl=$($_.analysis.shell_gamelist) post_update=$($_.analysis.shell_post_update) runapp=$($_.analysis.shell_runapp)"
}) -join "`n")

## Artifacts
- trace: ``reports/e10a_shell_ac8_trace.csv``
- log: ``logs/e10a_<case>_stdout.txt``

## Notes
- E10A compares ``descriptor_direct`` vs ``gwy_shell_core_continue`` without synthetic resource-ready or AC8 poke.
- If both paths AC8=0 at splash, blocker remains ``AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL`` (E9Z).
"@
[System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e10a_gwy_shell_prelaunch_verdict.md'), $md, [System.Text.UTF8Encoding]::new($false))

$results | ForEach-Object { $_.analysis } | Format-Table -AutoSize | Out-Host
Write-Host "E10A done mode=$Mode primary=$primary"
