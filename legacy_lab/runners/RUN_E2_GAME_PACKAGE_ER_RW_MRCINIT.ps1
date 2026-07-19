# Stage E2: game_package ER_RW/R9 → robotol/mmochat mrc_init
# Builds on Stage E product track; adds FixR9 C_FUNCTION_P image+4 publish + bind timeline.
param(
  [int]$Seconds = 90,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..')).Path
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
$env:JJFB_LIFECYCLE_EVENT_TRACE = '1'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_GAME_SELF_PATCH = '0'
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'

@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_RUNAPP_NATIVE_ONLY'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

$isJjfb = ($env:JJFB_PRIMARY_TARGET -match 'jjfb\.mrp$')
$tag = if ($isJjfb) { 'stage_e2_jjfb' } else { 'stage_e2_wxjwq' }

# Reuse Stage E runner core via direct invoke with same target/seconds after env set.
$eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
  (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
  '-Target', $Target, '-Seconds', "$Seconds")
if ($SkipBuild) { $eArgs += '-SkipBuild' }
& powershell @eArgs
$rc = $LASTEXITCODE

# Rename/copy logs to E2 names
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$srcLog = if ($isJjfb) {
  Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
} else {
  Join-Path $logDir 'stage_e_wxjwq_control_stdout.txt'
}
$dstLog = Join-Path $logDir "${tag}_game_package_er_rw_stdout.txt"
if (Test-Path $srcLog) { Copy-Item -Force $srcLog $dstLog }

$all = ''
if (Test-Path $dstLog) {
  $all = [System.IO.File]::ReadAllText($dstLog)
}

function Hit([string]$pat) { return [bool]($all -match $pat) }

$r9ok = Hit 'JJFB_R9_SWITCH_OK.*robotol\.ext|JJFB_R9_SWITCH_OK.*mmochat\.ext|JJFB_R9_SWITCH_OK.*package=gwy/'
$mrc = Hit '\[JJFB_MRC_INIT\]'
$ret0ok = Hit 'JJFB_INIT_SEQ\] delivered.*ret0=0\b'
$draw = Hit '\[JJFB_DRAW\]|\[JJFB_REFRESH\]'
$attempt = Hit 'JJFB_MRC_INIT_ATTEMPT'
$pSlot = Hit 'JJFB_CFN_P_SLOT'
$erBind = Hit 'JJFB_ER_RW_BIND.*robotol|JJFB_ER_RW_BIND.*mmochat|JJFB_ER_RW_BIND.*mrc_loader'
$pFill = Hit 'JJFB_GAME_P_TIMELINE.*ER_RW_BOUND|JJFB_ER_RW_BIND.*p_base=0x(?!0+\b)'
$missing = Hit 'JJFB_GAME_ER_RW_SOURCE_MISSING'

$verdict = Join-Path $reportDir 'stage_e2_verdict.md'
$timeline = Join-Path $reportDir 'stage_e2_game_package_er_rw_timeline.md'
$ctrl = Join-Path $reportDir 'stage_e2_wxjwq_control.md'

$lines = @()
if (Test-Path $dstLog) {
  $lines = Select-String -Path $dstLog -Pattern 'JJFB_CFN_P_SLOT|JJFB_GAME_P_TIMELINE|JJFB_ER_RW_BIND|JJFB_R9_SWITCH|JJFB_MRC_INIT|JJFB_GAME_ER_RW|JJFB_ROBOTOL_ENTRY|JJFB_HELPER_RETARGET|JJFB_INIT_SEQ' -ErrorAction SilentlyContinue |
    ForEach-Object { $_.Line } | Select-Object -First 60
}

@"
# Stage E2 — game_package ER_RW / R9 / mrc_init

- **target:** ``$($env:JJFB_PRIMARY_TARGET)``
- **source:** ``descriptor_launcher``
- **seconds:** $Seconds
- **runner_exit:** $rc

## Gates

| Gate | Result |
|------|--------|
| CFN P slot image+4 | $(if ($pSlot) { 'yes' } else { 'no' }) |
| ER_RW_BIND (game module) | $(if ($erBind) { 'yes' } else { 'no' }) |
| P+0/+4 bound timeline | $(if ($pFill) { 'yes' } else { 'no' }) |
| E2-min R9_SWITCH_OK | $(if ($r9ok) { 'yes' } else { 'no' }) |
| E2-mid JJFB_MRC_INIT | $(if ($mrc) { 'yes' } else { 'no' }) |
| E4 ret0=0 | $(if ($ret0ok) { 'yes' } else { 'no' }) |
| E4 DRAW/REFRESH | $(if ($draw) { 'yes' } else { 'no' }) |
| MRC_INIT_ATTEMPT seen | $(if ($attempt) { 'yes' } else { 'no' }) |
| ER_RW source missing | $(if ($missing) { 'yes' } else { 'no' }) |

## Log

- ``$dstLog``
"@ | Set-Content -Path $(if ($isJjfb) { $verdict } else { $ctrl }) -Encoding utf8

@"
# Stage E2 — game_package ER_RW timeline samples

``````
$($lines -join "`n")
``````
"@ | Set-Content -Path $timeline -Encoding utf8

Write-Host "e2_verdict=$(if ($isJjfb) { $verdict } else { $ctrl })"
Write-Host "e2_log=$dstLog r9ok=$r9ok mrc_init=$mrc ret0ok=$ret0ok draw=$draw p_slot=$pSlot er_bind=$erBind"

if (-not $r9ok) { exit 2 }
if (-not $mrc) { Write-Host '[PARTIAL] E2-min R9_OK; MRC_INIT still open'; exit 3 }
if (-not $ret0ok) { Write-Host '[PARTIAL] E3 MRC_INIT; E4 ret0!=0'; exit 4 }
if (-not $draw) { Write-Host '[PARTIAL] E4 ret0=0; DRAW/REFRESH still open'; exit 5 }
Write-Host '[OK] Stage E4: ret0=0 + DRAW/REFRESH'
exit 0
