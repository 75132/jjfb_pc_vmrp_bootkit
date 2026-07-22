# Stage E9G: splash/loading UI state entry — NOT product success.
# Goal: game path reaches 0x2EF86C / UI_MODE=0x45 and naturally requests
# loadingbar/textbar/top. No JJFB_E9F_REWRITE_REQUEST as success.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 16,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('auto','splash','debug')]
  [string]$Mode = 'splash',
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
  'p:0x2FC418','p:0x2EFA33','p:0x2EFA53'
) -join ','

function Clear-E9Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE',
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_E9C_MODE','JJFB_E9D_MODE','JJFB_E9E_MODE',
    'JJFB_E9F_MODE','JJFB_E9G_MODE',
    'JJFB_VISIBLE_WINDOW','JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST',
    'JJFB_FAST_A64_RESOURCE_ASSIST','JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH',
    'JJFB_REAL_MRP_MEMBER_BRIDGE','JJFB_REAL_MRP_MEMBER_BRIDGE_ALL','JJFB_REAL_MRP_PATH',
    'JJFB_E8Z_BMP_PATH','JJFB_E8Z_SCREENSHOT','JJFB_E8Z_SCREENSHOT_BEFORE','JJFB_E8Z_SCREENSHOT_AFTER',
    'JJFB_E8Y_INSN_LIMIT','JJFB_E8W_REENTER_E88CC','JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB',
    'JJFB_FAST_STATE','JJFB_FAST_CASE156_R1','JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30',
    'JJFB_FAST_C6C22','JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_BYPASS_CF5_GATE','JJFB_WINDOW_ZOOM',
    'JJFB_E9B_HOLD_SEC','JJFB_E9B_HWND_CAPTURE','JJFB_E9D_STRCMP_SHIM','JJFB_E9D_BRIDGE_FALLBACK',
    'JJFB_E9D_COMPARE_CSV','JJFB_E9D_REQUEST_CSV','JJFB_E9E_POSTMATCH_SHIMS',
    'JJFB_E9F_REWRITE_REQUEST','JJFB_E9F_MULTI_POSTMATCH','JJFB_E9F_PREFER',
    'JJFB_E9F_REQUEST_CSV','JJFB_E9F_RESULTS_JSONL',
    'JJFB_FAST_SPLASH_CALL','JJFB_E9G_UI_MODE_ASSIST','JJFB_E9G_DEBUG',
    'JJFB_E9G_REQUEST_CSV','JJFB_E9G_UIMODE_CSV'
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

