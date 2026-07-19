# Stage E8H: bootstrap dispatcher provenance + observe-only SVC #0xAB trap
param(
  [int]$Seconds = 90,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('NONE', 'ALL')]
  [string]$Counterfactual = 'NONE',
  [switch]$SkipBuild,
  [switch]$SkipCf
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8h_tmp'
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

Write-Host "== E8H static dispatcher + SVC #0xAB xref =="
python (Join-Path $Root 'tools\e8h_dispatcher_svc_xref.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
$dispCsv = (Get-Content (Join-Path $outDir 'dispatcher_bp_csv.txt') -Raw).Trim()

function Invoke-E8HLive([string]$CfMode, [bool]$Skip) {
  $env:JJFB_E8H_MODE = '1'
  $env:JJFB_E8C_IDLE_WATCH = '1'
  $env:JJFB_E8C_WATCH_OFFSETS = $offsets
  $env:JJFB_E8D_EARLY_WATCH = '1'
  $env:JJFB_E8H_DISPATCHER_BP = '1'
  $env:JJFB_E8H_DISPATCHER_PCS = $dispCsv
  $env:JJFB_E8H_SVC_TRAP = '1'
  $env:JJFB_E8H_SVC_STOP = '1'
  # Focus: no E8G caller spray / no writer BP / no event probes.
  Remove-Item Env:JJFB_E8G_CALLER_BP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8G_FAULT_WATCH -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8F_WRITER_BP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8E_EVENT_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8D_10165_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8F_SIBLING_PROBE -ErrorAction SilentlyContinue
  if ($CfMode -ne 'NONE') {
    $env:JJFB_E8F_COUNTERFACTUAL = $CfMode
    $env:JJFB_E8G_FAULT_WATCH = '1'
  } else {
    Remove-Item Env:JJFB_E8F_COUNTERFACTUAL -ErrorAction SilentlyContinue
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
    'JJFB_E8B_MODE', 'JJFB_E8C_MODE', 'JJFB_E8D_MODE', 'JJFB_E8E_MODE', 'JJFB_E8F_MODE', 'JJFB_E8G_MODE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($Skip) { $eArgs += '-SkipBuild' }
  & powershell @eArgs

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ($CfMode -eq 'NONE') {
    $dst = Join-Path $logDir 'stage_e8h_jjfb_stdout.txt'
  } else {
    $dst = Join-Path $logDir ("stage_e8h_cf_{0}_stdout.txt" -f $CfMode.ToLower())
  }
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  return ,$dst
}

Write-Host "== E8H live product (Counterfactual=NONE) =="
$null = Invoke-E8HLive -CfMode 'NONE' -Skip:$SkipBuild

if (-not $SkipCf) {
  Write-Host "== E8H CF ALL (SVC classify only; not product) =="
  $null = Invoke-E8HLive -CfMode 'ALL' -Skip:$true
} elseif ($Counterfactual -ne 'NONE') {
  Write-Host "== E8H live Counterfactual=$Counterfactual =="
  $null = Invoke-E8HLive -CfMode $Counterfactual -Skip:$true
}

$primaryLog = Join-Path $logDir 'stage_e8h_jjfb_stdout.txt'
$cfLog = Join-Path $logDir 'stage_e8h_cf_all_stdout.txt'

function Hit([string]$pat, [string]$log) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -ErrorAction SilentlyContinue)
}
function Grab([string]$pat, [string]$log) {
  if (-not (Test-Path $log)) { return '' }
  $m = Select-String -Path $log -Pattern $pat | Select-Object -Last 1
  if ($m) { return $m.Line }
  return ''
}

$dispHit = Hit 'JJFB_E8H_DISPATCHER_HIT\]' $primaryLog
$hit30103c = Hit 'tag=d30103c' $primaryLog
$hit3020c8 = Hit 'tag=d3020c8' $primaryLog
$hit302340 = Hit 'tag=d302340' $primaryLog
$hitWriter = Hit 'tag=d2f4e82' $primaryLog
$hitDispFn = Hit 'tag=d300714' $primaryLog
$svcProduct = Hit 'JJFB_E8H_SVC_AB\]' $primaryLog
$svcCf = Hit 'JJFB_E8H_SVC_AB\]' $cfLog
$sumLine = Grab 'JJFB_E8H_DISPATCHER_SUMMARY\]' $primaryLog
$snap8d0 = Grab 'R9_state=' $primaryLog
$draw = Hit '\[JJFB_DRAW\]' $primaryLog

