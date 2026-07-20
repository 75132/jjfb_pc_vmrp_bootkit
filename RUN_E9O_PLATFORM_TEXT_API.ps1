# Stage E9O: formal platform 0x11F00 drawText API — NOT product success.
# Modes: trace | api | compare
# No E9N textshim on success path. Guest string + real x/y via platform handler.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 12,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('trace','api','compare')]
  [string]$Mode = 'api',
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
    'JJFB_PLATFORM_TEXT_API_11F00','JJFB_E9O_11F00_CSV','JJFB_E9O_DRAW_CSV'
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

function Analyze-E9O([string]$log) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }
  $apiOk = $t -match 'JJFB_E9O_CLASS\] class=PLATFORM_TEXT_API_11F00_RENDERED[^_]'
  $apiFb = $t -match 'PLATFORM_TEXT_API_11F00_RENDERED_WITH_FONT_FALLBACK'
  $apiAny = $t -match 'JJFB_PLATFORM_TEXT_API_11F00\]'
  $encBlock = $t -match 'PLATFORM_TEXT_API_11F00_BLOCKED_BY_ENCODING'
  $fontBlock = $t -match 'PLATFORM_TEXT_API_11F00_BLOCKED_BY_FONT'
  $surfBlock = $t -match 'PLATFORM_TEXT_API_11F00_BLOCKED_BY_SURFACE'
  $e9nShim = $t -match 'JJFB_PLATFORM_TEXT_DRAW_COMPAT\]|PLATFORM_TEXT_DRAW_COMPAT_RENDERED'
  $hit11 = $t -match 'JJFB_E9N_11F00\]|JJFB_E9O_11F00_TRACE\]|plat_11F00_drawText|0x11F00'
  $blit = $t -match 'JJFB_E9N_GLYPH_BLIT_CORE\]|TEXT_305C3C_REACHED_PLATFORM_DRAW'
  $ui = $t -match 'SPLASH_LOADING_UI_VISIBLE'
  $prog = $t -match 'SPLASH_PROGRESS_DRAWN_NO_REWRITE'
  $hold = $t -match 'JJFB_VISIBLE_WINDOW_HOLD_DONE\]|JJFB_E9K_HOLD'
  $cls = if ($apiOk) { 'PLATFORM_TEXT_API_11F00_RENDERED' }
    elseif ($apiFb) { 'PLATFORM_TEXT_API_11F00_RENDERED_WITH_FONT_FALLBACK' }
    elseif ($apiAny -and -not $e9nShim) { 'SPLASH_TEXT_RENDERED_NO_E9N_SHIM' }
    elseif ($encBlock) { 'PLATFORM_TEXT_API_11F00_BLOCKED_BY_ENCODING' }
    elseif ($fontBlock) { 'PLATFORM_TEXT_API_11F00_BLOCKED_BY_FONT' }
    elseif ($surfBlock) { 'PLATFORM_TEXT_API_11F00_BLOCKED_BY_SURFACE' }
    elseif ($hit11 -and $blit -and -not $apiAny) { 'TEXT_305C3C_BLOCKED_BY_PLATFORM_TEXT_API' }
    elseif ($ui -or $prog) { 'PRODUCT_STILL_NEEDS_BD0_NATURALIZATION' }
    else { 'PRODUCT_STILL_NEEDS_BD0_NATURALIZATION' }
  return [pscustomobject]@{
    class = $cls
    evidence = if ($apiOk -or $apiFb -or $hit11) { 'OBSERVED' } else { 'HYPOTHESIS' }
    api = [bool]($apiOk -or $apiFb -or $apiAny)
    e9n_shim = [bool]$e9nShim
    hit11 = [bool]$hit11
    loading_ui = [bool]$ui
    progress = [bool]$prog
    hold = [bool]$hold
  }
}

