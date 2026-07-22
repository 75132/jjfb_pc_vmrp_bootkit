# Stage E9J: splash progress object / post-r4 UI — NOT product success.
# Focus: R9+0xBD0 status string + BA0+0x2C progress count; keep natural loadingbar.
# No JJFB_E9F_REWRITE_REQUEST. No REAL_MRP_MEMBER_BRIDGE success.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 20,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('natural','r4assist','trace')]
  [string]$Mode = 'r4assist',
  [switch]$SkipBuild,
  [switch]$UiModeAssist
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
$shotDir = Join-Path $Root 'evidence\screenshots'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $shotDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(120, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$mrpPath = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'
$bmpSrc = Join-Path $Root 'out\e8z_resources\wy_jiao1_11_11.bmp'
if (-not (Test-Path $bmpSrc)) {
  py -3 (Join-Path $Root 'tools\e8z_resource_probe.py') | Out-Null
}

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (-not (Test-Path $flagMap)) {
  py -3 (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp') | Out-Host
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
foreach ($need in @('2256','3229','3317','3948','3952','3956','2072','2076','2096','2648','2652','2656','2660','2664','2668')) {
  if ($offsets -notmatch "(^|,)$need(,|$)") {
    $offsets = if ($offsets) { "$offsets,$need" } else { $need }
  }
}
$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x3066B8','p:0x3066C6','p:0x306740','p:0x2E88CC',
  'p:0x2E8914','p:0x2E8980','p:0x2E89A8','p:0x2F2854','p:0x2EA188','p:0x2F449C',
  'p:0x2D92E4','p:0x2F44C8','p:0x2F44CE','p:0x310BBC','p:0x304BF0','p:0x2FD868',
  'p:0x304F26','p:0x304F7A','p:0x304F92','p:0x2EF86C','p:0x30662C','p:0x306344',
  'p:0x2FC418','p:0x2FC444','p:0x2EFA33','p:0x2EFA46','p:0x2EFA56','p:0x2EFA5C',
  'p:0x2EFA7C','p:0x2EFA9A','p:0x2EC6B8','p:0x2EFA9E','p:0x2EFAE2','p:0x2EFAF2',
  'p:0x2EFAFA','p:0x2EFB0E','p:0x2EFBA2','p:0x305BFC','p:0x30ED98'
) -join ','

function Clear-E9Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE',
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_E9C_MODE','JJFB_E9D_MODE','JJFB_E9E_MODE',
    'JJFB_E9F_MODE','JJFB_E9G_MODE','JJFB_E9H_MODE','JJFB_E9I_MODE','JJFB_E9J_MODE',
    'JJFB_VISIBLE_WINDOW','JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST',
    'JJFB_FAST_A64_RESOURCE_ASSIST','JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH',
    'JJFB_REAL_MRP_MEMBER_BRIDGE','JJFB_REAL_MRP_MEMBER_BRIDGE_ALL','JJFB_REAL_MRP_PATH',
    'JJFB_E8Z_BMP_PATH','JJFB_E8Z_SCREENSHOT','JJFB_E8Z_SCREENSHOT_BEFORE','JJFB_E8Z_SCREENSHOT_AFTER',
    'JJFB_E8Y_INSN_LIMIT','JJFB_E8W_REENTER_E88CC','JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB',
    'JJFB_FAST_STATE','JJFB_FAST_CASE156_R1','JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30',
    'JJFB_FAST_C6C22','JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_WINDOW_ZOOM',
    'JJFB_E9B_HOLD_SEC','JJFB_E9B_HWND_CAPTURE','JJFB_E9D_STRCMP_SHIM','JJFB_E9D_BRIDGE_FALLBACK',
    'JJFB_E9D_COMPARE_CSV','JJFB_E9D_REQUEST_CSV','JJFB_E9E_POSTMATCH_SHIMS',
    'JJFB_E9F_REWRITE_REQUEST','JJFB_E9F_MULTI_POSTMATCH','JJFB_E9F_PREFER',
    'JJFB_E9F_REQUEST_CSV','JJFB_E9F_RESULTS_JSONL',
    'JJFB_FAST_SPLASH_CALL','JJFB_E9G_UI_MODE_ASSIST','JJFB_E9G_DEBUG',
    'JJFB_E9G_REQUEST_CSV','JJFB_E9G_UIMODE_CSV',
    'JJFB_E9H_R4_TRACE','JJFB_FAST_SPLASH_R4_ASSIST','JJFB_E9H_R4_CSV','JJFB_E9H_SEQ_CSV',
    'JJFB_SPLASH_COORD_ASSIST','JJFB_E9I_SKIP_SIBLING','JJFB_E9I_COORD_CSV','JJFB_E9I_R4_CSV',
    'JJFB_E9I_SEQ_CSV','JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST','JJFB_E9J_PROGRESS_COUNT',
    'JJFB_E9J_WRITER_CSV','JJFB_E9J_POSTR4_CSV','JJFB_E9C_DEFER_HOLD','JJFB_E9J_HOLD_AFTER_BLIT'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Stop-E9Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
      ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'ROBOTOL_MRCINIT|stage_e_product')
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Convert-BmpToPng([string]$BmpPath, [string]$PngPath) {
  if (-not (Test-Path $BmpPath)) { return $false }
  try {
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop
    $img = [System.Drawing.Image]::FromFile((Resolve-Path $BmpPath))
    $img.Save($PngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    return $true
  } catch { return $false }
}

function Analyze-E9J([string]$log) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }
  $ui = $t -match 'JJFB_E9J_CLASS\] class=SPLASH_LOADING_UI_VISIBLE|JJFB_E9I_CLASS\] class=SPLASH_LOADING_UI_VISIBLE'
  $prog = $t -match 'SPLASH_PROGRESS_DRAWN_NO_REWRITE'
  $tb = $t -match 'SPLASH_TEXTBAR_DRAWN_NO_REWRITE'
  $post = $t -match 'SPLASH_PROGRESS_OBJECT_ASSIST_REACHED_POST_R4|JJFB_E9J_POST_R4\]'
  $src = $t -match 'SPLASH_PROGRESS_OBJECT_SOURCE_FOUND_NEXT_GAP'
  $bd0z = $t -match 'SPLASH_BLOCKED_BY_BD0_ZERO'
  $b6c = $t -match 'SPLASH_BLOCKED_BY_B6C_GATE'
  $field = $t -match 'SPLASH_BLOCKED_BY_PROGRESS_OBJECT_FIELD'
  $lb = $t -match 'SPLASH_LOADINGBAR_DRAWN_NO_REWRITE|NATURAL_COORD_VISIBLE'
  $assist = $t -match 'JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST\]'
  $rewrite = $t -match 'JJFB_E9F_REQUEST_REWRITE\]'
  $cls = if ($ui) { 'SPLASH_LOADING_UI_VISIBLE' }
    elseif ($prog) { 'SPLASH_PROGRESS_DRAWN_NO_REWRITE' }
    elseif ($tb) { 'SPLASH_TEXTBAR_DRAWN_NO_REWRITE' }
    elseif ($post) { 'SPLASH_PROGRESS_OBJECT_ASSIST_REACHED_POST_R4' }
    elseif ($bd0z -and -not $assist) { 'SPLASH_BLOCKED_BY_BD0_ZERO' }
    elseif ($b6c) { 'SPLASH_BLOCKED_BY_B6C_GATE' }
    elseif ($field) { 'SPLASH_BLOCKED_BY_PROGRESS_OBJECT_FIELD' }
    elseif ($src -and -not $assist) { 'SPLASH_PROGRESS_OBJECT_SOURCE_FOUND_NEXT_GAP' }
    elseif ($assist -and $lb -and -not $post -and -not $prog) { 'SPLASH_PROGRESS_OBJECT_ASSIST_REACHED_POST_R4' }
    elseif ($rewrite) { 'REWRITE_USED_NOT_PRODUCT' }
    elseif ($lb) { 'SPLASH_BLOCKED_BY_NEXT_RESOURCE' }
    else { 'PRODUCT_STILL_NEEDS_STATE_MACHINE_NATURALIZATION' }
  return [pscustomobject]@{
    class = $cls
    evidence = if ($ui -or $prog -or $tb -or $post -or $lb -or $src) { 'OBSERVED' } else { 'HYPOTHESIS' }
    loadingbar = [bool]$lb
    progress = [bool]$prog
    textbar = [bool]$tb
    post_r4 = [bool]$post
    assist = [bool]$assist
    rewrite = [bool]$rewrite
  }
}

