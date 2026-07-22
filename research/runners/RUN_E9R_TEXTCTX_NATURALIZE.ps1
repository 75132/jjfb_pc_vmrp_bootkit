# Stage E9R: platform screen dims replace FAST_TEXTCTX_ASSIST — NOT product success.
# Modes: natural | platform | compare
# Success path: JJFB_PLATFORM_SCREEN_DIMS=1, NO FAST_TEXTCTX_ASSIST.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 12,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('natural','platform','compare')]
  [string]$Mode = 'platform',
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
  'p:0x2EFAFA','p:0x2EFB08','p:0x2EFB0E','p:0x2EFBA2','p:0x2EFBA6','p:0x305BFC',
  'p:0x2F2174','p:0x305E78','p:0x305EA0','p:0x2EFB48','p:0x303C50','p:0x304558',
  'p:0x305C2E','p:0x305C32','p:0x305C3C','p:0x305C54','p:0x305CA0','p:0x305C90',
  'p:0x305C98','p:0x305D08','p:0x305D16','p:0x305D58','p:0x305D5C','p:0x2F2360',
  'p:0x2F99A4','p:0x30ED98','p:0x310BBC'
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
    'JJFB_E9K_MODE','JJFB_E9L_MODE','JJFB_E9M_MODE','JJFB_E9N_MODE','JJFB_E9O_MODE',
    'JJFB_E9P_MODE','JJFB_E9Q_MODE','JJFB_E9R_MODE','JJFB_VISIBLE_WINDOW',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST',
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
    'JJFB_E9J_WRITER_CSV','JJFB_E9J_POSTR4_CSV','JJFB_E9C_DEFER_HOLD','JJFB_E9J_HOLD_AFTER_BLIT',
    'JJFB_E9K_HOLD_AFTER_POST_R4','JJFB_E9K_MIN_BLITS','JJFB_E9K_STOP_ON_TEXTBAR_DRAW',
    'JJFB_E9K_POSTR4_CSV','JJFB_E9K_DRAW_CSV','JJFB_FAST_TEXTCTX_ASSIST',
    'JJFB_E9L_WRITER_CSV','JJFB_E9L_305E78_CSV','JJFB_E9L_TEXT_CSV',
    'JJFB_FAST_TEXT_MEASURE_SHIM','JJFB_FAST_TEXT_LAYOUT_ASSIST',
    'JJFB_E9M_ABI_CSV','JJFB_E9M_MEAS_CSV','JJFB_E9M_LAYOUT_CSV',
    'JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM','JJFB_E9N_GLYPHTRACE',
    'JJFB_E9N_305C3C_CSV','JJFB_E9N_CLIP_CSV','JJFB_E9N_GLYPH_CSV',
    'JJFB_PLATFORM_TEXT_API_11F00','JJFB_E9O_11F00_CSV','JJFB_E9O_DRAW_CSV',
    'JJFB_E9P_TEXT_BLIT','JJFB_E9P_COMPARE_CSV',
    'JJFB_PLATFORM_TEXT_MEASURE_12340','JJFB_E9Q_12340_CSV','JJFB_E9Q_METRICS_CSV',
    'JJFB_PLATFORM_SCREEN_DIMS'
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

