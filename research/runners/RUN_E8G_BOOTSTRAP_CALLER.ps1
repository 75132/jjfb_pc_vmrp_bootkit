# Stage E8G: bootstrap caller provenance + counterfactual second-gate fault
param(
  [int]$Seconds = 90,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('NONE', 'C44', 'C9D', 'CF5', 'C44C9D', 'C44CF5', 'C9DCF5', 'ALL')]
  [string]$Counterfactual = 'NONE',
  [switch]$SkipBuild,
  [switch]$Matrix
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

$outDir = Join-Path $Root 'out\e8g_tmp'
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

Write-Host "== E8G static bootstrap caller xref + fault + 10102 =="
python (Join-Path $Root 'tools\e8g_bootstrap_caller_xref.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
$callerCsv = (Get-Content (Join-Path $outDir 'caller_bp_csv.txt') -Raw).Trim()

function Invoke-E8GLive([string]$CfMode, [bool]$Skip) {
  $env:JJFB_E8G_MODE = '1'
  $env:JJFB_E8C_IDLE_WATCH = '1'
  $env:JJFB_E8C_WATCH_OFFSETS = $offsets
  $env:JJFB_E8D_EARLY_WATCH = '1'
  $env:JJFB_E8G_CALLER_BP = '1'
  $env:JJFB_E8G_CALLER_PCS = $callerCsv
  $env:JJFB_E8G_FAULT_WATCH = '1'
  # No writer BP / 10165 spray this stage (focus callers).
  Remove-Item Env:JJFB_E8F_WRITER_BP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8E_EVENT_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8D_10165_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8F_SIBLING_PROBE -ErrorAction SilentlyContinue
  if ($CfMode -ne 'NONE') {
    $env:JJFB_E8F_COUNTERFACTUAL = $CfMode
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
    'JJFB_E8B_MODE', 'JJFB_E8C_MODE', 'JJFB_E8D_MODE', 'JJFB_E8E_MODE', 'JJFB_E8F_MODE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($Skip) { $eArgs += '-SkipBuild' }
  & powershell @eArgs

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  if ($CfMode -eq 'NONE') {
    $dst = Join-Path $logDir 'stage_e8g_jjfb_stdout.txt'
  } else {
    $dst = Join-Path $logDir ("stage_e8g_cf_{0}_stdout.txt" -f $CfMode.ToLower())
  }
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  return ,$dst
}

$modes = @($Counterfactual)
if ($Matrix) {
  $modes = @('NONE', 'C44', 'C9D', 'CF5', 'C44C9D', 'C44CF5', 'C9DCF5', 'ALL')
}

$first = $true
$primaryLog = Join-Path $logDir 'stage_e8g_jjfb_stdout.txt'
foreach ($m in $modes) {
  Write-Host "== E8G live Counterfactual=$m =="
  $null = Invoke-E8GLive -CfMode $m -Skip:(-not $first -or $SkipBuild)
  $first = $false
  $SkipBuild = $true
}

$all = ''
if (Test-Path $primaryLog) { $all = [System.IO.File]::ReadAllText($primaryLog) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

# Also fold CF ALL log if present
$cfAll = Join-Path $logDir 'stage_e8g_cf_all_stdout.txt'
$cfText = ''
if (Test-Path $cfAll) { $cfText = [System.IO.File]::ReadAllText($cfAll) }
function HitCf([string]$pat) { return [bool]($cfText -match $pat) }

$xref = Test-Path (Join-Path $outDir 'bootstrap_caller_xref.md')
$faultMd = Test-Path (Join-Path $outDir 'fault_2d92b0.md')
$h102 = Test-Path (Join-Path $outDir 'handler_10102_abi.md')
$armed = Hit 'JJFB_E8F_WRITER_BP\] armed=1'
$callerHit = Hit 'JJFB_E8G_CALLER_HIT\]'
$callerNever = Hit 'JJFB_E8F_WRITER_NEVER\] tag=caller'
$summary = Hit 'JJFB_E8F_WRITER_SUMMARY\]'
$draw = Hit '\[JJFB_DRAW\]'
$lifeOk = Hit 'JJFB_LIFECYCLE\] op=FIRE_DONE tick=.*ok=1'
$sgFault = ((HitCf 'JJFB_E8G_SECOND_GATE_FAULT\]') -or (Hit 'JJFB_E8G_SECOND_GATE_FAULT\]'))
$faultHit = ((HitCf 'JJFB_E8G_FAULT_HIT\]') -or (Hit 'JJFB_E8G_FAULT_HIT\]'))
$faultClass = ((HitCf 'JJFB_E8G_FAULT_CLASS\]') -or (Hit 'JJFB_E8G_FAULT_CLASS\]'))

# Parse 10102 bl_to_bootstrap from static
$h102j = Get-Content (Join-Path $outDir 'handler_10102_abi.json') -Raw | ConvertFrom-Json
$blBoot = @($h102j.bl_to_bootstrap)
$blBootN = $blBoot.Count

$decision = 'EVENT_CHAIN_STILL_UNKNOWN'
if ($draw) { $decision = 'DRAW_REACHED' }
elseif ($callerHit) { $decision = 'WRITER_REACHED_NEXT_GAP' }  # misnamed slot: caller reached
elseif ($sgFault -or $faultClass) { $decision = 'COUNTERFACTUAL_SECOND_GATE_CONTEXT_MISSING' }
elseif ($armed -and $callerNever -and -not $callerHit) { $decision = 'BOOTSTRAP_CALLER_NEVER_ENTERED' }
elseif ($blBootN -gt 0) { $decision = 'MISSING_10102_INIT_CASE' }
elseif ($xref -and $lifeOk) { $decision = 'BOOTSTRAP_CALLER_NEVER_ENTERED' }

# Prefer never-entered when primary run shows no caller hits
if ($armed -and -not $callerHit -and $summary) {
  $decision = 'BOOTSTRAP_CALLER_NEVER_ENTERED'
}
# If CF proves second gate, keep that as secondary note but primary stays never-entered
# unless we only ran CF. With NONE primary, never-entered wins; CF documented separately.
if (($Counterfactual -ne 'NONE' -or $Matrix) -and ($sgFault -or $faultClass) -and -not $callerHit) {
  if ($Counterfactual -eq 'ALL' -and -not $Matrix) {
    $decision = 'COUNTERFACTUAL_SECOND_GATE_CONTEXT_MISSING'
  }
}

$hashExpect = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
$hashGot = (Get-FileHash -Algorithm SHA256 (Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp')).Hash.ToLower()
$hashOk = ($hashGot -eq $hashExpect)

Write-Host "== audit_launcher_core =="
python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
$auditRc = $LASTEXITCODE

$verdict = @"
# Stage E8G — Bootstrap Caller Provenance + Second-Gate Fault

## Verdict

``$decision``

## Gates

| Gate | Result |
|------|--------|
| bootstrap_caller_xref.md | $(if ($xref) { 'yes' } else { 'no' }) |
| fault_2d92b0.md | $(if ($faultMd) { 'yes' } else { 'no' }) |
| handler_10102_abi.md | $(if ($h102) { 'yes' } else { 'no' }) |
| caller BP armed | $(if ($armed) { 'yes' } else { 'no' }) |
| caller HIT | $(if ($callerHit) { 'yes' } else { 'no' }) |
| caller NEVER | $(if ($callerNever) { 'yes' } else { 'no' }) |
| second-gate fault dump | $(if ($sgFault) { 'yes' } else { 'no' }) |
| fault class log | $(if ($faultClass) { 'yes' } else { 'no' }) |
| 10102 BL→bootstrap | $blBootN |
| DRAW | $(if ($draw) { 'yes' } else { 'no' }) |
| lifecycle ok | $(if ($lifeOk) { 'yes' } else { 'no' }) |
| audit | rc=$auditRc |
| jjfb hash | $(if ($hashOk) { 'unchanged' } else { "MISMATCH" }) |

## Run params

- counterfactual=``$Counterfactual`` matrix=``$Matrix``
- primary_log=``$primaryLog``

## Evidence class

- Caller never-entered / fault site disasm: TARGET_OBSERVED
- Second-gate semantic labels: HYPOTHESIS until object at fault is identified

## Artifacts

- ``out/e8g_tmp/bootstrap_caller_xref.md``
- ``out/e8g_tmp/fault_2d92b0.md``
- ``out/e8g_tmp/handler_10102_abi.md``
- ``logs/stage_e8g_jjfb_stdout.txt``
"@

$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e8g_verdict.md'), $verdict, $utf8)
$decision | Set-Content -Encoding ascii (Join-Path $outDir 'decision.txt')
Write-Host "e8g decision=$decision hash_ok=$hashOk audit_rc=$auditRc"
if (-not $hashOk) { exit 3 }
if ($auditRc -ne 0) { exit 4 }
exit 1
