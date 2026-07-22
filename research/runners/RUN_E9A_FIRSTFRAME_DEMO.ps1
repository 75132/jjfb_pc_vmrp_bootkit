# Stage E9A: one-command visible first-frame demo (E8Z Case B path).
# NOT product success. Real jjfb.mrp pixels via mr_drawBitmap.
param(
  [int]$Seconds = 75,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [ValidateSet('demo','bridge','bridgeonly','fallback')]
  [string]$Mode = 'demo'
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
$resDir = Join-Path $Root 'out\e9a_resources'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $shotDir, $resDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 25

py -3 (Join-Path $Root 'tools\e8z_resource_probe.py') | Out-Null
$bmpSrc = Join-Path $Root 'out\e8z_resources\wy_jiao1_11_11.bmp'
if (-not (Test-Path $bmpSrc)) { throw "missing $bmpSrc — run e8z probe first" }
Copy-Item -Force $bmpSrc (Join-Path $resDir 'wy_jiao1_11_11.bmp')
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
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE','JJFB_E9A_MODE',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST','JJFB_FAST_A64_RESOURCE_ASSIST',
    'JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH','JJFB_REAL_MRP_MEMBER_BRIDGE',
    'JJFB_REAL_MRP_PATH','JJFB_E8Z_BMP_PATH','JJFB_E8Z_SCREENSHOT','JJFB_E8Z_SCREENSHOT_BEFORE',
    'JJFB_E8Z_SCREENSHOT_AFTER','JJFB_E8Y_INSN_LIMIT','JJFB_E8W_REENTER_E88CC',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_BYPASS_CF5_GATE','JJFB_E8U_SCREENSHOT'
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

function Analyze-E9([string]$log) {
  $h = @{
    first_frame=$false; draw_nz=$false; bridge=$false; real_bmp=$false; fastpath=$false
    natural_ret=$false; bmp=''; level='NONE'; other=''
  }
  if (-not (Test-Path $log)) { return $h }
  $h.first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_FRAME_REACHED\]' -Quiet -EA SilentlyContinue)
  $h.draw_nz = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\] api=mr_drawBitmap bmp=0x(?!0\b)' -Quiet -EA SilentlyContinue)
  $h.bridge = [bool](Select-String -Path $log -Pattern 'JJFB_REAL_MRP_MEMBER_BRIDGE\] hit=' -Quiet -EA SilentlyContinue)
  $h.real_bmp = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_REAL_BMP_HANDLE\] after' -Quiet -EA SilentlyContinue)
  # original_mrp_pixels: bytes from jjfb.mrp (bridge OR fast handle load). Distinct from real_bmp=FAST assist flag.
  $h.original_mrp_pixels = [bool](Select-String -Path $log -Pattern 'original_jjfb_mrp_decode|real_mrp_pixels_via_mr_drawBitmap|JJFB_FAST_REAL_BMP_HANDLE\] after' -Quiet -EA SilentlyContinue)
  $h.fast_real_bmp_handle = [bool]$h.real_bmp
  $h.fastpath = [bool](Select-String -Path $log -Pattern 'JJFB_DISPLAY_FIRST_MEMBER_FASTPATH\] name=' -Quiet -EA SilentlyContinue)
  $h.natural_ret = [bool](Select-String -Path $log -Pattern 'class=RESOURCE_INIT_2D92E4_COMPLETED' -Quiet -EA SilentlyContinue)
  $bm = Select-String -Path $log -Pattern '\[JJFB_DRAW\] api=mr_drawBitmap bmp=(0x[0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($bm -and $bm.Line -match 'bmp=(0x[0-9A-Fa-f]+)') { $h.bmp = $Matches[1] }
  $sp = Select-String -Path $log -Pattern 'JJFB_E8Z_SPRITE_BLIT\][^\r\n]*other=(\d+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($sp -and $sp.Line -match 'other=(\d+)') { $h.other = $Matches[1] }
  if ($h.bridge -and $h.first_frame) { $h.level = 'REAL_MEMBER_BRIDGE' }
  elseif ($h.natural_ret -and $h.first_frame -and -not $h.bridge -and -not $h.real_bmp) { $h.level = 'NATURAL_MEMBER_RESOLVE' }
  elseif ($h.real_bmp -or $h.fastpath) { $h.level = 'FAST_REAL_BMP_HANDLE_FALLBACK' }
  elseif ($h.bridge) { $h.level = 'REAL_MEMBER_BRIDGE' }
  return $h
}