# Verdict selection (product path first, then SVC classify).
$verdict = 'BOOTSTRAP_DISPATCHER_NEVER_ENTERED'
if ($draw) {
  $verdict = 'DRAW_REACHED'
} elseif ($hitWriter) {
  $verdict = 'WRITER_REACHED_NEXT_GAP'
} elseif ($hit3020c8 -and -not $hit302340) {
  $verdict = 'MISSING_STATE_CODE_FOR_3020C8'
} elseif ($hit30103c -and -not $hit3020c8) {
  $verdict = 'BOOTSTRAP_DISPATCHER_BRANCH_UNMET'
} elseif ($hitDispFn -and -not $hit30103c) {
  $verdict = 'BOOTSTRAP_DISPATCHER_BRANCH_UNMET'
} elseif ($svcProduct) {
  $verdict = 'SVC_AB_PRODUCT_REQUIRED'
} elseif (-not $hitDispFn -and -not $hit30103c) {
  $verdict = 'BOOTSTRAP_DISPATCHER_NEVER_ENTERED'
}

$svcClass = 'NONE'
if ($svcProduct) { $svcClass = 'SVC_AB_PRODUCT_REQUIRED' }
elseif ($svcCf) { $svcClass = 'SVC_AB_POST_GATE_REQUIRED' }
elseif ($svcProduct -eq $false -and (Test-Path $cfLog) -and -not $svcCf) {
  $svcClass = 'SVC_AB_NOT_OBSERVED'
}

Write-Host "== E8H audit + hash =="
python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
$jjfb = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'
$hash = (Get-FileHash -Algorithm SHA256 $jjfb).Hash.ToLower()
$expect = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'

$report = @"
# Stage E8H — Bootstrap Dispatcher Provenance + SVC #0xAB

## Verdict

``$verdict``

Secondary SVC classify: ``$svcClass``

## Gates

| Gate | Result |
|------|--------|
| dispatcher_svc_xref.md | yes |
| dispatcher BP armed | yes |
| 0x300714 hit (product) | $(if($hitDispFn){'yes'}else{'no'}) |
| 0x30103C hit (product) | $(if($hit30103c){'yes'}else{'no'}) |
| 0x3020C8 hit (product) | $(if($hit3020c8){'yes'}else{'no'}) |
| 0x302340 hit (product) | $(if($hit302340){'yes'}else{'no'}) |
| writer 0x2F4E82 hit | $(if($hitWriter){'yes'}else{'no'}) |
| SVC #0xAB (product) | $(if($svcProduct){'yes'}else{'no'}) |
| SVC #0xAB (CF ALL) | $(if($svcCf){'yes'}else{'no'}) |
| DRAW | $(if($draw){'yes'}else{'no'}) |
| jjfb hash | $(if($hash -eq $expect){'unchanged'}else{"MISMATCH $hash"}) |

## Line A — dispatcher

Static path (TARGET_OBSERVED):

- ``0x300158`` → BL ``0x3002C0`` → ``0x300714``
- ``0x300714`` loads ``*(R9+(0x800+0xD0))``; arm ``== 38`` reaches ``0x30103A`` / ``0x30103C``
- ``0x30103C`` BL → ``0x3020C8`` (r4 = original arg to ``0x300714``)
- ``0x3020C8`` CMP r4 cases → ``0x302340``/``0x302362`` → C44 writer ``0x2F4E82``

Live product:

- dispatcher hits observed: $(if($dispHit){'yes'}else{'no'})
- summary: ``$sumLine``
- R9+(0x800+0xD0) snap: ``$snap8d0``

## Line B — SVC #0xAB (observe-only)

- Single robotol site ``0x2D92AE`` in stub ``0x2D92A4``: STRB r0,[sp]; MOVS r0,#3; MOV r1,sp; SVC #0xAB
- Stub callers reconstruct original r0 mostly ``#10`` (see xref)
- Trap dumps PC/LR/R0-R3/arg block; **does not** fake success
- Product SVC: $(if($svcProduct){'OBSERVED'}else{'NEVER'})
- CF ALL SVC: $(if($svcCf){'OBSERVED'}else{'NEVER'})

## Minimal handler proposal (not implemented)

Deferred until callsite semantics are fully classified. Do **not** return success blindly.
If ``$svcClass`` is POST_GATE_REQUIRED, handler is needed only after real idle unlock — still must decode ``sp_byte`` / service ``r0=3``.

## Next gap

1. If dispatcher never entered: find who should call ``0x300158`` / ``0x300714`` (app-init / resource / network / platform return).
2. If ``R9+(0x800+0xD0) != 38``: derive what writes (0x800+0xD0) (state code producer).
3. If SVC POST_GATE: derive justified ``SVC #0xAB`` host bridge from stub ABI + caller r0 values.

## Artifacts

- ``tools/e8h_dispatcher_svc_xref.py``
- ``RUN_E8H_DISPATCHER_SVC.ps1``
- ``out/e8h_tmp/``
- ``logs/stage_e8h_jjfb_stdout.txt``
- ``logs/stage_e8h_cf_all_stdout.txt`` (if CF run)
"@

$reportPath = Join-Path $reportDir 'stage_e8h_verdict.md'
Set-Content -Path $reportPath -Value $report -Encoding UTF8
Write-Host "Verdict=$verdict SVC=$svcClass"
Write-Host "Report=$reportPath"
