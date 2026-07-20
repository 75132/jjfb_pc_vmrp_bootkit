# Stage E8X-FirstFrame: 0x2F2854 real draw path + real/structural F74
# NOT product success. No fake DRAW / no framebuffer paint / no C9D/CF5 poke.
param(
  [int]$Seconds = 90,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8x_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$shotDir = Join-Path $Root 'screenshots'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir, $shotDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 25
$summaryPath = Join-Path $reportDir 'stage_e8x_firstframe_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }

# Static decode (refresh)
py -3 (Join-Path $Root 'tools\e8x_2f2854_decode.py') | Out-Host

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (-not (Test-Path $flagMap)) {
  py -3 (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp') | Out-Host
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
foreach ($need in @('2256','3229','3317','3948','3952','3956','2072','2076','2096')) {
  if ($offsets -notmatch "(^|,)$need(,|$)") {
    $offsets = if ($offsets) { "$offsets,$need" } else { $need }
  }
}

$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x3066B8','p:0x3066C6','p:0x306740','p:0x2E88CC',
  'p:0x2E8914','p:0x2E8980','p:0x2E89A8','p:0x2F2854','p:0x2EA188','p:0x2F449C',
  'p:0x310BBC','p:0x305BFC','p:0x2EA058','p:0x2E8920','p:0x2F99D0','p:0x2F5B38',
  'p:0x2E2520','p:0x2E4788','p:0x2E4840'
) -join ','

$insnDefault = '500000'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8X_MODE','JJFB_E8V_E88CC_TRACE','JJFB_E8V_CALL_2E993C',
    'JJFB_E8V_INSN_LIMIT','JJFB_E8X_INSN_LIMIT','JJFB_E8X_CALL_2F99D0','JJFB_E8X_DEEP',
    'JJFB_E8W_REENTER_E88CC','JJFB_E8W_DEEP',
    'JJFB_FAST_F6C_OBJECT_ASSIST','JJFB_FAST_F74_DESCRIPTOR_ASSIST',
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

function Stop-E8XChildren {
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
    hit_idle_success = $false; hit_2F2854 = $false; hit_2EA188 = $false
    hit_2F449C = $false; hit_310BBC = $false; hit_305BFC = $false; hit_2EA058 = $false
    hit_DRAW = $false; hit_first_frame = $false; hit_zero_args = $false
    hit_desc = $false; hit_producer = $false; hit_producer_done = $false
    hit_resource = $false; producer_r0 = ''; class = ''; r830 = ''; fault_pc = ''; c44nz = $false
  }
  if (-not $log -or -not (Test-Path $log)) { return $h }
  $h.hit_idle_success = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_IDLE_SUCCESS\]' -Quiet -EA SilentlyContinue)
  $h.hit_2F2854 = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_2F2854_ENTRY\]|site=0x2F2854|pc=0x2F2854\b' -Quiet -EA SilentlyContinue)
  $h.hit_2EA188 = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_DRAW_PATH\] pc=0x2EA188' -Quiet -EA SilentlyContinue)
  $h.hit_2F449C = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_DRAW_PATH\] pc=0x2F449C|deeper_draw_path[^\r\n]*0x2F449C' -Quiet -EA SilentlyContinue)
  $h.hit_310BBC = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_DRAW_PATH\] pc=0x310BBC' -Quiet -EA SilentlyContinue)
  $h.hit_305BFC = [bool](Select-String -Path $log -Pattern 'site=0x305BFC|pc=0x305BFC\b' -Quiet -EA SilentlyContinue)
  $h.hit_2EA058 = [bool](Select-String -Path $log -Pattern 'site=0x2EA058|pc=0x2EA058\b' -Quiet -EA SilentlyContinue)
  $h.hit_DRAW = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\]|JJFB_E8U_DRAW\]' -Quiet -EA SilentlyContinue)
  $h.hit_first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_FIRST_REAL_FRAME\]' -Quiet -EA SilentlyContinue)
  $h.hit_zero_args = [bool](Select-String -Path $log -Pattern 'class=2F2854_ZERO_LAYOUT_ARGS' -Quiet -EA SilentlyContinue)
  $h.hit_desc = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_F74_DESCRIPTOR_ASSIST\]' -Quiet -EA SilentlyContinue)
  $h.hit_producer = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_2F99D0_CALL\]' -Quiet -EA SilentlyContinue)
  $h.hit_producer_done = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_2F99D0_DONE\]' -Quiet -EA SilentlyContinue)
  $h.hit_resource = [bool](Select-String -Path $log -Pattern 'JJFB_E8X_RESOURCE_LOAD\]' -Quiet -EA SilentlyContinue)
  $pr = Select-String -Path $log -Pattern 'JJFB_E8X_2F99D0_DONE\][^\r\n]*r0_after=0x([0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($pr -and $pr.Line -match 'r0_after=0x([0-9A-Fa-f]+)') { $h.producer_r0 = $Matches[1] }
  $h.c44nz = [bool](Select-String -Path $log -Pattern 'C44_after=0x1|E8R_C44_UNLOCKED' -Quiet -EA SilentlyContinue)
  $cl = Select-String -Path $log -Pattern 'JJFB_E8X_CLASS\] class=([A-Z0-9_]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($cl -and $cl.Line -match 'class=([A-Z0-9_]+)') { $h.class = $Matches[1] }
  $d = Select-String -Path $log -Pattern 'JJFB_E8X_DIMS\][^\r\n]*R9_830=0x([0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($d -and $d.Line -match 'R9_830=0x([0-9A-Fa-f]+)') { $h.r830 = $Matches[1] }
  $fm = Select-String -Path $log -Pattern 'UC_MEM_.*UNMAPPED' -EA SilentlyContinue | Select-Object -Last 1
  if ($fm) { $h.fault_pc = $fm.Line.Substring(0, [Math]::Min(120, $fm.Line.Length)) }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.hit_first_frame) { return 'FIRST_REAL_FRAME_REACHED' }
  if ($h.hit_DRAW) { return 'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' }
  if ($h.hit_310BBC) { return '2F2854_REACHED_NEW_PLATFORM_API_NEXT_GAP' }
  if ($h.hit_resource) { return '2F2854_REACHED_RESOURCE_LOAD_NEXT_GAP' }
  if ($h.hit_2F449C -and $h.hit_desc -and $h.class -eq '2F2854_NONZERO_ARGS') {
    return 'F74_DESCRIPTOR_ASSIST_REACHED_DRAW_CANDIDATE'
  }
  if ($h.hit_305BFC) { return 'E88CC_REACHED_305BFC_NEXT_GAP' }
  if ($h.hit_2EA058) { return 'E88CC_REACHED_2EA058_NEXT_GAP' }
  if ($h.hit_producer_done -and $h.producer_r0 -and $h.producer_r0 -ne '0') {
    return 'REAL_F74_PRODUCER_REACHED_NEXT_GAP'
  }
  if ($h.hit_producer_done -or ($h.hit_producer -and $h.fault_pc)) {
    return 'REAL_F74_PRODUCER_BRANCH_UNMET'
  }
  if ($h.hit_desc -and $h.class -eq '2F2854_NONZERO_ARGS') { return 'F74_DESCRIPTOR_LAYOUT_DERIVED_NEXT_GAP' }
  if ($h.hit_desc -and $h.hit_2F449C) { return 'F74_DESCRIPTOR_LAYOUT_DERIVED_NEXT_GAP' }
  if ($h.hit_2F449C -and ($h.hit_zero_args -or $h.class -eq '2F2854_ZERO_LAYOUT_ARGS')) {
    return '2F2854_REQUIRES_F74_DESCRIPTOR'
  }
  if ($h.hit_2F449C) { return 'F74_DESCRIPTOR_LAYOUT_DERIVED_NEXT_GAP' }
  if ($h.hit_zero_args -or $h.class -eq '2F2854_ZERO_LAYOUT_ARGS') { return '2F2854_RETURNS_NOOP_DUE_ZERO_ARGS' }
  if ($h.class -eq '2F2854_REQUIRES_R9_830') { return '2F2854_REQUIRES_F74_DESCRIPTOR' }
  if ($h.hit_2F2854 -and $h.hit_2EA188) { return '2F2854_RETURNS_NOOP_DUE_ZERO_ARGS' }
  if ($h.hit_2F2854) { return '2F2854_REQUIRES_F74_DESCRIPTOR' }
  if ($h.hit_idle_success) { return 'DISPLAY_FIRST_STILL_BLANK_AFTER_2F2854' }
  return 'DISPLAY_FIRST_STILL_BLANK_AFTER_2F2854'
}

