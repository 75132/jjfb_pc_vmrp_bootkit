# Stage E9Y-Fix: downimage/update-ready event contract
# Modes: parse | event_trace | event_call
# NOT product. No AC8 poke / 8D8 seed / branch patch / MRP edits / fake UI.
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('parse','event_trace','event_call')]
  [string]$Mode = 'parse',
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
$outFix = Join-Path $Root 'out\e9y_fix'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $shotDir, $outFix | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(150, $Seconds))
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
foreach ($need in @('2256','2760','3229','3317','3948','3952','3956','2072','2076','2096','2648','2652','2656','2660','2664','2668','2232','2688','2692','2696','2700','2704','2712','2732','2736','2740','2748','2756','2760','2776','2780','2784')) {
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
  'p:0x30CBBC','p:0x30CD82','p:0x30E15E',
  'p:0x2FC418','p:0x2FC444','p:0x2FC03C','p:0x2FC05E','p:0x30EE8A','p:0x30EE92',
  'p:0x3124D8','p:0x3124FE','p:0x2F55FA','p:0x2FEBBC','p:0x2DAE24','p:0x2FECAA',
  'p:0x30D300','p:0x300158','p:0x3056D5','p:0x2FE84C','p:0x2FEC88',
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
    'JJFB_E9U_MODE','JJFB_E9V_MODE','JJFB_E9W_MODE','JJFB_E9Y_MODE','JJFB_E9Y_FIX_MODE',
    'JJFB_VISIBLE_WINDOW',
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
    'JJFB_E9W_AC8_CSV','JJFB_E9W_RESOLVE_CSV','JJFB_E9W_LOGO_CSV','JJFB_E9W_TIMER_CSV',
    'JJFB_E9Y_NO_DEBUG_AC8','JJFB_E9Y_NO_WORKBUF_SEED','JJFB_PLATFORM_WORKBUF_ALLOC',
    'JJFB_FAST_REAL_SPLASH_STATE_EVENT','JJFB_FAST_DOWNIMAGE_READY_EVENT',
    'JJFB_E9Y_DOWNIMAGE_READY_PC','JJFB_E9Y_DOWNIMAGE_READY_ABI',
    'JJFB_E9Y_STATE_CSV','JJFB_E9Y_WORKBUF_CSV','JJFB_E9Y_EVENT_CSV',
    'JJFB_E9Y_A80_CSV','JJFB_E9Y_CONTRACT_CSV'
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

function Invoke-ParseOnly {
  Write-Host '=== E9Y-Fix parse: inventory + logo branch annotate ==='
  py -3 (Join-Path $Root 'tools\dump_jjfb_downimage_contract.py') | Out-Host
  if ($LASTEXITCODE -ne 0) { throw 'dump_jjfb_downimage_contract failed' }
  py -3 (Join-Path $Root 'tools\e9y_fix_annotate_logo_branch.py') | Out-Host
  if ($LASTEXITCODE -ne 0) { throw 'e9y_fix_annotate_logo_branch failed' }
  $inv = Join-Path $reportDir 'e9y_downimage_contract_inventory.csv'
  $ann = Join-Path $outFix '2ef86c_logo_branch_annotated.txt'
  $xref = Join-Path $outFix 'downimage_string_xrefs.txt'
  $md = @"
# Stage E9Y-Fix Downimage Event Verdict (parse)

- **Mode**: parse
- **Product success**: **NO** (``NOT_PRODUCT``)
- **Tags**: ``DOWNIMAGE_CONTRACT_PARSED``
- **Inventory**: ``$inv``
- **Annotated splash**: ``$ann``
- **String xrefs**: ``$xref``

## Notes
- Upper splash assets live on downimage pack / @pack path, not main-MRP-only.
- AC8 is a bool logo gate at 0x2EF8AE; no natural STR AC8=1 in static scan.
"@
  [System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e9y_fix_downimage_event_verdict.md'), $md, [System.Text.UTF8Encoding]::new($false))
  Write-Host 'DOWNIMAGE_CONTRACT_PARSED'
  return [pscustomobject]@{ class = 'DOWNIMAGE_CONTRACT_PARSED'; evidence = 'OBSERVED' }
}

function Analyze-E9YFix([string]$log) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }

  $contractParsed = Test-Path (Join-Path $reportDir 'e9y_downimage_contract_inventory.csv')
  $a80 = $t -match 'A80_AE0_SPLASH_STRUCT_SOURCE_FOUND'
  $cand = $t -match 'DOWNIMAGE_READY_EVENT_CANDIDATE_FOUND'
  $logoEvt = $t -match 'DOWNIMAGE_READY_EVENT_REACHED_LOGO_BRANCH|AC8_LOGO_BRANCH_REACHED_BY_REAL_EVENT'
  $logoNoDbg = $t -match 'SPLASH_LOGO_BRANCH_WITHOUT_DEBUG_AC8|SPLASH_LOGO_BRANCH_REACHED'
  $ac8Dbg = $t -match 'JJFB_DEBUG_AC8_FORCE\]|AC8_LOGO_BRANCH_REACHED_DEBUG_ONLY'
  $upper = $t -match 'SPLASH_UPPER_PANEL_VISIBLE|SPLASH_FULL_VISUAL_PARITY'
  $full = $t -match 'SPLASH_FULL_PARITY_WITH_REAL_DOWNIMAGE_EVENT'
  $blocked = $t -match 'AC8_STILL_BLOCKED_BY_MISSING_UPDATE_EVENT'
  $fastTried = $t -match 'JJFB_FAST_DOWNIMAGE_READY_EVENT\] target_pc'
  $dfOnly = $t -match 'AC8_STILL_DISPLAYFIRST_ONLY|branch=loading_only'
  $wb = $t -match 'WORKBUF_30CD82_ALLOC_REACHED|WORKBUF_8D8_NATURAL'
  $abiSkip = $t -match 'skip=abi_unknown'
  $seed = $t -match 'LOGO_NAME_WORKBUF_SEEDED|JJFB_E9W_STR_WORKBUF_SEED'

  $cls = 'PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN'
  if ($full -and -not $ac8Dbg -and -not $seed) { $cls = 'SPLASH_FULL_PARITY_WITH_REAL_DOWNIMAGE_EVENT' }
  elseif ($logoEvt -and -not $ac8Dbg) { $cls = 'AC8_LOGO_BRANCH_REACHED_BY_REAL_EVENT' }
  elseif ($logoNoDbg -and -not $ac8Dbg -and $cand) { $cls = 'DOWNIMAGE_READY_EVENT_REACHED_LOGO_BRANCH' }
  elseif ($cand) { $cls = 'DOWNIMAGE_READY_EVENT_CANDIDATE_FOUND' }
  elseif ($blocked -or ($fastTried -and $dfOnly)) { $cls = 'AC8_STILL_BLOCKED_BY_MISSING_UPDATE_EVENT' }
  elseif ($abiSkip) { $cls = 'AC8_STILL_BLOCKED_BY_MISSING_UPDATE_EVENT' }
  elseif ($a80) { $cls = 'A80_AE0_SPLASH_STRUCT_SOURCE_FOUND' }
  elseif ($dfOnly) { $cls = 'AC8_STILL_DISPLAYFIRST_ONLY' }
  elseif ($contractParsed) { $cls = 'DOWNIMAGE_CONTRACT_PARSED' }

  return [pscustomobject]@{
    class = $cls
    evidence = 'OBSERVED'
    a80 = [bool]$a80
    cand = [bool]$cand
    logo_evt = [bool]$logoEvt
    logo_no_debug = [bool]($logoNoDbg -and -not $ac8Dbg)
    upper = [bool]$upper
    workbuf = [bool]$wb
    seed_used = [bool]$seed
    ac8_debug = [bool]$ac8Dbg
    abi_skip = [bool]$abiSkip
  }
}

