# Stage E8V-FirstFrame: deep-trace 0x2E88CC after E8U idle success toward FIRST_REAL_FRAME
# NOT product success. No fake DRAW / no framebuffer paint / no C9D/CF5 poke.
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

$outDir = Join-Path $Root 'out\e8v_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$shotDir = Join-Path $Root 'evidence\screenshots'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir, $shotDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 20
$summaryPath = Join-Path $reportDir 'stage_e8v_firstframe_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (-not (Test-Path $flagMap)) {
  py -3 (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp') | Out-Host
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
foreach ($need in @('2256','3229','3317')) {
  if ($offsets -notmatch "(^|,)$need(,|$)") {
    $offsets = if ($offsets) { "$offsets,$need" } else { $need }
  }
}

$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x2F4E82','p:0x3046A8',
  'p:0x2E4788','p:0x2E4840','p:0x2E2F50','p:0x2E2520','p:0x30DDE2','p:0x2DB9DC',
  'p:0x3066B8','p:0x3066C6','p:0x3066DA','p:0x306740','p:0x2E88CC',
  'p:0x2E88E6','p:0x2E8914','p:0x2E898C','p:0x2F2854','p:0x305BFC','p:0x2EA058',
  'p:0x2E993C','p:0x305E78','p:0x2F9970',
  'p:0x30AA46','p:0x3115BA','p:0x2E7DC2','p:0x2F7F2C','p:0x30213E','p:0x301848'
) -join ','

$insnDefault = '500000'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8V_E88CC_TRACE','JJFB_E8V_CALL_2E993C','JJFB_E8V_INSN_LIMIT',
    'JJFB_E8V_DEEP',
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

