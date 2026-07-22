# Stage E8Z-FirstPixel: real wy_jiao1!11!11.bmp → mr_drawBitmap bmp!=0 → screenshot
# NOT product success. No fake UI / no invented pixels / no host framebuffer paint.
param(
  [int]$Seconds = 90,
  [string]$Target = 'gwy/jjfb.mrp',
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

$outDir = Join-Path $Root 'out\e8z_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$shotDir = Join-Path $Root 'evidence\screenshots'
$resDir = Join-Path $Root 'out\e8z_resources'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir, $shotDir, $resDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 25
$summaryPath = Join-Path $reportDir 'stage_e8z_firstpixel_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }

py -3 (Join-Path $Root 'tools\e8z_resource_probe.py') | Out-Host
$bmpPath = Join-Path $resDir 'wy_jiao1_11_11.bmp'
if (-not (Test-Path $bmpPath)) { throw "missing extracted BMP $bmpPath" }

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
  'p:0x2E2520','p:0x2E4788'
) -join ','

$insnDefault = '500000'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE','JJFB_E8Z_MODE',
    'JJFB_E8V_E88CC_TRACE','JJFB_E8V_CALL_2E993C','JJFB_E8V_INSN_LIMIT',
    'JJFB_E8X_INSN_LIMIT','JJFB_E8X_CALL_2F99D0','JJFB_E8X_DEEP','JJFB_E8Y_INSN_LIMIT','JJFB_E8Y_DEEP',
    'JJFB_E8W_REENTER_E88CC','JJFB_E8W_DEEP',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST','JJFB_FAST_A64_RESOURCE_ASSIST',
    'JJFB_FAST_REAL_BMP_HANDLE','JJFB_DISPLAY_FIRST_MEMBER_FASTPATH','JJFB_E8Z_BMP_PATH',
    'JJFB_E8Z_SCREENSHOT','JJFB_E8Z_SCREENSHOT_BEFORE','JJFB_E8Z_SCREENSHOT_AFTER',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3','JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_FAST_UI_INIT_CALL','JJFB_FAST_UI_STATE','JJFB_FAST_UI_ED8','JJFB_FAST_UI_CA3',
    'JJFB_FAST_UI_UPSTREAM','JJFB_FAST_UI_OBJECT_R0','JJFB_FAST_C9D_UNLOCK_CALL',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_BYPASS_CF5_GATE','JJFB_E8U_SCREENSHOT'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Stop-E8ZChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
      ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'ROBOTOL_MRCINIT|stage_e_product')
    } |
    ForEach-Object {
      try { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } catch {}
    }
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

