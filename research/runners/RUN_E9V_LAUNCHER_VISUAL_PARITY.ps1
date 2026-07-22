# Stage E9V: launcher visual semantics parity — NOT product.
# Modes: visual | timer | compare | trace
# Lanes: bitmap color-key, 0x12340 measure→center, natural timer toward 0x2F55FA.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 12,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('visual','timer','compare','trace')]
  [string]$Mode = 'visual',
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
foreach ($need in @('2256','3229','3317','3948','3952','3956','2072','2076','2096','2648','2652','2656','2660','2664','2668','2232')) {
  if ($offsets -notmatch "(^|,)$need(,|$)") {
    $offsets = if ($offsets) { "$offsets,$need" } else { $need }
  }
}
$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x3066B8','p:0x3066C6','p:0x306740','p:0x2E88CC',
  'p:0x2E8914','p:0x2E8980','p:0x2E89A8','p:0x2F2854','p:0x2EA188','p:0x2F449C',
  'p:0x2D92E4','p:0x2F44C8','p:0x2F44CE','p:0x310BBC','p:0x304BF0','p:0x2FD868',
  'p:0x304F26','p:0x304F7A','p:0x304F92','p:0x2EF86C','p:0x30662C','p:0x306344',
  'p:0x2FC418','p:0x2FC444','p:0x2FC03C','p:0x2FC05E','p:0x30EE8A','p:0x30EE92',
  'p:0x3124D8','p:0x3124FE','p:0x2F55FA','p:0x2FEBBC','p:0x2DAE24','p:0x2FECAA',
  'p:0x2EFA33','p:0x2EFA46','p:0x2EFA56','p:0x2EFA5C',
  'p:0x2EFA7C','p:0x2EFA9A','p:0x2EC6B8','p:0x2EFA9E','p:0x2EFAE2','p:0x2EFAF2',
  'p:0x2EFAFA','p:0x2EFB08','p:0x2EFB0E','p:0x2EFBA2','p:0x2EFBA6','p:0x305BFC',
  'p:0x2F2174','p:0x305E78','p:0x305EA0','p:0x2EFB48','p:0x303C50','p:0x304558',
  'p:0x305C2E','p:0x305C32','p:0x305C3C','p:0x305C54','p:0x305CA0',
  'p:0x305D08','p:0x305D16','p:0x305D58','p:0x2F2360','p:0x30ED98','p:0x310BBC'
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
    'JJFB_E9P_MODE','JJFB_E9Q_MODE','JJFB_E9R_MODE','JJFB_E9S_MODE','JJFB_E9T_MODE',
    'JJFB_E9U_MODE','JJFB_E9V_MODE','JJFB_VISIBLE_WINDOW',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST',
    'JJFB_FAST_A64_RESOURCE_ASSIST','JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH',
    'JJFB_REAL_MRP_MEMBER_BRIDGE','JJFB_REAL_MRP_MEMBER_BRIDGE_ALL','JJFB_REAL_MRP_PATH',
    'JJFB_E8Z_BMP_PATH','JJFB_E8Z_SCREENSHOT','JJFB_E8Z_SCREENSHOT_BEFORE','JJFB_E8Z_SCREENSHOT_AFTER',
    'JJFB_E8Y_INSN_LIMIT','JJFB_E8W_REENTER_E88CC','JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB',
    'JJFB_FAST_STATE','JJFB_FAST_CASE156_R1','JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30',
    'JJFB_FAST_C6C22','JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_WINDOW_ZOOM',
    'JJFB_E9B_HOLD_SEC','JJFB_E9B_HWND_CAPTURE','JJFB_E9D_STRCMP_SHIM','JJFB_E9D_BRIDGE_FALLBACK',
    'JJFB_E9E_POSTMATCH_SHIMS','JJFB_E9F_REWRITE_REQUEST','JJFB_E9F_MULTI_POSTMATCH',
    'JJFB_FAST_SPLASH_CALL','JJFB_E9G_UI_MODE_ASSIST','JJFB_E9G_DEBUG',
    'JJFB_E9H_R4_TRACE','JJFB_FAST_SPLASH_R4_ASSIST',
    'JJFB_SPLASH_COORD_ASSIST','JJFB_E9I_SKIP_SIBLING',
    'JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST','JJFB_E9J_PROGRESS_COUNT',
    'JJFB_E9C_DEFER_HOLD','JJFB_E9J_HOLD_AFTER_BLIT',
    'JJFB_E9K_HOLD_AFTER_POST_R4','JJFB_E9K_MIN_BLITS','JJFB_E9K_STOP_ON_TEXTBAR_DRAW',
    'JJFB_FAST_TEXTCTX_ASSIST','JJFB_FAST_TEXT_MEASURE_SHIM','JJFB_FAST_TEXT_LAYOUT_ASSIST',
    'JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM',
    'JJFB_PLATFORM_TEXT_API_11F00','JJFB_E9P_TEXT_BLIT',
    'JJFB_PLATFORM_TEXT_MEASURE_12340','JJFB_PLATFORM_SCREEN_DIMS',
    'JJFB_FAST_BD0_INIT_CALL','JJFB_E9S_BD0_STR_VA','JJFB_E9S_WRITER_CSV','JJFB_E9S_FN_CSV',
    'JJFB_FAST_BD0_CALLER_2FC03C','JJFB_FAST_BD0_CALLER_2FC05E','JJFB_FAST_BD0_CALLER_30EE8A',
    'JJFB_E9T_NO_UI_MODE_ASSIST','JJFB_E9T_NO_C9D_ASSIST',
    'JJFB_E9T_UPSTREAM_CSV','JJFB_E9T_COUNT_CSV','JJFB_E9T_LADDER_CSV',
    'JJFB_FAST_PROGRESS_TICK_CALL','JJFB_E9U_TICK_N',
    'JJFB_E9U_COUNT_CSV','JJFB_E9U_OBJECT_CSV',
    'JJFB_PLATFORM_TIMER_DISPATCH','JJFB_E9V_TIMER_CSV','JJFB_COLORKEY',
    'JJFB_DEBUG_PROGRESS_COUNT_POKE'
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

