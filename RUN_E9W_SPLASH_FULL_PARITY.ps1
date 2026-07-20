# Stage E9W: splash full parity — AC8 naturalize + generic archive exact member resolve.
# Modes: visual | ac8 | resolve | debuglogo | timer
# NOT product success. No slogo/jjfb resource-name hardcode. No MRP edits. No fake UI.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 12,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('visual','ac8','resolve','debuglogo','timer')]
  [string]$Mode = 'visual',
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
foreach ($need in @('2256','2760','3229','3317','3948','3952','3956','2072','2076','2096','2648','2652','2656','2660','2664','2668','2232')) {
  # 2760 = 0xAC8
  if ($offsets -notmatch "(^|,)$need(,|$)") {
    $offsets = if ($offsets) { "$offsets,$need" } else { $need }
  }
}
$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x3066B8','p:0x3066C6','p:0x306740','p:0x2E88CC',
  'p:0x2E8914','p:0x2E8980','p:0x2E89A8','p:0x2F2854','p:0x2EA188','p:0x2F449C',
  'p:0x2D92E4','p:0x2F44C8','p:0x2F44CE','p:0x310BBC','p:0x304BF0','p:0x2FD868',
  'p:0x304F26','p:0x304F7A','p:0x304F92','p:0x2EF86C','p:0x2EF8AE','p:0x30662C','p:0x306344',
  'p:0x2EF9AA','p:0x2EF9DE','p:0x2FB28C','p:0x2E4062','p:0x2F68FF',
  'p:0x2FC418','p:0x2FC444','p:0x2FC03C','p:0x2FC05E','p:0x30EE8A','p:0x30EE92',
  'p:0x3124D8','p:0x3124FE','p:0x2F55FA','p:0x2FEBBC','p:0x2DAE24','p:0x2FECAA',
  'p:0x2EFA33','p:0x2EFA46','p:0x2EFA56','p:0x2EFA5C',
  'p:0x2EFA7C','p:0x2EFA9A','p:0x2EC6B8','p:0x2EFA9E','p:0x2EFAE2','p:0x2EFAF2',
  'p:0x2EFAFA','p:0x2EFB08','p:0x2EFB0E','p:0x2EFBA2','p:0x2EFBA6','p:0x305BFC',
  'p:0x2F2174','p:0x305E78','p:0x305EA0','p:0x2EFB48','p:0x303C50','p:0x303C68','p:0x303C84','p:0x303CA4','p:0x304558',
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
    'JJFB_E9U_MODE','JJFB_E9V_MODE','JJFB_E9W_MODE','JJFB_VISIBLE_WINDOW',
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
    'JJFB_FAST_PROGRESS_TICK_CALL','JJFB_E9U_TICK_N',
    'JJFB_PLATFORM_TIMER_DISPATCH','JJFB_E9V_TIMER_CSV','JJFB_COLORKEY',
    'JJFB_DEBUG_PROGRESS_COUNT_POKE','JJFB_DEBUG_AC8_FORCE','JJFB_E9W_ARCHIVE_EXACT',
    'JJFB_E9W_AC8_CSV','JJFB_E9W_RESOLVE_CSV','JJFB_E9W_LOGO_CSV','JJFB_E9W_TIMER_CSV'
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