function Convert-BmpToPng([string]$BmpPath, [string]$PngPath) {
  if (-not (Test-Path $BmpPath)) { return $false }
  try {
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop
    $img = [System.Drawing.Image]::FromFile((Resolve-Path $BmpPath))
    $img.Save($PngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    return $true
  } catch {
    Copy-Item -Force $BmpPath ($PngPath -replace '\.png$','.bmp')
    return $false
  }
}

function Analyze-Hits([string]$log) {
  $h = @{
    hit_2D92E4 = $false; hit_304BF0 = $false; hit_ret = $false
    hit_310BBC = $false; hit_DRAW = $false; hit_DRAW_nz = $false
    hit_first_frame = $false; hit_sprite = $false; hit_real_bmp = $false
    hit_fastpath = $false; hit_null_bmp = $false; hit_no_delta = $false
    class = ''; name = ''; bmp = ''; fault_pc = ''; c44nz = $false
  }
  if (-not $log -or -not (Test-Path $log)) { return $h }
  $h.hit_2D92E4 = [bool](Select-String -Path $log -Pattern 'JJFB_E8Y_2D92E4_ENTRY\]' -Quiet -EA SilentlyContinue)
  $h.hit_304BF0 = [bool](Select-String -Path $log -Pattern 'JJFB_E8Z_304BF0\]|JJFB_E8Y_HELPER\] pc=0x304BF0' -Quiet -EA SilentlyContinue)
  $h.hit_ret = [bool](Select-String -Path $log -Pattern 'JJFB_E8Y_2D92E4_RETURN\]|class=RESOURCE_INIT_2D92E4_COMPLETED|REAL_BMP_MEMBER_LOADED' -Quiet -EA SilentlyContinue)
  $h.hit_310BBC = [bool](Select-String -Path $log -Pattern 'JJFB_E8Y_310BBC\]|JJFB_E8X_DRAW_PATH\] pc=0x310BBC' -Quiet -EA SilentlyContinue)
  $h.hit_DRAW = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\] api=mr_drawBitmap' -Quiet -EA SilentlyContinue)
  $h.hit_DRAW_nz = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\] api=mr_drawBitmap bmp=0x(?!0\b)[0-9A-Fa-f]+' -Quiet -EA SilentlyContinue)
  $h.hit_first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_FRAME_REACHED\]' -Quiet -EA SilentlyContinue)
  $h.hit_sprite = [bool](Select-String -Path $log -Pattern 'JJFB_E8Z_SPRITE_BLIT\]' -Quiet -EA SilentlyContinue)
  $h.hit_real_bmp = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_REAL_BMP_HANDLE\] after' -Quiet -EA SilentlyContinue)
  $h.hit_fastpath = [bool](Select-String -Path $log -Pattern 'JJFB_DISPLAY_FIRST_MEMBER_FASTPATH\] name=' -Quiet -EA SilentlyContinue)
  $h.hit_null_bmp = [bool](Select-String -Path $log -Pattern 'class=DRAW_API_WITH_NULL_BMP|mr_drawBitmap bmp=0x0\b' -Quiet -EA SilentlyContinue)
  $h.hit_no_delta = [bool](Select-String -Path $log -Pattern 'DRAW_API_WITH_NONZERO_BMP_NO_FRAMEBUFFER_DELTA' -Quiet -EA SilentlyContinue)
  $h.c44nz = [bool](Select-String -Path $log -Pattern 'C44_after=0x1|E8R_C44_UNLOCKED' -Quiet -EA SilentlyContinue)
  $cl = Select-String -Path $log -Pattern 'JJFB_E8Z_CLASS\] class=([A-Z0-9_]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($cl -and $cl.Line -match 'class=([A-Z0-9_]+)') { $h.class = $Matches[1] }
  $nm = Select-String -Path $log -Pattern 'name="(wy_jiao[^"]*)"' -EA SilentlyContinue | Select-Object -First 1
  if ($nm -and $nm.Line -match 'name="([^"]*)"') { $h.name = $Matches[1] }
  $bm = Select-String -Path $log -Pattern '\[JJFB_DRAW\] api=mr_drawBitmap bmp=(0x[0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($bm -and $bm.Line -match 'bmp=(0x[0-9A-Fa-f]+)') { $h.bmp = $Matches[1] }
  $fm = Select-String -Path $log -Pattern 'UC_MEM_.*UNMAPPED' -EA SilentlyContinue | Select-Object -Last 1
  if ($fm) { $h.fault_pc = $fm.Line.Substring(0, [Math]::Min(120, $fm.Line.Length)) }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.hit_first_frame) { return 'FIRST_REAL_FRAME_REACHED' }
  if ($h.hit_DRAW_nz -and $h.hit_no_delta) { return 'DRAW_API_WITH_NONZERO_BMP_NO_FRAMEBUFFER_DELTA' }
  if ($h.hit_DRAW_nz -and $h.hit_sprite) { return 'DRAW_API_WITH_NONZERO_BMP_NO_FRAMEBUFFER_DELTA' }
  if ($h.hit_DRAW_nz) { return 'BMP_HANDLE_ASSIST_REACHED_DRAW_API' }
  if ($h.hit_null_bmp -or ($h.hit_DRAW -and -not $h.hit_DRAW_nz)) { return 'DRAW_API_WITH_NULL_BMP' }
  if ($h.hit_fastpath -or ($h.hit_real_bmp -and $h.hit_ret)) { return 'REAL_BMP_MEMBER_LOADED_NEXT_GAP' }
  if ($h.hit_real_bmp) { return 'BMP_HANDLE_LAYOUT_DERIVED_NEXT_GAP' }
  if ($h.hit_304BF0 -and -not $h.hit_ret) { return 'MEMBER_RESOLVE_304BF0_STILL_BLOCKED' }
  if ($h.hit_2D92E4 -and $h.hit_ret) { return 'MEMBER_RESOLVE_304BF0_FIXED_NEXT_GAP' }
  if ($h.hit_2D92E4) { return 'MEMBER_RESOLVE_304BF0_STILL_BLOCKED' }
  return 'DISPLAY_FIRST_STILL_BLANK_AFTER_REAL_BMP'
}

