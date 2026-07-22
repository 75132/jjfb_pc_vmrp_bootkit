# Stage E8Y-FirstFrame: 0x2D92E4 resource resolve → A64 → 0x310BBC / DRAW
# NOT product success. No fake DRAW / no invented pixels / no C9D/CF5 poke.
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

$outDir = Join-Path $Root 'out\e8y_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$shotDir = Join-Path $Root 'evidence\screenshots'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir, $shotDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 25
$summaryPath = Join-Path $reportDir 'stage_e8y_firstframe_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }

py -3 (Join-Path $Root 'tools\e8y_2d92e4_decode.py') | Out-Host

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
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8Y_MODE',
    'JJFB_E8V_E88CC_TRACE','JJFB_E8V_CALL_2E993C','JJFB_E8V_INSN_LIMIT',
    'JJFB_E8X_INSN_LIMIT','JJFB_E8X_CALL_2F99D0','JJFB_E8X_DEEP','JJFB_E8Y_INSN_LIMIT','JJFB_E8Y_DEEP',
    'JJFB_E8W_REENTER_E88CC','JJFB_E8W_DEEP',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST','JJFB_FAST_A64_RESOURCE_ASSIST',
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

function Stop-E8YChildren {
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

function Analyze-Hits([string]$log) {
  $h = @{
    hit_2D92E4 = $false; hit_ret_nz = $false; hit_ret_z = $false
    hit_310BBC = $false; hit_DRAW = $false; hit_first_frame = $false
    hit_a64_assist = $false; hit_strlen_fp_zero = $false; hit_resource = $false
    hit_2F449C = $false; hit_helper_304 = $false; hit_file_io = $false
    class = ''; name = ''; fault_pc = ''; c44nz = $false
  }
  if (-not $log -or -not (Test-Path $log)) { return $h }
  $h.hit_2D92E4 = [bool](Select-String -Path $log -Pattern 'JJFB_E8Y_2D92E4_ENTRY\]' -Quiet -EA SilentlyContinue)
  $h.hit_ret_nz = [bool](Select-String -Path $log -Pattern 'class=RESOURCE_INIT_2D92E4_COMPLETED' -Quiet -EA SilentlyContinue)
  $h.hit_ret_z = [bool](Select-String -Path $log -Pattern 'class=RESOURCE_INIT_2D92E4_RETURNS_WITH_A64_ZERO' -Quiet -EA SilentlyContinue)
  $h.hit_310BBC = [bool](Select-String -Path $log -Pattern 'JJFB_E8Y_310BBC\]|JJFB_E8X_DRAW_PATH\] pc=0x310BBC' -Quiet -EA SilentlyContinue)
  $h.hit_DRAW = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\]|JJFB_E8U_DRAW\]' -Quiet -EA SilentlyContinue)
  $h.hit_first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_FIRST_REAL_FRAME\]' -Quiet -EA SilentlyContinue)
  $h.hit_a64_assist = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_A64_RESOURCE_ASSIST\] after' -Quiet -EA SilentlyContinue)
  $h.hit_strlen_fp_zero = [bool](Select-String -Path $log -Pattern 'class=2D92E4_NEEDS_STRLEN_FP' -Quiet -EA SilentlyContinue)
  $h.hit_resource = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_RESOURCE_LOAD\]|JJFB_E8Y_2D92E4_ENTRY\]' -Quiet -EA SilentlyContinue)
  $h.hit_2F449C = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_DRAW_PATH\] pc=0x2F449C' -Quiet -EA SilentlyContinue)
  $h.hit_helper_304 = [bool](Select-String -Path $log -Pattern 'JJFB_E8Y_HELPER\] pc=0x304BF0' -Quiet -EA SilentlyContinue)
  $h.hit_file_io = [bool](Select-String -Path $log -Pattern 'JJFB_FILEOPEN\]|VM_FILE_READ\]|VM_FILE_SEEK\]' -Quiet -EA SilentlyContinue)
  $h.c44nz = [bool](Select-String -Path $log -Pattern 'C44_after=0x1|E8R_C44_UNLOCKED' -Quiet -EA SilentlyContinue)
  $cl = Select-String -Path $log -Pattern 'JJFB_E8Y_CLASS\] class=([A-Z0-9_]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($cl -and $cl.Line -match 'class=([A-Z0-9_]+)') { $h.class = $Matches[1] }
  $nm = Select-String -Path $log -Pattern 'JJFB_E8Y_2D92E4_ENTRY\][^\r\n]*name="([^"]*)"' -EA SilentlyContinue | Select-Object -First 1
  if ($nm -and $nm.Line -match 'name="([^"]*)"') { $h.name = $Matches[1] }
  $fm = Select-String -Path $log -Pattern 'UC_MEM_.*UNMAPPED' -EA SilentlyContinue | Select-Object -Last 1
  if ($fm) { $h.fault_pc = $fm.Line.Substring(0, [Math]::Min(120, $fm.Line.Length)) }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.hit_first_frame) { return 'FIRST_REAL_FRAME_REACHED' }
  if ($h.hit_DRAW -and -not $h.hit_a64_assist) { return 'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' }
  if ($h.hit_DRAW -and $h.hit_a64_assist) { return 'A64_RESOURCE_ASSIST_REACHED_DRAW_API' }
  if ($h.hit_310BBC -and $h.hit_a64_assist) { return 'A64_RESOURCE_ASSIST_REACHED_310BBC' }
  if ($h.hit_310BBC) { return '2F449C_REACHED_310BBC_NEXT_GAP' }
  if ($h.hit_ret_nz) { return 'RESOURCE_INIT_2D92E4_COMPLETED_NEXT_GAP' }
  if ($h.hit_strlen_fp_zero -or ($h.fault_pc -and $h.hit_2D92E4)) {
    return 'RESOURCE_INIT_2D92E4_BLOCKED_BY_SVC'
  }
  if ($h.hit_ret_z -and $h.name) { return 'RESOURCE_INIT_2D92E4_RETURNS_WITH_A64_ZERO' }
  if ($h.hit_a64_assist -and $h.hit_2F449C -and -not $h.hit_310BBC) {
    return 'A64_RESOURCE_TABLE_LAYOUT_DERIVED_NEXT_GAP'
  }
  if ($h.hit_2D92E4 -and -not $h.hit_ret_nz -and -not $h.hit_ret_z -and
      ($h.hit_helper_304 -or $h.hit_file_io)) {
    return 'RESOURCE_INIT_2D92E4_BLOCKED_BY_MISSING_FILE'
  }
  if ($h.hit_2D92E4 -and $h.hit_ret_z) { return 'RESOURCE_INIT_2D92E4_RETURNS_WITH_A64_ZERO' }
  if ($h.hit_2D92E4) { return '2F449C_BLOCKED_BY_RESOURCE_TABLE' }
  if ($h.hit_a64_assist -and -not $h.hit_2F449C) {
    return 'A64_RESOURCE_TABLE_LAYOUT_DERIVED_NEXT_GAP'
  }
  if ($h.hit_2F449C) { return '2F449C_BLOCKED_BY_RESOURCE_TABLE' }
  return 'DISPLAY_FIRST_STILL_BLANK_AFTER_RESOURCE_INIT'
}