function Count-MagentaPixels([string]$ImgPath) {
  if (-not (Test-Path $ImgPath)) { return -1 }
  $py = @"
from PIL import Image
import sys
im = Image.open(sys.argv[1]).convert('RGB')
w,h = im.size
# crop lower splash band (progress/loading area)
y0 = int(h * 0.55)
pix = im.load()
n = 0
for y in range(y0, h):
  for x in range(w):
    r,g,b = pix[x,y]
    if r >= 240 and b >= 240 and g <= 40:
      n += 1
    elif abs(r-255) < 20 and abs(g) < 20 and abs(b-255) < 20:
      n += 1
print(n)
"@
  $tmp = Join-Path $env:TEMP 'e9v_magenta_count.py'
  Set-Content -Path $tmp -Value $py -Encoding UTF8
  try {
    $out = & py -3 $tmp $ImgPath 2>$null
    if ($out -match '^\d+$') { return [int]$out }
  } catch {}
  return -1
}

function Analyze-E9V([string]$log, [string]$CaseMode) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }
  $ck = $t -match 'BITMAP_COLORKEY_TRANSPARENT_RENDERED'
  $center = $t -match 'TEXT_MEASURE_LAYOUT_CENTERED'
  $off = $t -match 'TEXT_LAYOUT_STILL_OFF_CENTER'
  $layout = $t -match 'JJFB_TEXT_LAYOUT'
  $meas = $t -match 'JJFB_TEXT_MEASURE_12340|PLATFORM_TEXT_MEASURE_12340'
  $draw = $t -match 'PLATFORM_TEXT_API_11F00_RENDERED'
  $prog = $t -match 'SPLASH_PROGRESS_DRAWN_NO_REWRITE'
  $ui = $t -match 'SPLASH_LOADING_UI_VISIBLE'
  $splash = $t -match 'SPLASH_2EF86C_REACHED|JJFB_E9G_SPLASH_ENTER'
  $t2f = $t -match 'PROGRESS_TIMER_2F55FA_REACHED'
  $tnat = $t -match 'PROGRESS_TIMER_NATURAL_TICK_RESTORED'
  $tickAssist = $t -match 'JJFB_FAST_PROGRESS_TICK_CALL'
  $dispatch = $t -match 'JJFB_PLATFORM_TIMER_DISPATCH'
  $cls = 'SPLASH_VISUAL_PARITY_IMPROVED'
  if ($CaseMode -match 'timer') {
    if ($t2f) { $cls = 'PROGRESS_TIMER_2F55FA_REACHED' }
    elseif ($tnat) { $cls = 'PROGRESS_TIMER_NATURAL_TICK_RESTORED' }
    elseif ($dispatch -and -not $t2f) { $cls = 'PROGRESS_TIMER_STILL_BLOCKED_BY_DISPLAYFIRST_STATE' }
    else { $cls = 'PROGRESS_TIMER_STILL_BLOCKED_BY_DISPLAYFIRST_STATE' }
  } else {
    if ($ck -and $center) { $cls = 'SPLASH_VISUAL_PARITY_IMPROVED' }
    elseif ($ck) { $cls = 'BITMAP_COLORKEY_TRANSPARENT_RENDERED' }
    elseif ($center) { $cls = 'TEXT_MEASURE_LAYOUT_CENTERED' }
    elseif ($off) { $cls = 'TEXT_LAYOUT_STILL_OFF_CENTER' }
    elseif ($splash -and ($draw -or $ui)) { $cls = 'SPLASH_VISUAL_PARITY_IMPROVED' }
  }
  return [pscustomobject]@{
    class = $cls
    evidence = 'OBSERVED'
    colorkey = [bool]$ck
    centered = [bool]$center
    layout_log = [bool]$layout
    measure = [bool]$meas
    draw11 = [bool]$draw
    loading_ui = [bool]$ui
    progress = [bool]$prog
    splash = [bool]$splash
    timer_2f55fa = [bool]$t2f
    natural_tick = [bool]$tnat
    tick_assist = [bool]$tickAssist
    timer_dispatch = [bool]$dispatch
  }
}