function Write-CaseDone([string]$Name, [string]$Verdict, [double]$Elapsed, [hashtable]$Hits) {
  $line = "== E8Z_CASE_DONE name=$Name verdict=$Verdict elapsed=$([Math]::Round($Elapsed,1))"
  Write-Host $line
  [Console]::Out.Flush()
  $obj = [ordered]@{
    case_name = $Name; verdict = $Verdict; elapsed_sec = [Math]::Round($Elapsed, 2)
    hit_2D92E4 = [bool]$Hits.hit_2D92E4; hit_304BF0 = [bool]$Hits.hit_304BF0
    hit_310BBC = [bool]$Hits.hit_310BBC; hit_DRAW = [bool]$Hits.hit_DRAW
    hit_DRAW_nz = [bool]$Hits.hit_DRAW_nz; hit_first_frame = [bool]$Hits.hit_first_frame
    hit_real_bmp = [bool]$Hits.hit_real_bmp; hit_fastpath = [bool]$Hits.hit_fastpath
    class = $Hits.class; name = $Hits.name; bmp = $Hits.bmp; c44nz = [bool]$Hits.c44nz
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content -Path $summaryPath -Encoding utf8
}

function Invoke-E8ZCase([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8Z $Label =="
  [Console]::Out.Flush()
  $t0 = Get-Date
  Clear-E8Modes
  $env:JJFB_E8Z_MODE = '1'
  $env:JJFB_E8Y_MODE = '1'
  $env:JJFB_E8X_MODE = '1'
  $env:JJFB_E8W_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_E8Z_BMP_PATH = $bmpPath
  $env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'e8z_first_real_frame.bmp')
  $env:JJFB_E8Z_SCREENSHOT_BEFORE = (Join-Path $shotDir 'e8z_first_real_frame_before.bmp')
  $env:JJFB_E8Z_SCREENSHOT_AFTER = (Join-Path $shotDir 'e8z_first_real_frame_after.bmp')
  $env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir "e8z_${Label}_frame.bmp")
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
  foreach ($k in $ExtraEnv.Keys) { Set-Item -Path "Env:$k" -Value $ExtraEnv[$k] }

  $caseLog = Join-Path $logDir "stage_e8z_${Label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e8z_${Label}_stderr.txt"
  $prodScript = Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'
  $argList = @(
    '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $prodScript,
    '-Target', $Target, '-Seconds', "$CASE_TIMEOUT_SEC"
  )
  if ($script:SkipBuildNext) { $argList += '-SkipBuild' }

  $verdict = 'OK'
  try {
    $p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList `
      -WorkingDirectory $Root -PassThru `
      -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
    $ok = $p.WaitForExit($OUTER_KILL_SEC * 1000)
    if (-not $ok) {
      $verdict = 'TIMEOUT'
      try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
      Stop-E8ZChildren
      Start-Sleep -Milliseconds 200
    } elseif ($p.ExitCode -ne 0) {
      $verdict = 'FATAL_ERROR'
    }
  } catch {
    $verdict = 'FATAL_ERROR'
    Write-Host "E8Z case exception: $_"
    Stop-E8ZChildren
  }

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  $prefer = $null
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E8Z_|JJFB_E8Y_|JJFB_DRAW|JJFB_FIRST_REAL_FRAME' -Quiet -EA SilentlyContinue)) {
    $prefer = $src
  } elseif ((Test-Path $vm) -and (Select-String -Path $vm -Pattern 'JJFB_E8Z_|JJFB_DRAW' -Quiet -EA SilentlyContinue)) {
    $prefer = $vm
  } elseif ((Test-Path $src) -and ((Get-Item $src).Length -gt 5000)) {
    $prefer = $src
  }
  if ($prefer) { Copy-Item -Force $prefer $caseLog }

  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $hits = Analyze-Hits $caseLog
  if ($verdict -eq 'FATAL_ERROR' -or $verdict -eq 'TIMEOUT' -or $verdict -eq 'OK') {
    $v2 = Case-Verdict $hits
    if ($v2 -ne 'DISPLAY_FIRST_STILL_BLANK_AFTER_REAL_BMP' -or $hits.c44nz -or $hits.hit_2D92E4 -or $hits.hit_DRAW) {
      $verdict = $v2
    }
  }
  Write-CaseDone -Name $Label -Verdict $verdict -Elapsed $elapsed -Hits $hits
  $script:SkipBuildNext = $true
  $stopMore = ($hits.hit_first_frame)
  return @{ log = $caseLog; verdict = $verdict; hits = $hits; elapsed = $elapsed; stopMore = $stopMore }
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'bridge.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'main.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}
$script:SkipBuildNext = [bool]$SkipBuild

