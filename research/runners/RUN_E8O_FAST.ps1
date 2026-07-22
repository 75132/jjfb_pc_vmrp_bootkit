# Stage E8O-Fast: controlled FAST_ASSIST matrix — NOT product success
param(
  [int]$Seconds = 160,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [string]$SvcMode = 'return0'  # observe|return0|preserve — default return0 to expose next gap
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

$outDir = Join-Path $Root 'out\e8o_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  $rob = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpSpec = 'p:0x300158,p:0x3002BA,p:0x3002C0,p:0x300714,p:0x30103C,p:0x3020C8,p:0x302340,e:0x2DFC3C,e:0x2DFCAC,e:0x30D300,q:0x2DC80C'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3','JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8ORun([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8O-Fast $Label =="
  Clear-E8Modes
  $env:JJFB_E8O_MODE = '1'
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
  foreach ($k in $ExtraEnv.Keys) { Set-Item -Path "Env:$k" -Value $ExtraEnv[$k] }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir "stage_e8o_${Label}_stdout.txt"
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

$matrix = @(
  @{ L = 's20_r18';          State = '20'; R1 = '18'; Seq = 'case156'; Need10165 = $false },
  @{ L = 's20_r20';          State = '20'; R1 = '20'; Seq = 'case156'; Need10165 = $false },
  @{ L = 's38_r18';          State = '38'; R1 = '18'; Seq = 'case156'; Need10165 = $false },
  @{ L = 's38_r20';          State = '38'; R1 = '20'; Seq = 'case156'; Need10165 = $false },
  @{ L = '10165_s38_r18';    State = '38'; R1 = '18'; Seq = '10165_case156'; Need10165 = $true },
  @{ L = '10165_s38_r20';    State = '38'; R1 = '20'; Seq = '10165_case156'; Need10165 = $true },
  @{ L = '310_s38_r18';      State = '38'; R1 = '18'; Seq = 'case310_case156'; Need10165 = $false },
  @{ L = '10165_310_s38_r20'; State = '38'; R1 = '20'; Seq = '10165_case310_case156'; Need10165 = $true }
)

$logs = @{}
foreach ($m in $matrix) {
  $extra = @{
    JJFB_FAST_STATE = $m.State
    JJFB_FAST_CASE156_R1 = $m.R1
    JJFB_FAST_SEQUENCE = $m.Seq
    JJFB_FAST_SVC_AB = $SvcMode
  }
  if ($m.Need10165) {
    $extra['JJFB_E8E_MODE'] = '1'
    $extra['JJFB_E8E_EVENT_PROBE'] = '1'
    $extra['JJFB_E8E_FE8_WATCH'] = '1'
    $extra['JJFB_E8E_CANDIDATE'] = 'R0_EVENTCODE_2'
    $extra['JJFB_E8E_DRAIN_ORDER'] = 'B'
  }
  $logs[$m.L] = Invoke-E8ORun -Label $m.L -ExtraEnv $extra
  $SkipBuild = $true
}

# One observe-only capture of SVC dump on the hottest path.
$obsLog = Invoke-E8ORun -Label 's38_r18_svc_observe' -ExtraEnv @{
  JJFB_FAST_STATE = '38'
  JJFB_FAST_CASE156_R1 = '18'
  JJFB_FAST_SEQUENCE = 'case156'
  JJFB_FAST_SVC_AB = 'observe'
}
$logs['s38_r18_svc_observe'] = $obsLog

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
  # Use PARENT_HIT only — WRITER_NEVER lines also contain tag=p30103C etc.
  return [pscustomobject]@{
    hit158 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p300158'
    hit714 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p300714'
    hit30103 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p30103C'
    hit3020 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p3020C8'
    c44 = Hit $log 'JJFB_E8C_FLAG_TRANSITION\][^\r\n]*off=0xC(44|9D|F5)'
    svc = Hit $log 'JJFB_FAST_SVC_AB\]'
    svcCont = Hit $log 'JJFB_FAST_SVC_AB_CONTINUE\]'
    draw = Hit $log '\[JJFB_DRAW\]'
    unmap = Hit $log 'UC_MEM_READ_UNMAPPED|UC_MEM_WRITE_UNMAPPED'
    fireDone = Grab $log 'JJFB_FAST_FIRE_DONE\]'
    snap = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_sibling'
    svcLine = Grab $log 'JJFB_FAST_SVC_AB\]'
    clearPc = Grab $log 'JJFB_E8I_STATE_WRITE\].*new=0x0 writer_pc='
    platNew = Count $log 'JJFB_PLAT_CALL\]'
  }
}

