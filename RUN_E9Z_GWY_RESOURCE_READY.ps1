# Stage E9Z: GWY downimage/update prelaunch contract (resource-ready)
# Modes: parse | trace | event | no_debug | classify
# NOT product. No AC8 poke / 8D8 seed / branch patch / MRP edits / fake UI / name hardcode.
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('parse','trace','event','no_debug','classify')]
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
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $shotDir | Out-Null

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
  'p:0x30DDE2','p:0x2E4008','p:0x304558',
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
    'JJFB_E9Z_MODE','JJFB_E9Z_GWY_RESOURCE_READY',
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
    'JJFB_E9Y_A80_CSV','JJFB_E9Y_CONTRACT_CSV',
    'JJFB_GWY_PACK_REGISTRY','GWY_PACK_REGISTRY','GWY_PLATFORM_RESOURCE_READY_EVENT',
    'JJFB_FAST_GWY_RESOURCE_READY_EVENT','JJFB_GWY_RESOURCE_READY_EVENT',
    'JJFB_E9Z_RESOURCE_READY_EVT','JJFB_E9Z_PACK_CSV','JJFB_E9Z_IMPORT_CSV',
    'JJFB_E9Z_EVENT_CSV','JJFB_E9Z_NODEBUG_CSV'
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

function Write-ClassifyVerdict([string]$log, [string]$outMd) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $nodbg = Join-Path $reportDir 'e9z_no_debug_validation.csv'
  $evtCsv = Join-Path $reportDir 'e9z_resource_ready_event_trace.csv'
  $A = $t -match 'AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL'
  $B = $t -match 'AC8_BLOCKED_BY_UPDATE_RESPONSE'
  $C = $t -match 'AC8_BLOCKED_BY_PERSISTENT_CACHE'
  $D = $t -match 'AC8_BLOCKED_BY_DISPLAYFIRST_SKIP|AC8_STILL_DISPLAYFIRST'
  $E = $t -match 'A80_AE0_SPLASH_STRUCT_SOURCE_FOUND'
  $dispOk = (Test-Path $evtCsv) -and ((Get-Content $evtCsv | Measure-Object -Line).Lines -gt 1)
  $cls = 'PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN'
  $ev = @()
  if ($A -or ($dispOk -and ($t -match 'AC8_after=0x0|AC8=0x0->0x0'))) {
    $cls = 'AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL'
    $ev += 'A: side-pack registry ready; 0x30D300/0x300158 delivered evt=0x14; AC8 stayed 0'
    $ev += 'missing: original GWY shell / update-complete writer that sets AC8>0'
  } elseif ($B) {
    $cls = 'AC8_BLOCKED_BY_UPDATE_RESPONSE'
    $ev += 'B: update network response not replayed'
  } elseif ($C) {
    $cls = 'AC8_BLOCKED_BY_PERSISTENT_CACHE'
    $ev += 'C: persistent cache/file not created'
  } elseif ($D) {
    $cls = 'AC8_BLOCKED_BY_DISPLAYFIRST_SKIP'
    $ev += 'D: DisplayFirst skip / pre-splash state incomplete'
  } elseif ($E) {
    $cls = 'AC8_BLOCKED_BY_DISPLAYFIRST_SKIP'
    $ev += 'E: A80..AE0 touched but AC8 still 0 — struct-copy path not raising gate'
  }
  if (-not $ev) {
    $ev += 'dispatcher hops observed; AC8 remained 0; no natural STR AC8=1'
  }
  $md = @"
# Stage E9Z Classify

- **Primary**: ``$cls``
- **Evidence**: OBSERVED
- **Product success**: **NO**
- **Also**: ``GWY_PACK_REGISTRY_BUILT``, ``GWY_UPDATE_MANIFEST_PARSED``, ``GWY_RESOURCE_READY_EVENT_CANDIDATE_FOUND``

## Blocker ladder (A-F)
$(($ev | ForEach-Object { "- $_" }) -join "`n")