function Write-CaseDone([string]$Name, [string]$Verdict, [double]$Elapsed, [hashtable]$Hits) {
  $line = "== E8Y_CASE_DONE name=$Name verdict=$Verdict elapsed=$([Math]::Round($Elapsed,1))"
  Write-Host $line
  [Console]::Out.Flush()
  $obj = [ordered]@{
    case_name = $Name; verdict = $Verdict; elapsed_sec = [Math]::Round($Elapsed, 2)
    hit_2D92E4 = [bool]$Hits.hit_2D92E4; hit_ret_nz = [bool]$Hits.hit_ret_nz
    hit_ret_z = [bool]$Hits.hit_ret_z; hit_310BBC = [bool]$Hits.hit_310BBC
    hit_DRAW = [bool]$Hits.hit_DRAW; hit_first_frame = [bool]$Hits.hit_first_frame
    hit_a64_assist = [bool]$Hits.hit_a64_assist; hit_strlen_fp_zero = [bool]$Hits.hit_strlen_fp_zero
    class = $Hits.class; name = $Hits.name; fault_pc = $Hits.fault_pc
    timeout = ($Verdict -eq 'TIMEOUT'); c44nz = [bool]$Hits.c44nz
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content -Path $summaryPath -Encoding utf8
}

function Invoke-E8YCase([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8Y $Label =="
  [Console]::Out.Flush()
  $t0 = Get-Date
  Clear-E8Modes
  $env:JJFB_E8Y_MODE = '1'
  $env:JJFB_E8X_MODE = '1'
  $env:JJFB_E8W_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir "e8y_${Label}_frame.bmp")
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

  $caseLog = Join-Path $logDir "stage_e8y_${Label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e8y_${Label}_stderr.txt"
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
      Stop-E8YChildren
      Start-Sleep -Milliseconds 200
    } elseif ($p.ExitCode -ne 0) {
      $verdict = 'FATAL_ERROR'
    }
  } catch {
    $verdict = 'FATAL_ERROR'
    Write-Host "E8Y case exception: $_"
    Stop-E8YChildren
  }

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  $prefer = $null
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E8Y_|JJFB_E8X_|JJFB_E8U_IDLE_SUCCESS' -Quiet -EA SilentlyContinue)) {
    $prefer = $src
  } elseif ((Test-Path $vm) -and (Select-String -Path $vm -Pattern 'JJFB_E8Y_|JJFB_E8X_' -Quiet -EA SilentlyContinue)) {
    $prefer = $vm
  } elseif ((Test-Path $src) -and ((Get-Item $src).Length -gt 5000)) {
    $prefer = $src
  }
  if ($prefer) { Copy-Item -Force $prefer $caseLog }

  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $hits = Analyze-Hits $caseLog
  if ($verdict -eq 'FATAL_ERROR' -or $verdict -eq 'TIMEOUT' -or $verdict -eq 'OK') {
    $v2 = Case-Verdict $hits
    if ($v2 -ne 'DISPLAY_FIRST_STILL_BLANK_AFTER_RESOURCE_INIT' -or $hits.c44nz -or $hits.hit_2D92E4 -or $hits.hit_2F449C) {
      $verdict = $v2
    }
  }
  Write-CaseDone -Name $Label -Verdict $verdict -Elapsed $elapsed -Hits $hits
  $script:SkipBuildNext = $true
  $stopMore = ($hits.hit_first_frame -or $hits.hit_DRAW -or $hits.hit_310BBC)
  return @{ log = $caseLog; verdict = $verdict; hits = $hits; elapsed = $elapsed; stopMore = $stopMore }
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
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
  @{ L = 'A_resource_init_deep'; Extra = (New-FastExtra @{}) },
  @{ L = 'B_A64_writer_watch'; Extra = (New-FastExtra @{}) },
  @{ L = 'C_A64_struct_assist'; Extra = (New-FastExtra @{
      JJFB_FAST_A64_RESOURCE_ASSIST = '1'
    })
  }
)