$results = @{}
foreach ($k in $logs.Keys) { $results[$k] = Analyze-Log $logs[$k] }

$best = 'FAST_ASSISTED_PATH_STILL_BLOCKED'
foreach ($k in $results.Keys) {
  $r = $results[$k]
  if ($r.draw) { $best = 'FAST_ASSISTED_DRAW_REACHED'; break }
}
if ($best -ne 'FAST_ASSISTED_DRAW_REACHED') {
  $anyC44 = $false; $anySvcCont = $false; $anySvc = $false; $any301 = $false; $any714 = $false; $any302 = $false
  foreach ($k in $results.Keys) {
    $r = $results[$k]
    if ($r.c44) { $anyC44 = $true }
    if ($r.svcCont) { $anySvcCont = $true }
    if ($r.svc) { $anySvc = $true }
    if ($r.hit30103) { $any301 = $true }
    if ($r.hit3020) { $any302 = $true }
    if ($r.hit714) { $any714 = $true }
  }
  if ($anyC44) { $best = 'FAST_REACHED_C44_GATE_NEXT_GAP' }
  elseif ($anySvcCont) { $best = 'FAST_SVC_AB_RETURN_REACHED_NEXT_GAP' }
  elseif ($anySvc) { $best = 'FAST_REACHED_SVC_AB_NEXT_GAP' }
  elseif ($any302 -or $any301) { $best = 'FAST_REACHED_30103C_NEXT_GAP' }
  elseif ($any714) { $best = 'FAST_REACHED_300714_NEXT_GAP' }
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8O-Fast Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** FAST_ASSIST only — no MRP/EXT edits, no fake DRAW.')
$md.Add('')
$md.Add("Default SVC mode this run: ``$SvcMode``")
$md.Add('')
$md.Add('## Matrix')
$md.Add('')
$md.Add('| Run | 158 | 714 | 30103C | 3020C8 | C44* | SVC | SVCcont | DRAW |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- |')
$order = @('s20_r18','s20_r20','s38_r18','s38_r20','10165_s38_r18','10165_s38_r20','310_s38_r18','10165_310_s38_r20','s38_r18_svc_observe')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("| $name | $($r.hit158) | $($r.hit714) | $($r.hit30103) | $($r.hit3020) | $($r.c44) | $($r.svc) | $($r.svcCont) | $($r.draw) |")
}
$md.Add('')
$md.Add('## Evidence snippets')
$md.Add('')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("### $name")
  if ($r.fireDone) { $md.Add("- ``$($r.fireDone)``") }
  if ($r.svcLine) { $md.Add("- ``$($r.svcLine)``") }
  if ($r.snap) { $md.Add("- ``$($r.snap)``") }
  $md.Add('')
}
$md.Add('## Clean backfill hint')
$md.Add('')
$md.Add('- If 714/30103C reached only via FAST state poke → naturalize ``R9+0x8D0`` writer (``0x2DC80C`` / ``0x2DD068``).')
$md.Add('- If SVC_AB blocks → derive minimal real handler under guarded research.')
$md.Add('- If C44 gate reached → trace natural C44/C9D/CF5 writers.')
$md.Add('- If DRAW only under FAST → do not claim product; document assist path.')
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``logs/stage_e8o_*_stdout.txt``')
$md.Add('- ``RUN_E8O_FAST.ps1``')
$md.Add('')

$reportPath = Join-Path $reportDir 'stage_e8o_fast_verdict.md'
($md -join "`n") + "`n" | Set-Content -Path $reportPath -Encoding utf8
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8O-Fast complete (NOT product success)."
