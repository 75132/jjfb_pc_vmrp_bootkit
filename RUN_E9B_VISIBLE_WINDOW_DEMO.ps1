# Stage E9B: present real first frame to the actual VMRP HWND (not PNG-only).
# NOT product success. Pixels still from original jjfb.mrp via mr_drawBitmap.
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 30,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('demo','bridge')]
  [string]$Path = 'demo',
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

$CASE_TIMEOUT_SEC = [Math]::Max(90, [Math]::Min(180, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 40

py -3 (Join-Path $Root 'tools\e8z_resource_probe.py') | Out-Null
$bmpSrc = Join-Path $Root 'out\e8z_resources\wy_jiao1_11_11.bmp'
if (-not (Test-Path $bmpSrc)) { throw "missing $bmpSrc" }
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
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_VISIBLE_WINDOW',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST','JJFB_FAST_A64_RESOURCE_ASSIST',
    'JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH','JJFB_REAL_MRP_MEMBER_BRIDGE',
    'JJFB_REAL_MRP_PATH','JJFB_E8Z_BMP_PATH','JJFB_E8Z_SCREENSHOT','JJFB_E8Z_SCREENSHOT_BEFORE',
    'JJFB_E8Z_SCREENSHOT_AFTER','JJFB_E8Y_INSN_LIMIT','JJFB_E8W_REENTER_E88CC',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_BYPASS_CF5_GATE','JJFB_E8U_SCREENSHOT',
    'JJFB_WINDOW_ZOOM','JJFB_E9B_HOLD_SEC','JJFB_E9B_HWND_CAPTURE'
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

function Analyze-E9B([string]$log) {
  $h = @{
    first_frame=$false; presented=$false; hold_done=$false; hwnd_cap=$false
    hwnd_nonwhite=''; other=''; bmp=''; source=''
    fast_real_bmp_handle=$false; original_mrp_pixels=$false; bridge=$false
  }
  if (-not (Test-Path $log)) { return $h }
  $h.first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_FRAME_REACHED\]' -Quiet -EA SilentlyContinue)
  $h.presented = [bool](Select-String -Path $log -Pattern 'JJFB_VISIBLE_WINDOW_PRESENTED\]' -Quiet -EA SilentlyContinue)
  $h.hold_done = [bool](Select-String -Path $log -Pattern 'JJFB_VISIBLE_WINDOW_HOLD_DONE\]' -Quiet -EA SilentlyContinue)
  $h.hwnd_cap = [bool](Select-String -Path $log -Pattern 'JJFB_E9B_HWND_CAPTURE\] path=' -Quiet -EA SilentlyContinue)
  $h.bridge = [bool](Select-String -Path $log -Pattern 'JJFB_REAL_MRP_MEMBER_BRIDGE\] hit=' -Quiet -EA SilentlyContinue)
  $h.fast_real_bmp_handle = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_REAL_BMP_HANDLE\] after' -Quiet -EA SilentlyContinue)
  $h.original_mrp_pixels = [bool](Select-String -Path $log -Pattern 'real_mrp_pixels_via_mr_drawBitmap|original_jjfb_mrp_decode|REAL_MEMBER_BRIDGE' -Quiet -EA SilentlyContinue)
  $nw = Select-String -Path $log -Pattern 'JJFB_E9B_HWND_CAPTURE\][^\r\n]*nonwhite_or_nonblack=(\d+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($nw -and $nw.Line -match 'nonwhite_or_nonblack=(\d+)') { $h.hwnd_nonwhite = $Matches[1] }
  $sp = Select-String -Path $log -Pattern 'JJFB_E8Z_SPRITE_BLIT\][^\r\n]*other=(\d+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($sp -and $sp.Line -match 'other=(\d+)') { $h.other = $Matches[1] }
  $bm = Select-String -Path $log -Pattern '\[JJFB_DRAW\] api=mr_drawBitmap bmp=(0x[0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($bm -and $bm.Line -match 'bmp=(0x[0-9A-Fa-f]+)') { $h.bmp = $Matches[1] }
  $src = Select-String -Path $log -Pattern 'JJFB_SCREENSHOT_SOURCE\][^\r\n]*kind=([^\s]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($src -and $src.Line -match 'kind=([^\s]+)') { $h.source = $Matches[1] }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.presented -and $h.hwnd_cap -and ($h.hwnd_nonwhite -as [int]) -gt 0 -and $h.hold_done) {
    return 'VISIBLE_WINDOW_DEMO_STABLE'
  }
  if ($h.presented -and $h.hwnd_cap -and ($h.hwnd_nonwhite -as [int]) -gt 0) {
    return 'VISIBLE_WINDOW_PRESENTED'
  }
  if ($h.first_frame -and -not $h.presented) { return 'FRAMEBUFFER_ONLY_NOT_WINDOW' }
  if (Select-String -Path $script:lastLog -Pattern 'WINDOW_PRESENT_BLOCKED_BY_MESSAGE_PUMP' -Quiet -EA SilentlyContinue) {
    return 'WINDOW_PRESENT_BLOCKED_BY_MESSAGE_PUMP'
  }
  if (Select-String -Path $script:lastLog -Pattern 'WINDOW_PRESENT_BLOCKED_BY_SURFACE_COPY' -Quiet -EA SilentlyContinue) {
    return 'WINDOW_PRESENT_BLOCKED_BY_SURFACE_COPY'
  }
  if (Select-String -Path $script:lastLog -Pattern 'WINDOW_PRESENT_BLOCKED_BY_WHITE_BACKGROUND' -Quiet -EA SilentlyContinue) {
    return 'WINDOW_PRESENT_BLOCKED_BY_WHITE_BACKGROUND'
  }
  if ($h.hwnd_cap -and (($h.hwnd_nonwhite -as [int]) -eq 0)) { return 'WINDOW_CAPTURE_STILL_BLANK' }
  if ($h.first_frame) { return 'FRAMEBUFFER_ONLY_NOT_WINDOW' }
  return 'WINDOW_CAPTURE_STILL_BLANK'
}