function Analyze-E9W([string]$log, [string]$CaseMode) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }
  $ac8Nat = $t -match 'AC8_NATURAL_WRITER_FOUND_NEXT_GAP|JJFB_E9W_AC8_WRITE'
  $ac8Dbg = $t -match 'AC8_LOGO_BRANCH_REACHED_DEBUG_ONLY|JJFB_DEBUG_AC8_FORCE'
  $ac8Logo = $t -match 'SPLASH_LOGO_BRANCH_REACHED|AC8_LOGO_BRANCH_REACHED|JJFB_E9W_LOGO_SITE'
  $ac8Block = $t -match 'SPLASH_LOGO_BLOCKED_BY_AC8'
  $genFb = $t -match 'GENERIC_MEMBER_ARCHIVE_EXACT_FALLBACK_WORKS|GENERIC_AT_PACK_ARCHIVE_RESOLVE_WORKS'
  $atPack = $t -match 'GENERIC_AT_PACK_ARCHIVE_RESOLVE_WORKS'
  $slogoFb = $t -match 'SLOGO_RESOLVED_BY_GENERIC_FALLBACK|archive_exact_fallback'
  $logoDraw = $t -match 'SPLASH_LOGO_DRAWN_BY_GENERIC_RESOLVE|JJFB_E9W_LOGO_DRAW'
  $upper = $t -match 'SPLASH_UPPER_PANEL_VISIBLE'
  $gfxHeal = $t -match 'CLEARSCREEN_DRAWRECT_FP_HEALED|JJFB_E9W_GFX_HEAL'
  $drawRect = $t -match '\[JJFB_DRAW\] api=DrawRect'
  $logoStrNull = $t -match 'LOGO_NAME_BUF_NULL_NEXT_GAP|UC_MEM_WRITE_UNMAPPED at 0x0'
  $strSeed = $t -match 'LOGO_NAME_WORKBUF_SEEDED|JJFB_E9W_STR_WORKBUF_SEED'
  $mrpRes = $t -match '\[MRP_RESOLVE\]'
  $t2f = $t -match 'PROGRESS_TIMER_2F55FA_REACHED|0x2F55FA'
  $tickAssist = $t -match 'JJFB_FAST_PROGRESS_TICK_CALL'
  $ck = $t -match 'BITMAP_COLORKEY_TRANSPARENT_RENDERED'
  $center = $t -match 'TEXT_MEASURE_LAYOUT_CENTERED'
  $splash = $t -match 'SPLASH_2EF86C_REACHED|JJFB_E9G_SPLASH_ENTER'

  $cls = 'PRODUCT_STILL_NEEDS_DISPLAYFIRST_C9D_CLEANUP'
  # Match Mode tokens carefully: CaseName 'debuglogo_ac8_force' contains 'ac8'.
  if ($CaseMode -match '(^|_)timer($|_)' -or $CaseMode -eq 'timer') {
    if ($t -match 'PROGRESS_TIMER_2F55FA_REACHED') { $cls = 'PROGRESS_TIMER_2F55FA_REACHED' }
    else { $cls = 'PROGRESS_TIMER_STILL_BLOCKED_BY_DISPLAYFIRST_STATE' }
  } elseif ($CaseMode -match 'debuglogo') {
    if ($t -match 'SPLASH_FULL_VISUAL_PARITY_IMPROVED') { $cls = 'SPLASH_FULL_VISUAL_PARITY_IMPROVED' }
    elseif ($upper) { $cls = 'SPLASH_UPPER_PANEL_VISIBLE' }
    elseif ($logoDraw -or ($ac8Dbg -and $atPack) -or ($ac8Dbg -and $genFb)) {
      $cls = 'SPLASH_LOGO_DRAWN_BY_GENERIC_RESOLVE'
    }
    elseif ($ac8Dbg -and ($t -match 'JJFB_E9W_LOGO_SITE')) { $cls = 'SPLASH_LOGO_MEMBER_BL_REACHED' }
    elseif ($ac8Dbg -and $logoStrNull -and -not $strSeed) { $cls = 'LOGO_NAME_BUF_NULL_NEXT_GAP' }
    elseif ($ac8Dbg -and $drawRect -and $gfxHeal) { $cls = 'CLEARSCREEN_DRAWRECT_FP_HEALED' }
    elseif ($ac8Dbg -and $ac8Logo) { $cls = 'AC8_LOGO_BRANCH_REACHED_DEBUG_ONLY' }
    else { $cls = 'SPLASH_LOGO_BLOCKED_BY_AC8' }
  } elseif ($CaseMode -match '(^|_)ac8($|_)' -or $CaseMode -eq 'ac8') {
    if ($ac8Nat) { $cls = 'AC8_NATURAL_WRITER_FOUND_NEXT_GAP' }
    elseif ($ac8Logo -and -not $ac8Dbg) { $cls = 'AC8_LOGO_BRANCH_REACHED_BY_REAL_INIT' }
    elseif ($ac8Dbg -and $ac8Logo) { $cls = 'AC8_LOGO_BRANCH_REACHED_DEBUG_ONLY' }
    elseif ($ac8Block) { $cls = 'AC8_STILL_BLOCKED_BY_DISPLAYFIRST_STATE' }
    else { $cls = 'AC8_WRITER_REACHED_BRANCH_UNMET' }
  } elseif ($CaseMode -match 'resolve') {
    if ($atPack) { $cls = 'GENERIC_AT_PACK_ARCHIVE_RESOLVE_WORKS' }
    elseif ($genFb) { $cls = 'GENERIC_MEMBER_ARCHIVE_EXACT_FALLBACK_WORKS' }
    elseif ($slogoFb) { $cls = 'SLOGO_RESOLVED_BY_GENERIC_FALLBACK' }
    elseif ($mrpRes -and ($t -match 'source=sibling')) { $cls = 'GENERIC_MEMBER_ARCHIVE_EXACT_FALLBACK_WORKS' }
    elseif ($mrpRes -and ($t -match 'source=guest_index')) { $cls = 'MEMBER_RESOLVE_STILL_GUEST_INDEX_ONLY' }
    else { $cls = 'MEMBER_RESOLVE_FALLBACK_ABI_WRONG' }
  } else {
    # visual
    if ($upper) { $cls = 'SPLASH_FULL_VISUAL_PARITY_IMPROVED' }
    elseif ($logoDraw) { $cls = 'SPLASH_LOGO_DRAWN_BY_GENERIC_RESOLVE' }
    elseif ($ac8Logo -and $genFb) { $cls = 'SPLASH_LOGO_BRANCH_REACHED' }
    elseif ($genFb) { $cls = 'GENERIC_MEMBER_ARCHIVE_EXACT_FALLBACK_WORKS' }
    elseif ($ac8Nat) { $cls = 'AC8_NATURAL_WRITER_FOUND_NEXT_GAP' }
    elseif ($ac8Block -or ($splash -and -not $ac8Logo)) { $cls = 'SPLASH_LOGO_BLOCKED_BY_AC8' }
    elseif ($ck -and $center) { $cls = 'SPLASH_FULL_VISUAL_PARITY_IMPROVED' }
    else { $cls = 'PRODUCT_STILL_NEEDS_DISPLAYFIRST_C9D_CLEANUP' }
  }
  return [pscustomobject]@{
    class = $cls
    evidence = 'OBSERVED'
    ac8_natural = [bool]$ac8Nat
    ac8_debug = [bool]$ac8Dbg
    logo_branch = [bool]$ac8Logo
    generic_fallback = [bool]$genFb
    logo_draw = [bool]$logoDraw
    upper = [bool]$upper
    mrp_resolve = [bool]$mrpRes
    colorkey = [bool]$ck
    centered = [bool]$center
    splash = [bool]$splash
    timer_2f55fa = [bool]$t2f
    tick_assist = [bool]$tickAssist
  }
}