$fastBase = @{
  JJFB_FAST_ASSIST = '1'
  JJFB_FAST_SVC_AB = 'return0'
  JJFB_FAST_STATE = '38'
  JJFB_FAST_CASE156_R1 = '20'
  JJFB_FAST_SEQUENCE = 'case156'
  JJFB_FAST_C6C22 = '1'
  JJFB_FAST_DEC30 = '1'
  JJFB_FAST_INSN_LIMIT = $insnDefault
  JJFB_FAST_UNLOCK_CALL = '1'
  JJFB_FAST_UNLOCK_WHEN = 'before'
  JJFB_DISPLAY_FIRST = '1'
  JJFB_BYPASS_C9D_GATE = '1'
  JJFB_FAST_F6C_OBJECT_ASSIST = '1'
  JJFB_FAST_F74_DESCRIPTOR_ASSIST = '1'
  JJFB_E8W_REENTER_E88CC = '1'
  JJFB_E8Y_INSN_LIMIT = '5000'
}

function New-FastExtra([hashtable]$Overlay) {
  $h = @{}
  foreach ($k in $fastBase.Keys) { $h[$k] = $fastBase[$k] }
  foreach ($k in $Overlay.Keys) { $h[$k] = $Overlay[$k] }
  return $h
}

$matrix = @(
  @{ L = 'A_member_resolve_deep'; Extra = (New-FastExtra @{}) },
  @{ L = 'B_member_fastpath_real_bytes'; Extra = (New-FastExtra @{
      JJFB_DISPLAY_FIRST_MEMBER_FASTPATH = '1'
      JJFB_FAST_REAL_BMP_HANDLE = '1'
    })
  },
  @{ L = 'C_real_bmp_handle_draw'; Extra = (New-FastExtra @{
      JJFB_FAST_A64_RESOURCE_ASSIST = '1'
      JJFB_FAST_REAL_BMP_HANDLE = '1'
    })
  }
)

Write-Host "E8Z-FirstPixel timeout=${CASE_TIMEOUT_SEC}s cases=$($matrix.Count) NOT_PRODUCT_SUCCESS bmp=$bmpPath"
[Console]::Out.Flush()

$results = @{}
foreach ($m in $matrix) {
  $results[$m.L] = Invoke-E8ZCase -Label $m.L -ExtraEnv $m.Extra
  if ($results[$m.L].stopMore) {
    Write-Host "E8Z stop-early: FIRST_REAL_FRAME in $($m.L)"
    break
  }
}

