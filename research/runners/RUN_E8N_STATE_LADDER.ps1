# Stage E8N: R9+0x8D0 state ladder provenance + CF ladder probes
param(
  [int]$Seconds = 180,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [switch]$SkipProduct,
  [switch]$SkipCf,
  [switch]$SkipSeq
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

$outDir = Join-Path $Root 'out\e8n_tmp'
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

Write-Host "== E8N static state writers + ladder =="
python (Join-Path $Root 'tools\e8n_state_ladder.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpSpec = (Get-Content (Join-Path $outDir 'e8n_bp_spec.txt') -Raw).Trim()
# Cap BP count for speed — keep path + first 24 writers
$bpParts = $bpSpec -split ','
if ($bpParts.Count -gt 40) {
  $bpSpec = ($bpParts[0..39] -join ',')
}
Write-Host "E8N BP count=$(($bpSpec -split ',').Count)"

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH',
    'JJFB_E8D_10165_PROBE','JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP',
    'JJFB_E8F_SIBLING_PROBE','JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS',
    'JJFB_E8L_10102_R1','JJFB_E8L_10102_R2','JJFB_E8L_10102_R3',
    'JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8NRun([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8N live $Label =="
  Clear-E8Modes
  $env:JJFB_E8N_MODE = '1'
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
  $dst = Join-Path $logDir "stage_e8n_${Label}_stdout.txt"
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

$productLog = $null
if (-not $SkipProduct) {
  $productLog = Invoke-E8NRun -Label 'product' -ExtraEnv @{}
  $SkipBuild = $true
}

# CF ladder probes (COUNTERFACTUAL_ONLY) — not product success.
$cfLogs = @{}
if (-not $SkipCf) {
  foreach ($st in @(1, 2, 19, 20, 38)) {
    $cfLogs["cf_state_$st"] = Invoke-E8NRun -Label "cf_state_$st" -ExtraEnv @{
      JJFB_E8N_CF_STATE = "$st"
      JJFB_E8L_10102_REGS = '156,18,0,0'
      JJFB_E8K_10102_CASE = '156'
      JJFB_E8M_PARENT_TRACE = '1'
    }
    $SkipBuild = $true
  }
}

# One combined seq for completeness (E8M already covered A-F; keep E as spot-check).
$seqLog = $null
if (-not $SkipSeq) {
  $seqLog = Invoke-E8NRun -Label 'seq_E_10165_310_156_18' -ExtraEnv @{
    JJFB_E8M_SEQ = '10165+310+156:18'
    JJFB_E8E_MODE = '1'
    JJFB_E8E_EVENT_PROBE = '1'
    JJFB_E8E_FE8_WATCH = '1'
    JJFB_E8E_CANDIDATE = 'R0_EVENTCODE_2'
    JJFB_E8E_DRAIN_ORDER = 'B'
    JJFB_E8M_PARENT_TRACE = '1'
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
    stateWrite = Hit $log 'JJFB_E8N_STATE_WRITE\]|JJFB_E8I_STATE_WRITE\]'
    stateN = Count $log 'JJFB_E8N_STATE_WRITE\]'
    cf = Grab $log 'JJFB_E8N_CF_STATE\]'
    hit714 = Hit $log 'tag=p300714|note=bl_300714|pc=0x3002C0'
    hitGate = Hit $log 'note=gate_cmp20'
    hitState0 = Hit $log 'note=state0_arm'
    hit30103c = Hit $log 'tag=p30103[cC]|pc=0x30103C'
    draw = Hit $log '\[JJFB_DRAW\]'
    pathGate = Grab $log 'JJFB_E8M_PARENT_PATH\][^\r\n]*gate'
    pathBl = Grab $log 'JJFB_E8M_PARENT_PATH\][^\r\n]*bl_300714'
    pathS0 = Grab $log 'JJFB_E8M_PARENT_PATH\][^\r\n]*state0'
    snap = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_sibling'
    writerHit = Hit $log 'JJFB_E8J_UPSTREAM_HIT\]|JJFB_E8F_WRITER_HIT\]'
  }
}

$P = if ($productLog) { Analyze-Log $productLog } else { $null }
$cfResults = @{}
foreach ($k in $cfLogs.Keys) { $cfResults[$k] = Analyze-Log $cfLogs[$k] }
$S = if ($seqLog) { Analyze-Log $seqLog } else { $null }

$any714 = $false
$anyNaturalWrite = $false
if ($P -and $P.stateWrite) { $anyNaturalWrite = $true }
if ($S -and $S.stateWrite) { $anyNaturalWrite = $true }
foreach ($k in $cfResults.Keys) {
  if ($cfResults[$k].hit714) { $any714 = $true }
}

$verdict = 'R9_8D0_WRITER_NEVER_REACHED'
if (($P -and $P.draw) -or ($cfResults.Values | Where-Object { $_.draw })) {
  $verdict = 'DRAW_REACHED'
} elseif ($any714) {
  # CF ladder reached 714 — ladder derived; product writers still cold unless natural write
  if ($anyNaturalWrite) {
    $verdict = 'STATE_REACHED_300714_NEXT_GAP'
  } else {
    $verdict = 'STATE_LADDER_DERIVED_NEXT_GAP'
  }
} elseif ($anyNaturalWrite) {
  $verdict = 'R9_8D0_WRITER_BRANCH_UNMET'
} else {
  $verdict = 'R9_8D0_WRITER_NEVER_REACHED'
}

$static = Get-Content (Join-Path $outDir 'state_writers.md') -Raw -ErrorAction SilentlyContinue

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8N Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$verdict``")
$md.Add('')
$md.Add('Observe-only / COUNTERFACTUAL ladder — **not** product success.')
$md.Add('')
$md.Add('## Static summary')
$md.Add('')
$md.Add('- See ``out/e8n_tmp/state_writers.md``')
$md.Add("- direct writers + imm#38 scan + state0 arm + E6C overlap")
$md.Add('')
$md.Add('## Product / seq (natural writers)')
$md.Add('')
if ($P) {
  $md.Add("- product: state_writes=$($P.stateN) hit714=$($P.hit714) draw=$($P.draw)")
}
if ($S) {
  $md.Add("- seq_E: state_writes=$($S.stateN) hit714=$($S.hit714) FE8/path from E8M still cold on 8D0")
}
$md.Add('')
$md.Add('## CF ladder (COUNTERFACTUAL_ONLY)')
$md.Add('')
$md.Add('| CF state | state0_arm | gate | bl_300714 | 30103C | DRAW |')
$md.Add('| --- | --- | --- | --- | --- | --- |')
foreach ($st in @(1, 2, 19, 20, 38)) {
  $k = "cf_state_$st"
  if (-not $cfResults.ContainsKey($k)) { continue }
  $r = $cfResults[$k]
  $md.Add("| $st | $($r.hitState0) | $($r.hitGate) | $($r.hit714) | $($r.hit30103c) | $($r.draw) |")
}
$md.Add('')
$md.Add('## Ladder derived')
$md.Add('')
$md.Add('```')
$md.Add('0  → state0 arm 0x3004C8 (no 714) — needs external writer')
$md.Add('1/2/19 → (map from CF rows)')
$md.Add('>=20 unlisted → gate pass → BL 0x300714')
$md.Add('38 → inside 0x300714 → 0x30103C arm')
$md.Add('```')
$md.Add('')
$md.Add('## Forbidden (held)')
$md.Add('')
$md.Add('- CF state poke is COUNTERFACTUAL_ONLY — not product success')
$md.Add('- no force C44/C9D/CF5, no SVC #0xAB, no fake DRAW')
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``out/e8n_tmp/state_writers.md`` / ``state_writers.json``')
$md.Add('- ``logs/stage_e8n_*_stdout.txt``')
$md.Add('- ``tools/e8n_state_ladder.py`` / ``RUN_E8N_STATE_LADDER.ps1``')
$md.Add('')

$reportPath = Join-Path $reportDir 'stage_e8n_verdict.md'
($md -join "`n") + "`n" | Set-Content -Path $reportPath -Encoding utf8
Write-Host "Verdict=$verdict -> $reportPath"
Write-Host "E8N complete."