function Analyze-E9R([string]$log) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }
  $synced = $t -match 'JJFB_E9R_CLASS\] class=PLATFORM_TEXTCTX_DIMS_SYNCED'
  $assistGone = $t -match 'TEXTCTX_ASSIST_REMOVED_SUCCESSFULLY'
  $assistUsed = $t -match 'JJFB_FAST_TEXTCTX_ASSIST\]'
  $blocked = $t -match 'SPLASH_TEXT_BLOCKED_BY_FONT_CONTEXT'
  $meas = $t -match 'PLATFORM_TEXT_MEASURE_12340_RENDERED|TEXT_MEASURE_SHIM_REMOVED_SUCCESSFULLY'
  $trans = $t -match 'PLATFORM_TEXT_TRANSPARENT_RENDERED'
  $draw = $t -match 'PLATFORM_TEXT_API_11F00_RENDERED'
  $ui = $t -match 'SPLASH_LOADING_UI_VISIBLE'
  $prog = $t -match 'SPLASH_PROGRESS_DRAWN_NO_REWRITE'
  $hold = $t -match 'JJFB_VISIBLE_WINDOW_HOLD_DONE\]|JJFB_E9K_HOLD'
  $hit305 = $t -match '305BFC|TEXTCTX_ASSIST_REACHED_305BFC|JJFB_E9M_305BFC'
  $cls = 'PRODUCT_STILL_NEEDS_BD0_NATURALIZATION'
  if ($Mode -eq 'natural') {
    $cls = if ($blocked -and -not $assistUsed) { 'SPLASH_TEXT_BLOCKED_BY_FONT_CONTEXT' }
      elseif ($assistUsed) { 'TEXTCTX_ASSIST_STILL_ACTIVE' }
      elseif ($synced) { 'PLATFORM_DIMS_UNEXPECTED_ON_NATURAL' }
      else { 'TEXTCTX_NATURAL_WRITER_NOT_REACHED' }
  } elseif ($Mode -eq 'compare') {
    $cls = if ($assistUsed -and $draw) { 'TEXTCTX_ASSIST_BASELINE_OK' }
      else { 'TEXTCTX_COMPARE_INCONCLUSIVE' }
  } else {
    if ($synced -and $assistGone -and -not $assistUsed -and $draw -and $trans) {
      $cls = 'PLATFORM_TEXTCTX_DIMS_SYNCED'
    } elseif ($synced -and $assistGone -and -not $assistUsed) {
      $cls = 'PLATFORM_TEXTCTX_DIMS_SYNCED'
    } elseif ($assistUsed) {
      $cls = 'TEXTCTX_ASSIST_REGRESSED'
    } elseif ($blocked) {
      $cls = 'SPLASH_TEXT_BLOCKED_BY_FONT_CONTEXT'
    } elseif ($draw) {
      $cls = 'PRODUCT_STILL_NEEDS_BD0_NATURALIZATION'
    }
  }
  return [pscustomobject]@{
    class = $cls
    evidence = if ($synced -or $blocked -or $draw) { 'OBSERVED' } else { 'HYPOTHESIS' }
    synced = [bool]$synced
    assist_removed = [bool]$assistGone
    assist_used = [bool]$assistUsed
    measure = [bool]$meas
    transparent = [bool]$trans
    draw11 = [bool]$draw
    loading_ui = [bool]$ui
    progress = [bool]$prog
    hold = [bool]$hold
    hit305 = [bool]$hit305
  }
}

