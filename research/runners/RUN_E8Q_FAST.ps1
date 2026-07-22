# Stage E8Q-Fast: C44 unlock + R1=20 success arm — NOT product success
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

$outDir = Join-Path $Root 'out\e8q_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (Test-Path $ext) {
  python (Join-Path $Root 'tools\e8q_c44_unlock_success_arm.py') --ext $ext --out-dir $outDir
}

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

# Success arm + unlock + reset + dispatcher
$bpSpec = 'p:0x30213E,p:0x301848,p:0x301864,p:0x304558,p:0x2FC8C0,p:0x2FC8CE,p:0x2F4E82,p:0x3020C8,p:0x302340,p:0x302360,p:0x300714,p:0x30103C,e:0x2E4840,e:0x30DDE2,e:0x2DB9DC'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE','JJFB_E8Q_MODE',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3','JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22','JJFB_FAST_INSN_LIMIT'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8QRun([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8Q-Fast $Label =="
  Clear-E8Modes
  $env:JJFB_E8Q_MODE = '1'
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
  $env:JJFB_FAST_STATE = '38'
  $env:JJFB_FAST_CASE156_R1 = '20'
  $env:JJFB_FAST_SEQUENCE = 'case156'
  $env:JJFB_FAST_C6C22 = '1'
  $env:JJFB_FAST_DEC30 = '1'
  $env:JJFB_FAST_INSN_LIMIT = '2000000'
  foreach ($k in $ExtraEnv.Keys) { Set-Item -Path "Env:$k" -Value $ExtraEnv[$k] }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir "stage_e8q_${Label}_stdout.txt"
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
  @{ L = 'A_success'; Extra = @{} },
  @{ L = 'B_success_eec7c'; Extra = @{ JJFB_FAST_EEC7C = '1' } },
  @{ L = 'C_10165_success'; Extra = @{
      JJFB_FAST_SEQUENCE = '10165_case156'
      JJFB_E8E_MODE = '1'; JJFB_E8E_EVENT_PROBE = '1'; JJFB_E8E_FE8_WATCH = '1'
      JJFB_E8E_CANDIDATE = 'R0_EVENTCODE_2'; JJFB_E8E_DRAIN_ORDER = 'B'
    }
  },
  @{ L = 'D_310_success'; Extra = @{ JJFB_FAST_SEQUENCE = 'case310_case156' } },
  @{ L = 'E_10165_310_success'; Extra = @{
      JJFB_FAST_SEQUENCE = '10165_case310_case156'
      JJFB_E8E_MODE = '1'; JJFB_E8E_EVENT_PROBE = '1'; JJFB_E8E_FE8_WATCH = '1'
      JJFB_E8E_CANDIDATE = 'R0_EVENTCODE_2'; JJFB_E8E_DRAIN_ORDER = 'B'
    }
  },
  @{ L = 'F_success_5e6'; Extra = @{ JJFB_FAST_INSN_LIMIT = '5000000' } }
)

$logs = @{}
foreach ($m in $matrix) {
  $logs[$m.L] = Invoke-E8QRun -Label $m.L -ExtraEnv $m.Extra
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

function Analyze-Log([string]$log) {
  # Keep patterns simple: complex alternation previously false-negatived under some PS hosts.
  return [pscustomobject]@{
    hit213e = Hit $log 'tag=p30213E'
    hit1848 = Hit $log 'tag=p301848'
    hit4558 = Hit $log 'tag=p304558'
    hitUnlockFn = Hit $log 'tag=p2FC8C0'
    hitUnlock = Hit $log 'tag=p2FC8CE|JJFB_E8Q_C44_UNLOCK'
    hitReset = Hit $log 'tag=p2F4E82'
    c44t = Hit $log 'JJFB_E8C_FLAG_TRANSITION\].*off=0xC44'
    c44nz = Hit $log 'JJFB_E8C_FLAG_TRANSITION\].*off=0xC44.*new=0x[1-9A-Fa-f]'
    draw = Hit $log '\[JJFB_DRAW\]'
    svc = Hit $log 'JJFB_FAST_SVC_AB\]'
    snap = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_sibling'
    arm1848 = Grab $log 'JJFB_E8Q_ARM\] tag=p301848'
    arm4558 = Grab $log 'JJFB_E8Q_ARM\] tag=p304558'
    fire = Grab $log 'JJFB_FAST_FIRE_DONE\]'
  }
}

$results = @{}
foreach ($k in $logs.Keys) { $results[$k] = Analyze-Log $logs[$k] }

$best = 'FAST_PATH_STILL_BLOCKED'
$anyDraw = $false; $anyC44nz = $false; $anyUnlock = $false; $any1848 = $false; $any4558 = $false; $anySvc = $false
foreach ($k in $results.Keys) {
  $r = $results[$k]
  if ($r.draw) { $anyDraw = $true }
  if ($r.c44nz -or ($r.c44t -and $r.hitUnlock)) { $anyC44nz = $true }
  if ($r.hitUnlock) { $anyUnlock = $true }
  if ($r.hit1848) { $any1848 = $true }
  if ($r.hit4558) { $any4558 = $true }
  if ($r.svc) { $anySvc = $true }
}
if ($anyDraw) { $best = 'FAST_REACHED_DRAW' }
elseif ($anyC44nz) { $best = 'FAST_REACHED_C44_TRANSITION_NEXT_GAP' }
elseif ($anyUnlock) { $best = 'SUCCESS_ARM_REACHES_C44_NONZERO_WRITER' }
elseif ($any1848 -and $any4558) { $best = 'R1_20_SUCCESS_REACHED_NEXT_GAP' }
elseif ($any1848) { $best = 'C44_NONZERO_REQUIRES_301848_SIDE_EFFECT' }
elseif ($any4558) { $best = 'C44_NONZERO_REQUIRES_304558_SIDE_EFFECT' }
elseif ($anySvc) { $best = 'FAST_REACHED_SVC_AB_NEXT_GAP' }

# Static co-claim always available
$staticUnlock = 'C44_NONZERO_WRITER_FOUND_NEXT_GAP'

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8Q-Fast Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add("**Static co-claim:** ``$staticUnlock`` - sole ``MOVS#1->C44`` site is ``0x2FC8CE`` in ``0x2FC8C0``.")
$md.Add('')
$md.Add('**NOT product success.** Do not treat ``0x2F4E82`` as unlock. Success arm does not call the unlock writer.')
$md.Add('')
$md.Add('## Static facts')
$md.Add('')
$md.Add('- C44 unlock writer: ``0x2FC8CE`` (``STRB #1``), fn ``0x2FC8C0``.')
$md.Add('- Callers of unlock: ``0x2DB9DC``, ``0x2E4840``..``0x2E4B06`` UI-init cluster, ``0x30DDE2``.')
$md.Add('- Historical ``0x2FC8B8`` is a *different* adjacent fn (writes state ``0x10D``), not the STRB#1 site.')
$md.Add('- ``0x301848``: small gate on ``R9+0x858``; may ``BL 0x301864``; does **not** call ``0x2FC8C0``.')
$md.Add('- ``0x304558``: plat bridge via function pointer (``BLX``); success arm uses plat code ``0x1E213``.')
$md.Add('- ``0x2F4E82`` = C44 reset (write 0); avoided when R1=20 success arm is taken.')
$md.Add('')
$md.Add('## Matrix')
$md.Add('')
$md.Add('| Run | 30213E | 301848 | 304558 | 2FC8C0 | 2FC8CE | 2F4E82 | C44T | DRAW | SVC |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |')
$order = @('A_success','B_success_eec7c','C_10165_success','D_310_success','E_10165_310_success','F_success_5e6')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("| $name | $($r.hit213e) | $($r.hit1848) | $($r.hit4558) | $($r.hitUnlockFn) | $($r.hitUnlock) | $($r.hitReset) | $($r.c44t) | $($r.draw) | $($r.svc) |")
}
$md.Add('')
$md.Add('## Evidence')
$md.Add('')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("### $name")
  if ($r.arm1848) { $md.Add("- ``$($r.arm1848)``") }
  if ($r.arm4558) { $md.Add("- ``$($r.arm4558)``") }
  if ($r.snap) { $md.Add("- ``$($r.snap)``") }
  if ($r.fire) { $md.Add("- ``$($r.fire)``") }
  $md.Add('')
}
$md.Add('## Clean backfill')
$md.Add('')
$md.Add('- Reach unlock naturally via callers of ``0x2FC8C0`` (UI-init ``0x2E4840`` cluster / ``0x30DDE2`` / ``0x2DB9DC``).')
$md.Add('- Naturalize ``C6C+0x22`` + ``DEC+0x30`` for success arm.')
$md.Add('- Naturalize state writer + ``0x10102`` case156 delivery.')
$md.Add('- Do not prioritize SVC; do not force C44.')
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``out/e8q_tmp/e8q_deps.md``')
$md.Add('- ``logs/stage_e8q_*_stdout.txt``')
$md.Add('- ``RUN_E8Q_FAST.ps1``')
$md.Add('')

$reportPath = Join-Path $reportDir 'stage_e8q_fast_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best (static=$staticUnlock) -> $reportPath"
Write-Host "E8Q-Fast complete (NOT product success)."
