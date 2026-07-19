# Stage E8M: 0x300158 parent prerequisite tracing + ordered sequence probes
param(
  [int]$Seconds = 180,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [switch]$SkipPathTrace,
  [switch]$SkipSeq
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8m_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

$rob = $null
@(
  'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext',
  'out\e8f_tmp\jjfb_ext\robotol.ext'
) | ForEach-Object {
  $p = Join-Path $Root $_
  if (-not $rob -and (Test-Path $p)) { $rob = $p }
}
if (-not $rob) { throw "robotol.ext not found" }

Write-Host "== E8M static parent prereq =="
python (Join-Path $Root 'tools\e8m_parent_prereq.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpSpec = (Get-Content (Join-Path $outDir 'e8m_bp_spec.txt') -Raw).Trim()
Write-Host "E8M BP count=$(($bpSpec -split ',').Count)"

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH',
    'JJFB_E8D_10165_PROBE','JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP',
    'JJFB_E8F_SIBLING_PROBE','JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS',
    'JJFB_E8L_10102_R1','JJFB_E8L_10102_R2','JJFB_E8L_10102_R3',
    'JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8MRun([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8M live $Label =="
  Clear-E8Modes
  $env:JJFB_E8M_MODE = '1'
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

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir "stage_e8m_${Label}_stdout.txt"
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  if ((Test-Path $dst) -and (Test-Path $vm) -and ((Get-Item $dst).Length -lt 10000)) {
    Copy-Item -Force $vm $dst
  }
  return $dst
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}

$pathLogs = @{}
if (-not $SkipPathTrace) {
  # Path traces for R1=0/18/20 with parent insn log (observe-only).
  foreach ($pair in @(
    @{ L = 'path_r1_0';  Regs = '156,0,0,0' },
    @{ L = 'path_r1_18'; Regs = '156,18,0,0' },
    @{ L = 'path_r1_20'; Regs = '156,20,0,0' }
  )) {
    $pathLogs[$pair.L] = Invoke-E8MRun -Label $pair.L -ExtraEnv @{
      JJFB_E8M_PARENT_TRACE = '1'
      JJFB_E8L_10102_REGS = $pair.Regs
      JJFB_E8K_10102_CASE = '156'
    }
    $SkipBuild = $true
  }
}

$seqLogs = @{}
if (-not $SkipSeq) {
  $seqs = @(
    @{ L = 'seq_A_10165_156_18'; Seq = '10165+156:18' },
    @{ L = 'seq_B_10165_156_20'; Seq = '10165+156:20' },
    @{ L = 'seq_C_310_156_18';   Seq = '310+156:18' },
    @{ L = 'seq_D_310_156_20';   Seq = '310+156:20' },
    @{ L = 'seq_E_10165_310_156_18'; Seq = '10165+310+156:18' },
    @{ L = 'seq_F_10165_310_156_20'; Seq = '10165+310+156:20' }
  )
  foreach ($s in $seqs) {
    $extra = @{
      JJFB_E8M_SEQ = $s.Seq
      JJFB_E8M_PARENT_TRACE = '1'
    }
    if ($s.Seq -match '10165') {
      $extra['JJFB_E8E_MODE'] = '1'
      $extra['JJFB_E8E_EVENT_PROBE'] = '1'
      $extra['JJFB_E8E_FE8_WATCH'] = '1'
      $extra['JJFB_E8E_CANDIDATE'] = 'R0_EVENTCODE_2'
      $extra['JJFB_E8E_DRAIN_ORDER'] = 'B'
    }
    $seqLogs[$s.L] = Invoke-E8MRun -Label $s.L -ExtraEnv $extra
    $SkipBuild = $true
  }
}

function Hit([string]$log, [string]$pat) {
  if (-not $log -or -not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -ErrorAction SilentlyContinue)
}
function Grab([string]$log, [string]$pat) {
  if (-not $log -or -not (Test-Path $log)) { return '' }
  $m = Select-String -Path $log -Pattern $pat | Select-Object -Last 1
  if ($m) { return $m.Line }
  return ''
}
function Count([string]$log, [string]$pat) {
  if (-not $log -or -not (Test-Path $log)) { return 0 }
  return @(Select-String -Path $log -Pattern $pat -ErrorAction SilentlyContinue).Count
}

function Analyze-Log([string]$log) {
  return [pscustomobject]@{
    fireDone = Hit $log 'JJFB_E8L_10102_FIRE_DONE\]'
    hitParent = Hit $log 'tag=p300158'
    hitState0 = Hit $log 'JJFB_E8M_PARENT_PATH\] pc=0x3004C8|note=state0_arm'
    hitGate = Hit $log 'JJFB_E8M_PARENT_PATH\] pc=0x3002BA|note=gate_cmp20'
    hitBl714 = Hit $log 'JJFB_E8M_PARENT_PATH\] pc=0x3002C0|tag=p3002C0|tag=p300714'
    hitDisp = Hit $log 'tag=p300714'
    fe8 = Hit $log 'JJFB_E8E_FE8_WRITE\]'
    state = Hit $log 'JJFB_E8I_STATE_WRITE\]'
    draw = Hit $log '\[JJFB_DRAW\]'
    snapAfter = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_sibling'
    snap10165 = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_seq_10165'
    pathState0 = Grab $log 'JJFB_E8M_PARENT_PATH\][^\r\n]*state0'
    pathGate = Grab $log 'JJFB_E8M_PARENT_PATH\][^\r\n]*gate'
    pathBl = Grab $log 'JJFB_E8M_PARENT_PATH\][^\r\n]*bl_300714'
    insnN = Count $log 'JJFB_E8M_INSN\]'
    summary = Grab $log 'JJFB_E8M_PARENT_TRACE_SUMMARY\]'
  }
}

$results = @{}
foreach ($k in $pathLogs.Keys) { $results[$k] = Analyze-Log $pathLogs[$k] }
foreach ($k in $seqLogs.Keys) { $results[$k] = Analyze-Log $seqLogs[$k] }

$any714 = $false
$anyState0 = $false
foreach ($k in $results.Keys) {
  if ($results[$k].hitBl714 -or $results[$k].hitDisp) { $any714 = $true }
  if ($results[$k].hitState0) { $anyState0 = $true }
}

$verdict = 'PARENT_BRANCH_CONDITION_UNMET'
if ($results.Values | Where-Object { $_.draw }) {
  $verdict = 'DRAW_REACHED'
} elseif ($any714) {
  $verdict = 'PARENT_REACHED_300714_NEXT_GAP'
} elseif ($anyState0) {
  $verdict = 'PARENT_BRANCH_CONDITION_UNMET'
} else {
  $verdict = 'MISSING_NATURAL_10102_DELIVERY'
}

# Prefer more specific if seq shows FE8 but still state0
$seqHitFe8 = $false
foreach ($k in @('seq_A_10165_156_18','seq_B_10165_156_20','seq_E_10165_310_156_18','seq_F_10165_310_156_20')) {
  if ($results.ContainsKey($k) -and $results[$k].fe8) { $seqHitFe8 = $true }
}
if ($verdict -eq 'PARENT_BRANCH_CONDITION_UNMET' -and $seqHitFe8 -and $anyState0) {
  # FE8 alone does not lift state0 arm — still branch condition on R9+0x8D0
  $verdict = 'PARENT_BRANCH_CONDITION_UNMET'
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8M Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$verdict``")
$md.Add('')
$md.Add('Observe-only diagnostics — **not** product success. Product still lacks natural `0x10102` delivery.')
$md.Add('')
$md.Add('## Static finding (why E8L missed `0x300714`)')
$md.Add('')
$md.Add('- `0x300158` switch subject is **`R9+0x8D0` state** (load via `R9+0x7D8+0x80+0x78`).')
$md.Add('- Incoming event code is saved in `r4`; it does **not** select the switch arm.')
$md.Add('- Sole `BL 0x300714` at `0x3002C0` after `CMP r0,#20; BLT skip` on the **state word**.')
$md.Add('- **state==0** → arm `0x3004C8` → plat call → POP; never `0x3002BA`/`0x300714`.')
$md.Add('- See `out/e8m_tmp/parent_prereq.md`.')
$md.Add('')
$md.Add('## Path traces (case156)')
$md.Add('')
$md.Add('| Probe | hitParent | state0_arm | gate_cmp20 | bl_300714 | insnN | DRAW |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- |')
foreach ($name in @('path_r1_0','path_r1_18','path_r1_20')) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("| $name | $($r.hitParent) | $($r.hitState0) | $($r.hitGate) | $($r.hitBl714) | $($r.insnN) | $($r.draw) |")
}
$md.Add('')
$md.Add('## Sequence probes A–F')
$md.Add('')
$md.Add('| Seq | FE8 | state0 | bl714 | state_write | DRAW | snap |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- |')
foreach ($name in @('seq_A_10165_156_18','seq_B_10165_156_20','seq_C_310_156_18','seq_D_310_156_20','seq_E_10165_310_156_18','seq_F_10165_310_156_20')) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $snap = if ($r.snapAfter) { $r.snapAfter } else { $r.snap10165 }
  $md.Add("| $name | $($r.fe8) | $($r.hitState0) | $($r.hitBl714) | $($r.state) | $($r.draw) | ``$snap`` |")
}
$md.Add('')
$md.Add('## Path diff R1=0 vs 18 vs 20')
$md.Add('')
$md.Add('With `R9_state=0`, all three take the **same state0 arm**; event code in `r4` has no effect on reaching `0x300714`.')
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- `out/e8m_tmp/parent_prereq.md`')
$md.Add('- `logs/stage_e8m_*_stdout.txt`')
$md.Add('- `tools/e8m_parent_prereq.py` / `RUN_E8M_PARENT_PREREQ.ps1`')
$md.Add('')

$reportPath = Join-Path $reportDir 'stage_e8m_verdict.md'
($md -join "`n") + "`n" | Set-Content -Path $reportPath -Encoding utf8
Write-Host "Verdict=$verdict -> $reportPath"
Write-Host "E8M complete."
