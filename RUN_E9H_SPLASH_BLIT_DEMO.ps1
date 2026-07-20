# Stage E9H: splash loadingbar blit (0x2EFA97 → 0x2EC6B8) — NOT product success.
# Goal: after natural loadingbar bind, reach real blit with guest pixels.
# No JJFB_E9F_REWRITE_REQUEST. No blind jump to 0x2F45A2.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 16,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('trace','blit','r4assist')]
  [string]$Mode = 'blit',
  [switch]$SkipBuild,
  [switch]$UiModeAssist
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
  'p:0x2FC418','p:0x2EFA33','p:0x2EFA46','p:0x2EFA56','p:0x2EFA7C','p:0x2EFA9A',
  'p:0x2EC6B8','p:0x2EFA9E','p:0x2EFAF2'
) -join ','

function Clear-E9Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE',
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_E9C_MODE','JJFB_E9D_MODE','JJFB_E9E_MODE',
    'JJFB_E9F_MODE','JJFB_E9G_MODE','JJFB_E9H_MODE',
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
    'JJFB_E9H_R4_TRACE','JJFB_FAST_SPLASH_R4_ASSIST','JJFB_E9H_R4_CSV','JJFB_E9H_SEQ_CSV'
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

function Analyze-E9H([string]$log) {
  $t = Get-Content $log -Raw -ErrorAction SilentlyContinue
  if (-not $t) { return [pscustomobject]@{ class = 'NO_LOG'; evidence = 'EMPTY' } }
  $drawn = $t -match 'JJFB_E9H_CLASS\] class=SPLASH_LOADINGBAR_DRAWN'
  $blit = $t -match 'JJFB_E9H_2EC6B8_BLIT\]|JJFB_E9H_2EC6B8\][^\r\n]*onscreen=1'
  $badxy = $t -match 'SPLASH_BLIT_BAD_XY'
  $site = $t -match 'JJFB_E9H_BLIT_SITE\]|JJFB_E9H_SPLASH_CONT_BLIT\]|SPLASH_LOADINGBAR_POSTMATCH_ARM_BLIT|JJFB_E9H_BLIT_SETUP\]'
  $gate = $t -match 'JJFB_E9H_R4_GATE\]'
  $r4z = $t -match 'JJFB_E9H_R4_GATE\][^\r\n]*r4=0x0\b'
  $unmap = $t -match 'UC_MEM_READ_UNMAPPED|UC_MEM_WRITE_UNMAPPED'
  $rewrite = $t -match 'JJFB_E9F_REQUEST_REWRITE\]|rewrite_applied=1'
  $drawnOk = $drawn -or ($t -match 'JJFB_E9H_2EC6B8_BLIT\][^\r\n]*loadingbar')
  $cls = if ($drawnOk) { 'SPLASH_LOADINGBAR_DRAWN_NO_REWRITE' }
    elseif ($badxy -and -not ($t -match '2EC6B8_BLIT')) { 'SPLASH_BLIT_BAD_XY' }
    elseif ($blit) { 'SPLASH_BLIT_ONSITE_NO_CLASS' }
    elseif ($site -and $gate -and $r4z) { 'SPLASH_BLIT_REACHED_R4_STILL_ZERO' }
    elseif ($site) { 'SPLASH_BLIT_SITE_REACHED_NO_DRAW' }
    elseif ($unmap) { 'SPLASH_BLIT_UNMAPPED' }
    elseif ($rewrite) { 'REWRITE_USED_NOT_PRODUCT' }
    else { 'SPLASH_BLIT_NOT_REACHED' }
  return [pscustomobject]@{
    class = $cls
    evidence = if ($drawnOk) { 'OBSERVED' } else { 'HYPOTHESIS' }
    blit_site = [bool]$site
    blit_done = [bool]$drawnOk
    r4_gate = [bool]$gate
    rewrite = [bool]$rewrite
    unmapped = [bool]$unmap
  }
}

if (-not $SkipBuild) {
  Write-Host '=== E9H force-rebuild launcher_core + vmrp gwy ==='
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Get-ChildItem (Join-Path $Root 'build-i686') -Recurse -Filter 'jjfb_bmp_meta.c.obj' -EA SilentlyContinue |
    Remove-Item -Force -EA SilentlyContinue
  Remove-Item -Force -EA SilentlyContinue (Join-Path $Root 'build-i686\liblauncher_core.a')
}

$r4Csv = Join-Path $reportDir 'e9h_r4_provenance_trace.csv'
$seqCsv = Join-Path $reportDir 'e9h_splash_resource_sequence.csv'
$verdictMd = Join-Path $reportDir 'stage_e9h_splash_blit_verdict.md'
$shotBmp = Join-Path $shotDir "e9h_${Mode}_fb.bmp"
$hwndBmp = Join-Path $shotDir "e9h_${Mode}_hwnd.bmp"
$hwndPng = Join-Path $shotDir "e9h_${Mode}_hwnd.png"
$caseLog = Join-Path $logDir "e9h_${Mode}_stdout.txt"
$caseErr = Join-Path $logDir "e9h_${Mode}_stderr.txt"