function Invoke-E9WCase([string]$CaseName, [hashtable]$Opts) {
  $ac8Csv = Join-Path $reportDir 'e9w_ac8_writer_trace.csv'
  $resolveCsv = Join-Path $reportDir 'e9w_member_resolve_trace.csv'
  $logoCsv = Join-Path $reportDir 'e9w_logo_draw_trace.csv'
  $timerCsv = Join-Path $reportDir 'e9w_timer_trace.csv'
  $verdictMd = Join-Path $reportDir 'stage_e9w_splash_full_parity_verdict.md'
  $hwndBmp = Join-Path $shotDir 'e9w_actual_window_capture.bmp'
  $hwndPng = Join-Path $shotDir 'e9w_actual_window_capture.png'
  $beforePng = Join-Path $shotDir 'e9w_before_logo.png'
  $afterPng = Join-Path $shotDir 'e9w_after_logo_branch.png'
  $fullPng = Join-Path $shotDir 'e9w_splash_full_parity.png'
  $caseLog = Join-Path $logDir 'e9w_splash_full_parity_stdout.txt'
  $caseErr = Join-Path $logDir 'e9w_splash_full_parity_stderr.txt'
  $shotBmp = Join-Path $shotDir 'e9w_splash_visible.bmp'

  if ((Test-Path (Join-Path $shotDir 'e9v_actual_window_capture.png')) -and -not (Test-Path $beforePng)) {
    Copy-Item -Force (Join-Path $shotDir 'e9v_actual_window_capture.png') $beforePng -EA SilentlyContinue
  }

  Clear-E9Modes
  Stop-E9Children

  # Static AC8 xref seed (appended by runtime)
  py -3 (Join-Path $Root 'tools\e9w_ac8_writer_xref.py') `
    --ext $ext --csv $ac8Csv --md (Join-Path $reportDir 'e9w_ac8_static_xref.md') | Out-Host

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
  $env:JJFB_E9W_AC8_CSV = $ac8Csv
  $env:JJFB_E9W_RESOLVE_CSV = $resolveCsv
  $env:JJFB_E9W_LOGO_CSV = $logoCsv
  $env:JJFB_E9W_TIMER_CSV = $timerCsv
  $env:JJFB_E9V_TIMER_CSV = $timerCsv
  $env:JJFB_E8Z_SCREENSHOT = $shotBmp
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9w_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9w_after.bmp')
  $env:JJFB_E9B_HWND_CAPTURE = $hwndBmp
  $env:JJFB_E8Y_INSN_LIMIT = '12000'
  $env:JJFB_E9U_TICK_N = "$TickN"
  $env:JJFB_FAST_BD0_INIT_CALL = '1'
  # Logo path may spend more guest insns before 2D92E4 (measure + gates).
  if ($Opts.ContainsKey('DebugAc8') -and $Opts.DebugAc8) {
    $env:JJFB_FAST_INSN_LIMIT = '2000000'
  } else {
    $env:JJFB_FAST_INSN_LIMIT = '500000'
  }
  Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXTCTX_ASSIST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXT_MEASURE_SHIM -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXT_LAYOUT_ASSIST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_SPLASH_PROGRESS_OBJECT_ASSIST -EA SilentlyContinue
  Remove-Item Env:JJFB_DEBUG_PROGRESS_COUNT_POKE -EA SilentlyContinue

  if ($Opts.ContainsKey('DebugAc8') -and $Opts.DebugAc8) {
    $env:JJFB_DEBUG_AC8_FORCE = '1'
  }
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
  # FAST_INSN_LIMIT set above for debuglogo headroom
  $env:JJFB_FAST_UNLOCK_CALL = '1'
  $env:JJFB_FAST_UNLOCK_WHEN = 'before'
  $env:JJFB_FAST_F6C_OBJECT_ASSIST = '1'
  $env:JJFB_FAST_F74_DESCRIPTOR_ASSIST = '1'
  $env:JJFB_E8W_REENTER_E88CC = '1'

  Write-Host "=== E9W case=$CaseName timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT ==="
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
    Write-Host "E9W outer kill after ${OUTER_KILL_SEC}s"
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9W_|MRP_RESOLVE|JJFB_E9W_AC8|GENERIC_MEMBER' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }
  Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
  if (Test-Path $hwndPng) {
    Copy-Item -Force $hwndPng $afterPng -EA SilentlyContinue
    Copy-Item -Force $hwndPng $fullPng -EA SilentlyContinue
  }
  # Prefer Mode token; CaseName may embed other tokens (e.g. debuglogo_ac8_force).
  $an = Analyze-E9W $caseLog $Mode
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

  $md = @"
# Stage E9W Splash Full Parity Verdict

- **Case**: $CaseName
- **Mode**: $Mode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **Product success**: **NO** (``NOT_PRODUCT_SUCCESS`` / ``PRODUCT_STILL_NEEDS_DISPLAYFIRST_C9D_CLEANUP``)

## Lane A — AC8 / splash logo branch
- **Natural AC8 write**: $($an.ac8_natural)
- **DEBUG_ONLY AC8 force**: $($an.ac8_debug)
- **Logo branch reached**: $($an.logo_branch)
- Note: direct AC8 poke never counts as product/stage success.

## Lane B — generic member resolve
- **MRP_RESOLVE logs**: $($an.mrp_resolve)
- **GENERIC archive exact fallback**: $($an.generic_fallback)
- Order: guest_index → sibling → archive_exact / @-pack (`gwy/jjfbol/<stem>.mrp`) — no name hardcode

## Lane C — logo visual
- **Logo draw**: $($an.logo_draw)
- **Upper panel visible**: $($an.upper)

## Lane D — timer (secondary)
- **0x2F55FA**: $($an.timer_2f55fa)
- **FAST tick assist**: $($an.tick_assist)

## E9V keeps
- Color-key / centered text: $($an.colorkey) / $($an.centered)
- Splash reached: $($an.splash)

## Artifacts
| Kind | Path |
|------|------|
| Verdict | ``$verdictMd`` |
| Log | ``$caseLog`` |
| AC8 CSV | ``$ac8Csv`` |
| Resolve CSV | ``$resolveCsv`` |
| Logo CSV | ``$logoCsv`` |
| Timer CSV | ``$timerCsv`` |
| Before | ``$beforePng`` |
| After logo | ``$afterPng`` |
| Full parity | ``$fullPng`` |
| HWND | ``$hwndPng`` |

## Forbidden checks
- No slogo / jjfb resource-name hardcode in common resolver
- No fake UI / invented pixels / MRP-EXT edits / request rewrite
- No direct AC8 poke as success
- No direct BA0+0x2C / C9D poke as success
"@
  [System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))
  Write-Host "== E9W_CASE_DONE name=$CaseName verdict=$($an.class) elapsed=${elapsed}s =="
  return $an
}

if (-not $SkipBuild) {
  Write-Host '=== E9W rebuild launcher_core + vmrp gwy ==='
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'build-i686\liblauncher_core.a')
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'third_party\vmrp_upstream\bridge.o')
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
  $SkipBuild = $true
}

$final = $null
# Max 3 cases: visual / ac8 / resolve first; debuglogo optional; timer secondary.
if ($Mode -eq 'visual') {
  $final = Invoke-E9WCase -CaseName 'visual_ac8_resolve' -Opts @{ Tick = $true; TimerDispatch = $false; DebugAc8 = $false }
} elseif ($Mode -eq 'ac8') {
  $final = Invoke-E9WCase -CaseName 'ac8_writer_trace' -Opts @{ Tick = $true; TimerDispatch = $false; DebugAc8 = $false }
} elseif ($Mode -eq 'resolve') {
  # Exercise guest_index + sibling on loading path (AC8=0). Logo archive_exact
  # needs post-AC8 0x303C68 gate — covered by debuglogo, not here.
  $final = Invoke-E9WCase -CaseName 'resolve_archive_exact' -Opts @{ Tick = $true; TimerDispatch = $false; DebugAc8 = $false }
} elseif ($Mode -eq 'debuglogo') {
  $final = Invoke-E9WCase -CaseName 'debuglogo_ac8_force' -Opts @{ Tick = $true; TimerDispatch = $false; DebugAc8 = $true }
} else {
  $final = Invoke-E9WCase -CaseName 'timer_natural_2F55FA' -Opts @{ Tick = $false; TimerDispatch = $true; DebugAc8 = $false }
}

Write-Host "E9W final verdict=$($final.class) NOT_PRODUCT_SUCCESS"
exit 0