function Stop-E8VChildren {
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
    hit_2E4788 = $false; hit_2E4840 = $false; hit_2E2520 = $false
    hit_2FC8C0 = $false; hit_2FC8CE = $false
    hit_c9d_assist = $false; hit_idle_success = $false
    hit_e88cc = $false; hit_early_exit = $false; hit_requires_ui = $false
    hit_draw_cand = $false; hit_DRAW = $false; hit_first_frame = $false
    hit_downstream = $false; hit_2F2854 = $false; hit_305BFC = $false; hit_2EA058 = $false
    f6c_p4 = ''; f6c_p8 = ''; d14 = ''; class = ''; fault_pc = ''
    c44nz = $false
  }
  if (-not $log -or -not (Test-Path $log)) { return $h }
  $h.hit_2E4788 = [bool](Select-String -Path $log -Pattern 'tag=p2E4788\b|FAST_UI_INIT_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2E4840 = [bool](Select-String -Path $log -Pattern 'tag=p2E4840\b' -Quiet -EA SilentlyContinue)
  $h.hit_2E2520 = [bool](Select-String -Path $log -Pattern 'tag=p2E2520\b|FAST_UI_UPSTREAM_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2FC8C0 = [bool](Select-String -Path $log -Pattern 'tag=p2FC8C0\b|FAST_UNLOCK_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2FC8CE = [bool](Select-String -Path $log -Pattern 'tag=p2FC8CE\b|E8R_C44_UNLOCKED|C44_after=0x1' -Quiet -EA SilentlyContinue)
  $h.hit_c9d_assist = [bool](Select-String -Path $log -Pattern 'DISPLAY_FIRST_BRANCH_ASSIST\][^\r\n]*gate=C9D' -Quiet -EA SilentlyContinue)
  $h.hit_idle_success = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_IDLE_SUCCESS\]' -Quiet -EA SilentlyContinue)
  $h.hit_e88cc = [bool](Select-String -Path $log -Pattern 'JJFB_E8V_E88CC_CTX\]|tag=gDRAW\b|tag=vNULL\b|tag=vD14\b' -Quiet -EA SilentlyContinue)
  $h.hit_early_exit = [bool](Select-String -Path $log -Pattern 'JJFB_E8V_E88CC_EARLY_EXIT\]' -Quiet -EA SilentlyContinue)
  $h.hit_requires_ui = [bool](Select-String -Path $log -Pattern 'class=REQUIRES_UI_OBJECT' -Quiet -EA SilentlyContinue)
  $h.hit_draw_cand = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_DRAW_CANDIDATE\][^\r\n]*drawish_bl_target|tag=v2F2854\b|tag=v305BFC\b|tag=v2EA058\b' -Quiet -EA SilentlyContinue)
  # Platform DRAW only — guest BL candidates are hit_draw_cand, not hit_DRAW.
  $h.hit_DRAW = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\]|JJFB_E8U_DRAW\]' -Quiet -EA SilentlyContinue)
  $h.hit_first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_FIRST_REAL_FRAME\]' -Quiet -EA SilentlyContinue)
  $h.hit_downstream = [bool](Select-String -Path $log -Pattern 'JJFB_E8V_DOWNSTREAM_CALL\]|JJFB_E8V_DOWNSTREAM_DONE\]' -Quiet -EA SilentlyContinue)
  $h.hit_2F2854 = [bool](Select-String -Path $log -Pattern 'tag=v2F2854\b|pc=0x2F2854\b' -Quiet -EA SilentlyContinue)
  $h.hit_305BFC = [bool](Select-String -Path $log -Pattern 'tag=v305BFC\b|pc=0x305BFC\b' -Quiet -EA SilentlyContinue)
  $h.hit_2EA058 = [bool](Select-String -Path $log -Pattern 'tag=v2EA058\b|pc=0x2EA058\b' -Quiet -EA SilentlyContinue)
  $h.c44nz = [bool](Select-String -Path $log -Pattern 'C44_after=0x1|E8R_C44_UNLOCKED|off=0xC44.*to=0x1' -Quiet -EA SilentlyContinue)
  $ctx = Select-String -Path $log -Pattern 'JJFB_E8V_E88CC_CTX\]' -EA SilentlyContinue | Select-Object -First 1
  if ($ctx -and $ctx.Line -match 'F6C_4=0x([0-9A-Fa-f]+)') { $h.f6c_p4 = $Matches[1] }
  if ($ctx -and $ctx.Line -match 'F6C_8=0x([0-9A-Fa-f]+)') { $h.f6c_p8 = $Matches[1] }
  if ($ctx -and $ctx.Line -match 'D14_s16=(-?\d+)') { $h.d14 = $Matches[1] }
  $cl = Select-String -Path $log -Pattern 'JJFB_E8V_E88CC_CLASS\] class=([A-Z0-9_]+)' -EA SilentlyContinue | Select-Object -First 1
  if ($cl -and $cl.Line -match 'class=([A-Z0-9_]+)') { $h.class = $Matches[1] }
  $fm = Select-String -Path $log -Pattern 'UC_MEM_.*UNMAPPED|FAULT.*pc=0x([0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($fm) { $h.fault_pc = $fm.Line.Substring(0, [Math]::Min(80, $fm.Line.Length)) }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.hit_first_frame) { return 'FIRST_REAL_FRAME_REACHED' }
  if ($h.hit_DRAW) { return 'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' }
  if ($h.hit_draw_cand -or $h.hit_2F2854 -or $h.hit_305BFC -or $h.hit_2EA058) {
    return 'E88CC_DRAW_CANDIDATE_REACHED_NEXT_GAP'
  }
  if ($h.hit_requires_ui -or ($h.hit_early_exit -and $h.class -eq 'REQUIRES_UI_OBJECT')) {
    return 'E88CC_REQUIRES_UI_OBJECT'
  }
  if ($h.class -eq 'SKIP_BY_D14_GATE') { return 'E88CC_SKIPPED_BY_D14_GATE' }
  if ($h.hit_idle_success -and $h.hit_e88cc -and -not $h.hit_DRAW) {
    return 'E88CC_RETURNS_NO_DRAW_NEEDS_NEXT_TICK'
  }
  if ($h.hit_idle_success) { return 'E88CC_ENTERED_NO_SIDE_EFFECT' }
  if ($h.hit_c9d_assist) { return 'C9D_GATE_BYPASSED_NEXT_GAP' }
  if ($h.fault_pc) { return 'E88CC_FAULT_BEFORE_DRAW' }
  return 'E88CC_STILL_BLANK'
}

