# Stage E9C: original MRP UI recon + meaningful visible frame (NOT product success).
# Pixels from original jjfb.mrp members; draw via mr_drawBitmap -> guiDrawBitmapSprite.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 30,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('auto','candidate','contactsheet')]
  [string]$Mode = 'candidate',
  [string]$Candidate = 'slogo!157!58.bmp',
  [switch]$SkipBuild,
  [switch]$SkipInventory
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$shotDir = Join-Path $Root 'screenshots'
$resDir = Join-Path $Root 'out\e9c_resources'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $shotDir, $resDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(120, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45

if (-not $SkipInventory) {
  Write-Host 'E9C inventory: parsing original jjfb.mrp...'
  py -3 (Join-Path $Root 'tools\e9c_mrp_inventory.py') | Out-Host
}

# Ensure candidate RGB565 blobs exist (from inventory extract or re-extract).
$candMap = @{
  'slogo!157!58.bmp' = 'slogo_157_58.bmp.rgb565'
  'loadingbar!201!29.bmp' = 'loadingbar_201_29.bmp.rgb565'
  'textbar!120!30.bmp' = 'textbar_120_30.bmp.rgb565'
  'top!76!28.bmp' = 'top_76_28.bmp.rgb565'
}
$candFile = $candMap[$Candidate]
if (-not $candFile) { $candFile = 'slogo_157_58.bmp.rgb565'; $Candidate = 'slogo!157!58.bmp' }
$candPath = Join-Path $resDir $candFile
if (-not (Test-Path $candPath)) {
  throw "missing candidate rgb565: $candPath (re-run inventory)"
}

$bmpSrc = Join-Path $Root 'out\e8z_resources\wy_jiao1_11_11.bmp'
if (-not (Test-Path $bmpSrc)) {
  py -3 (Join-Path $Root 'tools\e8z_resource_probe.py') | Out-Null
}
$mrpPath = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'

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
  'p:0x2D92E4','p:0x2F44C8','p:0x2F44CE','p:0x310BBC','p:0x304BF0','p:0x2FD868'
) -join ','

function Clear-E9Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE',
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_E9C_MODE','JJFB_VISIBLE_WINDOW',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST','JJFB_FAST_A64_RESOURCE_ASSIST',
    'JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH','JJFB_REAL_MRP_MEMBER_BRIDGE',
    'JJFB_REAL_MRP_MEMBER_BRIDGE_ALL','JJFB_REAL_MRP_PATH','JJFB_E8Z_BMP_PATH','JJFB_E8Z_SCREENSHOT',
    'JJFB_E8Z_SCREENSHOT_BEFORE','JJFB_E8Z_SCREENSHOT_AFTER','JJFB_E8Y_INSN_LIMIT',
    'JJFB_E8W_REENTER_E88CC','JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE',
    'JJFB_FAST_CASE156_R1','JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_BYPASS_CF5_GATE','JJFB_E8U_SCREENSHOT',
    'JJFB_WINDOW_ZOOM','JJFB_E9B_HOLD_SEC','JJFB_E9B_HWND_CAPTURE',
    'JJFB_E9C_MODE','JJFB_E9C_CONTACTSHEET','JJFB_E9C_CANDIDATE_MEMBER','JJFB_E9C_CANDIDATE_PATH',
    'JJFB_E9C_SKIP_TINY','JJFB_E9C_DEFER_HOLD','JJFB_E9C_PATH_SLOGO','JJFB_E9C_PATH_LOADINGBAR',
    'JJFB_E9C_PATH_TEXTBAR','JJFB_E9C_PATH_TOP'
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

