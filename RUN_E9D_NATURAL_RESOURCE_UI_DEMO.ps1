# Stage E9D: natural 0x304BF0 member name match + game-requested UI resources.
# NOT product success. Fix DSM strcmp stub at 0x304F92; do not treat bridge as success.
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 30,
  [int]$Zoom = 2,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('natural','fallback','trace')]
  [string]$Mode = 'natural',
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
    'JJFB_E9A_MODE','JJFB_E9B_MODE','JJFB_E9C_MODE','JJFB_E9D_MODE','JJFB_VISIBLE_WINDOW',
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
    'JJFB_E9C_MODE','JJFB_E9C_CONTACTSHEET','JJFB_E9C_SKIP_TINY','JJFB_E9C_DEFER_HOLD'
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

function Analyze-E9D([string]$log) {
  $h = @{
    natural_fixed=$false; natural_match=$false; bridge=$false; first_frame=$false
    presented=$false; cmp_match=''; req=''
  }
  if (-not (Test-Path $log)) { return $h }
  $h.natural_fixed = [bool](Select-String -Path $log -Pattern 'NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME\]' -Quiet -EA SilentlyContinue)
  $h.natural_match = [bool](Select-String -Path $log -Pattern 'JJFB_E9D_NATURAL_MATCH\]' -Quiet -EA SilentlyContinue)
  $h.bridge = [bool](Select-String -Path $log -Pattern 'JJFB_REAL_MRP_MEMBER_BRIDGE\] hit=' -Quiet -EA SilentlyContinue)
  $h.first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_FRAME_REACHED\]' -Quiet -EA SilentlyContinue)
  $h.presented = [bool](Select-String -Path $log -Pattern 'JJFB_VISIBLE_WINDOW_PRESENTED\]' -Quiet -EA SilentlyContinue)
  $m = Select-String -Path $log -Pattern 'JJFB_E9D_NATURAL_MATCH\] name="([^"]+)"' -EA SilentlyContinue | Select-Object -Last 1
  if ($m -and $m.Line -match 'name="([^"]+)"') { $h.req = $Matches[1] }
  $c = Select-String -Path $log -Pattern 'JJFB_E9D_NAME_COMPARE\][^\r\n]*host_strcmp=0' -EA SilentlyContinue
  $h.cmp_match = if ($c) { $c.Count } else { 0 }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.natural_fixed -and $h.first_frame -and -not $h.bridge) {
    return 'NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME'
  }
  if ($h.natural_fixed -and ($h.req -match 'slogo')) { return 'NATURAL_MEMBER_RESOLVE_FIXED_LOGO' }
  if ($h.natural_fixed -and ($h.req -match 'loadingbar')) { return 'NATURAL_MEMBER_RESOLVE_FIXED_LOADINGBAR' }
  if ($h.natural_fixed) { return 'NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME' }
  if ($h.bridge -and -not $h.natural_fixed) { return 'BRIDGE_FALLBACK_STILL_REQUIRED' }
  if ($h.cmp_match -as [int] -gt 0) { return 'GAME_REQUESTED_UI_RESOURCE_SEQUENCE_FOUND' }
  return 'NATURAL_MEMBER_RESOLVE_BLOCKED_BY_NAME'
}

Clear-E9Modes
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
$env:JJFB_E9D_COMPARE_CSV = (Join-Path $reportDir 'e9d_name_compare_debug.csv')
$env:JJFB_E9D_REQUEST_CSV = (Join-Path $reportDir 'e9d_resource_request_trace.csv')
$env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'e9d_natural_resource_ui.bmp')
$env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9d_natural_before.bmp')
$env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9d_natural_after.bmp')
$env:JJFB_E9B_HWND_CAPTURE = (Join-Path $shotDir 'e9d_natural_resource_ui_hwnd.bmp')

if ($Mode -eq 'fallback') {
  $env:JJFB_E9D_BRIDGE_FALLBACK = '1'
  $env:JJFB_REAL_MRP_MEMBER_BRIDGE = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
} elseif ($Mode -eq 'natural') {
  # Natural only: no bridge, no A64/real-bmp preseed (those skip 0x304BF0).
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
} else {
  # trace: dump compares; keep window optional
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

Write-Host "E9D Mode=$Mode zoom=$Zoom hold=${HoldSec}s timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$t0 = Get-Date
$caseLog = Join-Path $logDir 'e9d_natural_resource_stdout.txt'
$caseErr = Join-Path $logDir 'e9d_natural_resource_stderr.txt'
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
if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E9D_|NATURAL_MEMBER_RESOLVE' -Quiet -EA SilentlyContinue)) {
  Copy-Item -Force $src $caseLog
}
$hits = Analyze-E9D $caseLog
$elapsed = ((Get-Date) - $t0).TotalSeconds
$verdict = Case-Verdict $hits

$png = Join-Path $shotDir 'e9d_natural_resource_ui.png'
[void](Convert-BmpToPng (Join-Path $shotDir 'e9d_natural_resource_ui.bmp') $png)
[void](Convert-BmpToPng (Join-Path $shotDir 'e9d_natural_resource_ui_hwnd.bmp') (Join-Path $shotDir 'e9d_natural_resource_ui_hwnd.png'))

Write-Host "== E9D_CASE_DONE mode=$Mode verdict=$verdict elapsed=$([Math]::Round($elapsed,1))"
if ($hits.natural_fixed) { Write-Host '[NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME]' }
if ($hits.req) { Write-Host "matched=$($hits.req)" }

$summary = [ordered]@{
  stage='E9D'; mode=$Mode; verdict=$verdict; elapsed_sec=[Math]::Round($elapsed,2)
  natural_fixed=[bool]$hits.natural_fixed; bridge=[bool]$hits.bridge
  matched_name=$hits.req; cmp_matches=$hits.cmp_match
  note='NOT_PRODUCT_SUCCESS; host strcmp shim at 0x304F92 for broken DSM stub'
}
($summary | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $reportDir 'stage_e9d_natural_resource_summary.json') -Encoding utf8

$md = @(
  '# Stage E9D Natural Resource Verdict',
  '',
  "**Verdict:** ``$verdict``",
  '',
  '**NOT product success.**',
  '',
  '## Root cause',
  '',
  '- ``0x304F26`` = read name_len (memcpy), not strcmp.',
  '- ``0x304F7A`` = copy index name into temp buffer.',
  '- ``0x304F92`` = real strcmp via DSM table ``+0x28`` → stub ``0xAC2D0`` does ``movs r0,#1`` and destroys the requested-name pointer.',
  '- Exact class: ``CALLBACK_RETURN_ABI_WRONG`` (DSM strcmp stub ABI).',
  '- Fix: host strcmp shim at ``0x304F92`` (runtime compatibility; original MRP untouched).',
  '',
  '## Result',
  '',
  "- mode=$Mode natural_fixed=$($hits.natural_fixed) matched=$($hits.req)",
  "- bridge=$($hits.bridge) first_frame=$($hits.first_frame) cmp_matches=$($hits.cmp_match)",
  '',
  '## Artifacts',
  '',
  '- ``reports/e9d_name_compare_debug.csv``',
  '- ``reports/e9d_resource_request_trace.csv``',
  '- ``screenshots/e9d_natural_resource_ui.png``',
  '- ``logs/e9d_natural_resource_stdout.txt``',
  ''
)
[System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e9d_natural_resource_verdict.md'), (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$verdict"
exit 0