Write-Host "E8Y-FirstFrame timeout=${CASE_TIMEOUT_SEC}s cases=$($matrix.Count) NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$results = @{}
foreach ($m in $matrix) {
  $results[$m.L] = Invoke-E8YCase -Label $m.L -ExtraEnv $m.Extra
  if ($results[$m.L].stopMore) {
    Write-Host "E8Y stop-early: draw progress in $($m.L)"
    break
  }
}

$best = 'DISPLAY_FIRST_STILL_BLANK_AFTER_RESOURCE_INIT'
$rank = @{
  'FIRST_REAL_FRAME_REACHED' = 100
  'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' = 95
  'A64_RESOURCE_ASSIST_REACHED_DRAW_API' = 93
  'A64_RESOURCE_ASSIST_REACHED_310BBC' = 90
  '2F449C_REACHED_310BBC_NEXT_GAP' = 88
  'RESOURCE_INIT_2D92E4_COMPLETED_NEXT_GAP' = 80
  'A64_RESOURCE_TABLE_LAYOUT_DERIVED_NEXT_GAP' = 70
  'RESOURCE_INIT_2D92E4_RETURNS_WITH_A64_ZERO' = 60
  'RESOURCE_INIT_2D92E4_BLOCKED_BY_MISSING_FILE' = 55
  'RESOURCE_INIT_2D92E4_BLOCKED_BY_SVC' = 50
  '2F449C_BLOCKED_BY_RESOURCE_TABLE' = 40
  '2F449C_BLOCKED_BY_NEW_PLATFORM_API' = 35
  'DISPLAY_FIRST_STILL_BLANK_AFTER_RESOURCE_INIT' = 10
}
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $v = $results[$m.L].verdict
  $rv = if ($rank.ContainsKey($v)) { $rank[$v] } else { 5 }
  $rb = if ($rank.ContainsKey($best)) { $rank[$best] } else { 0 }
  if ($rv -gt $rb) { $best = $v }
}