function Analyze-E9G([string]$log) {
  $h = @{
    splash_enter=$false; splash_call=$false; splash_done=$false
    uimode_45=$false; uimode_writer=$false; natural_match=$false; postmatch=$false
    bridge=$false; rewrite=$false; first_frame=$false; hwnd_nw=''
    loading=$false; textbar=$false; top=$false; only_wy=$false
    slogo_absent=$false; name=''; names=@(); req_ui=$false
  }
  if (-not (Test-Path $log)) { return $h }
  $h.splash_enter = [bool](Select-String -Path $log -Pattern 'JJFB_E9G_SPLASH_ENTER\]' -Quiet -EA SilentlyContinue)
  $h.splash_call = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_SPLASH_CALL\] entry=' -Quiet -EA SilentlyContinue)
  $h.splash_done = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_SPLASH_DONE\]' -Quiet -EA SilentlyContinue)
  $h.uimode_45 = [bool](Select-String -Path $log -Pattern 'JJFB_E9G_UI_MODE_45\]|UI_MODE_ASSIST\][^\r\n]*new=0x45|class=SPLASH_UI_MODE_45' -Quiet -EA SilentlyContinue)
  $h.uimode_writer = [bool](Select-String -Path $log -Pattern 'JJFB_E9G_UIMODE_WRITER\]' -Quiet -EA SilentlyContinue)
  $h.natural_match = [bool](Select-String -Path $log -Pattern 'JJFB_E9D_NATURAL_MATCH\]' -Quiet -EA SilentlyContinue)
  $h.postmatch = [bool](Select-String -Path $log -Pattern 'JJFB_E9E_POSTMATCH_SHIM\][^\r\n]*role=member_bytes' -Quiet -EA SilentlyContinue)
  $h.bridge = [bool](Select-String -Path $log -Pattern 'JJFB_REAL_MRP_MEMBER_BRIDGE\] hit=' -Quiet -EA SilentlyContinue)
  $h.rewrite = [bool](Select-String -Path $log -Pattern 'JJFB_E9F_REQUEST_REWRITE\]' -Quiet -EA SilentlyContinue)
  $h.first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_FRAME_REACHED\]' -Quiet -EA SilentlyContinue)
  $h.slogo_absent = [bool](Select-String -Path $log -Pattern 'SLOGO_ABSENT_FROM_GUEST_INDEX_CONFIRMED|SLOGO_RAW_INVENTORY_ONLY' -Quiet -EA SilentlyContinue)
  $nw = Select-String -Path $log -Pattern 'JJFB_E9B_HWND_CAPTURE\][^\r\n]*nonwhite_or_nonblack=(\d+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($nw -and $nw.Line -match 'nonwhite_or_nonblack=(\d+)') { $h.hwnd_nw = $Matches[1] }
  $ms = Select-String -Path $log -Pattern 'JJFB_E9E_POSTMATCH_SHIM\][^\r\n]*name="([^"]+)"' -EA SilentlyContinue
  foreach ($m in $ms) {
    if ($m.Line -match 'name="([^"]+)"') {
      $n = $Matches[1]
      $h.names += $n
      $h.name = $n
      if ($n -match 'loadingbar') { $h.loading = $true; $h.req_ui = $true }
      if ($n -match 'textbar') { $h.textbar = $true; $h.req_ui = $true }
      if ($n -match '^top!|top!') { $h.top = $true; $h.req_ui = $true }
    }
  }
  $reqs = Select-String -Path $log -Pattern 'JJFB_E9D_REQUEST\][^\r\n]*name="([^"]+)"' -EA SilentlyContinue
  if (-not $reqs) {
    $reqs = Select-String -Path $log -Pattern 'JJFB_E8Y_2D92E4_ENTRY\][^\r\n]*name="([^"]+)"' -EA SilentlyContinue
  }
  $reqNames = @()
  foreach ($m in $reqs) {
    if ($m.Line -match 'name="([^"]+)"') { $reqNames += $Matches[1] }
  }
  if ($reqNames.Count -gt 0 -and -not $h.name) { $h.name = $reqNames[-1] }
  $hasSplashReq = [bool]($reqNames | Where-Object { $_ -match 'loadingbar|textbar|top!|slogo' })
  $hasWy = [bool]($reqNames | Where-Object { $_ -match 'wy_jiao1' })
  if ($hasSplashReq) { $h.req_ui = $true }
  if ($hasWy -and -not $hasSplashReq) { $h.only_wy = $true }
  return $h
}

function Case-Verdict([hashtable]$h, [string]$mode) {
  if ($h.bridge) { return 'BRIDGE_FALLBACK_STILL_REQUIRED' }
  if ($h.rewrite) { return 'REQUEST_REWRITE_STILL_REQUIRED' }
  # Prefer game-requested UI draw over mere UI_MODE reach.
  if ($h.loading -and $h.postmatch -and $h.first_frame -and (($h.hwnd_nw -as [int]) -gt 0) -and -not $h.rewrite) {
    return 'GAME_REQUESTED_LOADINGBAR_DRAWN'
  }
  if ($h.textbar -and $h.postmatch -and $h.first_frame -and (($h.hwnd_nw -as [int]) -gt 0)) {
    return 'GAME_REQUESTED_TEXTBAR_DRAWN'
  }
  if ($h.top -and $h.postmatch -and $h.first_frame -and (($h.hwnd_nw -as [int]) -gt 0)) {
    return 'GAME_REQUESTED_TOP_DRAWN'
  }
  if ($h.req_ui -and $h.postmatch -and $h.first_frame -and (($h.hwnd_nw -as [int]) -gt 0)) {
    return 'NATURAL_SPLASH_LOADING_UI_VISIBLE'
  }
  if ($h.loading -and $h.postmatch -and $h.splash_enter -and -not $h.rewrite) {
    return 'SPLASH_2EF86C_REACHED_NEXT_GAP'
  }
  if ($h.uimode_45 -and $h.splash_enter) { return 'SPLASH_UI_MODE_45_REACHED' }
  if ($h.splash_enter -or ($h.splash_call -and $h.splash_done)) {
    return 'SPLASH_2EF86C_REACHED_NEXT_GAP'
  }
  if ($h.uimode_writer -eq $false -and $mode -ne 'auto' -and -not $h.splash_enter) {
    if ($h.splash_call) { return 'UI_MODE_45_WRITER_NEVER_REACHED' }
  }
  if ($h.slogo_absent -and $mode -eq 'debug') { return 'SLOGO_ABSENT_FROM_GUEST_INDEX_CONFIRMED' }
  if ($mode -eq 'auto' -and $h.only_wy) { return 'PRODUCT_STILL_NEEDS_STATE_MACHINE_NATURALIZATION' }
  if ($h.splash_call -and -not $h.splash_enter) { return 'SPLASH_REQUIRES_UI_INIT' }
  if ($h.uimode_45 -and -not $h.splash_enter) { return 'UI_MODE_45_WRITER_BRANCH_UNMET' }
  return 'PRODUCT_STILL_NEEDS_STATE_MACHINE_NATURALIZATION'
}

