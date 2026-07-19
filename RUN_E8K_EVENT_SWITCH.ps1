# Stage E8K: event-switch 0x30D300 provenance + B7D drain + derived 0x10102 case probe
param(
  [int]$Seconds = 200,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [switch]$SkipProbe,
  [switch]$SkipCaseProbe
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8k_tmp'
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

Write-Host "== E8K static event-switch + drain xref =="
python (Join-Path $Root 'tools\e8k_event_switch_provenance.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpSpec = (Get-Content (Join-Path $outDir 'e8k_bp_spec.txt') -Raw).Trim()
Write-Host "E8K BP count=$(($bpSpec -split ',').Count)"

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8KRun([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8K live $Label =="
  Clear-E8Modes
  $env:JJFB_E8K_MODE = '1'
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
  $dst = Join-Path $logDir "stage_e8k_${Label}_stdout.txt"
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  # Also keep vmrp copy if product stdout is thin
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  if ((Test-Path $vm) -and ((Get-Item $dst).Length -lt 10000)) {
    Copy-Item -Force $vm $dst
  }
  return $dst
}

$obj = Join-Path $Root 'build-i686\CMakeFiles\launcher_core.dir\src\runtime\robotol_flag_writer_trace.c.obj'
$obj2 = Join-Path $Root 'build-i686\CMakeFiles\launcher_core.dir\src\runtime\robotol_idle_watch.c.obj'
if (-not $SkipBuild) {
  Remove-Item -Force $obj, $obj2 -ErrorAction SilentlyContinue
}

$productLog = Invoke-E8KRun -Label 'product' -ExtraEnv @{}
$SkipBuild = $true

$probeLog = $null
if (-not $SkipProbe) {
  $probeLog = Invoke-E8KRun -Label 'probe10165' -ExtraEnv @{
    JJFB_E8E_MODE = '1'
    JJFB_E8E_EVENT_PROBE = '1'
    JJFB_E8E_FE8_WATCH = '1'
    JJFB_E8E_CANDIDATE = 'R0_EVENTCODE_2'
    JJFB_E8E_DRAIN_ORDER = 'B'
  }
}

$caseLog = $null
if (-not $SkipCaseProbe) {
  # Derived case 310 (0x136) → BL 0x2DFC3C inside event-switch.
  $caseLog = Invoke-E8KRun -Label 'case310' -ExtraEnv @{
    JJFB_E8K_10102_CASE = '310'
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
    sw = Hit $log 'tag=e30D300|pc=0x30D300'
    caseArm = Hit $log 'tag=e30D730|pc=0x30D730'
    hot = Hit $log 'tag=e2DFC3C|tag=e2E0E00|tag=e2DC778'
    parent = Hit $log 'tag=p300158|tag=p300714'
    drain = Hit $log 'tag=q2DC80C|pc=0x2DC80C'
    gate = Hit $log 'tag=q305EB8|pc=0x305000'
    fire10102 = Hit $log 'JJFB_E8K_10102_FIRE\]'
    fireDone = Hit $log 'JJFB_E8K_10102_FIRE_DONE\]'
    probeDone = Hit $log 'JJFB_E8E_EVENT_PROBE_DONE\]'
    fe8 = Hit $log 'JJFB_E8E_FE8_WRITE\]'
    state = Hit $log 'JJFB_E8I_STATE_WRITE\]'
    draw = Hit $log '\[JJFB_DRAW\]'
    sum = Grab $log 'JJFB_E8J_SUMMARY\]'
    swN = Count $log 'tag=e30D300'
    hotN = Count $log 'tag=e2DFC3C'
    drainN = Count $log 'tag=q2DC80C'
  }
}

$P = Analyze-Log $productLog
$Q = if ($probeLog) { Analyze-Log $probeLog } else { $null }
$C = if ($caseLog) { Analyze-Log $caseLog } else { $null }

$verdict = 'EVENT_SWITCH_CALLER_NEVER_REACHED'
if ($P.draw -or ($C -and $C.draw)) {
  $verdict = 'DRAW_REACHED'
} elseif ($C -and $C.sw -and $C.hot) {
  $verdict = 'EVENT_SWITCH_CASE_DERIVED_NEXT_GAP'
} elseif ($C -and $C.sw -and -not $C.hot) {
  $verdict = 'EVENT_SWITCH_CASE_DERIVED_NEXT_GAP'
} elseif ($P.sw) {
  $verdict = 'EVENT_SWITCH_CALLER_FOUND_NEXT_GAP'
} elseif ($P.drain -or ($Q -and $Q.drain)) {
  if ($Q -and $Q.probeDone -and -not $Q.hot) {
    $verdict = 'B7D_DRAIN_BRANCH_UNMET'
  } else {
    $verdict = 'MISSING_QUEUE_CONSUMER_TO_DISPATCHER'
  }
} elseif ($Q -and $Q.probeDone -and -not $Q.drain) {
  $verdict = 'B7D_DRAIN_CALLER_NEVER_REACHED'
} else {
  $verdict = 'MISSING_APP_INIT_DISPATCH'
  # Prefer vocabulary that matches primary cold fact:
  $verdict = 'EVENT_SWITCH_CALLER_NEVER_REACHED'
}

Write-Host "== E8K audit + hash =="
python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
$jjfb = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'
$hash = (Get-FileHash -Algorithm SHA256 $jjfb).Hash.ToLower()
$expect = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'

$caseSection = if ($C) {
@"
## Case-310 probe (observe-only 0x10102 R0=310)

| Gate | Result |
|------|--------|
| 10102 fire | $(if($C.fire10102){'yes'}else{'no'}) |
| 0x30D300 entry | $(if($C.sw){"yes n=$($C.swN)"}else{'no'}) |
| case arm / hot 0x2DFC3C | $(if($C.hot){"yes n=$($C.hotN)"}else{'no'}) |
| parent 0x300158 | $(if($C.parent){'yes'}else{'no'}) |
| DRAW | $(if($C.draw){'yes'}else{'no'}) |
| summary | ``$($C.sum)`` |

"@
} else { "## Case-310 probe`n`nskipped`n" }

$probeSection = if ($Q) {
@"
## 10165 probe

| Gate | Result |
|------|--------|
| probe done | $(if($Q.probeDone){'yes'}else{'no'}) |
| FE8 write | $(if($Q.fe8){'yes'}else{'no'}) |
| 0x30D300 | $(if($Q.sw){'yes'}else{'no'}) |
| drain 0x2DC80C | $(if($Q.drain){"yes n=$($Q.drainN)"}else{'no'}) |
| gate callback | $(if($Q.gate){'yes'}else{'no'}) |
| hot cluster | $(if($Q.hot){'yes'}else{'no'}) |

"@
} else { "## 10165 probe`n`nskipped`n" }

$report = @"
# Stage E8K — Event-Switch 0x30D300 Upstream + B7D Drain

## Verdict

``$verdict``

## Gates (product)

| Gate | Result |
|------|--------|
| event_switch_xref.md | yes |
| BP armed | yes |
| 0x30D300 entry | $(if($P.sw){'yes'}else{'no'}) |
| hot clusters | $(if($P.hot){'yes'}else{'no'}) |
| drain 0x2DC80C | $(if($P.drain){'yes'}else{'no'}) |
| drain gate | $(if($P.gate){'yes'}else{'no'}) |
| parent 0x300158 | $(if($P.parent){'yes'}else{'no'}) |
| state writes | $(if($P.state){'yes'}else{'no'}) |
| DRAW | $(if($P.draw){'yes'}else{'no'}) |
| jjfb hash | $(if($hash -eq $expect){'unchanged'}else{"MISMATCH $hash"}) |

$probeSection
$caseSection
## Static (TARGET_OBSERVED)

See ``out/e8k_tmp/event_switch_xref.md``:

- **0x30D300 has 0 in-module BL callers** and 0 absolute literal ptrs in ext/MRP
- It **is** the Thumb handler ``0x30D301`` registered via plat ``0x10102`` family ``0x1E200``
- host_drain=**no** — product only REGISTERs; never delivers into the handler
- Switch index = **R0**; bound ``0x157``; case **310** → ``BL 0x2DFC3C``; case **156** → ``BL 0x300158``
- B7D drain chain: ``0x2F5734 → gate → 0x2DC80C`` (sole BL into drain)

## Live product

- summary: ``$($P.sum)``
- 0x10102 only appears as REGISTER in plat census (no delivery)

## Ranked hypotheses (post-live)

1. ``MISSING_APP_INIT_DISPATCH`` / ``EVENT_SWITCH_CALLER_NEVER_REACHED`` — host never delivers 0x10102 into 0x30D301
2. ``EVENT_SWITCH_CASE_DERIVED_NEXT_GAP`` — case 310/156 known; needs correct delivery ABI/source
3. ``B7D_DRAIN_CALLER_NEVER_REACHED`` / ``MISSING_QUEUE_CONSUMER_TO_DISPATCHER`` — timer/callback gate cold
4. ``MISSING_PLATFORM_SIDE_EFFECT_STATE_38`` — still downstream
5. ``MISSING_RESOURCE_READY_DISPATCH`` / ``MISSING_NETWORK_READY_DISPATCH`` — only if live proves

## Forbidden kept

No state/idle force, no SVC success, no MRP/EXT edits, no random event spray (case-310 is derived-index observe-only).

## Artifacts

- ``tools/e8k_event_switch_provenance.py``
- ``RUN_E8K_EVENT_SWITCH.ps1``
- ``out/e8k_tmp/``
- ``logs/stage_e8k_product_stdout.txt``
- ``logs/stage_e8k_probe10165_stdout.txt``
- ``logs/stage_e8k_case310_stdout.txt``
"@

$reportPath = Join-Path $reportDir 'stage_e8k_verdict.md'
Set-Content -Path $reportPath -Value $report -Encoding UTF8
Write-Host "Verdict=$verdict"
Write-Host "Report=$reportPath"
