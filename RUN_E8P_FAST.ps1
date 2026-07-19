# Stage E8P-Fast: 0x3020C8 → C44 dependency cracking — NOT product success
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

$outDir = Join-Path $Root 'out\e8p_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

# Static deps (idempotent)
$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (Test-Path $ext) {
  python (Join-Path $Root 'tools\e8p_c44_dep_crack.py') --ext $ext --out-dir $outDir
}

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpSpec = 'p:0x300158,p:0x300714,p:0x30103C,p:0x3020C8,p:0x3021FA,p:0x302340,p:0x302360,p:0x302362,p:0x2F4E64,p:0x2F4E82,e:0x2DFC3C,e:0x2DFCAC,e:0x30D300,q:0x2DC80C'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3','JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_INSN_LIMIT'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8PRun([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8P-Fast $Label =="
  Clear-E8Modes
  $env:JJFB_E8P_MODE = '1'
  $env:JJFB_FAST_ASSIST = '1'
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
  $env:JJFB_FAST_SVC_AB = 'observe'
  $env:JJFB_FAST_INSN_LIMIT = '2000000'
  foreach ($k in $ExtraEnv.Keys) { Set-Item -Path "Env:$k" -Value $ExtraEnv[$k] }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir "stage_e8p_${Label}_stdout.txt"
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

# Phase 1: observe matrix A-E (no field poke)
$observe = @(
  @{ L = 'A_s38_r18';          State = '38'; R1 = '18'; Seq = 'case156'; Need10165 = $false },
  @{ L = 'B_s38_r20';          State = '38'; R1 = '20'; Seq = 'case156'; Need10165 = $false },
  @{ L = 'C_10165_s38_r20';    State = '38'; R1 = '20'; Seq = '10165_case156'; Need10165 = $true },
  @{ L = 'D_310_s38_r20';      State = '38'; R1 = '20'; Seq = 'case310_case156'; Need10165 = $false },
  @{ L = 'E_10165_310_s38_r20'; State = '38'; R1 = '20'; Seq = '10165_case310_case156'; Need10165 = $true }
)

# Phase 2: field assists on hottest path (state=38 + R1=20)
$assist = @(
  @{ L = 'F_eec7c1';     Eec = '1'; Dec = $null },
  @{ L = 'G_dec30_1';    Eec = $null; Dec = '1' },
  @{ L = 'H_eec7c1_dec30'; Eec = '1'; Dec = '1' },
  @{ L = 'I_eec7c1_dec30_nonzero'; Eec = '1'; Dec = '0x10000' }
)

$logs = @{}
foreach ($m in $observe) {
  $extra = @{
    JJFB_FAST_STATE = $m.State
    JJFB_FAST_CASE156_R1 = $m.R1
    JJFB_FAST_SEQUENCE = $m.Seq
  }
  if ($m.Need10165) {
    $extra['JJFB_E8E_MODE'] = '1'
    $extra['JJFB_E8E_EVENT_PROBE'] = '1'
    $extra['JJFB_E8E_FE8_WATCH'] = '1'
    $extra['JJFB_E8E_CANDIDATE'] = 'R0_EVENTCODE_2'
    $extra['JJFB_E8E_DRAIN_ORDER'] = 'B'
  }
  $logs[$m.L] = Invoke-E8PRun -Label $m.L -ExtraEnv $extra
  $SkipBuild = $true
}

