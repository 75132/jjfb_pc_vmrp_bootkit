# Stage E8W-FirstFrame: F70/F74 acquire + re-enter 0x2E88CC toward FIRST_REAL_FRAME
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

$outDir = Join-Path $Root 'out\e8w_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$shotDir = Join-Path $Root 'screenshots'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir, $shotDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 25
$summaryPath = Join-Path $reportDir 'stage_e8w_firstframe_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }

# Static layout (always refresh)
py -3 (Join-Path $Root 'tools\e8w_f6c_object_xref.py') --out-dir $outDir | Out-Host
py -3 (Join-Path $Root 'tools\e8w_f6c_writers_fixed.py') | Out-Host

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (-not (Test-Path $flagMap)) {
  py -3 (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp') | Out-Host
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
foreach ($need in @('2256','3229','3317','3948','3952','3956')) {
  if ($offsets -notmatch "(^|,)$need(,|$)") {
    $offsets = if ($offsets) { "$offsets,$need" } else { $need }
  }
}

$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x3066B8','p:0x3066C6','p:0x306740','p:0x2E88CC',
  'p:0x2E8914','p:0x2E8980','p:0x2E89A8','p:0x2F2854','p:0x305BFC','p:0x2EA058',
  'p:0x2D9CFC','p:0x2FBD18','p:0x2E8920','p:0x30AA32','p:0x30AA34','p:0x30AA42',
  'p:0x2E2520','p:0x2E4788','p:0x2E4840'
) -join ','

$insnDefault = '500000'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8V_MODE','JJFB_E8W_MODE','JJFB_E8V_E88CC_TRACE','JJFB_E8V_CALL_2E993C',
    'JJFB_E8V_INSN_LIMIT','JJFB_E8W_REENTER_E88CC','JJFB_E8W_DEEP',
    'JJFB_FAST_F6C_OBJECT_ASSIST',
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

function Stop-E8WChildren {
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
    hit_idle_success = $false; hit_e88cc = $false; hit_writer = $false
    hit_assist = $false; hit_reenter = $false; hit_draw_cand = $false
    hit_DRAW = $false; hit_first_frame = $false; hit_2F2854 = $false
    hit_305BFC = $false; hit_2EA058 = $false; hit_2E2520 = $false
    class = ''; f70 = ''; f74 = ''; scroll = ''; fault_pc = ''; c44nz = $false
  }
  if (-not $log -or -not (Test-Path $log)) { return $h }
  $h.hit_idle_success = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_IDLE_SUCCESS\]' -Quiet -EA SilentlyContinue)
  $h.hit_e88cc = [bool](Select-String -Path $log -Pattern 'JJFB_E8W_F6C_WORDS\]|JJFB_E8V_E88CC_CTX\]' -Quiet -EA SilentlyContinue)
  $h.hit_writer = [bool](Select-String -Path $log -Pattern 'JJFB_E8W_F6C_WRITER\]' -Quiet -EA SilentlyContinue)
  $h.hit_assist = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_F6C_OBJECT_ASSIST\][^\r\n]*after' -Quiet -EA SilentlyContinue)
  $h.hit_reenter = [bool](Select-String -Path $log -Pattern 'JJFB_E8W_REENTER_DONE\]' -Quiet -EA SilentlyContinue)
  $h.hit_draw_cand = [bool](Select-String -Path $log -Pattern 'JJFB_FIRST_REAL_DRAW_CANDIDATE\][^\r\n]*drawish_bl_target|JJFB_E8W_DRAW_SITE\]' -Quiet -EA SilentlyContinue)
  $h.hit_DRAW = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\]|JJFB_E8U_DRAW\]' -Quiet -EA SilentlyContinue)
  $h.hit_first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_FIRST_REAL_FRAME\]' -Quiet -EA SilentlyContinue)
  $h.hit_2F2854 = [bool](Select-String -Path $log -Pattern 'site=0x2F2854|pc=0x2F2854\b|tag=v2F2854\b' -Quiet -EA SilentlyContinue)
  $h.hit_305BFC = [bool](Select-String -Path $log -Pattern 'site=0x305BFC|pc=0x305BFC\b' -Quiet -EA SilentlyContinue)
  $h.hit_2EA058 = [bool](Select-String -Path $log -Pattern 'site=0x2EA058|pc=0x2EA058\b' -Quiet -EA SilentlyContinue)
  $h.hit_2E2520 = [bool](Select-String -Path $log -Pattern 'tag=p2E2520\b|FAST_UI_UPSTREAM_CALL' -Quiet -EA SilentlyContinue)
  $h.c44nz = [bool](Select-String -Path $log -Pattern 'C44_after=0x1|E8R_C44_UNLOCKED' -Quiet -EA SilentlyContinue)
  $w = Select-String -Path $log -Pattern 'JJFB_E8W_F6C_WORDS\]' -EA SilentlyContinue | Select-Object -First 1
  if ($w -and $w.Line -match 'F70=0x([0-9A-Fa-f]+)') { $h.f70 = $Matches[1] }
  if ($w -and $w.Line -match 'F74=0x([0-9A-Fa-f]+)') { $h.f74 = $Matches[1] }
  if ($w -and $w.Line -match 'scroll200=0x([0-9A-Fa-f]+)') { $h.scroll = $Matches[1] }
  $cl = Select-String -Path $log -Pattern 'JJFB_E8V_E88CC_CLASS\] class=([A-Z0-9_]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($cl -and $cl.Line -match 'class=([A-Z0-9_]+)') { $h.class = $Matches[1] }
  $fm = Select-String -Path $log -Pattern 'UC_MEM_.*UNMAPPED' -EA SilentlyContinue | Select-Object -Last 1
  if ($fm) { $h.fault_pc = $fm.Line.Substring(0, [Math]::Min(100, $fm.Line.Length)) }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.hit_first_frame) { return 'FIRST_REAL_FRAME_REACHED' }
  if ($h.hit_DRAW) { return 'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' }
  if ($h.hit_2F2854) { return 'E88CC_REACHED_2F2854_NEXT_GAP' }
  if ($h.hit_305BFC) { return 'E88CC_REACHED_305BFC_NEXT_GAP' }
  if ($h.hit_2EA058) { return 'E88CC_REACHED_2EA058_NEXT_GAP' }
  if ($h.hit_assist -and $h.hit_draw_cand) { return 'F6C_OBJECT_ASSIST_REACHED_DRAW_CANDIDATE' }
  if ($h.hit_assist -and $h.fault_pc) { return 'F6C_OBJECT_ASSIST_FAULT_NEEDS_FIELD' }
  if ($h.hit_writer) { return 'F6C_REAL_WRITER_FOUND_NEXT_GAP' }
  if ($h.class -eq 'OBJECT_PRESENT_DRAW_PATH_OPEN' -or ($h.f70 -and $h.f70 -ne '0') -or ($h.f74 -and $h.f74 -ne '0')) {
    return 'F6C_OBJECT_LAYOUT_DERIVED_NEXT_GAP'
  }
  if ($h.hit_e88cc -and -not $h.hit_writer -and -not $h.hit_assist) { return 'F6C_WRITER_NEVER_REACHED' }
  if ($h.hit_idle_success) { return 'DISPLAY_FIRST_STILL_BLANK_AFTER_F6C' }
  return 'DISPLAY_FIRST_STILL_BLANK_AFTER_F6C'
}