if (-not $SkipBuild) {
  Write-Host '=== E9J force-rebuild launcher_core + vmrp gwy ==='
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'build-i686\liblauncher_core.a')
  # main.c defer-hold lives in out/vmrp build tree
  Get-ChildItem (Join-Path $Root 'out') -Recurse -Filter 'main.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Get-ChildItem (Join-Path $Root 'out') -Recurse -Filter 'bridge.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
}

$writerCsv = Join-Path $reportDir 'e9j_bd0_b6c_writer_trace.csv'
$postr4Csv = Join-Path $reportDir 'e9j_post_r4_path_trace.csv'
$verdictMd = Join-Path $reportDir 'stage_e9j_splash_progress_verdict.md'
$shotBmp = Join-Path $shotDir 'e9j_splash_progress_ui.bmp'
$hwndBmp = Join-Path $shotDir 'e9j_actual_window_capture.bmp'
$hwndPng = Join-Path $shotDir 'e9j_actual_window_capture.png'
$uiPng = Join-Path $shotDir 'e9j_splash_progress_ui.png'
$caseLog = Join-Path $logDir 'e9j_splash_progress_stdout.txt'
$caseErr = Join-Path $logDir 'e9j_splash_progress_stderr.txt'

Clear-E9Modes
Stop-E9Children

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
$env:JJFB_BYPASS_C9D_GATE = '1'
$env:JJFB_E9B_MODE = '1'
$env:JJFB_VISIBLE_WINDOW = '1'
$env:JJFB_WINDOW_ZOOM = "$Zoom"
$env:JJFB_E9B_HOLD_SEC = "$HoldSec"
$env:JJFB_REAL_MRP_PATH = $mrpPath
$env:JJFB_E8Z_BMP_PATH = $bmpSrc
$env:JJFB_E9F_MULTI_POSTMATCH = '1'
$env:JJFB_E9H_R4_TRACE = '1'
$env:JJFB_E9I_SKIP_SIBLING = '0'
$env:JJFB_E9J_WRITER_CSV = $writerCsv
$env:JJFB_E9J_POSTR4_CSV = $postr4Csv
$env:JJFB_E9I_COORD_CSV = (Join-Path $reportDir 'e9j_coord_trace.csv')
$env:JJFB_E9I_R4_CSV = (Join-Path $reportDir 'e9j_r4_trace.csv')
$env:JJFB_E9I_SEQ_CSV = (Join-Path $reportDir 'e9j_splash_resource_sequence.csv')
$env:JJFB_E9H_R4_CSV = $env:JJFB_E9I_R4_CSV
$env:JJFB_E9H_SEQ_CSV = $env:JJFB_E9I_SEQ_CSV
$env:JJFB_E9D_COMPARE_CSV = (Join-Path $reportDir 'e9j_name_compare_debug.csv')
$env:JJFB_E9D_REQUEST_CSV = (Join-Path $reportDir 'e9j_e9d_resource_request.csv')
$env:JJFB_E9G_REQUEST_CSV = (Join-Path $reportDir 'e9j_game_ui_request_trace.csv')
$env:JJFB_E9G_UIMODE_CSV = (Join-Path $reportDir 'e9j_ui_mode_trace.csv')
$env:JJFB_E9F_REQUEST_CSV = (Join-Path $reportDir 'e9j_e9f_request.csv')
$env:JJFB_E8Z_SCREENSHOT = $shotBmp
$env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9j_before.bmp')
$env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9j_after.bmp')
$env:JJFB_E9B_HWND_CAPTURE = $hwndBmp
$env:JJFB_E8Y_INSN_LIMIT = '12000'
$env:JJFB_E9C_DEFER_HOLD = '1'
$env:JJFB_E9J_HOLD_AFTER_BLIT = '5'
Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_SPLASH_COORD_ASSIST -ErrorAction SilentlyContinue