Clear-E9Modes
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
$env:JJFB_E9B_HWND_CAPTURE = (Join-Path $shotDir 'e9b_actual_window_capture.bmp')
$env:JJFB_REAL_MRP_PATH = $mrpPath
$env:JJFB_E8Z_BMP_PATH = $bmpSrc
$env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'e9b_internal_surface.bmp')
$env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9b_internal_before.bmp')
$env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9b_internal_after.bmp')
$env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir 'e9b_e8u_frame.bmp')

if ($Path -eq 'bridge') {
  $env:JJFB_REAL_MRP_MEMBER_BRIDGE = '1'
} else {
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
}

Write-Host "E9B Path=$Path zoom=$Zoom hold=${HoldSec}s timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT_SUCCESS"
Write-Host "Watch the VMRP window (logical 240x320). zoom=$Zoom -> $((240*$Zoom))x$((320*$Zoom)); runtime clamps to fit screen."
[Console]::Out.Flush()

$t0 = Get-Date
$caseLog = Join-Path $logDir 'e9b_visible_window_stdout.txt'
$caseErr = Join-Path $logDir 'e9b_visible_window_stderr.txt'
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
  Write-Host "E9B child exit=$($p.ExitCode) — see $caseErr"
  Get-Content $caseErr -Tail 30 -EA SilentlyContinue | Write-Host
}
$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
# Only accept product log if it contains E9B markers (avoid stale E9A log after build fail).
if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_WINDOW\]|JJFB_VISIBLE_WINDOW|JJFB_E9B_|e9b_internal' -Quiet -EA SilentlyContinue)) {
  Copy-Item -Force $src $caseLog
} elseif ((Test-Path $caseLog) -and -not (Select-String -Path $caseLog -Pattern 'JJFB_WINDOW\]|JJFB_VISIBLE_WINDOW' -Quiet -EA SilentlyContinue)) {
  Write-Host "WARN: no E9B window markers in log (build/run may have failed)"
}
$script:lastLog = $caseLog
$hits = Analyze-E9B $caseLog
$elapsed = ((Get-Date) - $t0).TotalSeconds
$verdict = Case-Verdict $hits

$internalPng = Join-Path $shotDir 'e9b_internal_surface.png'
$hwndPng = Join-Path $shotDir 'e9b_actual_window_capture.png'
$e9aPng = Join-Path $shotDir 'e9a_first_frame.png'
[void](Convert-BmpToPng (Join-Path $shotDir 'e9b_internal_surface.bmp') $internalPng)
[void](Convert-BmpToPng (Join-Path $shotDir 'e9b_actual_window_capture.bmp') $hwndPng)
# Keep e9a alias if present for side-by-side compare
if ((Test-Path $internalPng) -and -not (Test-Path $e9aPng)) {
  Copy-Item -Force $internalPng $e9aPng
}

Write-Host "== E9B_CASE_DONE name=visible_window verdict=$verdict elapsed=$([Math]::Round($elapsed,1))"
if ($hits.presented) { Write-Host "[JJFB_VISIBLE_WINDOW_PRESENTED]" }
if ($hits.first_frame) { Write-Host "[JJFB_FIRST_REAL_FRAME_REACHED]" }
if (Test-Path $hwndPng) { Write-Host "HWND capture: $hwndPng (nonwhite=$($hits.hwnd_nonwhite))" }
if (Test-Path $internalPng) { Write-Host "Internal surface: $internalPng" }