function Write-CaseDone([string]$Name, [string]$Verdict, [double]$Elapsed, [hashtable]$Hits) {
  $line = "== E8V_CASE_DONE name=$Name verdict=$Verdict elapsed=$([Math]::Round($Elapsed,1))"
  Write-Host $line
  [Console]::Out.Flush()
  $obj = [ordered]@{
    case_name = $Name
    verdict = $Verdict
    elapsed_sec = [Math]::Round($Elapsed, 2)
    hit_idle_success = [bool]$Hits.hit_idle_success
    hit_e88cc = [bool]$Hits.hit_e88cc
    hit_early_exit = [bool]$Hits.hit_early_exit
    hit_requires_ui = [bool]$Hits.hit_requires_ui
    hit_draw_cand = [bool]$Hits.hit_draw_cand
    hit_DRAW = [bool]$Hits.hit_DRAW
    hit_first_frame = [bool]$Hits.hit_first_frame
    hit_downstream = [bool]$Hits.hit_downstream
    hit_2E2520 = [bool]$Hits.hit_2E2520
    hit_2E4788 = [bool]$Hits.hit_2E4788
    class = $Hits.class
    f6c_p4 = $Hits.f6c_p4
    f6c_p8 = $Hits.f6c_p8
    d14 = $Hits.d14
    fault_pc = $Hits.fault_pc
    timeout = ($Verdict -eq 'TIMEOUT')
    c44nz = [bool]$Hits.c44nz
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content -Path $summaryPath -Encoding utf8
}

function Invoke-E8VCase([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8V $Label =="
  [Console]::Out.Flush()
  $t0 = Get-Date
  Clear-E8Modes
  $env:JJFB_E8V_MODE = '1'
  $env:JJFB_E8V_E88CC_TRACE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir "e8v_${Label}_frame.bmp")
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

  $caseLog = Join-Path $logDir "stage_e8v_${Label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e8v_${Label}_stderr.txt"
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
      Stop-E8VChildren
      Start-Sleep -Milliseconds 200
    } elseif ($p.ExitCode -ne 0) {
      $verdict = 'FATAL_ERROR'
    }
  } catch {
    $verdict = 'FATAL_ERROR'
    Write-Host "E8V case exception: $_"
    Stop-E8VChildren
  }

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  # Prefer product/vmrp logs when they contain E8V evidence (redirected caseLog may be build-only on Case A).
  $prefer = $null
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E8V_|JJFB_E8U_IDLE_SUCCESS' -Quiet -EA SilentlyContinue)) {
    $prefer = $src
  } elseif ((Test-Path $vm) -and (Select-String -Path $vm -Pattern 'JJFB_E8V_|JJFB_E8U_IDLE_SUCCESS' -Quiet -EA SilentlyContinue)) {
    $prefer = $vm
  } elseif ((Test-Path $src) -and ((Get-Item $src).Length -gt 5000)) {
    $prefer = $src
  }
  if ($prefer) { Copy-Item -Force $prefer $caseLog }

  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $hits = Analyze-Hits $caseLog
  if ($verdict -eq 'FATAL_ERROR' -or $verdict -eq 'TIMEOUT' -or $verdict -eq 'OK') {
    $v2 = Case-Verdict $hits
    if ($v2 -ne 'E88CC_STILL_BLANK' -or $hits.c44nz -or $hits.hit_idle_success) {
      $verdict = $v2
    }
  }
  Write-CaseDone -Name $Label -Verdict $verdict -Elapsed $elapsed -Hits $hits
  $script:SkipBuildNext = $true

  $stopMore = ($hits.hit_first_frame -or $hits.hit_DRAW)
  return @{ log = $caseLog; verdict = $verdict; hits = $hits; elapsed = $elapsed; stopMore = $stopMore }
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'bridge.c.obj' -ErrorAction SilentlyContinue |
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
  JJFB_E8V_INSN_LIMIT = '4000'
}

function New-FastExtra([hashtable]$Overlay) {
  $h = @{}
  foreach ($k in $fastBase.Keys) { $h[$k] = $fastBase[$k] }
  foreach ($k in $Overlay.Keys) { $h[$k] = $Overlay[$k] }
  return $h
}

# Case A: E8U success + deep-trace 0x2E88CC (~30 ticks); no CF5 assist
# Case B: A + UI BP watch (no r0=0 UI call)
# Case C: A + FAST call 0x2E993C after first idle success
$matrix = @(
  @{ L = 'A_e88cc_deep_trace'; Extra = (New-FastExtra @{}) },
  @{ L = 'B_ui_bp_watch'; Extra = (New-FastExtra @{
      JJFB_FAST_UI_UPSTREAM = '2E2520'
      JJFB_FAST_UI_STATE = '20'
      JJFB_FAST_UI_CA3 = '1'
    })
  },
  @{ L = 'C_downstream_2e993c'; Extra = (New-FastExtra @{
      JJFB_E8V_CALL_2E993C = '1'
    })
  }
)

Write-Host "E8V-FirstFrame timeout=${CASE_TIMEOUT_SEC}s cases=$($matrix.Count) NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

# Static note
$staticNote = @'
# E8V static notes (0x2E88CC)