function Write-CaseDone([string]$Name, [string]$Verdict, [double]$Elapsed, [hashtable]$Hits) {
  $line = "== E8X_CASE_DONE name=$Name verdict=$Verdict elapsed=$([Math]::Round($Elapsed,1))"
  Write-Host $line
  [Console]::Out.Flush()
  $obj = [ordered]@{
    case_name = $Name
    verdict = $Verdict
    elapsed_sec = [Math]::Round($Elapsed, 2)
    hit_idle_success = [bool]$Hits.hit_idle_success
    hit_2F2854 = [bool]$Hits.hit_2F2854
    hit_2EA188 = [bool]$Hits.hit_2EA188
    hit_2F449C = [bool]$Hits.hit_2F449C
    hit_310BBC = [bool]$Hits.hit_310BBC
    hit_305BFC = [bool]$Hits.hit_305BFC
    hit_2EA058 = [bool]$Hits.hit_2EA058
    hit_DRAW = [bool]$Hits.hit_DRAW
    hit_first_frame = [bool]$Hits.hit_first_frame
    hit_zero_args = [bool]$Hits.hit_zero_args
    hit_desc = [bool]$Hits.hit_desc
    hit_producer = [bool]$Hits.hit_producer
    hit_producer_done = [bool]$Hits.hit_producer_done
    class = $Hits.class
    r830 = $Hits.r830
    fault_pc = $Hits.fault_pc
    timeout = ($Verdict -eq 'TIMEOUT')
    c44nz = [bool]$Hits.c44nz
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content -Path $summaryPath -Encoding utf8
}

function Invoke-E8XCase([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8X $Label =="
  [Console]::Out.Flush()
  $t0 = Get-Date
  Clear-E8Modes
  $env:JJFB_E8X_MODE = '1'
  $env:JJFB_E8W_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir "e8x_${Label}_frame.bmp")
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

  $caseLog = Join-Path $logDir "stage_e8x_${Label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e8x_${Label}_stderr.txt"
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
      Stop-E8XChildren
      Start-Sleep -Milliseconds 200
    } elseif ($p.ExitCode -ne 0) {
      $verdict = 'FATAL_ERROR'
    }
  } catch {
    $verdict = 'FATAL_ERROR'
    Write-Host "E8X case exception: $_"
    Stop-E8XChildren
  }

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  $prefer = $null
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E8X_|JJFB_E8W_|JJFB_E8U_IDLE_SUCCESS' -Quiet -EA SilentlyContinue)) {
    $prefer = $src
  } elseif ((Test-Path $vm) -and (Select-String -Path $vm -Pattern 'JJFB_E8X_|JJFB_E8W_|JJFB_E8U_IDLE_SUCCESS' -Quiet -EA SilentlyContinue)) {
    $prefer = $vm
  } elseif ((Test-Path $src) -and ((Get-Item $src).Length -gt 5000)) {
    $prefer = $src
  }
  if ($prefer) { Copy-Item -Force $prefer $caseLog }

  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $hits = Analyze-Hits $caseLog
  if ($verdict -eq 'FATAL_ERROR' -or $verdict -eq 'TIMEOUT' -or $verdict -eq 'OK') {
    $v2 = Case-Verdict $hits
    if ($v2 -ne 'DISPLAY_FIRST_STILL_BLANK_AFTER_2F2854' -or $hits.c44nz -or $hits.hit_idle_success -or $hits.hit_2F2854) {
      $verdict = $v2
    }
  }
  Write-CaseDone -Name $Label -Verdict $verdict -Elapsed $elapsed -Hits $hits
  $script:SkipBuildNext = $true
  # Stop matrix only on real platform DRAW / screenshot / 310BBC / later F74 loop.
  # Hard stop only on real DRAW / screenshot / later platform; keep matrix for producer/descriptor.
  $stopMore = ($hits.hit_first_frame -or $hits.hit_DRAW -or $hits.hit_310BBC -or $hits.hit_305BFC -or $hits.hit_2EA058)
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
  JJFB_E8X_INSN_LIMIT = '3000'
}