$summary = [ordered]@{
  stage='E9B'
  path=$Path
  verdict=$verdict
  elapsed_sec=[Math]::Round($elapsed,2)
  zoom=$Zoom
  hold_sec=$HoldSec
  screenshot_source = if ($hits.source) { $hits.source } else { 'SDL_GetWindowSurface_SaveBMP' }
  screenshot_source_note = 'internal SDL software surface dump; NOT HWND capture'
  hwnd_capture = if (Test-Path $hwndPng) { 'screenshots/e9b_actual_window_capture.png' } else { $null }
  hwnd_nonwhite = $hits.hwnd_nonwhite
  internal_surface = if (Test-Path $internalPng) { 'screenshots/e9b_internal_surface.png' } else { $null }
  bmp=$hits.bmp
  other=$hits.other
  # Clarify ambiguous E9A field:
  fast_real_bmp_handle = [bool]$hits.fast_real_bmp_handle
  original_mrp_pixels = [bool]$hits.original_mrp_pixels
  bridge = [bool]$hits.bridge
  note = 'NOT_PRODUCT_SUCCESS; present real mr_drawBitmap pixels to HWND'
}
($summary | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $reportDir 'stage_e9b_visible_window_summary.json') -Encoding utf8
($summary | ConvertTo-Json -Compress) | Set-Content (Join-Path $reportDir 'stage_e9b_visible_window_summary.jsonl') -Encoding utf8

# Also fix E9A ambiguity artifact if present
$e9aFix = Join-Path $reportDir 'stage_e9a_firstframe_summary.jsonl'
if (Test-Path $e9aFix) {
  $lines = Get-Content $e9aFix
  $fixed = foreach ($line in $lines) {
    if ($line -match 'B_real_member_bridge' -and $line -match '"real_bmp":false') {
      $line -replace '"real_bmp":false', '"real_bmp":false,"fast_real_bmp_handle":false,"original_mrp_pixels":true,"real_bmp_note":"real_bmp meant FAST_REAL_BMP_HANDLE assist; pixels still from original jjfb.mrp via bridge"'
    } elseif ($line -match 'demo_visible_frame' -and $line -match '"real_bmp":true') {
      $line -replace '"real_bmp":true', '"real_bmp":true,"fast_real_bmp_handle":true,"original_mrp_pixels":true,"real_bmp_note":"real_bmp=FAST_REAL_BMP_HANDLE assist; bytes from original jjfb.mrp member"'
    } else { $line }
  }
  $fixed | Set-Content $e9aFix -Encoding utf8
}

$md = @(
  '# Stage E9B-VisibleWindow Present Verdict',
  '',
  "**Verdict:** ``$verdict``",
  '',
  '**NOT product success.** E9A PNG != visible HWND.',
  '',
  '## Screenshot source (exact)',
  '',
  '- Internal ``e9a/e9b_*surface*.png``: ``SDL_GetWindowSurface`` + ``SDL_SaveBMP`` (software surface dump).',
  '- **Not** HWND/PrintWindow/BitBlt unless ``e9b_actual_window_capture.png`` exists.',
  '- E9A ``FIRST_REAL_FRAME_REACHED`` was framebuffer/surface success only.',
  '',
  '## Fixes applied',
  '',
  '- Removed ``SDL_WINDOW_OPENGL`` (UpdateWindowSurface was dumping surface while HWND stayed white).',
  '- ``guiPumpEvents`` during Unicorn slices + present hold.',
  '- Optional ``JJFB_WINDOW_ZOOM`` (nearest-neighbor display scale only).',
  '- HWND client capture via GDI BitBlt.',
  '',
  "## Results",
  '',
  "- presented=$($hits.presented) hold_done=$($hits.hold_done) hwnd_nonwhite=$($hits.hwnd_nonwhite)",
  "- bmp=$($hits.bmp) other=$($hits.other) zoom=$Zoom",
  "- fast_real_bmp_handle=$($hits.fast_real_bmp_handle) original_mrp_pixels=$($hits.original_mrp_pixels)",
  '',
  '## Artifacts',
  '',
  '- ``screenshots/e9b_actual_window_capture.png``',
  '- ``screenshots/e9b_internal_surface.png``',
  '- ``logs/e9b_visible_window_stdout.txt``',
  '- ``reports/stage_e9b_visible_window_summary.json``',
  ''
)
$verdictPath = Join-Path $reportDir 'stage_e9b_visible_window_verdict.md'
[System.IO.File]::WriteAllText($verdictPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))

Write-Host "Verdict=$verdict -> $verdictPath"
Write-Host "E9B complete (NOT product success)."
exit 0