function Invoke-E9RCase([string]$CaseName, [string]$CaseMode) {
  $writerCsv = Join-Path $reportDir 'e9r_818_81c_writer_trace.csv'
  $verdictMd = Join-Path $reportDir 'stage_e9r_textctx_naturalize_verdict.md'
  $shotBmp = Join-Path $shotDir 'e9r_textctx_naturalize_window.bmp'
  $hwndBmp = Join-Path $shotDir 'e9r_actual_window_capture.bmp'
  $hwndPng = Join-Path $shotDir 'e9r_actual_window_capture.png'
  $uiPng = Join-Path $shotDir 'e9r_textctx_naturalize_window.png'
  $caseLog = Join-Path $logDir 'e9r_textctx_naturalize_stdout.txt'
  $caseErr = Join-Path $logDir 'e9r_textctx_naturalize_stderr.txt'

  Clear-E9Modes
  Stop-E9Children

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
  $env:JJFB_E9K_HOLD_AFTER_POST_R4 = '1'
  $env:JJFB_E9K_MIN_BLITS = '64'
  $env:JJFB_E9K_STOP_ON_TEXTBAR_DRAW = '0'
  $env:JJFB_E9C_DEFER_HOLD = '1'
  $env:JJFB_E9J_HOLD_AFTER_BLIT = '64'
  $env:JJFB_FAST_SPLASH_CALL = '1'
  $env:JJFB_E9G_UI_MODE_ASSIST = '1'
  $env:JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST = '1'
  $env:JJFB_E9J_PROGRESS_COUNT = '4'
  $env:JJFB_PLATFORM_TEXT_API_11F00 = '1'
  $env:JJFB_PLATFORM_TEXT_MEASURE_12340 = '1'
  $env:JJFB_E9P_TEXT_BLIT = 'transparent'
  $env:JJFB_E9L_WRITER_CSV = $writerCsv
  $env:JJFB_E9O_11F00_CSV = (Join-Path $reportDir 'e9r_e9o_11f00_compat.csv')
  $env:JJFB_E9O_DRAW_CSV = (Join-Path $reportDir 'e9r_e9o_draw_compat.csv')
  $env:JJFB_E9Q_12340_CSV = (Join-Path $reportDir 'e9r_e9q_12340_compat.csv')
  $env:JJFB_E9Q_METRICS_CSV = (Join-Path $reportDir 'e9r_e9q_metrics_compat.csv')
  $env:JJFB_E8Z_SCREENSHOT = $shotBmp
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9r_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9r_after.bmp')
  $env:JJFB_E9B_HWND_CAPTURE = $hwndBmp
  $env:JJFB_E8Y_INSN_LIMIT = '12000'
  Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXT_LAYOUT_ASSIST -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXT_MEASURE_SHIM -ErrorAction SilentlyContinue

  switch ($CaseMode) {
    'natural' {
      Remove-Item Env:JJFB_PLATFORM_SCREEN_DIMS -EA SilentlyContinue
      Remove-Item Env:JJFB_FAST_TEXTCTX_ASSIST -EA SilentlyContinue
      $env:JJFB_FAST_TEXTCTX_ASSIST = '0'
    }
    'platform' {
      $env:JJFB_PLATFORM_SCREEN_DIMS = '1'
      Remove-Item Env:JJFB_FAST_TEXTCTX_ASSIST -EA SilentlyContinue
    }
    'compare' {
      Remove-Item Env:JJFB_PLATFORM_SCREEN_DIMS -EA SilentlyContinue
      $env:JJFB_FAST_TEXTCTX_ASSIST = '1'
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

  Write-Host "=== E9R case=$CaseName mode=$CaseMode timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT ==="
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
    Write-Host "E9R outer kill after ${OUTER_KILL_SEC}s"
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9R_|JJFB_PLATFORM_SCREEN_DIMS|JJFB_FAST_TEXTCTX|818_81C|PLATFORM_TEXT' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }

  Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
  Convert-BmpToPng $shotBmp $uiPng | Out-Null
  if (Test-Path $hwndPng) { Copy-Item -Force $hwndPng $uiPng -EA SilentlyContinue }
  $an = Analyze-E9R $caseLog
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

  $md = @"
# Stage E9R Platform TEXTCTX Dims Verdict

- **Case**: $CaseName
- **Mode**: $CaseMode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **Platform screen dims**: $($an.synced)
- **TEXTCTX assist removed**: $($an.assist_removed)
- **FAST_TEXTCTX_ASSIST used**: $($an.assist_used)
- **0x12340 measure**: $($an.measure)
- **0x11F00 draw**: $($an.draw11)
- **Transparent text**: $($an.transparent)
- **Loading UI / progress**: $($an.loading_ui) / $($an.progress)
- **HWND hold**: $($an.hold)
- **Log**: ``$caseLog``
- **Writer CSV**: ``$writerCsv``
- **HWND**: ``$hwndPng``
- **UI**: ``$uiPng``

## ABI / dims
- ``R9+0x818`` / ``R9+0x81C`` = screen W/H for ``0x305E78`` measure layout
- Same family as ``830/834/824``; no robotol STR writer observed
- Platform fills zero slots from host surface (prefer seeded 830/834, else 240x320)

## Assists still active (NOT product)
- BD0 / DisplayFirst / state / C9D remain
- TEXTCTX FAST assist must be OFF on platform success path
"@
  [System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

  Write-Host "== E9R_CASE_DONE name=$CaseName verdict=$($an.class) elapsed=${elapsed}s =="
  return $an
}

if (-not $SkipBuild) {
  Write-Host '=== E9R rebuild launcher_core + vmrp gwy ==='
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'build-i686\liblauncher_core.a')
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
}

$an = Invoke-E9RCase -CaseName $Mode -CaseMode $Mode
Write-Host "verdict=$(Join-Path $reportDir 'stage_e9r_textctx_naturalize_verdict.md')"
if ($an.class -match 'REGRESSED|NO_LOG') { exit 1 }
exit 0