switch ($Mode) {
  'natural' {
    $env:JJFB_FAST_SPLASH_CALL = '1'
    $env:JJFB_E9G_UI_MODE_ASSIST = '1'
    Remove-Item Env:JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST -EA SilentlyContinue
  }
  'r4assist' {
    $env:JJFB_FAST_SPLASH_CALL = '1'
    $env:JJFB_E9G_UI_MODE_ASSIST = '1'
    $env:JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST = '1'
    $env:JJFB_E9J_PROGRESS_COUNT = '4'
  }
  'trace' {
    $env:JJFB_FAST_SPLASH_CALL = '1'
    $env:JJFB_E9G_UI_MODE_ASSIST = '1'
    $env:JJFB_E9H_R4_TRACE = '1'
    if ($UiModeAssist) { $env:JJFB_E9G_UI_MODE_ASSIST = '1' }
  }
}

$env:JJFB_E8C_IDLE_WATCH = '1'
$env:JJFB_E8C_WATCH_OFFSETS = $offsets
$env:JJFB_E8D_EARLY_WATCH = '1'
$env:JJFB_E8J_CLUSTER_BP = '1'
$env:JJFB_E8J_BP_SPEC = $bpSpec
$env:JJFB_E8I_STATE_WATCH = '1'
$env:JJFB_PLAT_CENSUS = '1'
$env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
$env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
$env:JJFB_LAUNCH_PATH = 'descriptor_direct'
$env:JJFB_PRIMARY_TARGET = ($Target -replace '\\', '/')
$env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
$env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
$env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
$env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
$env:JJFB_GAME_PACKAGE_ER_RW_SOURCE = 'module_map_or_mrpgcmap'
$env:JJFB_GAME_PACKAGE_CONTEXT_PROVIDER = '1'
$env:JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
$env:JJFB_MODULE_REGISTRY_TRACE = '1'
$env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
$env:JJFB_MRC_INIT_TRACE = '1'
$env:JJFB_LIFECYCLE_EVENT_TRACE = '1'
$env:JJFB_E7_LIFECYCLE_MODE = '1'
$env:JJFB_POST_START_SCHEDULER_TRACE = '1'
$env:JJFB_GAME_SELF_PATCH = '0'
$env:GWY_MODULE_R9_SWITCH = '1'
$env:GWY_CALLBACK_FRAME = '1'
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'
$env:JJFB_FAST_ASSIST = '1'
$env:JJFB_FAST_SVC_AB = 'return0'
$env:JJFB_FAST_STATE = '38'
$env:JJFB_FAST_CASE156_R1 = '20'
$env:JJFB_FAST_SEQUENCE = 'case156'
$env:JJFB_FAST_C6C22 = '1'
$env:JJFB_FAST_DEC30 = '1'
$env:JJFB_FAST_INSN_LIMIT = '500000'
$env:JJFB_FAST_UNLOCK_CALL = '1'
$env:JJFB_FAST_UNLOCK_WHEN = 'before'
$env:JJFB_FAST_F6C_OBJECT_ASSIST = '1'
$env:JJFB_FAST_F74_DESCRIPTOR_ASSIST = '1'
$env:JJFB_E8W_REENTER_E88CC = '1'

