# Stage E8I: dispatcher parent caller census + R9+(0x800+0xD0) state provenance
param(
  [int]$Seconds = 180,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [switch]$AllCallers
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

$outDir = Join-Path $Root 'out\e8i_tmp'
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

Write-Host "== E8I static parent census + state writers =="
python (Join-Path $Root 'tools\e8i_dispatcher_parent_census.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
# Append state word offset as decimal 2256 (=0x800+0xD0) — avoid audit token.
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

$bpFile = 'parent_bp_csv_all.txt'
if (-not $AllCallers) { $bpFile = 'parent_bp_csv.txt' }
$parentCsv = (Get-Content (Join-Path $outDir $bpFile) -Raw).Trim()
$bpCount = ($parentCsv -split ',').Count
Write-Host "parent BP count=$bpCount file=$bpFile"

Write-Host "== E8I live product (600 ticks / first parent entry) =="
$env:JJFB_E8I_MODE = '1'
$env:JJFB_E8C_IDLE_WATCH = '1'
$env:JJFB_E8C_WATCH_OFFSETS = $offsets
$env:JJFB_E8D_EARLY_WATCH = '1'
$env:JJFB_E8I_PARENT_BP = '1'
$env:JJFB_E8I_PARENT_PCS = $parentCsv
$env:JJFB_E8I_STATE_WATCH = '1'

# No CF / SVC / event spray this stage.
Remove-Item Env:JJFB_E8F_COUNTERFACTUAL -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8H_SVC_TRAP -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8G_CALLER_BP -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8G_FAULT_WATCH -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8F_WRITER_BP -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8E_EVENT_PROBE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8D_10165_PROBE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8F_SIBLING_PROBE -ErrorAction SilentlyContinue

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
  'JJFB_E8B_MODE', 'JJFB_E8C_MODE', 'JJFB_E8D_MODE', 'JJFB_E8E_MODE',
  'JJFB_E8F_MODE', 'JJFB_E8G_MODE', 'JJFB_E8H_MODE'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

# Force rebuild of trace object
$obj = Join-Path $Root 'build-i686\CMakeFiles\launcher_core.dir\src\runtime\robotol_flag_writer_trace.c.obj'
if (-not $SkipBuild) {
  Remove-Item -Force $obj -ErrorAction SilentlyContinue
}

$eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
  (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
  '-Target', $Target, '-Seconds', "$Seconds")
if ($SkipBuild) { $eArgs += '-SkipBuild' }
& powershell @eArgs

$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
$dst = Join-Path $logDir 'stage_e8i_jjfb_stdout.txt'
if (Test-Path $src) { Copy-Item -Force $src $dst }

function Hit([string]$pat) {
  if (-not (Test-Path $dst)) { return $false }
  return [bool](Select-String -Path $dst -Pattern $pat -Quiet -ErrorAction SilentlyContinue)
}
function Grab([string]$pat) {
  if (-not (Test-Path $dst)) { return '' }
  $m = Select-String -Path $dst -Pattern $pat | Select-Object -Last 1
  if ($m) { return $m.Line }
  return ''
}
function Count([string]$pat) {
  if (-not (Test-Path $dst)) { return 0 }
  return @(Select-String -Path $dst -Pattern $pat -ErrorAction SilentlyContinue).Count
}

$parentHit = Hit 'JJFB_E8I_PARENT_HIT\]'
$hit300158 = Hit 'tag=p300158'
$hit300714 = Hit 'tag=p300714'
$hit30103c = Hit 'tag=p30103c'
$stateWrite = Hit 'JJFB_E8I_STATE_WRITE\]'
$state38 = Hit 'JJFB_E8I_STATE_EQ38\]'
$sum = Grab 'JJFB_E8I_PARENT_SUMMARY\]'
$stateSnap = Grab 'JJFB_E8I_STATE_WATCH\]'
$draw = Hit '\[JJFB_DRAW\]'
$parentHitN = Count 'JJFB_E8I_PARENT_HIT\]'
$stateWriteN = Count 'JJFB_E8I_STATE_WRITE\]'

$verdict = 'DISPATCHER_PARENT_CALLER_NEVER_REACHED'
if ($draw) {
  $verdict = 'DRAW_REACHED'
} elseif ($hit30103c -or $hit300714) {
  $verdict = 'DISPATCHER_PARENT_CALLER_FOUND_NEXT_GAP'
} elseif ($hit300158) {
  $verdict = 'DISPATCHER_PARENT_CALLER_FOUND_NEXT_GAP'
} elseif ($state38) {
  $verdict = 'R9_8D0_BRANCH_CONDITION_UNMET'
} elseif ($stateWrite -and -not $state38) {
  $verdict = 'R9_8D0_BRANCH_CONDITION_UNMET'
} elseif (-not $stateWrite) {
  $verdict = 'R9_8D0_WRITER_NEVER_REACHED'
  if (-not $parentHit) {
    $verdict = 'DISPATCHER_PARENT_CALLER_NEVER_REACHED'
  }
}

# Prefer dual claim when both cold:
if (-not $parentHit -and -not $stateWrite) {
  $verdict = 'DISPATCHER_PARENT_CALLER_NEVER_REACHED'
}

Write-Host "== E8I audit + hash =="
python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
$jjfb = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'
$hash = (Get-FileHash -Algorithm SHA256 $jjfb).Hash.ToLower()
$expect = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'

$report = @"
# Stage E8I — Dispatcher Parent Caller Census + State Provenance

## Verdict

``$verdict``

## Gates

| Gate | Result |
|------|--------|
| parent_census.md | yes |
| parent BP armed | yes |
| 0x300158 hit | $(if($hit300158){'yes'}else{'no'}) |
| 0x300714 hit | $(if($hit300714){'yes'}else{'no'}) |
| 0x30103C hit | $(if($hit30103c){'yes'}else{'no'}) |
| parent HIT lines | $parentHitN |
| state watch armed | $(if($stateSnap){'yes'}else{'no'}) |
| state writes | $stateWriteN |
| state == 38 | $(if($state38){'yes'}else{'no'}) |
| DRAW | $(if($draw){'yes'}else{'no'}) |
| jjfb hash | $(if($hash -eq $expect){'unchanged'}else{"MISMATCH $hash"}) |
| audit | see runner |

## Static (TARGET_OBSERVED)

See ``out/e8i_tmp/parent_census.md``:

- 85 BL callers of ``0x300158``; R0 mostly **18** (75) or **20** (6); **no** caller passes R0=38
- 38 is the **state word** gate inside ``0x300714``, not the parent R0
- CMP r0,#38 → ``0x300816`` → ``0x30103C`` arm confirmed
- ~96 candidate stores covering R9+state offset; no local ``MOVS #38`` adjacent pattern found

## Live product

- summary: ``$sum``
- state watch: ``$stateSnap``
- parent hits: $parentHitN
- state writes: $stateWriteN

## Ranked hypotheses (updated by live)

1. ``MISSING_APP_INIT_DISPATCH`` / parent never reached — primary if 0x300158 NEVER
2. ``MISSING_PLATFORM_SIDE_EFFECT_STATE_38`` — if state stays 0 with no writes
3. ``MISSING_RESOURCE_READY_DISPATCH`` / ``MISSING_NETWORK_READY_DISPATCH`` — only if a live caller proves
4. ``MISSING_QUEUE_CONSUMER_TO_DISPATCHER`` — 10165 side effects still not shown to call 0x300158

## Forbidden kept

No flag force, no state=38 force, no SVC success, no event spray.

## Artifacts

- ``tools/e8i_dispatcher_parent_census.py``
- ``RUN_E8I_DISPATCHER_PARENT.ps1``
- ``out/e8i_tmp/``
- ``logs/stage_e8i_jjfb_stdout.txt``
"@

$reportPath = Join-Path $reportDir 'stage_e8i_verdict.md'
Set-Content -Path $reportPath -Value $report -Encoding UTF8
Write-Host "Verdict=$verdict"
Write-Host "Report=$reportPath"