function Write-CaseDone([string]$Name, [string]$Verdict, [double]$Elapsed, [hashtable]$Hits) {
  $line = "== E8W_CASE_DONE name=$Name verdict=$Verdict elapsed=$([Math]::Round($Elapsed,1))"
  Write-Host $line
  [Console]::Out.Flush()
  $obj = [ordered]@{
    case_name = $Name
    verdict = $Verdict
    elapsed_sec = [Math]::Round($Elapsed, 2)
    hit_idle_success = [bool]$Hits.hit_idle_success
    hit_e88cc = [bool]$Hits.hit_e88cc
    hit_writer = [bool]$Hits.hit_writer
    hit_assist = [bool]$Hits.hit_assist
    hit_reenter = [bool]$Hits.hit_reenter
    hit_draw_cand = [bool]$Hits.hit_draw_cand
    hit_DRAW = [bool]$Hits.hit_DRAW
    hit_first_frame = [bool]$Hits.hit_first_frame
    hit_2F2854 = [bool]$Hits.hit_2F2854
    hit_305BFC = [bool]$Hits.hit_305BFC
    hit_2EA058 = [bool]$Hits.hit_2EA058
    class = $Hits.class
    f70 = $Hits.f70
    f74 = $Hits.f74
    scroll = $Hits.scroll
    fault_pc = $Hits.fault_pc
    timeout = ($Verdict -eq 'TIMEOUT')
    c44nz = [bool]$Hits.c44nz
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content -Path $summaryPath -Encoding utf8
}

function Invoke-E8WCase([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8W $Label =="
  [Console]::Out.Flush()
  $t0 = Get-Date
  Clear-E8Modes
  $env:JJFB_E8W_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir "e8w_${Label}_frame.bmp")
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

  $caseLog = Join-Path $logDir "stage_e8w_${Label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e8w_${Label}_stderr.txt"
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
      Stop-E8WChildren
      Start-Sleep -Milliseconds 200
    } elseif ($p.ExitCode -ne 0) {
      $verdict = 'FATAL_ERROR'
    }
  } catch {
    $verdict = 'FATAL_ERROR'
    Write-Host "E8W case exception: $_"
    Stop-E8WChildren
  }

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  $prefer = $null
  if ((Test-Path $src) -and (Select-String -Path $src -Pattern 'JJFB_E8W_|JJFB_E8U_IDLE_SUCCESS' -Quiet -EA SilentlyContinue)) {
    $prefer = $src
  } elseif ((Test-Path $vm) -and (Select-String -Path $vm -Pattern 'JJFB_E8W_|JJFB_E8U_IDLE_SUCCESS' -Quiet -EA SilentlyContinue)) {
    $prefer = $vm
  } elseif ((Test-Path $src) -and ((Get-Item $src).Length -gt 5000)) {
    $prefer = $src
  }
  if ($prefer) { Copy-Item -Force $prefer $caseLog }

  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $hits = Analyze-Hits $caseLog
  if ($verdict -eq 'FATAL_ERROR' -or $verdict -eq 'TIMEOUT' -or $verdict -eq 'OK') {
    $v2 = Case-Verdict $hits
    if ($v2 -ne 'DISPLAY_FIRST_STILL_BLANK_AFTER_F6C' -or $hits.c44nz -or $hits.hit_idle_success) {
      $verdict = $v2
    }
  }
  Write-CaseDone -Name $Label -Verdict $verdict -Elapsed $elapsed -Hits $hits
  $script:SkipBuildNext = $true
  $stopMore = ($hits.hit_first_frame -or $hits.hit_DRAW -or $hits.hit_2F2854 -or $hits.hit_305BFC -or $hits.hit_2EA058)
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
}