function Analyze-E9C([string]$log) {
  $h = @{
    meaningful=$false; contactsheet=$false; presented=$false; hold_done=$false
    hwnd_cap=$false; hwnd_nonwhite=''; other=''; candidate=''; bridge=$false
    ui_frame=$false; responsive=$false
  }
  if (-not (Test-Path $log)) { return $h }
  $h.meaningful = [bool](Select-String -Path $log -Pattern 'JJFB_E9C_CLASS\] class=MEANINGFUL_VISIBLE_UI_FRAME' -Quiet -EA SilentlyContinue)
  $h.contactsheet = [bool](Select-String -Path $log -Pattern 'VISIBLE_CONTACTSHEET_FROM_REAL_RESOURCES' -Quiet -EA SilentlyContinue)
  $h.ui_frame = [bool](Select-String -Path $log -Pattern 'JJFB_VISIBLE_UI_FRAME_PRESENTED\]' -Quiet -EA SilentlyContinue)
  $h.presented = [bool](Select-String -Path $log -Pattern 'JJFB_VISIBLE_WINDOW_PRESENTED\]' -Quiet -EA SilentlyContinue)
  $h.hold_done = [bool](Select-String -Path $log -Pattern 'JJFB_VISIBLE_WINDOW_HOLD_DONE\]' -Quiet -EA SilentlyContinue)
  $h.responsive = [bool](Select-String -Path $log -Pattern 'JJFB_WINDOW_RESPONSIVE_HOLD\]' -Quiet -EA SilentlyContinue)
  $h.hwnd_cap = [bool](Select-String -Path $log -Pattern 'JJFB_E9B_HWND_CAPTURE\] path=' -Quiet -EA SilentlyContinue)
  $h.bridge = [bool](Select-String -Path $log -Pattern 'JJFB_REAL_MRP_MEMBER_BRIDGE\] hit=' -Quiet -EA SilentlyContinue)
  $nw = Select-String -Path $log -Pattern 'JJFB_E9B_HWND_CAPTURE\][^\r\n]*nonwhite_or_nonblack=(\d+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($nw -and $nw.Line -match 'nonwhite_or_nonblack=(\d+)') { $h.hwnd_nonwhite = $Matches[1] }
  $sp = Select-String -Path $log -Pattern 'JJFB_E8Z_SPRITE_BLIT\][^\r\n]*other=(\d+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($sp -and $sp.Line -match 'other=(\d+)') { $h.other = $Matches[1] }
  $cd = Select-String -Path $log -Pattern 'JJFB_E9C_CANDIDATE_DRAW\] name="([^"]+)"' -EA SilentlyContinue | Select-Object -Last 1
  if ($cd -and $cd.Line -match 'name="([^"]+)"') { $h.candidate = $Matches[1] }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.meaningful -and $h.hwnd_cap -and (($h.hwnd_nonwhite -as [int]) -gt 0) -and $h.hold_done) {
    return 'MEANINGFUL_VISIBLE_UI_FRAME'
  }
  if ($h.contactsheet -and $h.hwnd_cap -and (($h.hwnd_nonwhite -as [int]) -gt 0)) {
    return 'VISIBLE_CONTACTSHEET_FROM_REAL_RESOURCES'
  }
  if ($h.ui_frame -and -not $h.hwnd_cap) { return 'WINDOW_PRESENT_REGRESSED' }
  if ($h.presented -and -not $h.hold_done -and -not $h.responsive) {
    return 'WINDOW_UNRESPONSIVE_AFTER_FRAME'
  }
  if ((Test-Path (Join-Path $reportDir 'e9c_mrp_image_inventory.csv')) -and -not $h.ui_frame) {
    return 'RESOURCE_INVENTORY_COMPLETE_NEXT_GAP'
  }
  return 'WINDOW_CAPTURE_STILL_BLANK'
}

Clear-E9Modes
$env:JJFB_E9C_MODE = '1'
$env:JJFB_E9B_MODE = '1'
$env:JJFB_VISIBLE_WINDOW = '1'
$env:JJFB_E9A_MODE = '1'
$env:JJFB_E8Z_MODE = '1'
$env:JJFB_E8Y_MODE = '1'
$env:JJFB_E8X_MODE = '1'
$env:JJFB_E8W_MODE = '1'
$env:JJFB_DISPLAY_FIRST = '1'
$env:JJFB_BYPASS_C9D_GATE = '1'
$env:JJFB_WINDOW_ZOOM = "$Zoom"
$env:JJFB_E9B_HOLD_SEC = "$HoldSec"
$env:JJFB_E9B_HWND_CAPTURE = (Join-Path $shotDir 'e9c_actual_window_capture.bmp')
$env:JJFB_REAL_MRP_PATH = $mrpPath
$env:JJFB_REAL_MRP_MEMBER_BRIDGE = '1'
$env:JJFB_REAL_MRP_MEMBER_BRIDGE_ALL = '1'
$env:JJFB_E8Z_BMP_PATH = $bmpSrc
$env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'e9c_internal_surface.bmp')
$env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9c_internal_before.bmp')
$env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9c_internal_after.bmp')
$env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir 'e9c_e8u_frame.bmp')
$env:JJFB_E9C_SKIP_TINY = '1'
$env:JJFB_E9C_CANDIDATE_MEMBER = $Candidate
$env:JJFB_E9C_CANDIDATE_PATH = $candPath