function New-FastExtra([hashtable]$Overlay) {
  $h = @{}
  foreach ($k in $fastBase.Keys) { $h[$k] = $fastBase[$k] }
  foreach ($k in $Overlay.Keys) { $h[$k] = $Overlay[$k] }
  return $h
}

# C first: dims at gate-retry (highest FIRST_REAL_FRAME odds), then A deep, then B producer.
$matrix = @(
  @{ L = 'C_descriptor_assist'; Extra = (New-FastExtra @{
      JJFB_FAST_F6C_OBJECT_ASSIST = '1'
      JJFB_FAST_F74_DESCRIPTOR_ASSIST = '1'
      JJFB_E8W_REENTER_E88CC = '1'
    })
  },
  @{ L = 'A_2F2854_deep'; Extra = (New-FastExtra @{
      JJFB_FAST_F6C_OBJECT_ASSIST = '1'
      JJFB_E8W_REENTER_E88CC = '1'
    })
  },
  @{ L = 'B_real_F74_producer'; Extra = (New-FastExtra @{
      JJFB_E8X_CALL_2F99D0 = '1'
      JJFB_E8W_REENTER_E88CC = '1'
    })
  }
)

Write-Host "E8X-FirstFrame timeout=${CASE_TIMEOUT_SEC}s cases=$($matrix.Count) NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$results = @{}
foreach ($m in $matrix) {
  $results[$m.L] = Invoke-E8XCase -Label $m.L -ExtraEnv $m.Extra
  if ($results[$m.L].stopMore) {
    Write-Host "E8X stop-early: draw progress in $($m.L)"
    break
  }
}