- Idle success lands at 0x2E88CC (layout/draw scheduler).
- Early exits: D14 gate @ 0x2E88E6; null F6C+4/+8 @ 0x2E8914.
- Draw-ish BLs: 0x2F2854, 0x305BFC, 0x2EA058 (only if object path open).
- Case A product path often only hits 0x2F9970 / 0x305E78 then returns.
'@
[System.IO.File]::WriteAllText((Join-Path $outDir 'e88cc_static_notes.md'), $staticNote + "`n", [System.Text.UTF8Encoding]::new($false))

$results = @{}
foreach ($m in $matrix) {
  $results[$m.L] = Invoke-E8VCase -Label $m.L -ExtraEnv $m.Extra
  if ($results[$m.L].stopMore) {
    Write-Host "E8V stop-early: first real draw/frame in $($m.L)"
    break
  }
}

$best = 'E88CC_STILL_BLANK'
$rank = @{
  'FIRST_REAL_FRAME_REACHED' = 100
  'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' = 90
  'E88CC_DRAW_CANDIDATE_REACHED_NEXT_GAP' = 75
  'E88CC_REQUIRES_UI_OBJECT' = 60
  'E88CC_SKIPPED_BY_D14_GATE' = 55
  'E88CC_RETURNS_NO_DRAW_NEEDS_NEXT_TICK' = 50
  'E88CC_ENTERED_NO_SIDE_EFFECT' = 40
  'C9D_GATE_BYPASSED_NEXT_GAP' = 30
  'E88CC_FAULT_BEFORE_DRAW' = 20
  'E88CC_STILL_BLANK' = 10
}
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $v = $results[$m.L].verdict
  $rv = if ($rank.ContainsKey($v)) { $rank[$v] } else { 5 }
  $rb = if ($rank.ContainsKey($best)) { $rank[$best] } else { 0 }
  if ($rv -gt $rb) { $best = $v }
}

$canon = Join-Path $shotDir 'e8v_first_real_frame.bmp'
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $cand = Join-Path $shotDir "e8v_$($m.L)_frame.bmp"
  if ((Test-Path $cand) -and ((Get-Item $cand).Length -gt 1000)) {
    Copy-Item -Force $cand $canon
    break
  }
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8V-FirstFrame Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** E8V deep-traces ``0x2E88CC`` after E8U idle success — no C9D/CF5 poke, no fake DRAW, no host framebuffer paint.')
$md.Add('')
$md.Add('## Rules')
$md.Add('')
$md.Add('- Case A base = E8U success (C44 unlock + helper skip + C9D bypass)')
$md.Add('- Deep-trace ``0x2E88CC`` F6C/D14 gates and draw-candidate BLs')
$md.Add('- No ``r0=0`` UI-init; Case C may FAST-call ``0x2E993C`` (R9-only)')
$md.Add('')
$md.Add('## Cases')
$md.Add('')
$md.Add('| Case | Verdict | Elapsed | IdleOK | E88CC | Class | DRAW | Frame | F6C+4 | F6C+8 |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |')
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $r = $results[$m.L]
  $h = $r.hits
  $cls = if ($h.class) { $h.class } else { '-' }
  $p4 = if ($h.f6c_p4) { "0x$($h.f6c_p4)" } else { '-' }
  $p8 = if ($h.f6c_p8) { "0x$($h.f6c_p8)" } else { '-' }
  $md.Add("| $($m.L) | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($h.hit_idle_success) | $($h.hit_e88cc) | $cls | $($h.hit_DRAW) | $($h.hit_first_frame) | $p4 | $p8 |")
}
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``reports/stage_e8v_firstframe_summary.jsonl``')
$md.Add('- ``logs/stage_e8v_*_stdout.txt``')
$md.Add('- ``logs/e8v_first_real_draw_stdout.txt``')
$md.Add('- ``out/e8v_tmp/e88cc_static_notes.md``')
$md.Add('- ``screenshots/e8v_first_real_frame.bmp`` (if frame appeared)')
$md.Add('')

foreach ($name in @('A_e88cc_deep_trace','B_ui_bp_watch','C_downstream_2e993c')) {
  if (-not $results.ContainsKey($name)) { continue }
  $h = $results[$name].hits
  if ($h.hit_DRAW -or $h.hit_first_frame -or $h.hit_e88cc -or $h.hit_idle_success) {
    Copy-Item -Force $results[$name].log (Join-Path $logDir 'e8v_first_real_draw_stdout.txt')
    break
  }
}

$reportPath = Join-Path $reportDir 'stage_e8v_firstframe_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8V-FirstFrame complete (NOT product success)."
[Console]::Out.Flush()