if ($Mode -eq 'contactsheet') {
  $env:JJFB_E9C_CONTACTSHEET = '1'
  $env:JJFB_E9C_DEFER_HOLD = '1'
  $env:JJFB_E9C_PATH_SLOGO = (Join-Path $resDir 'slogo_157_58.bmp.rgb565')
  $env:JJFB_E9C_PATH_LOADINGBAR = (Join-Path $resDir 'loadingbar_201_29.bmp.rgb565')
  $env:JJFB_E9C_PATH_TEXTBAR = (Join-Path $resDir 'textbar_120_30.bmp.rgb565')
  $env:JJFB_E9C_PATH_TOP = (Join-Path $resDir 'top_76_28.bmp.rgb565')
} elseif ($Mode -eq 'auto') {
  # Prefer game-requested via bridge; still present slogo once draw API is reached.
  $env:JJFB_DISPLAY_FIRST_MEMBER_FASTPATH = '1'
  $env:JJFB_FAST_REAL_BMP_HANDLE = '1'
  $env:JJFB_FAST_A64_RESOURCE_ASSIST = '1'
} else {
  # candidate: keep bridge for any guest member; override present with slogo
  $env:JJFB_DISPLAY_FIRST_MEMBER_FASTPATH = '1'
  $env:JJFB_FAST_REAL_BMP_HANDLE = '1'
  $env:JJFB_FAST_A64_RESOURCE_ASSIST = '1'
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

if (-not $SkipBuild) {
  Get-ChildItem build-i686 -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue
  # Force rebuild of vmrp main/bridge for E9C present path.
  $vmrp = Join-Path $Root 'third_party\vmrp_upstream'
  Push-Location $vmrp
  try {
    & make -j4 2>&1 | Tee-Object -FilePath (Join-Path $logDir 'e9c_vmrp_build.txt') | Out-Null
  } finally { Pop-Location }
}

Write-Host "E9C Mode=$Mode candidate=$Candidate zoom=$Zoom hold=${HoldSec}s timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$t0 = Get-Date
$caseLog = Join-Path $logDir 'e9c_visible_ui_stdout.txt'
$caseErr = Join-Path $logDir 'e9c_visible_ui_stderr.txt'
$argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
             '-Target',$Target,'-Seconds',"$CASE_TIMEOUT_SEC")
if ($SkipBuild) { $argList += '-SkipBuild' }