Write-Host "=== E9J Mode=$Mode timeout=${CASE_TIMEOUT_SEC}s hold=${HoldSec}s NOT_PRODUCT ==="
[Console]::Out.Flush()

$t0 = Get-Date
$argList = @(
  '-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
  '-Target',$Target,'-Seconds',"$CASE_TIMEOUT_SEC"
)
if ($SkipBuild) { $argList += '-SkipBuild' }

$p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -WorkingDirectory $Root -PassThru `
  -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
  Write-Host "E9J outer kill after ${OUTER_KILL_SEC}s"
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E9Children
}
$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9J_|JJFB_E9I_SCR_DIM|JJFB_E9H_2EC6B8' -Quiet -EA SilentlyContinue)) {
  Copy-Item -Force $src $caseLog
}
if ($p.ExitCode -ne 0 -and $p.ExitCode -ne $null) {
  Write-Host "E9J child exit=$($p.ExitCode) — see $caseErr"
  Get-Content $caseErr -Tail 30 -EA SilentlyContinue | Write-Host
}

Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
Convert-BmpToPng $shotBmp $uiPng | Out-Null
if (Test-Path $hwndPng) { Copy-Item -Force $hwndPng $uiPng -EA SilentlyContinue }
$an = Analyze-E9J $caseLog
$elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

$md = @"
# Stage E9J Splash Progress / Post-r4 Verdict

- **Mode**: $Mode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **Loadingbar**: $($an.loadingbar)
- **Progress bar segments**: $($an.progress)
- **Textbar**: $($an.textbar)
- **Post-r4 reached**: $($an.post_r4)
- **Progress object assist**: $($an.assist)
- **Rewrite**: $($an.rewrite)
- **Log**: ``$caseLog``
- **Writer CSV**: ``$writerCsv``
- **Post-r4 CSV**: ``$postr4Csv``
- **HWND**: ``$hwndPng``

## Decoded (Task 1-3)
- splash_obj = R9+0xBA0
- +0x20 bar, +0x24 textbar, +0x28 loadingbar, +0x2C progress count, +0x30 status string (BD0)
- r4 = *(R9+0xBD0) loaded at 0x2EF886
- Natural BD0 writer: 0x2FC418 -> 2d9648 concat -> STR at 0x2FC444
- Gate: *(R9+0xB6C)!=0 skips r4; else cmp r4 at 0x2EFAFA
- Progress loop 0x2EFA9E..0x2EFAE2 draws bar! when count > idx
- Post-r4 text path uses textbar width (+0x24) and 0x305BFC text draw

## Rules
- NOT product success. No rewrite / no bridge / no random BD0 object.
- Assist uses real robotol GBK status string only.
"@
Set-Content -Path $verdictMd -Value $md -Encoding UTF8

Write-Host "== E9J_CASE_DONE name=$Mode verdict=$($an.class) elapsed=${elapsed}s =="
Write-Host "verdict=$verdictMd"
exit 0