function New-FastExtra([hashtable]$Overlay) {
  $h = @{}
  foreach ($k in $fastBase.Keys) { $h[$k] = $fastBase[$k] }
  foreach ($k in $Overlay.Keys) { $h[$k] = $Overlay[$k] }
  return $h
}

# A: writer watch only
# B: upstream UI (ABI-safe 2E2520) + watch
# C: structural F74 assist + reenter (after layout derived)
$matrix = @(
  @{ L = 'A_real_writer_watch'; Extra = (New-FastExtra @{}) },
  @{ L = 'B_upstream_ui_object'; Extra = (New-FastExtra @{
      JJFB_FAST_UI_UPSTREAM = '2E2520'
      JJFB_FAST_UI_STATE = '20'
      JJFB_FAST_UI_CA3 = '1'
    })
  },
  @{ L = 'C_minimal_f6c_assist'; Extra = (New-FastExtra @{
      JJFB_FAST_F6C_OBJECT_ASSIST = '1'
      JJFB_E8W_REENTER_E88CC = '1'
    })
  }
)

Write-Host "E8W-FirstFrame timeout=${CASE_TIMEOUT_SEC}s cases=$($matrix.Count) NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$results = @{}
foreach ($m in $matrix) {
  $results[$m.L] = Invoke-E8WCase -Label $m.L -ExtraEnv $m.Extra
  if ($results[$m.L].stopMore) {
    Write-Host "E8W stop-early: draw progress in $($m.L)"
    break
  }
}

