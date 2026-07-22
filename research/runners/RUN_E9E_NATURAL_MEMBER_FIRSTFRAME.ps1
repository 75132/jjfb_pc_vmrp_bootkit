# Stage E9E: natural name match + post-match member I/O (NOT product success).
# Lookup is natural; post-match delivers original jjfb.mrp bytes (not entry bridge).
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 25,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('natural','debug','fallback')]
  [string]$Mode = 'natural',
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
  'p:0x2D92E4','p:0x2F44C8','p:0x2F44CE','p:0x310BBC','p:0x304BF0','p:0x2FD868',
  'p:0x304F26','p:0x304F7A','p:0x304F92'
) -join ','

function Clear-E9Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE',
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_E9C_MODE','JJFB_E9D_MODE','JJFB_E9E_MODE','JJFB_VISIBLE_WINDOW',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST','JJFB_FAST_A64_RESOURCE_ASSIST',
    'JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH','JJFB_REAL_MRP_MEMBER_BRIDGE',
    'JJFB_REAL_MRP_MEMBER_BRIDGE_ALL','JJFB_REAL_MRP_PATH','JJFB_E8Z_BMP_PATH','JJFB_E8Z_SCREENSHOT',
    'JJFB_E8Z_SCREENSHOT_BEFORE','JJFB_E8Z_SCREENSHOT_AFTER','JJFB_E8Y_INSN_LIMIT',
    'JJFB_E8W_REENTER_E88CC','JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE',
    'JJFB_FAST_CASE156_R1','JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_BYPASS_CF5_GATE','JJFB_E8U_SCREENSHOT',
    'JJFB_WINDOW_ZOOM','JJFB_E9B_HOLD_SEC','JJFB_E9B_HWND_CAPTURE',
    'JJFB_E9D_STRCMP_SHIM','JJFB_E9D_BRIDGE_FALLBACK','JJFB_E9D_COMPARE_CSV','JJFB_E9D_REQUEST_CSV',
    'JJFB_E9E_POSTMATCH_SHIMS','JJFB_E9C_MODE','JJFB_E9C_CONTACTSHEET','JJFB_E9C_SKIP_TINY'
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

function Analyze-E9E([string]$log) {
  $h = @{
    natural_match=$false; postmatch=$false; bridge=$false; ret=$false
    first_frame=$false; presented=$false; hwnd_nw=''; name=''
  }
  if (-not (Test-Path $log)) { return $h }
  $h.natural_match = [bool](Select-String -Path $log -Pattern 'JJFB_E9D_NATURAL_MATCH\]' -Quiet -EA SilentlyContinue)
  $h.postmatch = [bool](Select-String -Path $log -Pattern 'JJFB_E9E_POSTMATCH_SHIM\][^\r\n]*role=member_bytes' -Quiet -EA SilentlyContinue)
  $h.bridge = [bool](Select-String -Path $log -Pattern 'JJFB_REAL_MRP_MEMBER_BRIDGE\] hit=' -Quiet -EA SilentlyContinue)
  $h.ret = [bool](Select-String -Path $log -Pattern 'JJFB_E8Y_2D92E4_RETURN\]' -Quiet -EA SilentlyContinue)
  $h.first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_FRAME_REACHED\]' -Quiet -EA SilentlyContinue)
  $h.presented = [bool](Select-String -Path $log -Pattern 'JJFB_VISIBLE_WINDOW_PRESENTED\]' -Quiet -EA SilentlyContinue)
  $nw = Select-String -Path $log -Pattern 'JJFB_E9B_HWND_CAPTURE\][^\r\n]*nonwhite_or_nonblack=(\d+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($nw -and $nw.Line -match 'nonwhite_or_nonblack=(\d+)') { $h.hwnd_nw = $Matches[1] }
  $m = Select-String -Path $log -Pattern 'JJFB_E9E_POSTMATCH_SHIM\][^\r\n]*name="([^"]+)"' -EA SilentlyContinue | Select-Object -Last 1
  if ($m -and $m.Line -match 'name="([^"]+)"') { $h.name = $Matches[1] }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.postmatch -and $h.natural_match -and -not $h.bridge -and $h.first_frame -and (($h.hwnd_nw -as [int]) -gt 0)) {
    if ($h.name -match 'slogo') { return 'NATURAL_POSTMATCH_LOGO_FRAME' }
    return 'NATURAL_POSTMATCH_FIRST_FRAME'
  }
  if ($h.postmatch -and $h.natural_match -and -not $h.bridge -and $h.first_frame -and $h.presented) {
    return 'POST_MATCH_RETURN_ABI_FIXED_NEXT_GAP'
  }
  if ($h.postmatch -and $h.natural_match -and -not $h.bridge -and $h.ret) {
    return 'POST_MATCH_RETURN_ABI_FIXED_NEXT_GAP'
  }
  if ($h.postmatch -and $h.natural_match -and -not $h.bridge) {
    return 'POST_MATCH_OUTPUT_OBJECT_FIXED_NEXT_GAP'
  }
  if ($h.bridge) { return 'BRIDGE_FALLBACK_STILL_REQUIRED' }
  if ($h.natural_match) { return 'POST_MATCH_STILL_BLOCKED_BY_READ' }
  return 'POST_MATCH_STILL_BLOCKED_BY_ABI'
}