function Invoke-E9YFixCase([string]$CaseName, [hashtable]$Opts) {
  $stateCsv = Join-Path $reportDir 'e9y_ac8_state_chain_trace.csv'
  $workCsv = Join-Path $reportDir 'e9y_workbuf_alloc_trace.csv'
  $eventCsv = Join-Path $reportDir 'e9y_event_timer_trace.csv'
  $a80Csv = Join-Path $reportDir 'e9y_a80_ae0_struct_trace.csv'
  $contractCsv = Join-Path $reportDir 'e9y_event_contract_trace.csv'
  $verdictMd = Join-Path $reportDir 'stage_e9y_fix_downimage_event_verdict.md'
  $hwndBmp = Join-Path $shotDir 'e9y_fix_hwnd.bmp'
  $hwndPng = Join-Path $shotDir 'e9y_fix_hwnd.png'
  $fullPng = Join-Path $shotDir 'e9y_fix_no_debug_full_splash.png'
  $caseLog = Join-Path $logDir 'e9y_fix_downimage_event_stdout.txt'
  $caseErr = Join-Path $logDir 'e9y_fix_downimage_event_stderr.txt'
  $shotBmp = Join-Path $shotDir 'e9y_fix_splash_visible.bmp'

  Clear-E9Modes
  Stop-E9Children

  $env:JJFB_E9Y_MODE = '1'
  $env:JJFB_E9Y_FIX_MODE = '1'
  $env:JJFB_E9Y_NO_DEBUG_AC8 = '1'
  $env:JJFB_E9Y_NO_WORKBUF_SEED = '1'
  $env:JJFB_PLATFORM_WORKBUF_ALLOC = '1'
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
  $env:JJFB_E9Y_STATE_CSV = $stateCsv
  $env:JJFB_E9Y_WORKBUF_CSV = $workCsv
  $env:JJFB_E9Y_EVENT_CSV = $eventCsv
  $env:JJFB_E9Y_A80_CSV = $a80Csv
  $env:JJFB_E9Y_CONTRACT_CSV = $contractCsv
  $env:JJFB_E9V_TIMER_CSV = $eventCsv
  $env:JJFB_E8Z_SCREENSHOT = $shotBmp
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9y_fix_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9y_fix_after.bmp')
  $env:JJFB_E9B_HWND_CAPTURE = $hwndBmp
  $env:JJFB_E8Y_INSN_LIMIT = '12000'
  $env:JJFB_E9U_TICK_N = "$TickN"
  $env:JJFB_FAST_BD0_INIT_CALL = '1'
  $env:JJFB_FAST_INSN_LIMIT = '500000'
  $env:JJFB_FAST_PROGRESS_TICK_CALL = '1'
  Remove-Item Env:JJFB_DEBUG_AC8_FORCE -EA SilentlyContinue
  Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -EA SilentlyContinue

  if ($Opts.ContainsKey('DownimageReady') -and $Opts.DownimageReady) {
    if (-not $Opts.ContainsKey('AbiOk') -or -not $Opts.AbiOk) {
      Write-Host '[E9Y-Fix] event_call refused: candidate ABI not explicit (no poke)'
      $env:JJFB_FAST_DOWNIMAGE_READY_EVENT = '1'
      # Intentionally leave ABI unset → runtime logs skip=abi_unknown
    } else {
      $env:JJFB_FAST_DOWNIMAGE_READY_EVENT = '1'
      $env:JJFB_E9Y_DOWNIMAGE_READY_ABI = '1'
      if ($Opts.ContainsKey('ReadyPc') -and $Opts.ReadyPc) {
        $env:JJFB_E9Y_DOWNIMAGE_READY_PC = "$($Opts.ReadyPc)"
      }
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
  $env:JJFB_FAST_UNLOCK_CALL = '1'
  $env:JJFB_FAST_UNLOCK_WHEN = 'before'
  $env:JJFB_FAST_F6C_OBJECT_ASSIST = '1'
  $env:JJFB_FAST_F74_DESCRIPTOR_ASSIST = '1'
  $env:JJFB_E8W_REENTER_E88CC = '1'

  Write-Host "=== E9Y-Fix case=$CaseName timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT ==="
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
    Write-Host "E9Y-Fix outer kill after ${OUTER_KILL_SEC}s"
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9Y_|JJFB_E9W_|DOWNIMAGE_|A80_' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }
  Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
  if (Test-Path $hwndPng) {
    Copy-Item -Force $hwndPng $fullPng -EA SilentlyContinue
  } elseif (Test-Path $shotBmp) {
    Convert-BmpToPng $shotBmp $fullPng | Out-Null
  }
  $an = Analyze-E9YFix $caseLog
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

  $md = @"
# Stage E9Y-Fix Downimage Event Verdict

- **Case**: $CaseName
- **Mode**: $Mode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **Product success**: **NO** (``NOT_PRODUCT``)

## Tags
| Tag | Hit |
|-----|-----|
| DOWNIMAGE_CONTRACT_PARSED | $(Test-Path (Join-Path $reportDir 'e9y_downimage_contract_inventory.csv')) |
| A80_AE0_SPLASH_STRUCT_SOURCE_FOUND | $($an.a80) |
| DOWNIMAGE_READY_EVENT_CANDIDATE_FOUND | $($an.cand) |
| Logo via real event / no DEBUG | $($an.logo_evt) / $($an.logo_no_debug) |
| Upper panel | $($an.upper) |
| Workbuf natural | $($an.workbuf) |
| Seed used (must False) | $($an.seed_used) |
| DEBUG AC8 (must False) | $($an.ac8_debug) |
| ABI skip | $($an.abi_skip) |

## Artifacts
| Kind | Path |
|------|------|
| Verdict | ``$verdictMd`` |
| Log | ``$caseLog`` |
| Inventory | ``reports/e9y_downimage_contract_inventory.csv`` |
| A80 CSV | ``$a80Csv`` |
| Contract CSV | ``$contractCsv`` |
| Annotated | ``out/e9y_fix/2ef86c_logo_branch_annotated.txt`` |
| Screenshot | ``$fullPng`` |

## Forbidden checks
- No direct AC8 poke as success
- No 8D8 seed as success
- No 0x2EF8AE patch / PC jump to logo
- No show1 hardcode / fake UI / MRP-EXT edits

## Product
``PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN`` until downimage-ready arrives from natural boot.
"@
  [System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))
  Write-Host "== E9Y_FIX_DONE name=$CaseName verdict=$($an.class) elapsed=${elapsed}s =="
  return $an
}

# --- main ---
if ($Mode -eq 'parse') {
  $final = Invoke-ParseOnly
  Write-Host "E9Y-Fix final verdict=$($final.class) mode=$Mode"
  exit 0
}

if (-not $SkipBuild) {
  Write-Host '=== E9Y-Fix rebuild launcher_core + vmrp gwy ==='
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

# Ensure parse artifacts exist
if (-not (Test-Path (Join-Path $reportDir 'e9y_downimage_contract_inventory.csv'))) {
  Invoke-ParseOnly | Out-Null
}

$final = $null
if ($Mode -eq 'event_trace') {
  $final = Invoke-E9YFixCase -CaseName 'event_trace_a80_contract' -Opts @{
    DownimageReady = $false
  }
} else {
  # event_call: enable FAST with observed init entry 0x2FE82C (contains 0x2FE84C)
  $final = Invoke-E9YFixCase -CaseName 'event_call_downimage_ready' -Opts @{
    DownimageReady = $true
    AbiOk = $true
    ReadyPc = '0x2FE82C'
  }
}

Write-Host "E9Y-Fix final verdict=$($final.class) mode=$Mode"
exit 0