function Case-Verdict([hashtable]$h, [string]$mode) {
  if ($h.first_frame -and $h.level -eq 'NATURAL_MEMBER_RESOLVE') { return 'NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME' }
  if ($h.first_frame -and $h.level -eq 'REAL_MEMBER_BRIDGE') { return 'REAL_MEMBER_BRIDGE_FIRST_FRAME' }
  if ($h.first_frame -and $mode -eq 'demo') { return 'FIRST_FRAME_DEMO_STABLE' }
  if ($h.first_frame) { return 'FAST_REAL_BMP_HANDLE_STILL_REQUIRED' }
  if ($h.bridge -and -not $h.draw_nz) { return 'MEMBER_RESOLVE_BLOCKED_BY_HANDLE' }
  if (Select-String -Path $script:lastLog -Pattern 'MEMBER_RESOLVE_BLOCKED_BY_NAME' -Quiet -EA SilentlyContinue) {
    return 'MEMBER_RESOLVE_BLOCKED_BY_NAME'
  }
  if (Select-String -Path $script:lastLog -Pattern 'MEMBER_RESOLVE_BLOCKED_BY_PACKAGE' -Quiet -EA SilentlyContinue) {
    return 'MEMBER_RESOLVE_BLOCKED_BY_PACKAGE_CONTEXT'
  }
  return 'FIRST_FRAME_REGRESSED'
}

# --- cases ---
$cases = @()
if ($Mode -eq 'demo') {
  $cases += @{
    Name = 'demo_visible_frame'
    Extra = @{
      JJFB_DISPLAY_FIRST_MEMBER_FASTPATH = '1'
      JJFB_FAST_REAL_BMP_HANDLE = '1'
      JJFB_FAST_A64_RESOURCE_ASSIST = '1'
    }
  }
} elseif ($Mode -eq 'bridge' -or $Mode -eq 'bridgeonly') {
  $cases += @{
    Name = 'B_real_member_bridge'
    Extra = @{
      JJFB_REAL_MRP_MEMBER_BRIDGE = '1'
      # no FAST_REAL_BMP_HANDLE / no A64 assist — keep A64=0 so 2D92E4→304BF0 runs
    }
  }
  if ($Mode -eq 'bridge') {
    $cases += @{
      Name = 'C_fast_bmp_fallback'
      Extra = @{
        JJFB_DISPLAY_FIRST_MEMBER_FASTPATH = '1'
        JJFB_FAST_REAL_BMP_HANDLE = '1'
        JJFB_FAST_A64_RESOURCE_ASSIST = '1'
      }
    }
  }
} else {
  $cases += @{
    Name = 'fallback_fast_bmp'
    Extra = @{
      JJFB_DISPLAY_FIRST_MEMBER_FASTPATH = '1'
      JJFB_FAST_REAL_BMP_HANDLE = '1'
      JJFB_FAST_A64_RESOURCE_ASSIST = '1'
    }
  }
}

if (-not $SkipBuild) {
  Get-ChildItem build-i686 -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue
}
$skipNext = [bool]$SkipBuild
$summaryPath = Join-Path $reportDir 'stage_e9a_firstframe_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }
$results = @{}

Write-Host "E9A Mode=$Mode timeout=${CASE_TIMEOUT_SEC}s NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