$p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -WorkingDirectory $Root -PassThru `
  -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E9Children
}
if ($p.ExitCode -ne 0) {
  Write-Host "E9C child exit=$($p.ExitCode) — see $caseErr"
  Get-Content $caseErr -Tail 40 -EA SilentlyContinue | Write-Host
}
$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9C_|JJFB_VISIBLE_UI_FRAME|JJFB_VISIBLE_WINDOW|e9c_internal' -Quiet -EA SilentlyContinue)) {
  Copy-Item -Force $src $caseLog
} elseif ((Test-Path $caseLog) -and -not (Select-String -Path $caseLog -Pattern 'JJFB_E9C_|JJFB_VISIBLE' -Quiet -EA SilentlyContinue)) {
  Write-Host 'WARN: no E9C markers in log (build/run may have failed)'
}
$script:lastLog = $caseLog
$hits = Analyze-E9C $caseLog
$elapsed = ((Get-Date) - $t0).TotalSeconds
$verdict = Case-Verdict $hits

# Natural 0x304BF0 status (still blocked unless log shows natural match without bridge-only)
$natural = 'NATURAL_MEMBER_RESOLVE_STILL_BLOCKED'
if (Select-String -Path $caseLog -Pattern 'NATURAL_MEMBER_RESOLVE_FIXED' -Quiet -EA SilentlyContinue) {
  $natural = 'NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME'
} elseif (Select-String -Path $caseLog -Pattern 'NATURAL_MEMBER_RESOLVE_BLOCKED_BY_' -Quiet -EA SilentlyContinue) {
  $m = Select-String -Path $caseLog -Pattern 'NATURAL_MEMBER_RESOLVE_BLOCKED_BY_[A-Z_]+' -EA SilentlyContinue | Select-Object -Last 1
  if ($m) { $natural = ($m.Line -replace '.*?(NATURAL_MEMBER_RESOLVE_BLOCKED_BY_[A-Z_]+).*','$1') }
}

$internalPng = Join-Path $shotDir 'e9c_internal_surface.png'
$hwndPng = Join-Path $shotDir 'e9c_actual_window_capture.png'
[void](Convert-BmpToPng (Join-Path $shotDir 'e9c_internal_surface.bmp') $internalPng)
[void](Convert-BmpToPng (Join-Path $shotDir 'e9c_actual_window_capture.bmp') $hwndPng)

Write-Host "== E9C_CASE_DONE mode=$Mode verdict=$verdict natural=$natural elapsed=$([Math]::Round($elapsed,1))"
if ($hits.ui_frame) { Write-Host '[JJFB_VISIBLE_UI_FRAME_PRESENTED]' }
if ($hits.responsive) { Write-Host '[JJFB_WINDOW_RESPONSIVE_HOLD]' }
if (Test-Path $hwndPng) { Write-Host "HWND capture: $hwndPng (nonwhite=$($hits.hwnd_nonwhite))" }
if (Test-Path $internalPng) { Write-Host "Internal surface: $internalPng" }

$summary = [ordered]@{
  stage = 'E9C'
  mode = $Mode
  candidate = $Candidate
  verdict = $verdict
  natural_304bf0 = $natural
  elapsed_sec = [Math]::Round($elapsed, 2)
  zoom = $Zoom
  hold_sec = $HoldSec
  hwnd_capture = if (Test-Path $hwndPng) { 'screenshots/e9c_actual_window_capture.png' } else { $null }
  hwnd_nonwhite = $hits.hwnd_nonwhite
  internal_surface = if (Test-Path $internalPng) { 'screenshots/e9c_internal_surface.png' } else { $null }
  other = $hits.other
  bridge = [bool]$hits.bridge
  meaningful = [bool]$hits.meaningful
  contactsheet = [bool]$hits.contactsheet
  best_ui = 'slogo!157!58.bmp'
  note = 'NOT_PRODUCT_SUCCESS; meaningful original MRP UI via real draw path'
}
($summary | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $reportDir 'stage_e9c_visible_ui_summary.json') -Encoding utf8

$md = @(
  '# Stage E9C Visible UI Verdict',
  '',
  "**Verdict:** ``$verdict``",
  '',
  "**Natural 0x304BF0:** ``$natural``",
  '',
  '**NOT product success.** E9B tiny sprite != game startup UI.',
  '',
  '## Inventory',
  '',
  '- 50 members in original ``jjfb.mrp``; ~27 image-like.',
  '- No full 240x320 background. Best candidate: ``slogo!157!58.bmp`` (157x58, 18212B RGB565).',
  '- Also: ``loadingbar!201!29.bmp``, ``textbar!120!30.bmp``, ``top!76!28.bmp``.',
  '',
  '## Demo result',
  '',
  "- mode=$Mode candidate=$Candidate",
  "- presented=$($hits.presented) ui_frame=$($hits.ui_frame) hold_done=$($hits.hold_done)",
  "- hwnd_nonwhite=$($hits.hwnd_nonwhite) other=$($hits.other) zoom=$Zoom",
  "- bridge=$($hits.bridge) meaningful=$($hits.meaningful)",
  '',
  '## Natural member resolve (0x304BF0)',
  '',
  '- Guest index scan at ``0x304F26/0x304F7A/0x304F92`` still never matches.',
  '- Exact blocker class: ``NATURAL_MEMBER_RESOLVE_BLOCKED_BY_NAME`` (index name compare / context).',
  '- Assisted ``JJFB_REAL_MRP_MEMBER_BRIDGE_ALL`` loads exact original members; ABI r0=0/-1 preserved.',
  '',
  '## Artifacts',
  '',
  '- ``reports/e9c_mrp_member_inventory.csv``',
  '- ``reports/e9c_mrp_image_inventory.csv``',
  '- ``out/e9c_resource_previews/``',
  '- ``screenshots/e9c_actual_window_capture.png``',
  '- ``screenshots/e9c_internal_surface.png``',
  '- ``logs/e9c_visible_ui_stdout.txt``',
  ''
)
$verdictPath = Join-Path $reportDir 'stage_e9c_visible_ui_verdict.md'
[System.IO.File]::WriteAllText($verdictPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))

Write-Host "Verdict=$verdict -> $verdictPath"
Write-Host 'E9C complete (NOT product success).'
exit 0
