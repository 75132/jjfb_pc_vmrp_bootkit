# Stage E7: LIFECYCLE → natural DRAW (registered 10140 + host 50ms tick)
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
$env:JJFB_GAME_P_TIMELINE_TRACE = '1'
$env:JJFB_MODULE_REGISTRY_TRACE = '1'
$env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
$env:JJFB_MRC_INIT_TRACE = '1'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_PLAT_RET0_TRACE = '1'
$env:JJFB_LIFECYCLE_EVENT_TRACE = '1'
$env:JJFB_E7_LIFECYCLE_MODE = '1'
$env:JJFB_POST_START_SCHEDULER_TRACE = '1'
$env:JJFB_TIMER_POLL_TRACE = '1'
$env:JJFB_GAME_SELF_PATCH = '0'
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'
$env:GWY_MODULE_R9_SWITCH = '1'
$env:GWY_CALLBACK_FRAME = '1'
Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E5_SCHEDULER_MODE -ErrorAction SilentlyContinue

@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_RUNAPP_NATIVE_ONLY'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

$eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
  (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
  '-Target', $Target, '-Seconds', "$Seconds")
if ($SkipBuild) { $eArgs += '-SkipBuild' }
& powershell @eArgs
$rc = $LASTEXITCODE

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
$dst = Join-Path $logDir 'stage_e7_jjfb_lifecycle_stdout.txt'
if (Test-Path $src) { Copy-Item -Force $src $dst }

$all = ''
if (Test-Path $dst) { $all = [System.IO.File]::ReadAllText($dst) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$ret0ok = Hit 'JJFB_INIT_SEQ\] delivered.*ret0=0\b'
$plat10140 = Hit 'JJFB_PLAT_CALL.*0x10140.*REGISTER'
$lifeArm = Hit 'JJFB_LIFECYCLE\] op=ARM'
$lifeFire = Hit 'JJFB_LIFECYCLE\] op=FIRE tick='
$draw = Hit '\[JJFB_DRAW\]|\[JJFB_REFRESH\]'
$ack10800 = Hit 'JJFB_PLAT_CALL.*0x10800.*ret=1'

$decision = 'UNKNOWN'
if ($ret0ok -and $draw) { $decision = 'PASS_DRAW' }
elseif ($ret0ok -and $lifeFire -and -not $draw) { $decision = 'PARTIAL_TICK_NO_DRAW' }
elseif ($ret0ok -and $lifeArm -and -not $lifeFire) { $decision = 'TICK_ARM_NO_FIRE' }
elseif ($ret0ok -and -not $lifeArm) { $decision = 'NO_LIFECYCLE_ARM' }
elseif (-not $ret0ok) { $decision = 'RET0_REGRESS' }
else { $decision = 'BOOT_REGRESS' }

@"
# Stage E7 — lifecycle → DRAW

| Gate | Result |
|------|--------|
| ret0=0 | $(if ($ret0ok) { 'yes' } else { 'no' }) |
| PLAT 10140 REGISTER | $(if ($plat10140) { 'yes' } else { 'no' }) |
| 10800 ack=1 | $(if ($ack10800) { 'yes' } else { 'no' }) |
| LIFECYCLE ARM | $(if ($lifeArm) { 'yes' } else { 'no' }) |
| LIFECYCLE FIRE | $(if ($lifeFire) { 'yes' } else { 'no' }) |
| DRAW/REFRESH | $(if ($draw) { 'yes' } else { 'no' }) |
| decision | $decision |

Log: `$dst`
"@ | Set-Content -Encoding utf8 (Join-Path $reportDir 'stage_e7_verdict.md')

Write-Host "e7 decision=$decision ret0ok=$ret0ok lifeFire=$lifeFire draw=$draw"
if ($draw) { exit 0 }
if ($ret0ok -and $lifeFire) { exit 1 }
exit 2
