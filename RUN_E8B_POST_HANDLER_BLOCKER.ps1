# Stage E8B: handler returned — find real post-lifecycle blocker (no Thumb/ABI mutation)
param(
  # Wall clock must cover ~33s boot to mrc_init + post-handler ticks (plan: >=20s observe).
  [int]$Seconds = 50,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$env:JJFB_E8B_MODE = '1'
$env:JJFB_PLAT_CENSUS = '1'
$env:JJFB_PLAT_1E209_TRACE = '1'
$env:JJFB_VFS_RES_ALIAS = '1'
$env:JJFB_HANDLER_FORENSIC = '0'
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
$env:GWY_MODULE_R9_SWITCH = '1'
$env:GWY_CALLBACK_FRAME = '1'

$isWx = ($Target -match 'wxjwq')
if ($isWx) {
  $env:GWY_PACKAGE_APPID = '403095'
  $env:GWY_PACKAGE_APPVER = '1118'
} else {
  $env:GWY_PACKAGE_APPID = '400101'
  $env:GWY_PACKAGE_APPVER = '12'
}

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
New-Item -ItemType Directory -Force -Path $logDir, $reportDir | Out-Null
$src = if ($isWx) {
  Join-Path $logDir 'stage_e_wxjwq_control_stdout.txt'
} else {
  Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
}
$tag = if ($isWx) { 'wxjwq' } else { 'jjfb' }
$dst = Join-Path $logDir "stage_e8b_${tag}_stdout.txt"
if (Test-Path $src) { Copy-Item -Force $src $dst }

$all = ''
if (Test-Path $dst) { $all = [System.IO.File]::ReadAllText($dst) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$ownerOk = if ($isWx) {
  (Hit 'JJFB_PACKAGE_SCOPE\] package=gwy/wxjwq\.mrp primary=mmochat\.ext') -or
  (Hit 'JJFB_CLOAD_SCOPE\] request=cfunction\.ext package=gwy/wxjwq\.mrp resolved=mmochat\.ext') -or
  (Hit 'owner=mmochat\.ext')
} else {
  (Hit 'owner=robotol\.ext') -or
  (Hit 'JJFB_PACKAGE_SCOPE\] package=gwy/jjfb\.mrp primary=robotol\.ext')
}
$targetOk = if ($isWx) { Hit 'gwy/wxjwq\.mrp' } else { Hit 'gwy/jjfb\.mrp' }
# Contaminated only if wxjwq run actually scoped/cload'd jjfb primary, not merely because
# DSM identity lines mention jjfb.mrp or both packages appear in the module registry.
$contaminated = $isWx -and (
  (-not $ownerOk) -or
  (Hit 'JJFB_PACKAGE_SCOPE\] package=gwy/jjfb\.mrp') -or
  (Hit 'JJFB_CLOAD_SCOPE\][^\r\n]*resolved=robotol\.ext') -or
  (Hit 'JJFB_ROBOTOL_ENTRY_CALLED')
)
$lifeOk = Hit 'JJFB_LIFECYCLE\] op=FIRE_DONE tick=.*ok=1'
$census = Hit 'JJFB_PLAT_CENSUS'
$t1e209 = Hit 'JJFB_PLAT_1E209'
$draw = Hit '\[JJFB_DRAW\]'
$vfsAlias = Hit 'JJFB_VFS_ALIAS'
$fileMiss = Hit 'JJFB_FILEOPEN_MISS|JJFB_PLAT_CENSUS_MISS'

$decision = 'UNKNOWN'
if ($contaminated) { $decision = 'CONTAMINATED_WXJWQ' }
elseif ($draw) { $decision = 'DRAW_SEEN' }
elseif ($fileMiss -and -not $lifeOk) { $decision = 'RESOURCE_OPEN_MISS' }
elseif ($t1e209 -and $lifeOk -and -not $draw) {
  # Caller @0x30666A does BL then unconditional B (ret unused) — idle wait, not ret-gated.
  $decision = 'NO_DRAW_NO_NEW_PLAT'
}
elseif ($census -and $lifeOk -and -not $draw) { $decision = 'NO_DRAW_NO_NEW_PLAT' }
elseif ($lifeOk) { $decision = 'HANDLER_RETURNED_NEXT_PLATFORM_GAP' }
else { $decision = 'BOOT_REGRESS' }

@"
# Stage E8B — post-handler blocker

| Gate | Result |
|------|--------|
| target | $Target |
| owner_ok | $(if ($ownerOk) { 'yes' } else { 'no' }) |
| target_ok | $(if ($targetOk) { 'yes' } else { 'no' }) |
| contaminated | $(if ($contaminated) { 'YES' } else { 'no' }) |
| lifecycle ok=1 | $(if ($lifeOk) { 'yes' } else { 'no' }) |
| plat census | $(if ($census) { 'yes' } else { 'no' }) |
| 1E209 trace | $(if ($t1e209) { 'yes' } else { 'no' }) |
| VFS alias hit | $(if ($vfsAlias) { 'yes' } else { 'no' }) |
| FILEOPEN miss | $(if ($fileMiss) { 'yes' } else { 'no' }) |
| DRAW | $(if ($draw) { 'yes' } else { 'no' }) |
| decision | $decision |

Log: ``$dst``
"@ | Set-Content -Encoding utf8 (Join-Path $reportDir "stage_e8b_${tag}_verdict.md")

Write-Host "e8b decision=$decision log=$dst rc=$rc contaminated=$contaminated"
if ($contaminated) { exit 3 }
if ($draw) { exit 0 }
if ($lifeOk) { exit 1 }
exit 2