$best = 'DISPLAY_FIRST_STILL_BLANK_AFTER_2F2854'
$rank = @{
  'FIRST_REAL_FRAME_REACHED' = 100
  'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' = 95
  'F74_DESCRIPTOR_ASSIST_REACHED_DRAW_CANDIDATE' = 92
  '2F2854_REACHED_NEW_PLATFORM_API_NEXT_GAP' = 90
  '2F2854_REACHED_RESOURCE_LOAD_NEXT_GAP' = 88
  'E88CC_REACHED_305BFC_NEXT_GAP' = 86
  'E88CC_REACHED_2EA058_NEXT_GAP' = 86
  'REAL_F74_PRODUCER_REACHED_NEXT_GAP' = 80
  'F74_DESCRIPTOR_LAYOUT_DERIVED_NEXT_GAP' = 75
  '2F2854_REQUIRES_F74_DESCRIPTOR' = 70
  '2F2854_RETURNS_NOOP_DUE_ZERO_ARGS' = 65
  'REAL_F74_PRODUCER_BRANCH_UNMET' = 55
  'DISPLAY_FIRST_STILL_BLANK_AFTER_2F2854' = 10
}
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $v = $results[$m.L].verdict
  $rv = if ($rank.ContainsKey($v)) { $rank[$v] } else { 5 }
  $rb = if ($rank.ContainsKey($best)) { $rank[$best] } else { 0 }
  if ($rv -gt $rb) { $best = $v }
}

$canon = Join-Path $shotDir 'e8x_first_real_frame.bmp'
$canonPng = Join-Path $shotDir 'e8x_first_real_frame.png'
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $cand = Join-Path $shotDir "e8x_$($m.L)_frame.bmp"
  if ((Test-Path $cand) -and ((Get-Item $cand).Length -gt 1000)) {
    Copy-Item -Force $cand $canon
    break
  }
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8X-FirstFrame Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** E8X targets ``0x2F2854 → 0x2EA188 → 0x2F449C`` and F74/dims (``R9+0x830``), not peripheral idle gates.')
$md.Add('')
$md.Add('## Static (accepted)')
$md.Add('')
$md.Add('- ``0x2F2854`` thin wrapper → always ``r0=0``, mode stack ``0``, ``BL 0x2EA188``')
$md.Add('- ``0x2EA188`` mode0 → ``BL 0x2F449C(r1=layout, r2=*(R9+0x830))``')
$md.Add('- Caller ``r2`` from ``BL 0x2F9970`` = ``*(R9+0x830)``; E8W Case C had r1=r2=0')
$md.Add('- Real F74 producer: ``0x2E8920 ← BL 0x2F99D0`` (skipped when F74 already nonzero)')
$md.Add('')
$md.Add('## Cases')
$md.Add('')
$md.Add('| Case | Verdict | Elapsed | 2F2854 | 2EA188 | 2F449C | 310BBC | DRAW | class | R9_830 |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |')
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $r = $results[$m.L]; $h = $r.hits
  $p830 = if ($h.r830) { "0x$($h.r830)" } else { '-' }
  $cls = if ($h.class) { $h.class } else { '-' }
  $md.Add("| $($m.L) | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($h.hit_2F2854) | $($h.hit_2EA188) | $($h.hit_2F449C) | $($h.hit_310BBC) | $($h.hit_DRAW) | $cls | $p830 |")
}
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``reports/stage_e8x_firstframe_summary.jsonl``')
$md.Add('- ``logs/stage_e8x_*_stdout.txt``')
$md.Add('- ``logs/e8x_first_real_draw_stdout.txt``')
$md.Add('- ``out/e8x_tmp/e8x_decode_report.json``')
$md.Add('- ``screenshots/e8x_first_real_frame.bmp`` / ``.png`` (if frame appeared)')
$md.Add('')

foreach ($name in @('C_descriptor_assist','A_2F2854_deep','B_real_F74_producer')) {
  if (-not $results.ContainsKey($name)) { continue }
  $h = $results[$name].hits
  if ($h.hit_DRAW -or $h.hit_2F449C -or $h.hit_2F2854 -or $h.hit_producer) {
    Copy-Item -Force $results[$name].log (Join-Path $logDir 'e8x_first_real_draw_stdout.txt')
    break
  }
}

$reportPath = Join-Path $reportDir 'stage_e8x_firstframe_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8X-FirstFrame complete (NOT product success)."
[Console]::Out.Flush()
