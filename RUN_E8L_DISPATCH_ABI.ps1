# Stage E8L: 0x10102 / 0x30D300 real dispatch ABI + case 156/310 payload probes
param(
  [int]$Seconds = 200,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [switch]$SkipProduct,
  [switch]$OnlyProbe
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8l_tmp'
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

Write-Host "== E8L static dispatch ABI =="
python (Join-Path $Root 'tools\e8l_10102_dispatch_abi.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpSpec = (Get-Content (Join-Path $outDir 'e8l_bp_spec.txt') -Raw).Trim()
Write-Host "E8L BP count=$(($bpSpec -split ',').Count) spec=$bpSpec"

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8LRun([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8L live $Label =="
  Clear-E8Modes
  $env:JJFB_E8L_MODE = '1'
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
  $dst = Join-Path $logDir "stage_e8l_${Label}_stdout.txt"
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  if ((Test-Path $vm) -and ((Get-Item $dst).Length -lt 10000)) {
    Copy-Item -Force $vm $dst
  }
  return $dst
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}

$productLog = $null
if (-not $SkipProduct -and -not $OnlyProbe) {
  $productLog = Invoke-E8LRun -Label 'product' -ExtraEnv @{}
  $SkipBuild = $true
}

# Structured observe-only probes (NOT product success).
$probes = @(
  @{ Label = 'case156_A_zero'; Regs = '156,0,0,0'; Case = '156' },
  @{ Label = 'case156_B_r1_18'; Regs = '156,18,0,0'; Case = '156' },
  @{ Label = 'case156_C_r1_20'; Regs = '156,20,0,0'; Case = '156' },
  @{ Label = 'case310_D_null'; Regs = '310,0,0,0'; Case = '310' }
)

$probeLogs = @{}
$firstProbe = $true
foreach ($p in $probes) {
  # Build only on the first live invoke of this script run.
  $useSkip = $SkipBuild -or (-not $firstProbe)
  $savedSkip = $SkipBuild
  $SkipBuild = $useSkip
  $parts = $p.Regs -split ','
  $probeLogs[$p.Label] = Invoke-E8LRun -Label $p.Label -ExtraEnv @{
    JJFB_E8K_10102_CASE = $p.Case
    JJFB_E8L_10102_REGS = $p.Regs
    JJFB_E8L_10102_R1 = $parts[1]
    JJFB_E8L_10102_R2 = $parts[2]
    JJFB_E8L_10102_R3 = $parts[3]
  }
  $firstProbe = $false
  $SkipBuild = $true
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
  # Note: avoid property name 'parent' — PSCustomObject.Parent is reserved/shadowed.
  return [pscustomobject]@{
    fire = Hit $log 'JJFB_E8L_10102_FIRE\]'
    fireDone = Hit $log 'JJFB_E8L_10102_FIRE_DONE\]'
    fireLine = Grab $log 'JJFB_E8L_10102_FIRE\]'
    doneLine = Grab $log 'JJFB_E8L_10102_FIRE_DONE\]'
    sw = Hit $log 'tag=e30D300|pc=0x30D300'
    arm156 = Hit $log 'tag=e30DDF4|pc=0x30DDF4'
    arm310 = Hit $log 'tag=e30D72E|tag=e30D730|pc=0x30D730'
    hot = Hit $log 'tag=e2DFC3C'
    hotNull = Hit $log 'tag=e2DFCAC|pc=0x2DFCAC'
    hitParent = Hit $log 'tag=p300158'
    hitDisp = Hit $log 'tag=p300714'
    gate = Hit $log 'tag=p30103C|tag=p3020C8'
    drain = Hit $log 'tag=q2DC80C'
    state = Hit $log 'JJFB_E8I_STATE_WRITE\]'
    draw = Hit $log '\[JJFB_DRAW\]'
    unmap = Hit $log 'UC_MEM_READ_UNMAPPED|UC_MEM_WRITE_UNMAPPED'
    snapAfter = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_sibling'
    swN = Count $log 'tag=e30D300'
    parentN = Count $log 'tag=p300158'
    dispN = Count $log 'tag=p300714'
    hotN = Count $log 'tag=e2DFC3C'
  }
}

$results = @{}
if ($productLog) { $results['product'] = Analyze-Log $productLog }
foreach ($k in $probeLogs.Keys) {
  $results[$k] = Analyze-Log $probeLogs[$k]
}

$A = $results['case156_A_zero']
$B = $results['case156_B_r1_18']
$C = $results['case156_C_r1_20']
$D = $results['case310_D_null']

$verdict = 'EVENT_SWITCH_ABI_STILL_UNKNOWN'
if (($B -and $B.draw) -or ($C -and $C.draw) -or ($A -and $A.draw)) {
  $verdict = 'DRAW_REACHED'
} elseif (($B -and $B.hitParent) -or ($C -and $C.hitParent) -or ($A -and $A.hitParent)) {
  # Parent entry without dispatcher/state/DRAW = next gap inside/after 0x300158.
  $verdict = 'CASE_156_REACHED_NEXT_GAP'
} elseif ($D -and $D.hot -and $D.hotNull -and -not $D.hitParent) {
  $verdict = 'CASE_310_REQUIRES_PAYLOAD'
} elseif (($A -and $A.unmap) -or ($B -and $B.unmap)) {
  $verdict = 'CASE_156_REQUIRES_PAYLOAD'
} elseif (($A -and $A.fireDone -and -not $A.hitParent) -and ($B -and $B.fireDone -and -not $B.hitParent)) {
  $verdict = 'CASE_156_REQUIRES_PAYLOAD'
} else {
  $verdict = 'MISSING_10102_APP_INIT_EVENT'
}

$md = @()
$md += '# Stage E8L Verdict'
$md += ''
$md += "**Verdict:** ``$verdict``"
$md += ''
$md += '## Scope'
$md += ''
$md += '- Derive R0-R3/stack ABI of ``0x30D300``'
$md += '- Structured observe-only case 156 / case 310 probes'
$md += '- No 10165 spray, no force state/idle, no SVC #0xAB, no fake DRAW'
$md += ''
$md += '## Static ABI (summary)'
$md += ''
$md += '- R0 = switch case index'
$md += '- R1 saved to r4; case arms ``MOV r0,r4; BL target`` → **callee R0 = delivery R1**'
$md += '- case 156 → ``0x300158``: R1 hyp = event code int (18/20 from parent census)'
$md += '- case 310 → ``0x2DFC3C``: R1 must be non-NULL pointer (CMP r0,#0 early exit)'
$md += '- See ``out/e8l_tmp/dispatch_abi.md``'
$md += ''
$md += '## Live probe matrix'
$md += ''
$md += '| Probe | R0,R1,R2,R3 | fire | 30D300 | parent | 300714 | hot | state | DRAW |'
$md += '| --- | --- | --- | --- | --- | --- | --- | --- | --- |'
foreach ($name in @('product','case156_A_zero','case156_B_r1_18','case156_C_r1_20','case310_D_null')) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $regs = switch ($name) {
    'case156_A_zero' { '156,0,0,0' }
    'case156_B_r1_18' { '156,18,0,0' }
    'case156_C_r1_20' { '156,20,0,0' }
    'case310_D_null' { '310,0,0,0' }
    default { '(none)' }
  }
  $md += ("| $name | $regs | $($r.fireDone) | $($r.sw) | $($r.hitParent) | $($r.hitDisp) | $($r.hot) | $($r.state) | $($r.draw) |")
}
$md += ''
$md += '## Evidence lines'
$md += ''
foreach ($name in @('case156_A_zero','case156_B_r1_18','case156_C_r1_20','case310_D_null')) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md += "### $name"
  $md += ''
  if ($r.fireLine) { $md += "- fire: ``$($r.fireLine)``" }
  if ($r.doneLine) { $md += "- done: ``$($r.doneLine)``" }
  if ($r.snapAfter) { $md += "- snap: ``$($r.snapAfter)``" }
  $md += "- counts: sw=$($r.swN) parent=$($r.parentN) disp=$($r.dispN) hot=$($r.hotN) unmap=$($r.unmap)"
  $md += ''
}
$md += '## Ranking after E8L'
$md += ''
$md += '1. ``MISSING_10102_APP_INIT_EVENT`` — host still never naturally delivers 0x10102'
$md += '2. case 156 / 310 payload shape (event code vs object pointer)'
$md += '3. B7D drain secondary'
$md += ''
$md += '## Forbidden (held)'
$md += ''
$md += '- no product force of R9+0x8D0 / C44/C9D/CF5'
$md += '- no blind SVC #0xAB'
$md += '- no random 10165/10102 spray'
$md += '- probes marked observe-only / not product success'
$md += ''
$md += '## Artifacts'
$md += ''
$md += '- ``out/e8l_tmp/dispatch_abi.md``'
$md += '- ``logs/stage_e8l_*_stdout.txt``'
$md += '- ``tools/e8l_10102_dispatch_abi.py`` / ``RUN_E8L_DISPATCH_ABI.ps1``'
$md += ''

$reportPath = Join-Path $reportDir 'stage_e8l_verdict.md'
($md -join "`n") + "`n" | Set-Content -Path $reportPath -Encoding utf8
Write-Host "Verdict=$verdict -> $reportPath"
Write-Host "E8L complete."