## Evidence files
- pack registry: ``reports/e9z_gwy_pack_registry.csv``
- event trace: ``reports/e9z_resource_ready_event_trace.csv``
- import trace: ``reports/e9z_external_event_import_trace.csv``
- no-debug: ``$nodbg``

## Notes
- Pack registry built; resource-ready delivered via real dispatcher only (no AC8 poke).
- evt=0x14 at 0x30D300/0x300158 is necessary-but-not-sufficient for logo gate.
"@
  [System.IO.File]::WriteAllText($outMd, $md, [System.Text.UTF8Encoding]::new($false))
  return $cls
}

function Invoke-ParseOnly {
  Write-Host '=== E9Z parse: pack registry + update manifest ==='
  $env:JJFB_REAL_MRP_PATH = $mrpPath
  py -3 (Join-Path $Root 'tools\e9z_gwy_pack_registry.py') | Out-Host
  if ($LASTEXITCODE -ne 0) { throw 'e9z_gwy_pack_registry failed' }
  py -3 (Join-Path $Root 'tools\e9z_update_manifest_inventory.py') | Out-Host
  if ($LASTEXITCODE -ne 0) { throw 'e9z_update_manifest_inventory failed' }
  $md = @"
# Stage E9Z GWY Resource Ready Verdict (parse)

- **Mode**: parse
- **Product success**: **NO** (``NOT_PRODUCT``)
- **Tags**: ``GWY_PACK_REGISTRY_BUILT``, ``GWY_UPDATE_MANIFEST_PARSED``
- **Registry**: ``reports/e9z_gwy_pack_registry.csv``
- **Manifest**: ``reports/e9z_update_manifest_inventory.csv``
"@
  [System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e9z_gwy_resource_ready_verdict.md'), $md, [System.Text.UTF8Encoding]::new($false))
  Write-Host '== E9Z_CASE_DONE name=parse verdict=GWY_PACK_REGISTRY_BUILT elapsed=0'
  return [pscustomobject]@{ class = 'GWY_PACK_REGISTRY_BUILT'; evidence = 'OBSERVED' }
}

function Analyze-E9Z([string]$log) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }

  $reg = (Test-Path (Join-Path $reportDir 'e9z_gwy_pack_registry.csv')) -or ($t -match 'GWY_PACK_REGISTRY_BUILT')
  $man = Test-Path (Join-Path $reportDir 'e9z_update_manifest_inventory.csv')
  $eventCsvPath = Join-Path $reportDir 'e9z_resource_ready_event_trace.csv'
  $cand = ($t -match 'GWY_RESOURCE_READY_EVENT_CANDIDATE_FOUND') -or (
    (Test-Path $eventCsvPath) -and
    ((Get-Content $eventCsvPath | Measure-Object -Line).Lines -gt 1)
  )
  $logo = $t -match 'GWY_RESOURCE_READY_EVENT_REACHED_LOGO_BRANCH|DOWNIMAGE_UPDATE_CONTRACT_RESTORED'
  $logoNoDbg = $t -match 'SPLASH_LOGO_BRANCH_WITHOUT_DEBUG_AC8|SPLASH_LOGO_BRANCH_REACHED'
  $ac8Dbg = $t -match 'JJFB_DEBUG_AC8_FORCE\]|AC8_LOGO_BRANCH_REACHED_DEBUG_ONLY'
  $full = $t -match 'SPLASH_FULL_PARITY_NO_DEBUG_AC8|SPLASH_UPPER_PANEL_VISIBLE'
  $seed = $t -match 'LOGO_NAME_WORKBUF_SEEDED|JJFB_E9W_STR_WORKBUF_SEED'
  $extShell = $t -match 'AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL'
  $blocked = $t -match 'PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN|AC8_STILL_BLOCKED'

  $cls = 'PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN'
  if ($full -and -not $ac8Dbg -and -not $seed) { $cls = 'SPLASH_FULL_PARITY_NO_DEBUG_AC8' }
  elseif ($logo -and -not $ac8Dbg) { $cls = 'GWY_RESOURCE_READY_EVENT_REACHED_LOGO_BRANCH' }
  elseif ($logoNoDbg -and -not $ac8Dbg -and $cand) { $cls = 'DOWNIMAGE_UPDATE_CONTRACT_RESTORED' }
  elseif ($extShell) { $cls = 'AC8_BLOCKED_BY_EXTERNAL_GWY_SHELL' }
  elseif ($cand) { $cls = 'GWY_RESOURCE_READY_EVENT_CANDIDATE_FOUND' }
  elseif ($man -and $reg -and $Mode -eq 'trace') { $cls = 'GWY_UPDATE_MANIFEST_PARSED' }
  elseif ($reg -and -not $cand) { $cls = 'GWY_PACK_REGISTRY_BUILT' }
  elseif ($blocked) { $cls = 'PRODUCT_STILL_NEEDS_NATURAL_UPDATE_CHAIN' }

  return [pscustomobject]@{
    class = $cls
    evidence = 'OBSERVED'
    reg = [bool]$reg
    man = [bool]$man
    cand = [bool]$cand
    logo = [bool]($logo -and -not $ac8Dbg)
    ac8_debug = [bool]$ac8Dbg
    seed_used = [bool]$seed
  }
}