$canon = Join-Path $shotDir 'e8y_first_real_frame.bmp'
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $cand = Join-Path $shotDir "e8y_$($m.L)_frame.bmp"
  if ((Test-Path $cand) -and ((Get-Item $cand).Length -gt 1000)) {
    Copy-Item -Force $cand $canon
    break
  }
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8Y-FirstFrame Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** Target: ``0x2D92E4`` BMP resolve → ``A64/A68/A6C`` → ``0x310BBC`` / ``[JJFB_DRAW]``.')
$md.Add('')
$md.Add('## Static')
$md.Add('')
$md.Add('- ``0x2D92E4``: resource-name resolver; returns 0x14-byte handle (r4); no SVC in body')
$md.Add('- First name: ``wy_jiao1!11!11.bmp`` @ ``0x313514``')
$md.Add('- Caller ``0x2F449C`` stores return into ``R9+A64`` then A68/A6C/A58...')
$md.Add('- Mode10 uses ``A58/A5C/A60`` then ``BL 0x310BBC``')
$md.Add('- Needs ``*(R9+0x1450)`` strlen FP for r0==0 path')
$md.Add('')
$md.Add('## Cases')
$md.Add('')
$md.Add('| Case | Verdict | Elapsed | 2D92E4 | retNZ | 310BBC | DRAW | name | class |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- |')
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $r = $results[$m.L]; $h = $r.hits
  $nm = if ($h.name) { $h.name } else { '-' }
  $cls = if ($h.class) { $h.class } else { '-' }
  $md.Add("| $($m.L) | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($h.hit_2D92E4) | $($h.hit_ret_nz) | $($h.hit_310BBC) | $($h.hit_DRAW) | $nm | $cls |")
}
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``reports/stage_e8y_firstframe_summary.jsonl``')
$md.Add('- ``logs/stage_e8y_*_stdout.txt``')
$md.Add('- ``logs/e8y_first_real_draw_stdout.txt``')
$md.Add('- ``out/e8y_tmp/e8y_decode_report.json``')
$md.Add('')

foreach ($name in @('C_A64_struct_assist','A_resource_init_deep','B_A64_writer_watch')) {
  if (-not $results.ContainsKey($name)) { continue }
  $h = $results[$name].hits
  if ($h.hit_DRAW -or $h.hit_310BBC -or $h.hit_2D92E4 -or $h.hit_a64_assist) {
    Copy-Item -Force $results[$name].log (Join-Path $logDir 'e8y_first_real_draw_stdout.txt')
    break
  }
}

$reportPath = Join-Path $reportDir 'stage_e8y_firstframe_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8Y-FirstFrame complete (NOT product success)."
[Console]::Out.Flush()