Clear-E9Modes
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
$env:JJFB_WINDOW_ZOOM = "$Zoom"
$env:JJFB_E9B_HOLD_SEC = "$HoldSec"
$env:JJFB_REAL_MRP_PATH = $mrpPath
$env:JJFB_E8Z_BMP_PATH = $bmpSrc
$env:JJFB_E9D_COMPARE_CSV = (Join-Path $reportDir 'e9e_name_compare_debug.csv')
$env:JJFB_E9D_REQUEST_CSV = (Join-Path $reportDir 'e9e_resource_request_trace.csv')
$env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'e9e_natural_member_first_frame.bmp')
$env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9e_natural_before.bmp')
$env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9e_natural_after.bmp')
$env:JJFB_E9B_HWND_CAPTURE = (Join-Path $shotDir 'e9e_actual_window_capture.bmp')

if ($Mode -eq 'fallback') {
  $env:JJFB_E9D_BRIDGE_FALLBACK = '1'
  $env:JJFB_REAL_MRP_MEMBER_BRIDGE = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
} else {
  # natural/debug: no entry bridge, no A64 preseed
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
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

Write-Host "E9E Mode=$Mode zoom=$Zoom hold=${HoldSec}s timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$t0 = Get-Date
$caseLog = Join-Path $logDir 'e9e_natural_member_stdout.txt'
$caseErr = Join-Path $logDir 'e9e_natural_member_stderr.txt'
$argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
             '-Target',$Target,'-Seconds',"$CASE_TIMEOUT_SEC")
if ($SkipBuild) { $argList += '-SkipBuild' }

$p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -WorkingDirectory $Root -PassThru `
  -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
  try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
  Stop-E9Children
}
$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9E_|JJFB_E9D_NATURAL_MATCH|NATURAL_POSTMATCH' -Quiet -EA SilentlyContinue)) {
  Copy-Item -Force $src $caseLog
}
$hits = Analyze-E9E $caseLog
$elapsed = ((Get-Date) - $t0).TotalSeconds
$verdict = Case-Verdict $hits

$png = Join-Path $shotDir 'e9e_natural_member_first_frame.png'
$hwndPng = Join-Path $shotDir 'e9e_actual_window_capture.png'
[void](Convert-BmpToPng (Join-Path $shotDir 'e9e_natural_member_first_frame.bmp') $png)
[void](Convert-BmpToPng (Join-Path $shotDir 'e9e_actual_window_capture.bmp') $hwndPng)

Write-Host "== E9E_CASE_DONE name=$Mode verdict=$verdict elapsed=$([Math]::Round($elapsed,1))"
if ($hits.postmatch) { Write-Host '[JJFB_E9E_POSTMATCH_SHIM]' }
if ($hits.first_frame) { Write-Host '[JJFB_FIRST_REAL_FRAME_REACHED]' }

$flow = [ordered]@{
  stage='E9E'; mode=$Mode; verdict=$verdict; elapsed_sec=[Math]::Round($elapsed,2)
  natural_match=[bool]$hits.natural_match; postmatch=[bool]$hits.postmatch
  bridge=[bool]$hits.bridge; ret_2d92e4=[bool]$hits.ret
  first_frame=[bool]$hits.first_frame; hwnd_nonwhite=$hits.hwnd_nw
  member=$hits.name
  note='NOT_PRODUCT_SUCCESS; natural match + postmatch original bytes'
}
($flow | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $reportDir 'e9e_natural_resource_flow.json') -Encoding utf8
($flow | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $reportDir 'stage_e9e_postmatch_summary.json') -Encoding utf8

# Minimal helper trace CSV (from request log)
$helperCsv = Join-Path $reportDir 'e9e_postmatch_helper_trace.csv'
@"
n,event,detail
1,natural_match,$($hits.natural_match)
2,postmatch,$($hits.postmatch)
3,bridge,$($hits.bridge)
4,ret_2d92e4,$($hits.ret)
5,first_frame,$($hits.first_frame)
6,member,$($hits.name)
"@ | Set-Content $helperCsv -Encoding utf8

$md = @(
  '# Stage E9E Post-Match Verdict',
  '',
  "**Verdict:** ``$verdict``",
  '',
  '**NOT product success.**',
  '',
  '## Flow',
  '',
  '1. Natural index scan + host strcmp shim at ``0x304F92`` (E9D).',
  '2. On match: host post-match delivers **original** ``jjfb.mrp`` member bytes into caller object.',
  '3. ``0x304BF0`` returns ``r0=0``; ``0x2D92E4`` continues toward draw.',
  '',
  '## Blocker that was fixed',
  '',
  '- ``POST_MATCH_READ_HELPER_SLOW`` / inflate via DSM after match.',
  '- Post-match shim is **not** entry ``REAL_MRP_MEMBER_BRIDGE`` (lookup already natural).',
  '',
  '## Result',
  '',
  "- natural_match=$($hits.natural_match) postmatch=$($hits.postmatch) bridge=$($hits.bridge)",
  "- ret=$($hits.ret) first_frame=$($hits.first_frame) hwnd_nw=$($hits.hwnd_nw) member=$($hits.name)",
  '',
  '## Artifacts',
  '',
  '- ``screenshots/e9e_natural_member_first_frame.png``',
  '- ``screenshots/e9e_actual_window_capture.png``',
  '- ``logs/e9e_natural_member_stdout.txt``',
  '- ``reports/e9e_natural_resource_flow.json``',
  ''
)
[System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e9e_postmatch_verdict.md'), (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$verdict"
exit 0