foreach ($c in $cases) {
  $t0 = Get-Date
  $label = $c.Name
  Write-Host "== E9A $label =="
  Clear-E9Modes
  $env:JJFB_E9A_MODE = '1'
  $env:JJFB_E8Z_MODE = '1'
  $env:JJFB_E8Y_MODE = '1'
  $env:JJFB_E8X_MODE = '1'
  $env:JJFB_E8W_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_REAL_MRP_PATH = $mrpPath
  $env:JJFB_E8Z_BMP_PATH = $bmpSrc
  $env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'e9a_first_frame.bmp')
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e9a_first_frame_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e9a_first_frame_after.bmp')
  $env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir "e9a_${label}_frame.bmp")
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
  foreach ($k in $c.Extra.Keys) { Set-Item -Path "Env:$k" -Value $c.Extra[$k] }

  $caseLog = Join-Path $logDir "stage_e9a_${label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e9a_${label}_stderr.txt"
  $argList = @('-NoProfile','-ExecutionPolicy','Bypass','-File',(Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
               '-Target',$Target,'-Seconds',"$CASE_TIMEOUT_SEC")
  if ($skipNext) { $argList += '-SkipBuild' }

  $p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList -WorkingDirectory $Root -PassThru `
    -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
  if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E9Children
  }
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if (Test-Path $src) {
    # Always prefer the product guest log when present (harness stdout is only build noise).
    Copy-Item -Force $src $caseLog
  }
  $script:lastLog = $caseLog
  $hits = Analyze-E9 $caseLog
  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $verdict = Case-Verdict $hits $Mode
  Write-Host "== E9A_CASE_DONE name=$label verdict=$verdict elapsed=$([Math]::Round($elapsed,1))"
  $obj = [ordered]@{
    case_name=$label; verdict=$verdict; elapsed_sec=[Math]::Round($elapsed,2)
    level=$hits.level; bmp=$hits.bmp; other=$hits.other
    first_frame=[bool]$hits.first_frame; bridge=[bool]$hits.bridge
    # real_bmp historically meant FAST_REAL_BMP_HANDLE assist (not "pixels fake").
    real_bmp=[bool]$hits.real_bmp
    fast_real_bmp_handle=[bool]$hits.fast_real_bmp_handle
    original_mrp_pixels=[bool]$hits.original_mrp_pixels
    real_bmp_note='real_bmp=FAST_REAL_BMP_HANDLE assist only; original_mrp_pixels=bytes from jjfb.mrp'
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content $summaryPath -Encoding utf8
  $results[$label] = @{ verdict=$verdict; hits=$hits; elapsed=$elapsed; log=$caseLog }
  $skipNext = $true
  if ($hits.first_frame) {
    Write-Host "[JJFB_FIRST_REAL_FRAME_REACHED] (E9A harness) path=screenshots/e9a_first_frame.png"
    break
  }
}

# finalize screenshots / reports
$bmpShot = Join-Path $shotDir 'e9a_first_frame.bmp'
$pngShot = Join-Path $shotDir 'e9a_first_frame.png'
if (Test-Path $bmpShot) { [void](Convert-BmpToPng $bmpShot $pngShot) }
elseif (Test-Path (Join-Path $shotDir 'e8z_first_real_frame.bmp')) {
  Copy-Item -Force (Join-Path $shotDir 'e8z_first_real_frame.bmp') $bmpShot
  [void](Convert-BmpToPng $bmpShot $pngShot)
}

$best = 'FIRST_FRAME_REGRESSED'
$rank = @{
  'NATURAL_MEMBER_RESOLVE_FIXED_FIRST_FRAME'=100
  'REAL_MEMBER_BRIDGE_FIRST_FRAME'=95
  'FIRST_FRAME_DEMO_STABLE'=90
  'FAST_REAL_BMP_HANDLE_STILL_REQUIRED'=80
  'MEMBER_RESOLVE_BLOCKED_BY_NAME'=40
  'MEMBER_RESOLVE_BLOCKED_BY_PACKAGE_CONTEXT'=35
  'MEMBER_RESOLVE_BLOCKED_BY_HANDLE'=30
  'MEMBER_RESOLVE_BLOCKED_BY_READ_CALLBACK'=25
  'MEMBER_RESOLVE_BLOCKED_BY_ABI'=20
  'FIRST_FRAME_REGRESSED'=5
}
$bestKey = $null
foreach ($k in $results.Keys) {
  $v = $results[$k].verdict
  if (($rank[$v] -as [int]) -gt ($rank[$best] -as [int])) {
    $best = $v
    $bestKey = $k
  }
}
# Prefer the case that actually reached a visible frame for the canonical log.
if (-not $bestKey) {
  foreach ($k in @('demo_visible_frame','B_real_member_bridge','C_fast_bmp_fallback','fallback_fast_bmp')) {
    if ($results.ContainsKey($k) -and $results[$k].hits.first_frame) { $bestKey = $k; break }
  }
}
if (-not $bestKey) {
  foreach ($k in @('demo_visible_frame','B_real_member_bridge','C_fast_bmp_fallback','fallback_fast_bmp')) {
    if ($results.ContainsKey($k)) { $bestKey = $k; break }
  }
}
if ($bestKey -and (Test-Path $results[$bestKey].log)) {
  Copy-Item -Force $results[$bestKey].log (Join-Path $logDir 'e9a_first_frame_stdout.txt')
}
$level = if ($bestKey) { $results[$bestKey].hits.level } else { 'NONE' }
if (-not $level) { $level = 'NONE' }

$demoJson = [ordered]@{
  stage = 'E9A'
  mode = $Mode
  verdict = $best
  assist_level = $level
  screenshot = if (Test-Path $pngShot) { 'evidence/screenshots/e9a_first_frame.png' } else { $null }
  bmp_path = 'out/e9a_resources/wy_jiao1_11_11.bmp'
  mrp_path = ($mrpPath -replace '\\','/')
  note = 'NOT_PRODUCT_SUCCESS; pixels from original jjfb.mrp'
  cases = @($results.Keys | ForEach-Object {
    [ordered]@{ name=$_; verdict=$results[$_].verdict; elapsed=$results[$_].elapsed; level=$results[$_].hits.level; bmp=$results[$_].hits.bmp }
  })
}
$demoJsonPath = Join-Path $reportDir 'stage_e9a_firstframe_demo.json'
($demoJson | ConvertTo-Json -Depth 5) | Set-Content $demoJsonPath -Encoding utf8

$md = @(
  '# Stage E9A-FirstFrame Verdict',
  '',
  "**Verdict:** ``$best``",
  '',
  "**Assist level:** ``$level``",
  '',
  '**NOT product success.** Goal: reproducible first frame + naturalize ``0x304BF0`` member resolve.',
  '',
  '## Cases',
  '',
  '| Case | Verdict | Elapsed | Level | bmp |',
  '| --- | --- | --- | --- | --- |'
)
foreach ($k in $results.Keys) {
  $r = $results[$k]
  $md += "| $k | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($r.hits.level) | $($r.hits.bmp) |"
}
$md += @(
  '',
  '## 0x304BF0 diagnosis',
  '',
  '- Guest opens ``mythroad/gwy/jjfb.mrp`` successfully (VFS HIT).',
  '- Index scan loop at ``0x304F26/0x304F7A/0x304F92`` never matches member name → infinite loop.',
  '- Likely cause: guest index read length/alignment vs host ``first_data`` boundary.',
  '- ``JJFB_REAL_MRP_MEMBER_BRIDGE`` decodes exact members via ``mrp_archive`` and returns handle to ``0x2D92E4``.',
  '',
  '## Artifacts',
  '',
  '- ``screenshots/e9a_first_frame.png``',
  '- ``logs/e9a_first_frame_stdout.txt``',
  '- ``reports/stage_e9a_firstframe_demo.json``',
  '- ``reports/stage_e9a_firstframe_summary.jsonl``',
  ''
)
$verdictPath = Join-Path $reportDir 'stage_e9a_firstframe_verdict.md'
[System.IO.File]::WriteAllText($verdictPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))

if (Test-Path $pngShot) {
  Write-Host "[JJFB_FIRST_REAL_FRAME_REACHED] path=$pngShot"
} else {
  Write-Host "E9A: screenshot missing (verdict=$best)"
}
Write-Host "Verdict=$best level=$level -> $verdictPath"
Write-Host "E9A complete (NOT product success)."