function Invoke-E9GCase([string]$caseName, [hashtable]$opts) {
  Clear-E9Modes
  Stop-E9Children
  $env:JJFB_E9G_MODE = '1'
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
  # Never enable rewrite for E9G success paths.
  Remove-Item Env:JJFB_E9F_REWRITE_REQUEST -EA SilentlyContinue
  if ($opts.SplashCall) { $env:JJFB_FAST_SPLASH_CALL = '1' }
  if ($opts.DebugOnly) { $env:JJFB_E9G_DEBUG = '1' }
  if ($opts.UiModeAssist -or $UiModeAssist) { $env:JJFB_E9G_UI_MODE_ASSIST = '1' }
  $env:JJFB_E9D_COMPARE_CSV = (Join-Path $reportDir 'e9g_name_compare_debug.csv')
  $env:JJFB_E9D_REQUEST_CSV = (Join-Path $reportDir 'e9g_e9d_resource_request.csv')
  $env:JJFB_E9G_REQUEST_CSV = (Join-Path $reportDir 'e9g_game_ui_request_trace.csv')
  $env:JJFB_E9G_UIMODE_CSV = (Join-Path $reportDir 'e9g_ui_mode_trace.csv')
  $shotBmp = Join-Path $shotDir ([string]$opts.ShotBmp)
  $hwndBmp = Join-Path $shotDir 'e9g_actual_window_capture.bmp'
  $env:JJFB_E8Z_SCREENSHOT = $shotBmp
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9g_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9g_after.bmp')
  $env:JJFB_E9B_HWND_CAPTURE = $hwndBmp

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

  $caseLog = Join-Path $logDir 'e9g_splash_ui_stdout.txt'
  $caseErr = Join-Path $logDir 'e9g_splash_ui_stderr.txt'
  $t0 = Get-Date
  $argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
               '-Target',$Target,'-Seconds',"$CASE_TIMEOUT_SEC")
  if ($SkipBuild -or $script:builtOnce) { $argList += '-SkipBuild' }

  $p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -WorkingDirectory $Root -PassThru `
    -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
  if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $script:builtOnce = $true
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9G_|JJFB_E9E_|JJFB_FAST_SPLASH' -Quiet -EA SilentlyContinue)) {
    Copy-Item -Force $src $caseLog
  }
  $hits = Analyze-E9G $caseLog
  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $verdict = Case-Verdict $hits $caseName
  $png = [IO.Path]::ChangeExtension($shotBmp, '.png')
  [void](Convert-BmpToPng $shotBmp $png)
  [void](Convert-BmpToPng $hwndBmp (Join-Path $shotDir 'e9g_actual_window_capture.png'))
  if ($shotBmp -match 'splash') {
    Copy-Item -Force $png (Join-Path $shotDir 'e9g_splash_ui_window.png') -EA SilentlyContinue
  }
  Write-Host "== E9G_CASE_DONE name=$caseName verdict=$verdict elapsed=$([Math]::Round($elapsed,1))"
  return @{
    case=$caseName; verdict=$verdict; elapsed=[Math]::Round($elapsed,2); hits=$hits; log=$caseLog
  }
}

$script:builtOnce = $false
Write-Host "E9G Mode=$Mode zoom=$Zoom hold=${HoldSec}s timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$cases = @()
if ($Mode -eq 'splash') {
  $cases += @{ Name='splash'; SplashCall=$true; DebugOnly=$false; UiModeAssist=$UiModeAssist; ShotBmp='e9g_splash_ui_window.bmp' }
} elseif ($Mode -eq 'debug') {
  $cases += @{ Name='debug'; SplashCall=$false; DebugOnly=$true; UiModeAssist=$false; ShotBmp='e9g_debug_trace.bmp' }
} else {
  $cases += @{ Name='auto'; SplashCall=$false; DebugOnly=$false; UiModeAssist=$false; ShotBmp='e9g_auto_natural.bmp' }
}

$results = @()
$stop = $false
foreach ($c in $cases) {
  if ($stop) { break }
  if ($results.Count -ge 3) { break }
  $r = Invoke-E9GCase $c.Name $c
  $results += $r
  if ($r.verdict -match 'GAME_REQUESTED_|NATURAL_SPLASH_LOADING|SPLASH_UI_MODE_45_REACHED') {
    $stop = $true
  }
}

$best = $results | Select-Object -Last 1
$hits = $best.hits
$verdict = $best.verdict

$blocker = 'none'
if ($verdict -match 'STATE_MACHINE|REWRITE') {
  $blocker = 'DisplayFirst path still requests wy_jiao1; splash gate UI_MODE=0x45 / 0x2EF86C not fully naturalized'
} elseif ($verdict -eq 'SPLASH_2EF86C_REACHED_NEXT_GAP') {
  $blocker = 'entered 0x2EF86C but natural UI resource draw / HWND not completed'
} elseif ($verdict -eq 'UI_MODE_45_WRITER_NEVER_REACHED') {
  $blocker = 'natural writer 0x2FC418 never hit; FAST splash may be required'
} elseif ($hits.slogo_absent) {
  $blocker = 'slogo in MRP inventory but absent from guest 0x304BF0 index'
}

$flow = [ordered]@{
  stage='E9G'; mode=$Mode; verdict=$verdict; elapsed_sec=$best.elapsed
  splash_enter=[bool]$hits.splash_enter; splash_call=[bool]$hits.splash_call
  uimode_45=[bool]$hits.uimode_45; natural_match=[bool]$hits.natural_match
  postmatch=[bool]$hits.postmatch; bridge=[bool]$hits.bridge; rewrite=[bool]$hits.rewrite
  first_frame=[bool]$hits.first_frame; hwnd_nonwhite=$hits.hwnd_nw
  member=$hits.name; members=$hits.names; nearest_blocker=$blocker
  note='NOT_PRODUCT_SUCCESS; splash/loading UI state entry'
}
($flow | ConvertTo-Json -Depth 6) | Set-Content (Join-Path $reportDir 'e9g_splash_ui_flow.json') -Encoding utf8

$md = @(
  '# Stage E9G Splash/Loading UI Verdict',
  '',
  "**Verdict:** ``$verdict``",
  '',
  '**NOT product success.**',
  '',
  '## Confirmed approach',
  '',
  '- Enter real splash path via ``JJFB_FAST_SPLASH_CALL`` → ``0x2EF86C`` (r0=0x45, r1=0x13).',
  '- No ``JJFB_E9F_REWRITE_REQUEST`` success path.',
  '- Natural postmatch + ``mr_drawBitmap`` retained from E9E/E9F.',
  '- Per-bmp draw dim meta via ``jjfb_bmp_meta`` (not global LAST_WH alone).',
  '',
  '## Result',
  '',
  "- mode=$Mode member=$($hits.name) rewrite=$($hits.rewrite)",
  "- splash_enter=$($hits.splash_enter) splash_call=$($hits.splash_call) uimode_45=$($hits.uimode_45)",
  "- natural_match=$($hits.natural_match) postmatch=$($hits.postmatch) bridge=$($hits.bridge)",
  "- first_frame=$($hits.first_frame) hwnd_nw=$($hits.hwnd_nw)",
  "- nearest_blocker=$blocker",
  '',
  '## Artifacts',
  '',
  '- ``screenshots/e9g_splash_ui_window.png`` / ``e9g_actual_window_capture.png``',
  '- ``reports/e9g_game_ui_request_trace.csv``',
  '- ``reports/e9g_ui_mode_trace.csv``',
  '- ``logs/e9g_splash_ui_stdout.txt``',
  ''
)
[System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e9g_splash_ui_verdict.md'), (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$verdict"
exit 0