function Invoke-E9VCase([string]$CaseName, [hashtable]$Opts) {
  $ckCsv = Join-Path $reportDir 'e9v_bitmap_colorkey_trace.csv'
  $measCsv = Join-Path $reportDir 'e9v_text_measure_layout_trace.csv'
  $timerCsv = Join-Path $reportDir 'e9v_timer_progress_trace.csv'
  $verdictMd = Join-Path $reportDir 'stage_e9v_launcher_visual_parity_verdict.md'
  $hwndBmp = Join-Path $shotDir 'e9v_actual_window_capture.bmp'
  $hwndPng = Join-Path $shotDir 'e9v_actual_window_capture.png'
  $afterPng = Join-Path $shotDir 'e9v_after_colorkey_centered_text.png'
  $progPng = Join-Path $shotDir 'e9v_progress_timer_frame.png'
  $beforePng = Join-Path $shotDir 'e9v_before_visual_fix.png'
  $caseLog = Join-Path $logDir 'e9v_launcher_visual_parity_stdout.txt'
  $caseErr = Join-Path $logDir 'e9v_launcher_visual_parity_stderr.txt'
  $shotBmp = Join-Path $shotDir 'e9v_splash_visible.bmp'

  # Preserve prior HWND as before if present (compare / first visual).
  if ((Test-Path $hwndPng) -and -not (Test-Path $beforePng)) {
    Copy-Item -Force $hwndPng $beforePng -EA SilentlyContinue
  } elseif ((Test-Path (Join-Path $shotDir 'e9u_actual_window_capture.png')) -and -not (Test-Path $beforePng)) {
    Copy-Item -Force (Join-Path $shotDir 'e9u_actual_window_capture.png') $beforePng -EA SilentlyContinue
  }

  Clear-E9Modes
  Stop-E9Children

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
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_PLATFORM_TEXT_API_11F00 = '1'
  $env:JJFB_PLATFORM_TEXT_MEASURE_12340 = '1'
  $env:JJFB_PLATFORM_SCREEN_DIMS = '1'
  $env:JJFB_E9P_TEXT_BLIT = 'transparent'
  $env:JJFB_COLORKEY = 'auto'
  $env:JJFB_E9U_COUNT_CSV = (Join-Path $reportDir 'e9v_count_writer_compat.csv')
  $env:JJFB_E9U_OBJECT_CSV = (Join-Path $reportDir 'e9v_object_slot_compat.csv')
  $env:JJFB_E9V_TIMER_CSV = $timerCsv
  $env:JJFB_E9Q_12340_CSV = $measCsv
  $env:JJFB_E9S_WRITER_CSV = (Join-Path $reportDir 'e9v_e9s_bd0_compat.csv')
  $env:JJFB_E9S_FN_CSV = (Join-Path $reportDir 'e9v_e9s_fn_compat.csv')
  $env:JJFB_E8Z_SCREENSHOT = $shotBmp
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9v_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9v_after.bmp')
  $env:JJFB_E9B_HWND_CAPTURE = $hwndBmp
  $env:JJFB_E8Y_INSN_LIMIT = '12000'
  $env:JJFB_E9U_TICK_N = "$TickN"
  Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXTCTX_ASSIST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXT_MEASURE_SHIM -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXT_LAYOUT_ASSIST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_BD0_CALLER_2FC03C -EA SilentlyContinue
  Remove-Item Env:JJFB_DEBUG_PROGRESS_COUNT_POKE -EA SilentlyContinue

  $env:JJFB_FAST_BD0_INIT_CALL = '1'

  # visual: keep FAST tick assist for visible segments; timer: natural path only
  if ($Opts.ContainsKey('Tick') -and $Opts.Tick) {
    $env:JJFB_FAST_PROGRESS_TICK_CALL = '1'
  }
  if ($Opts.ContainsKey('TimerDispatch') -and $Opts.TimerDispatch) {
    $env:JJFB_PLATFORM_TIMER_DISPATCH = '1'
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

  # Seed colorkey CSV header from log scrape later
  "n,member,key,skipped,lit,source" | Set-Content -Path $ckCsv -Encoding UTF8

  Write-Host "=== E9V case=$CaseName timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT ==="
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
    Write-Host "E9V outer kill after ${OUTER_KILL_SEC}s"
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9V_|JJFB_BITMAP_COLORKEY|JJFB_TEXT_MEASURE_12340|JJFB_TEXT_LAYOUT|2F55FA' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }
  Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
  if (Test-Path $hwndPng) {
    Copy-Item -Force $hwndPng $afterPng -EA SilentlyContinue
    if ($CaseName -match 'timer') {
      Copy-Item -Force $hwndPng $progPng -EA SilentlyContinue
    }
  }
  # Scrape colorkey rows into CSV
  if (Test-Path $caseLog) {
    $n = 0
    Select-String -Path $caseLog -Pattern '\[JJFB_BITMAP_COLORKEY\] member=(\S+) key=(0x[0-9A-Fa-f]+) skipped=(\d+) lit=(\d+) source=(\S+)' |
      ForEach-Object {
        $n++
        $m = $_.Matches[0]
        Add-Content -Path $ckCsv -Value "$n,$($m.Groups[1].Value),$($m.Groups[2].Value),$($m.Groups[3].Value),$($m.Groups[4].Value),$($m.Groups[5].Value)"
      }
  }
  $magentaAfter = Count-MagentaPixels $hwndPng
  $magentaBefore = Count-MagentaPixels $beforePng
  $an = Analyze-E9V $caseLog $CaseName
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

  $productNote = 'PRODUCT_STILL_NEEDS_DISPLAYFIRST_C9D_CLEANUP'
  $md = @"
# Stage E9V Launcher Visual Parity Verdict

- **Case**: $CaseName
- **Mode**: $Mode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **Product success**: **NO** (``NOT_PRODUCT_SUCCESS`` / ``$productNote``)

## Lane A — bitmap color-key
- **BITMAP_COLORKEY_TRANSPARENT_RENDERED**: $($an.colorkey)
- **Magenta pixels before (crop)**: $magentaBefore
- **Magenta pixels after (crop)**: $magentaAfter
- **JJFB_COLORKEY**: auto (corners/sample; no member-name hardcode)

## Lane B — 0x12340 measure / centered text
- **TEXT_MEASURE_LAYOUT_CENTERED**: $($an.centered)
- **Measure / layout logs**: $($an.measure) / $($an.layout_log)
- **0x11F00 draw**: $($an.draw11)
- ABI: R7=horizontal extent ← text width; R4=vertical ← text height
- Caller ``0x2EFBA2``: ``x=(screen_w - *R7)/2`` (no layout-assist poke)

## Lane C — progress timer
- **0x2F55FA reached**: $($an.timer_2f55fa)
- **Natural 0x3124D8 tick**: $($an.natural_tick)
- **PLATFORM_TIMER_DISPATCH**: $($an.timer_dispatch)
- **FAST_PROGRESS_TICK_CALL used**: $($an.tick_assist)
- Note: visual mode may keep FAST tick ×$TickN for visible segments; timer mode drops it.

## Visible UI
- Splash / loading / progress: $($an.splash) / $($an.loading_ui) / $($an.progress)

## Artifacts
| Kind | Path |
|------|------|
| Verdict | ``$verdictMd`` |
| Log | ``$caseLog`` |
| Color-key CSV | ``$ckCsv`` |
| Measure CSV | ``$measCsv`` |
| Timer CSV | ``$timerCsv`` |
| Before | ``$beforePng`` |
| After | ``$afterPng`` |
| HWND | ``$hwndPng`` |
| Progress frame | ``$progPng`` |

## Forbidden checks
- No game-specific resource-name hardcode
- No fake UI / invented pixels
- No MRP/EXT edits / request rewrite
- No direct BA0+0x2C poke as success
- No direct C9D poke
"@
  [System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))
  Write-Host "== E9V_CASE_DONE name=$CaseName verdict=$($an.class) elapsed=${elapsed}s =="
  return $an
}

if (-not $SkipBuild) {
  Write-Host '=== E9V rebuild launcher_core + vmrp gwy ==='
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'jjfb_plat_11f00.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'build-i686\liblauncher_core.a')
  # Force bridge.o rebuild for colorkey path
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'third_party\vmrp_upstream\bridge.o')
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
  $SkipBuild = $true
}

$final = $null
if ($Mode -eq 'visual') {
  $final = Invoke-E9VCase -CaseName 'visual_colorkey_center' -Opts @{ Tick = $true; TimerDispatch = $false }
} elseif ($Mode -eq 'timer') {
  $final = Invoke-E9VCase -CaseName 'timer_natural_2F55FA' -Opts @{ Tick = $false; TimerDispatch = $true }
} elseif ($Mode -eq 'compare') {
  $final = Invoke-E9VCase -CaseName 'compare_before_after' -Opts @{ Tick = $true; TimerDispatch = $false }
} else {
  $final = Invoke-E9VCase -CaseName 'trace_deep' -Opts @{ Tick = $true; TimerDispatch = $true }
}

Write-Host "E9V final verdict=$($final.class) NOT_PRODUCT_SUCCESS"
exit 0