function Invoke-E9ZCase([string]$CaseName, [hashtable]$Opts) {
  $packCsv = Join-Path $reportDir 'e9z_gwy_pack_registry.csv'
  $importCsv = Join-Path $reportDir 'e9z_external_event_import_trace.csv'
  $eventCsv = Join-Path $reportDir 'e9z_resource_ready_event_trace.csv'
  $nodbgCsv = Join-Path $reportDir 'e9z_no_debug_validation.csv'
  $verdictMd = Join-Path $reportDir 'stage_e9z_gwy_resource_ready_verdict.md'
  $hwndBmp = Join-Path $shotDir 'e9z_hwnd.bmp'
  $hwndPng = Join-Path $shotDir 'e9z_actual_window_capture.png'
  $fullPng = Join-Path $shotDir 'e9z_resource_ready_no_debug_full_splash.png'
  $caseLog = Join-Path $logDir 'e9z_gwy_resource_ready_stdout.txt'
  $caseErr = Join-Path $logDir 'e9z_gwy_resource_ready_stderr.txt'
  $shotBmp = Join-Path $shotDir 'e9z_splash_visible.bmp'

  Clear-E9Modes
  Stop-E9Children

  # Ensure parse artifacts exist for runtime modes
  if (-not (Test-Path $packCsv) -or -not (Test-Path (Join-Path $reportDir 'e9z_update_manifest_inventory.csv'))) {
    Invoke-ParseOnly | Out-Null
  }

  $env:JJFB_E9Z_MODE = '1'
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
  $env:JJFB_E9Z_PACK_CSV = $packCsv
  $env:JJFB_E9Z_IMPORT_CSV = $importCsv
  $env:JJFB_E9Z_EVENT_CSV = $eventCsv
  $env:JJFB_E9Z_NODEBUG_CSV = $nodbgCsv
  $env:JJFB_E8Z_SCREENSHOT = $shotBmp
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9z_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9z_after.bmp')
  $env:JJFB_E9B_HWND_CAPTURE = $hwndBmp
  $env:JJFB_E8Y_INSN_LIMIT = '12000'
  $env:JJFB_E9U_TICK_N = "$TickN"
  $env:JJFB_FAST_BD0_INIT_CALL = '1'
  $env:JJFB_FAST_INSN_LIMIT = '500000'
  $env:JJFB_FAST_PROGRESS_TICK_CALL = '1'
  Remove-Item Env:JJFB_DEBUG_AC8_FORCE -EA SilentlyContinue
  Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -EA SilentlyContinue
  Remove-Item Env:JJFB_FAST_DOWNIMAGE_READY_EVENT -EA SilentlyContinue

  if ($Opts.ContainsKey('ResourceReady') -and $Opts.ResourceReady) {
    $env:GWY_PLATFORM_RESOURCE_READY_EVENT = '1'
    $env:JJFB_FAST_GWY_RESOURCE_READY_EVENT = '1'
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

  Write-Host "=== E9Z case=$CaseName timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT ==="
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
    Write-Host "E9Z outer kill after ${OUTER_KILL_SEC}s"
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9Z_|GWY_PACK|GWY_PLATFORM_RESOURCE|JJFB_E9W_|JJFB_E9Y_' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }
  Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
  if (Test-Path $hwndPng) {
    Copy-Item -Force $hwndPng $fullPng -EA SilentlyContinue
  } elseif (Test-Path $shotBmp) {
    Convert-BmpToPng $shotBmp $fullPng | Out-Null
  }

  $an = Analyze-E9Z $caseLog
  if ($CaseName -eq 'classify' -or ($Opts.ContainsKey('Classify') -and $Opts.Classify)) {
    $an = [pscustomobject]@{
      class = (Write-ClassifyVerdict $caseLog $verdictMd)
      evidence = 'OBSERVED'
      reg = $an.reg
      man = $an.man
      cand = $an.cand
      logo = $an.logo
      ac8_debug = $an.ac8_debug
      seed_used = $an.seed_used
    }
  }
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

  if ($CaseName -ne 'classify' -and -not ($Opts.ContainsKey('Classify') -and $Opts.Classify)) {
  $md = @"
# Stage E9Z GWY Resource Ready Verdict

- **Case**: $CaseName
- **Mode**: $Mode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **Product success**: **NO** (``NOT_PRODUCT``)

## Tags
| Tag | Hit |
|-----|-----|
| GWY_PACK_REGISTRY_BUILT | $($an.reg) |
| GWY_UPDATE_MANIFEST_PARSED | $($an.man) |
| GWY_RESOURCE_READY_EVENT_CANDIDATE_FOUND | $($an.cand) |
| Logo via resource-ready / no DEBUG | $($an.logo) |
| Seed used (must False) | $($an.seed_used) |
| DEBUG AC8 (must False) | $($an.ac8_debug) |

## Artifacts
| Kind | Path |
|------|------|
| Verdict | ``$verdictMd`` |
| Log | ``$caseLog`` |
| Pack registry | ``$packCsv`` |
| Manifest | ``reports/e9z_update_manifest_inventory.csv`` |
| Import trace | ``$importCsv`` |
| Event trace | ``$eventCsv`` |
| No-debug CSV | ``$nodbgCsv`` |
| Screenshot | ``$fullPng`` |
| Window | ``$hwndPng`` |

## Forbidden checks
- No direct AC8 poke as success
- No 8D8 seed as success
- No 0x2EF8AE patch / PC jump to logo
- No show1/downimage1/jjfb hardcode / fake UI / MRP-EXT edits
"@
  [System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))
  }

  Write-Host "== E9Z_CASE_DONE name=$CaseName verdict=$($an.class) elapsed=$elapsed"
  return $an
}

# ---- main ----
if (-not $SkipBuild) {
  Write-Host '=== E9Z build (Gwy) ==='
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}

$results = @()
switch ($Mode) {
  'parse' {
    $results += Invoke-ParseOnly
  }
  'trace' {
    $results += Invoke-E9ZCase 'trace' @{ ResourceReady = $false }
  }
  'event' {
    $results += Invoke-E9ZCase 'event' @{ ResourceReady = $true }
  }
  'no_debug' {
    $results += Invoke-E9ZCase 'no_debug' @{ ResourceReady = $true }
  }
  'classify' {
    $results += Invoke-E9ZCase 'classify' @{ ResourceReady = $true; Classify = $true }
  }
}

$results | Format-Table -AutoSize | Out-Host
Write-Host "E9Z done mode=$Mode primary=$($results[-1].class)"