Clear-E9Modes
Stop-E9Children

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
$env:JJFB_E9H_R4_CSV = $r4Csv
$env:JJFB_E9H_SEQ_CSV = $seqCsv
$env:JJFB_E9D_COMPARE_CSV = (Join-Path $reportDir 'e9h_name_compare_debug.csv')
$env:JJFB_E9D_REQUEST_CSV = (Join-Path $reportDir 'e9h_e9d_resource_request.csv')
$env:JJFB_E9G_REQUEST_CSV = (Join-Path $reportDir 'e9h_game_ui_request_trace.csv')
$env:JJFB_E9G_UIMODE_CSV = (Join-Path $reportDir 'e9h_ui_mode_trace.csv')
$env:JJFB_E9F_REQUEST_CSV = (Join-Path $reportDir 'e9h_e9f_request.csv')
$env:JJFB_E8Z_SCREENSHOT = $shotBmp
$env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir "e9h_${Mode}_before.bmp")
$env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir "e9h_${Mode}_after.bmp")
$env:JJFB_E9B_HWND_CAPTURE = $hwndBmp

# Never product-success rewrite.
Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -ErrorAction SilentlyContinue

switch ($Mode) {
  'trace' {
    $env:JJFB_FAST_SPLASH_CALL = '1'
    if ($UiModeAssist) { $env:JJFB_E9G_UI_MODE_ASSIST = '1' }
  }
  'blit' {
    $env:JJFB_FAST_SPLASH_CALL = '1'
    $env:JJFB_E9G_UI_MODE_ASSIST = '1'
  }
  'r4assist' {
    $env:JJFB_FAST_SPLASH_CALL = '1'
    $env:JJFB_E9G_UI_MODE_ASSIST = '1'
    $env:JJFB_FAST_SPLASH_R4_ASSIST = '1'
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
$env:JJFB_E8Y_INSN_LIMIT = '3000'

Write-Host "=== E9H Mode=$Mode timeout=${CASE_TIMEOUT_SEC}s hold=${HoldSec}s NOT_PRODUCT ==="
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
  Write-Host "E9H outer kill after ${OUTER_KILL_SEC}s"
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E9Children
}
$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
# Only adopt stage_e log when THIS run armed E9H (avoid stale E9G unmapped verdict).
if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9H_MODE\]|JJFB_E9H_BP\]|JJFB_E9H_CLASS\]|JJFB_E9H_2EC6B8' -Quiet -EA SilentlyContinue)) {
  Copy-Item -Force $src $caseLog
}
if ($p.ExitCode -ne 0 -and $p.ExitCode -ne $null) {
  Write-Host "E9H child exit=$($p.ExitCode) — see $caseErr"
  Get-Content $caseErr -Tail 30 -EA SilentlyContinue | Write-Host
}

Convert-BmpToPng $hwndBmp $hwndPng | Out-Null
$an = Analyze-E9H $caseLog
$elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)

$md = @"
# Stage E9H Splash Blit Verdict

- **Mode**: $Mode
- **Class**: $($an.class)
- **Evidence**: $($an.evidence)
- **Elapsed**: ${elapsed}s
- **Blit site reached**: $($an.blit_site)
- **Blit done**: $($an.blit_done)
- **R4 gate seen**: $($an.r4_gate)
- **Rewrite used**: $($an.rewrite)
- **Unmapped**: $($an.unmapped)
- **Log**: ``$caseLog``
- **R4 CSV**: ``$r4Csv``
- **Seq CSV**: ``$seqCsv``
- **HWND shot**: ``$hwndPng``

## Rules
- NOT product success.
- No ``JJFB_E9F_REWRITE_REQUEST`` as success path.
- Continue after loadingbar: ``0x2EFA46`` → ``0x2EFA7C`` → ``0x2EFA9A`` → ``0x2EC6B8``.
- Guest pixels only via real draw path / ``guiDrawBitmapSprite``.

## Layout note
robotol ``raw_base_refine`` maps at ``0x2D8DF4``. ``0x2EFA9E`` is *after* the blit call.
Skip ``0x2EFA46`` → ``0x2EFA5C`` (y-calc) then ``0x2EFA7C``/``0x2EFA9A``/``0x2EC6B8``.
"@
Set-Content -Path $verdictMd -Value $md -Encoding UTF8

Write-Host "E9H class=$($an.class) evidence=$($an.evidence) elapsed=${elapsed}s"
Write-Host "verdict=$verdictMd"
exit 0