function Invoke-E9OCase([string]$CaseName, [string]$CaseMode) {
  $callsCsv = Join-Path $reportDir 'e9o_platform_11f00_calls.csv'
  $drawCsv = Join-Path $reportDir 'e9o_text_draw_trace.csv'
  $verdictMd = Join-Path $reportDir 'stage_e9o_platform_text_api_verdict.md'
  $shotBmp = Join-Path $shotDir 'e9o_platform_text_api.bmp'
  $hwndBmp = Join-Path $shotDir 'e9o_actual_window_capture.bmp'
  $hwndPng = Join-Path $shotDir 'e9o_actual_window_capture.png'
  $uiPng = Join-Path $shotDir 'e9o_platform_text_api.png'
  $caseLog = Join-Path $logDir 'e9o_platform_text_api_stdout.txt'
  $caseErr = Join-Path $logDir 'e9o_platform_text_api_stderr.txt'

  Clear-E9Modes
  Stop-E9Children

  $env:JJFB_E9O_MODE = '1'
  $env:JJFB_E9N_MODE = '1'   # instrumentation BPs only; no textshim
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
  $env:JJFB_FAST_TEXTCTX_ASSIST = '1'
  $env:JJFB_FAST_TEXT_MEASURE_SHIM = '1'
  $env:JJFB_E9O_11F00_CSV = $callsCsv
  $env:JJFB_E9O_DRAW_CSV = $drawCsv
  $env:JJFB_E9N_305C3C_CSV = (Join-Path $reportDir 'e9o_e9n_305c3c_compat.csv')
  $env:JJFB_E9N_CLIP_CSV = (Join-Path $reportDir 'e9o_e9n_clip_compat.csv')
  $env:JJFB_E9N_GLYPH_CSV = (Join-Path $reportDir 'e9o_e9n_glyph_compat.csv')
  $env:JJFB_E8Z_SCREENSHOT = $shotBmp
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9o_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9o_after.bmp')
  $env:JJFB_E9B_HWND_CAPTURE = $hwndBmp
  $env:JJFB_E8Y_INSN_LIMIT = '12000'
  Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAST_TEXT_LAYOUT_ASSIST -ErrorAction SilentlyContinue
  # Critical: no E9N diagnostic textshim on E9O success path
  Remove-Item Env:JJFB_FAST_PLATFORM_TEXT_DRAW_SHIM -ErrorAction SilentlyContinue

  switch ($CaseMode) {
    'trace' {
      Remove-Item Env:JJFB_PLATFORM_TEXT_API_11F00 -EA SilentlyContinue
    }
    'api' {
      $env:JJFB_PLATFORM_TEXT_API_11F00 = '1'
    }
    'compare' {
      $env:JJFB_PLATFORM_TEXT_API_11F00 = '1'
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

  Write-Host "=== E9O case=$CaseName mode=$CaseMode timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT ==="
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
    Write-Host "E9O outer kill after ${OUTER_KILL_SEC}s"
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9O_|JJFB_PLATFORM_TEXT_API_11F00|JJFB_E9N_11F00|JJFB_E9K_305BFC' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }

  Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
  Convert-BmpToPng $shotBmp $uiPng | Out-Null
  if (Test-Path $hwndPng) { Copy-Item -Force $hwndPng $uiPng -EA SilentlyContinue }
  $an = Analyze-E9O $caseLog
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

  $md = @"
# Stage E9O Platform Text API 0x11F00 Verdict

- **Case**: $CaseName
- **Mode**: $CaseMode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **0x11F00 API draw**: $($an.api)
- **E9N textshim used**: $($an.e9n_shim)
- **0x11F00 hit**: $($an.hit11)
- **Loading UI / progress**: $($an.loading_ui) / $($an.progress)
- **HWND hold**: $($an.hold)
- **Log**: ``$caseLog``
- **Calls CSV**: ``$callsCsv``
- **Draw CSV**: ``$drawCsv``
- **HWND**: ``$hwndPng``
- **UI**: ``$uiPng``

## ABI (formalized)
- ``sendAppEvent(0x11F00, app, code_obj, param0)``
- param0: i16 y@+0, i16 x@+2, RGB888@+0x2C
- Guest C-string from 305BFC chain (noted) or code-object pointer scan
- Return: 0 (MR_SUCCESS) when handled

## Assists still active (NOT product)
- BD0 / DisplayFirst / state assists
- TEXTCTX 818/81C assist
- measure shim 0x12340
- Platform ``JJFB_PLATFORM_TEXT_API_11F00`` is runtime compatibility (allowed)

## Rules
- NOT product success until assists naturalized
- No hardcoded string/coords; no MRP/EXT edits; no rewrite
- No E9N textshim on success path
"@
  [System.IO.File]::WriteAllText($verdictMd, $md, [System.Text.UTF8Encoding]::new($false))

  Write-Host "== E9O_CASE_DONE name=$CaseName verdict=$($an.class) elapsed=${elapsed}s =="
  return $an
}

if (-not $SkipBuild) {
  Write-Host '=== E9O force-rebuild launcher_core + vmrp gwy ==='
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'jjfb_plat_11f00.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'gwy_ext_obs.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'build-i686\liblauncher_core.a')
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
}

$result = Invoke-E9OCase -CaseName $Mode -CaseMode $Mode
Write-Host "verdict=$(Join-Path $reportDir 'stage_e9o_platform_text_api_verdict.md')"
exit 0