$best = 'DISPLAY_FIRST_STILL_BLANK_AFTER_REAL_BMP'
$rank = @{
  'FIRST_REAL_FRAME_REACHED' = 100
  'DRAW_API_WITH_NONZERO_BMP_NO_FRAMEBUFFER_DELTA' = 90
  'BMP_HANDLE_ASSIST_REACHED_DRAW_API' = 85
  'REAL_BMP_MEMBER_LOADED_NEXT_GAP' = 70
  'BMP_HANDLE_LAYOUT_DERIVED_NEXT_GAP' = 65
  'MEMBER_RESOLVE_304BF0_FIXED_NEXT_GAP' = 55
  'DRAW_API_WITH_NULL_BMP' = 40
  'MEMBER_RESOLVE_304BF0_STILL_BLOCKED' = 30
  'BMP_DECODE_FAILED' = 20
  'DISPLAY_FIRST_STILL_BLANK_AFTER_REAL_BMP' = 10
}
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $v = $results[$m.L].verdict
  $rv = if ($rank.ContainsKey($v)) { $rank[$v] } else { 5 }
  $rb = if ($rank.ContainsKey($best)) { $rank[$best] } else { 0 }
  if ($rv -gt $rb) { $best = $v }
}

$bmpShot = Join-Path $shotDir 'e8z_first_real_frame.bmp'
$pngShot = Join-Path $shotDir 'e8z_first_real_frame.png'
if (Test-Path $bmpShot) {
  [void](Convert-BmpToPng $bmpShot $pngShot)
  foreach ($side in @('before','after')) {
    $b = Join-Path $shotDir "e8z_first_real_frame_$side.bmp"
    $p = Join-Path $shotDir "e8z_first_real_frame_$side.png"
    if (Test-Path $b) { [void](Convert-BmpToPng $b $p) }
  }
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8Z-FirstPixel Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** Target: real ``wy_jiao1!11!11.bmp`` pixels → ``mr_drawBitmap bmp!=0`` → nontrivial screenshot.')
$md.Add('')
$md.Add('## Resource probe')
$md.Add('')
$md.Add('- Member: ``wy_jiao1!11!11.bmp`` from original ``jjfb.mrp``')
$md.Add('- Decoded: 242 bytes raw RGB565 (11×11), sha256 in ``reports/e8z_resource_probe.json``')
$md.Add('- Saved: ``out/e8z_resources/wy_jiao1_11_11.bmp``')
$md.Add('')
$md.Add('## Cases')
$md.Add('')
$md.Add('| Case | Verdict | Elapsed | 2D92E4 | 310BBC | DRAW nz | FRAME | bmp |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- |')
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $r = $results[$m.L]; $h = $r.hits
  $bmp = if ($h.bmp) { $h.bmp } else { '-' }
  $md.Add("| $($m.L) | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($h.hit_2D92E4) | $($h.hit_310BBC) | $($h.hit_DRAW_nz) | $($h.hit_first_frame) | $bmp |")
}
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``reports/stage_e8z_firstpixel_summary.jsonl``')
$md.Add('- ``logs/stage_e8z_*_stdout.txt``')
$md.Add('- ``logs/e8z_first_real_draw_stdout.txt``')
$md.Add('- ``screenshots/e8z_first_real_frame.png`` (if reached)')
$md.Add('- ``reports/e8z_resource_probe.json``')
$md.Add('')

foreach ($name in @('C_real_bmp_handle_draw','B_member_fastpath_real_bytes','A_member_resolve_deep')) {
  if (-not $results.ContainsKey($name)) { continue }
  Copy-Item -Force $results[$name].log (Join-Path $logDir 'e8z_first_real_draw_stdout.txt')
  break
}

$reportPath = Join-Path $reportDir 'stage_e8z_firstpixel_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8Z-FirstPixel complete (NOT product success)."
[Console]::Out.Flush()
