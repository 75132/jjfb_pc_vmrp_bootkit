# Stage E6: PLATFORM_RET0_CAUSE — why mrc_init ret0=-1 under DOCUMENTED 6/8/0
param(
  [int]$Seconds = 60,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
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
$dst = Join-Path $logDir 'stage_e6_jjfb_plat_ret0_stdout.txt'
if (Test-Path $src) { Copy-Item -Force $src $dst }

$all = ''
if (Test-Path $dst) { $all = [System.IO.File]::ReadAllText($dst) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$mrc = Hit '\[JJFB_MRC_INIT\]'
$ret0ok = Hit 'JJFB_INIT_SEQ\] delivered.*ret0=0\b'
$plat10113 = Hit 'JJFB_PLAT_CALL.*0x10113.*GRAPHICS_FP'
$plat10102 = Hit 'JJFB_PLAT_CALL.*0x10102.*REGISTER'
$plat10120 = Hit 'JJFB_PLAT_CALL.*0x10120.*REGISTER'
$wrote = Hit 'JJFB_PLAT_CALL.*wrote=1.*GRAPHICS_FP'
$plat10140 = Hit 'JJFB_PLAT_CALL.*0x10140.*REGISTER'
$draw = Hit '\[JJFB_DRAW\]|\[JJFB_REFRESH\]'

$decision = 'UNKNOWN'
if ($ret0ok -and $draw) { $decision = 'RESOURCE_DRAW_CHAIN' }
elseif ($ret0ok) { $decision = 'LIFECYCLE_EVENT_REQUIRED' }
elseif ($plat10113 -and -not $wrote) { $decision = 'GRAPHICS_FP_WIRE' }
elseif ($mrc -and -not $ret0ok) { $decision = 'PLATFORM_RET0_CAUSE' }
else { $decision = 'BOOT_REGRESS' }

@"
# Stage E6 — platform ret0 cause

| Gate | Result |
|------|--------|
| MRC_INIT | $(if ($mrc) { 'yes' } else { 'no' }) |
| ret0=0 | $(if ($ret0ok) { 'yes' } else { 'no' }) |
| PLAT 10113 GRAPHICS_FP | $(if ($plat10113) { 'yes' } else { 'no' }) |
| PLAT 10113 wrote=1 | $(if ($wrote) { 'yes' } else { 'no' }) |
| PLAT 10102 REGISTER | $(if ($plat10102) { 'yes' } else { 'no' }) |
| PLAT 10120 REGISTER | $(if ($plat10120) { 'yes' } else { 'no' }) |
| PLAT 10140 REGISTER | $(if ($plat10140) { 'yes' } else { 'no' }) |
| DRAW/REFRESH | $(if ($draw) { 'yes' } else { 'no' }) |
| decision | $decision |

Log: `$dst`
"@ | Set-Content -Encoding utf8 (Join-Path $reportDir 'stage_e6_verdict.md')

Write-Host "e6 decision=$decision ret0ok=$ret0ok wrote=$wrote"
if ($ret0ok) { exit 0 }
exit 2
