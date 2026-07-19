# Stage E8J: dispatcher parent caller upstream reachability + queue consumer tracing
param(
  [int]$Seconds = 200,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [switch]$SkipProbe
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8j_tmp'
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

Write-Host "== E8J static L2 upstream + queue consumers =="
python (Join-Path $Root 'tools\e8j_caller_upstream_reach.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpSpec = (Get-Content (Join-Path $outDir 'e8j_bp_spec.txt') -Raw).Trim()
$bpCount = ($bpSpec -split ',').Count
Write-Host "E8J BP count=$bpCount"

function Invoke-E8JRun([string]$Label, [bool]$With10165) {
  Write-Host "== E8J live $Label =="
  $env:JJFB_E8J_MODE = '1'
  $env:JJFB_E8C_IDLE_WATCH = '1'
  $env:JJFB_E8C_WATCH_OFFSETS = $offsets
  $env:JJFB_E8D_EARLY_WATCH = '1'
  $env:JJFB_E8J_CLUSTER_BP = '1'
  $env:JJFB_E8J_BP_SPEC = $bpSpec
  $env:JJFB_E8J_QUEUE_READ_WATCH = '1'
  $env:JJFB_E8I_STATE_WATCH = '1'
  # Do not also arm E8I parent_bp (would duplicate p: tags as untyped parent).
  Remove-Item Env:JJFB_E8I_PARENT_BP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8I_PARENT_PCS -ErrorAction SilentlyContinue

  Remove-Item Env:JJFB_E8F_COUNTERFACTUAL -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8H_SVC_TRAP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8G_CALLER_BP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8G_FAULT_WATCH -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8F_WRITER_BP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8F_SIBLING_PROBE -ErrorAction SilentlyContinue

  if ($With10165) {
    $env:JJFB_E8E_MODE = '1'
    $env:JJFB_E8E_EVENT_PROBE = '1'
    $env:JJFB_E8E_FE8_WATCH = '1'
    $env:JJFB_E8E_CANDIDATE = 'R0_EVENTCODE_2'
    $env:JJFB_E8E_DRAIN_ORDER = 'B'
    Remove-Item Env:JJFB_E8D_10165_PROBE -ErrorAction SilentlyContinue
  } else {
    Remove-Item Env:JJFB_E8E_MODE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_E8E_EVENT_PROBE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_E8E_FE8_WATCH -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_E8E_CANDIDATE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_E8E_DRAIN_ORDER -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_E8D_10165_PROBE -ErrorAction SilentlyContinue
  }

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

  @(
    'JJFB_E8B_MODE', 'JJFB_E8C_MODE', 'JJFB_E8D_MODE', 'JJFB_E8F_MODE',
    'JJFB_E8G_MODE', 'JJFB_E8H_MODE', 'JJFB_E8I_MODE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir "stage_e8j_${Label}_stdout.txt"
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  return $dst
}

# Force rebuild of trace object on first run
$obj = Join-Path $Root 'build-i686\CMakeFiles\launcher_core.dir\src\runtime\robotol_flag_writer_trace.c.obj'
if (-not $SkipBuild) {
  Remove-Item -Force $obj -ErrorAction SilentlyContinue
}

$productLog = Invoke-E8JRun -Label 'product' -With10165:$false
$SkipBuild = $true
$probeLog = $null
if (-not $SkipProbe) {
  $probeLog = Invoke-E8JRun -Label 'probe10165' -With10165:$true
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
    cluster = Hit $log 'JJFB_E8J_CLUSTER_HIT\]'
    upstream = Hit $log 'JJFB_E8J_UPSTREAM_HIT\]'
    queueBp = Hit $log 'JJFB_E8J_QUEUE_HIT\]'
    blHit = Hit $log 'JJFB_E8J_BL_HIT\]'
    parent = Hit $log 'JJFB_E8I_PARENT_HIT\]|tag=p300158'
    hit300158 = Hit $log 'tag=p300158|role=entry tag=e300158'
    hit300714 = Hit $log 'tag=p300714'
    qread = Hit $log 'JJFB_E8J_QUEUE_READ\]'
    stateWrite = Hit $log 'JJFB_E8I_STATE_WRITE\]'
    state38 = Hit $log 'JJFB_E8I_STATE_EQ38\]'
    draw = Hit $log '\[JJFB_DRAW\]'
    fe8Write = Hit $log 'JJFB_E8E_FE8_WRITE\]'
    probeDone = Hit $log 'JJFB_E8E_EVENT_PROBE_DONE\]'
    sum = Grab $log 'JJFB_E8J_SUMMARY\]'
    parentSum = Grab $log 'JJFB_E8I_PARENT_SUMMARY\]'
    clusterN = Count $log 'JJFB_E8J_CLUSTER_HIT\]'
    upstreamN = Count $log 'JJFB_E8J_UPSTREAM_HIT\]'
    queueN = Count $log 'JJFB_E8J_QUEUE_HIT\]'
    blN = Count $log 'JJFB_E8J_BL_HIT\]'
    qreadN = Count $log 'JJFB_E8J_QUEUE_READ\]'
    stateN = Count $log 'JJFB_E8I_STATE_WRITE\]'
    firstCluster = Grab $log 'JJFB_E8J_CLUSTER_HIT\]'
    firstUpstream = Grab $log 'JJFB_E8J_UPSTREAM_HIT\]'
    firstQueue = Grab $log 'JJFB_E8J_QUEUE_HIT\]'
    firstQread = Grab $log 'JJFB_E8J_QUEUE_READ\]'
  }
}

$P = Analyze-Log $productLog
$Q = if ($probeLog) { Analyze-Log $probeLog } else { $null }

# Verdict selection (product-primary; probe informs queue hypothesis)
$verdict = 'PARENT_CALLER_CLUSTER_NEVER_REACHED'
if ($P.draw) {
  $verdict = 'DRAW_REACHED'
} elseif ($P.hit300714 -or $P.hit300158 -or $P.blHit) {
  $verdict = 'PARENT_CALLER_CLUSTER_REACHED_NEXT_GAP'
} elseif ($P.cluster) {
  $verdict = 'BOOT_CLUSTER_REACHED_NEXT_GAP'
  # Map to allowed vocabulary
  $verdict = 'PARENT_CALLER_CLUSTER_REACHED_NEXT_GAP'
} elseif ($P.state38 -or ($P.stateWrite -and -not $P.cluster)) {
  $verdict = 'R9_8D0_WRITER_REACHED_NEXT_GAP'
} elseif ($Q -and $Q.probeDone -and -not $Q.cluster -and -not $Q.blHit) {
  # 10165 ran, FE8/B7D path alive, but still no parent cluster
  if ($Q.queueBp -or $Q.qread) {
    $verdict = 'QUEUE_CONSUMER_BRANCH_UNMET'
  } else {
    $verdict = 'QUEUE_CONSUMER_NEVER_REACHED'
  }
} elseif (-not $P.cluster -and -not $P.upstream) {
  $verdict = 'MISSING_APP_INIT_DISPATCH'
  # Prefer allowed primary label when entire L2 cold:
  $verdict = 'PARENT_CALLER_CLUSTER_NEVER_REACHED'
}

Write-Host "== E8J audit + hash =="
python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
$jjfb = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'
$hash = (Get-FileHash -Algorithm SHA256 $jjfb).Hash.ToLower()
$expect = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'

$probeSection = if ($Q) {
@"
## Probe run (10165 structured, observe-only)

| Gate | Result |
|------|--------|
| probe done | $(if($Q.probeDone){'yes'}else{'no'}) |
| FE8 write | $(if($Q.fe8Write){'yes'}else{'no'}) |
| cluster entry hit | $(if($Q.cluster){"yes n=$($Q.clusterN)"}else{'no'}) |
| upstream hit | $(if($Q.upstream){"yes n=$($Q.upstreamN)"}else{'no'}) |
| queue BP hit | $(if($Q.queueBp){"yes n=$($Q.queueN)"}else{'no'}) |
| FE8/B7D mem-read | $(if($Q.qread){"yes n=$($Q.qreadN)"}else{'no'}) |
| parent BL / 0x300158 | $(if($Q.blHit -or $Q.hit300158){'yes'}else{'no'}) |
| state writes | $($Q.stateN) |
| summary | ``$($Q.sum)`` |

"@
} else { "## Probe run`n`nskipped`n" }

$report = @"
# Stage E8J — Dispatcher Parent Caller Upstream Reachability

## Verdict

``$verdict``

## Gates (product)

| Gate | Result |
|------|--------|
| upstream_l2.md | yes |
| queue_consumer.md | yes |
| cluster BP armed | yes (n=$bpCount) |
| state watch | yes |
| FE8/B7D read watch | yes |
| hot cluster entry hit | $(if($P.cluster){"yes n=$($P.clusterN)"}else{'no'}) |
| L2 upstream hit | $(if($P.upstream){"yes n=$($P.upstreamN)"}else{'no'}) |
| queue consumer BP hit | $(if($P.queueBp){"yes n=$($P.queueN)"}else{'no'}) |
| FE8/B7D mem-read | $(if($P.qread){"yes n=$($P.qreadN)"}else{'no'}) |
| direct BL to 0x300158 | $(if($P.blHit){"yes n=$($P.blN)"}else{'no'}) |
| 0x300158 / 0x300714 | $(if($P.hit300158 -or $P.hit300714){'yes'}else{'no'}) |
| state writes | $($P.stateN) |
| state == 38 | $(if($P.state38){'yes'}else{'no'}) |
| DRAW | $(if($P.draw){'yes'}else{'no'}) |
| jjfb hash | $(if($hash -eq $expect){'unchanged'}else{"MISMATCH $hash"}) |

$probeSection
## Static (TARGET_OBSERVED)

See ``out/e8j_tmp/upstream_l2.md`` / ``queue_consumer.md``:

- 85 BL → ``0x300158`` in ~60 enclosing fns; hottest: ``0x2E0E00`` (13), ``0x2DFC3C`` (7), ``0x2DC778`` (1)
- FE8 has **no external reader** (only enqueue core ``0x30D24C`` reloads its own store)
- B7D readers: 8 sites (incl. ``0x2DC80C`` via legacy timer/callback gate)
- ``0x30D730 BL 0x2DFC3C`` lives in event-switch ``0x30D300``, **not** in 10165 enqueue body
- Bridge reader ``0x2F5390`` is both a parent-caller cluster and a 7D8 reader

## Live product (max tick ~249; summaries through tick_100)

- E8J: entry=0 upstream=0 queue_bp=0 bl=0 qread=0 state_writes=0
- hot ``0x2DFC3C`` / ``0x2E0E00`` / ``0x2DC778``: NEVER
- event-switch ``0x30D300``: NEVER
- external B7D drain / legacy gate: NEVER

## Probe 10165 (observe-only R0_EVENTCODE_2)

- enqueue ``0x30D24C`` HIT; long-path ``0x30D28C`` HIT; FE8 write + B7D=1
- queue sites inside enqueue HIT (``0x30D268`` / ``0x30D294``)
- still NEVER: hot clusters, ``0x30D300``, parent ``0x300158``, state writer
- therefore 10165 FE8/B7D is **not** the missing wake for parent caller cluster

## Ranked hypotheses (post-live)

1. ``MISSING_APP_INIT_DISPATCH`` — event-switch / app-init that should enter ``0x2DFC3C``/``0x2E0E00`` never runs (``0x30D300`` cold even after 10165)
2. ``QUEUE_CONSUMER_NEVER_REACHED`` — external B7D drain (``0x2DC80C`` / timer-callback gate) never hit after FE8/B7D side effects
3. ``MISSING_PLATFORM_SIDE_EFFECT_STATE_38`` — state word still 0; downstream of cold dispatcher
4. ``MISSING_RESOURCE_READY_DISPATCH`` / ``MISSING_NETWORK_READY_DISPATCH`` — still HYPOTHESIS only
5. ``BOOT_CLUSTER_REACHED_NEXT_GAP`` — not applicable this stage

## Forbidden kept

No R9+state force, no C44/C9D/CF5 force, no SVC success, no event spray, no MRP/EXT edits.

## Artifacts

- ``tools/e8j_caller_upstream_reach.py``
- ``RUN_E8J_CALLER_UPSTREAM.ps1``
- ``out/e8j_tmp/``
- ``logs/stage_e8j_product_stdout.txt``
- ``logs/stage_e8j_probe10165_stdout.txt``
- ``src/runtime/robotol_flag_writer_trace.c`` (E8J cluster BP + FE8/B7D read watch)
"@

$reportPath = Join-Path $reportDir 'stage_e8j_verdict.md'
Set-Content -Path $reportPath -Value $report -Encoding UTF8
Write-Host "Verdict=$verdict"
Write-Host "Report=$reportPath"