foreach ($m in $assist) {
  $extra = @{
    JJFB_FAST_STATE = '38'
    JJFB_FAST_CASE156_R1 = '20'
    JJFB_FAST_SEQUENCE = 'case156'
  }
  if ($null -ne $m.Eec) { $extra['JJFB_FAST_EEC7C'] = $m.Eec }
  if ($null -ne $m.Dec) { $extra['JJFB_FAST_DEC30'] = $m.Dec }
  $logs[$m.L] = Invoke-E8PRun -Label $m.L -ExtraEnv $extra
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

function Analyze-Log([string]$log) {
  return [pscustomobject]@{
    hit3020 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p3020C8'
    hit21fa = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p3021FA'
    hit2340 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p302340'
    hit2360 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p302360'
    hit2362 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p302362'
    hit2f4e64 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p2[Ff]4[Ee]64|tag=p2F4E64'
    hitWriter = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p2[Ff]4[Ee]82|tag=p2F4E82'
    c44 = Hit $log 'JJFB_E8C_FLAG_TRANSITION\][^\r\n]*off=0xC(44|9D|F5)'
    svc = Hit $log 'JJFB_FAST_SVC_AB\]'
    draw = Hit $log '\[JJFB_DRAW\]'
    fireDone = Grab $log 'JJFB_FAST_FIRE_DONE\]'
    snap = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_sibling'
    obj = Grab $log 'JJFB_FAST_OBJ\]'
    eecAt3020 = Grab $log 'tag=p3020C8[^\r\n]*EEC7C='
  }
}

$results = @{}
foreach ($k in $logs.Keys) { $results[$k] = Analyze-Log $logs[$k] }

$best = 'OBJECT_INIT_NATURAL_SOURCE_STILL_UNKNOWN'
$anyWriter = $false; $anyC44 = $false; $any340 = $false; $any362 = $false; $anyDraw = $false; $anySvc = $false
foreach ($k in $results.Keys) {
  $r = $results[$k]
  if ($r.draw) { $anyDraw = $true }
  if ($r.c44) { $anyC44 = $true }
  if ($r.hitWriter) { $anyWriter = $true }
  if ($r.hit2340) { $any340 = $true }
  if ($r.hit2362 -or $r.hit2f4e64) { $any362 = $true }
  if ($r.svc) { $anySvc = $true }
}
if ($anyDraw) { $best = 'FAST_ASSISTED_DRAW_REACHED' }
elseif ($anyC44) { $best = 'FAST_REACHED_C44_TRANSITION_NEXT_GAP' }
elseif ($anyWriter) { $best = 'FAST_REACHED_C44_WRITER_NEXT_GAP' }
elseif ($any340 -or $any362) { $best = 'C44_DEP_REQUIRES_EEC' }  # refined below
elseif ($results.ContainsKey('F_eec7c1') -and $results['F_eec7c1'].hit2340) { $best = 'C44_DEP_REQUIRES_EEC' }
else {
  # Default from static: EEC+0x7C is the R1=20 gate
  $best = 'C44_DEP_REQUIRES_EEC'
}

# Refine multi-object if both fields needed for progress
if ($results.ContainsKey('H_eec7c1_dec30') -and $results.ContainsKey('F_eec7c1') -and $results.ContainsKey('G_dec30_1')) {
  $h = $results['H_eec7c1_dec30']; $f = $results['F_eec7c1']; $g = $results['G_dec30_1']
  if (($h.hitWriter -or $h.c44) -and -not ($f.hitWriter -or $g.hitWriter)) {
    $best = 'C44_DEP_REQUIRES_MULTI_OBJECTS'
  } elseif (($f.hit2340 -or $f.hitWriter) -and -not ($g.hit2340 -or $g.hitWriter)) {
    $best = 'C44_DEP_REQUIRES_EEC'
  } elseif (($g.hit2340 -or $g.hitWriter) -and -not ($f.hit2340 -or $f.hitWriter)) {
    $best = 'C44_DEP_REQUIRES_EEC'  # DEC is secondary; keep EEC as primary label if only DEC moves fail arm
  }
}
if ($anySvc -and $best -notmatch 'DRAW|C44_TRANSITION') { $best = 'FAST_REACHED_SVC_AB_NEXT_GAP' }

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8P-Fast Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** FAST_ASSIST / field poke only — no MRP/EXT edits, no fake DRAW.')
$md.Add('')
$md.Add('## Static finding (key)')
$md.Add('')
$md.Add('- ``R9+0xC6C / 0xEEC / 0x11D0`` are **R9-embedded struct bases**, not heap pointer slots.')
$md.Add('- R1=20 gate at ``0x302130``: ``*(R9+0xEEC+0x7C) == 1`` else ``B 0x302356`` (state clear + ``BL 0x2F4E64``).')
$md.Add('- Success arm ``0x30213E`` also needs ``*(R9+0xDEC+0x30) != 0``, then exits via ``0x302114`` (does **not** fall into ``0x302340``).')
$md.Add('- ``0x302340`` is reached by fall-through from R1=18 cleanup chain, not the R1=20 success arm.')
$md.Add('- ``0x2F4E82`` writes **C44=0** (reset) when a C6C byte test is zero — not an unlock-to-1.')
$md.Add('')
$md.Add('## Matrix')
$md.Add('')
$md.Add('| Run | 3020C8 | 3021FA | 302340 | 302360 | 302362/2F4E64 | 2F4E82 | C44* | DRAW |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- |')
$order = @('A_s38_r18','B_s38_r20','C_10165_s38_r20','D_310_s38_r20','E_10165_310_s38_r20','F_eec7c1','G_dec30_1','H_eec7c1_dec30','I_eec7c1_dec30_nonzero')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("| $name | $($r.hit3020) | $($r.hit21fa) | $($r.hit2340) | $($r.hit2360) | $($r.hit2362 -or $r.hit2f4e64) | $($r.hitWriter) | $($r.c44) | $($r.draw) |")
}
$md.Add('')
$md.Add('## Evidence')
$md.Add('')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("### $name")
  if ($r.obj) { $md.Add("- ``$($r.obj)``") }
  if ($r.eecAt3020) { $md.Add("- ``$($r.eecAt3020)``") }
  if ($r.fireDone) { $md.Add("- ``$($r.fireDone)``") }
  if ($r.snap) { $md.Add("- ``$($r.snap)``") }
  $md.Add('')
}
$md.Add('## Clean backfill')
$md.Add('')
$md.Add('- Naturalize writer of ``*(R9+0xEEC+0x7C)`` (candidates include ``0x2F0F64``; clearers ``0x2F4E92`` / ``0x2FC002`` / ``0x2FC804``).')
$md.Add('- Naturalize ``R9+0x8D0`` state writer + ``0x10102`` case156 delivery.')
$md.Add('- Do **not** invent heap objects for C6C/EEC/11D0 pointer slots — they are embedded.')
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``out/e8p_tmp/e8p_deps.md``')
$md.Add('- ``logs/stage_e8p_*_stdout.txt``')
$md.Add('- ``RUN_E8P_FAST.ps1``')
$md.Add('')

$reportPath = Join-Path $reportDir 'stage_e8p_fast_verdict.md'
($md -join "`n") + "`n" | Set-Content -Path $reportPath -Encoding utf8
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8P-Fast complete (NOT product success)."