$best = 'DISPLAY_FIRST_STILL_BLANK_AFTER_F6C'
$rank = @{
  'FIRST_REAL_FRAME_REACHED' = 100
  'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' = 95
  'E88CC_REACHED_2F2854_NEXT_GAP' = 90
  'E88CC_REACHED_305BFC_NEXT_GAP' = 88
  'E88CC_REACHED_2EA058_NEXT_GAP' = 88
  'F6C_OBJECT_ASSIST_REACHED_DRAW_CANDIDATE' = 85
  'F6C_REAL_WRITER_FOUND_NEXT_GAP' = 70
  'F6C_OBJECT_LAYOUT_DERIVED_NEXT_GAP' = 60
  'F6C_OBJECT_ASSIST_FAULT_NEEDS_FIELD' = 55
  'F6C_WRITER_NEVER_REACHED' = 40
  'DISPLAY_FIRST_STILL_BLANK_AFTER_F6C' = 10
}
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $v = $results[$m.L].verdict
  $rv = if ($rank.ContainsKey($v)) { $rank[$v] } else { 5 }
  $rb = if ($rank.ContainsKey($best)) { $rank[$best] } else { 0 }
  if ($rv -gt $rb) { $best = $v }
}

$canon = Join-Path $shotDir 'e8w_first_real_frame.bmp'
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $cand = Join-Path $shotDir "e8w_$($m.L)_frame.bmp"
  if ((Test-Path $cand) -and ((Get-Item $cand).Length -gt 1000)) {
    Copy-Item -Force $cand $canon
    break
  }
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8W-FirstFrame Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** E8W targets embedded ``R9+0xF6C`` struct words ``F70/F74`` (not a heap object pointer at ``[R9+F6C]``).')
$md.Add('')
$md.Add('## Layout (static)')
$md.Add('')
$md.Add('- Gate open iff ``*(R9+0xF74) != 0`` OR ``*(R9+0xF70) != 0``')
$md.Add('- Early exit ``0x2E8914`` = BEQ when F70==0 after F74 was already 0')
$md.Add('- First draw BL ``0x2F2854`` @ ``0x2E8980`` / ``0x2E89A8`` before F74[] loop')
$md.Add('- Later ``0x305BFC`` / ``0x2EA058`` index ``F74[i]``')
$md.Add('- Scroll sentinel ``R9+0x200 == 0x3E7`` forces epilogue @ ``0x2E8994``')
$md.Add('')
$md.Add('## Cases')
$md.Add('')
$md.Add('| Case | Verdict | Elapsed | IdleOK | Writer | Assist | Reenter | 2F2854 | DRAW | F70 | F74 |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |')
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $r = $results[$m.L]; $h = $r.hits
  $p70 = if ($h.f70) { "0x$($h.f70)" } else { '-' }
  $p74 = if ($h.f74) { "0x$($h.f74)" } else { '-' }
  $md.Add("| $($m.L) | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($h.hit_idle_success) | $($h.hit_writer) | $($h.hit_assist) | $($h.hit_reenter) | $($h.hit_2F2854) | $($h.hit_DRAW) | $p70 | $p74 |")
}
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``reports/stage_e8w_firstframe_summary.jsonl``')
$md.Add('- ``logs/stage_e8w_*_stdout.txt``')
$md.Add('- ``logs/e8w_first_real_draw_stdout.txt``')
$md.Add('- ``out/e8w_tmp/f6c_object_notes.md``')
$md.Add('- ``screenshots/e8w_first_real_frame.bmp`` (if frame appeared)')
$md.Add('')

foreach ($name in @('C_minimal_f6c_assist','A_real_writer_watch','B_upstream_ui_object')) {
  if (-not $results.ContainsKey($name)) { continue }
  $h = $results[$name].hits
  if ($h.hit_DRAW -or $h.hit_draw_cand -or $h.hit_assist -or $h.hit_e88cc) {
    Copy-Item -Force $results[$name].log (Join-Path $logDir 'e8w_first_real_draw_stdout.txt')
    break
  }
}

$reportPath = Join-Path $reportDir 'stage_e8w_firstframe_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8W-FirstFrame complete (NOT product success)."
[Console]::Out.Flush()
