# Stage E8C: robotol idle state-machine exit — flag watch (no 0x1E209 ret mutation)
param(
  [int]$Seconds = 75,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8c_tmp'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$rob = $null
@(
  'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext',
  'out\e8c_tmp\jjfb_ext\robotol.ext',
  'out\e8b_tmp\jjfb_ext\robotol.ext'
) | ForEach-Object {
  $p = Join-Path $Root $_
  if (-not $rob -and (Test-Path $p)) { $rob = $p }
}
if (-not $rob) {
  python (Join-Path $Root 'tools\mrp_inspect.py') `
    (Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp') `
    --extract (Join-Path $outDir 'jjfb_ext') | Out-Null
  $rob = Join-Path $outDir 'jjfb_ext\robotol.ext'
}
if (-not (Test-Path $rob)) { throw "missing robotol.ext" }

Write-Host "== E8C flag resolve =="
python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir $outDir
if ($LASTEXITCODE -ne 0) { throw 'flag resolve failed' }
python (Join-Path $Root 'tools\e8c_flag_write_xref.py') --ext $rob --flag-map (Join-Path $outDir 'flag_map.json') `
  -o (Join-Path $outDir 'flag_write_xref.md')
if ($LASTEXITCODE -ne 0) { throw 'flag xref failed' }

$map = Get-Content (Join-Path $outDir 'flag_map.json') -Raw | ConvertFrom-Json
$offsets = $map.watch_offsets_csv
if (-not $offsets) { throw 'empty watch_offsets_csv' }
Write-Host "JJFB_E8C_WATCH_OFFSETS=$offsets"

$env:JJFB_E8C_MODE = '1'
$env:JJFB_E8C_IDLE_WATCH = '1'
$env:JJFB_E8C_WATCH_OFFSETS = $offsets
Remove-Item Env:JJFB_E8C_WATCH_ADDRS -ErrorAction SilentlyContinue

$env:JJFB_PLAT_CENSUS = '1'
$env:JJFB_PLAT_1E209_TRACE = '1'
$env:JJFB_VFS_RES_ALIAS = '0'
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
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'

Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E5_SCHEDULER_MODE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8B_MODE -ErrorAction SilentlyContinue
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
$src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
$dst = Join-Path $logDir 'stage_e8c_jjfb_stdout.txt'
if (Test-Path $src) { Copy-Item -Force $src $dst }

$all = ''
if (Test-Path $dst) { $all = [System.IO.File]::ReadAllText($dst) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$lifeOk = Hit 'JJFB_LIFECYCLE\] op=FIRE_DONE tick=.*ok=1'
$armed = Hit 'JJFB_E8C_IDLE_WATCH\] armed=1'
$snap = Hit 'JJFB_E8C_FLAG_SNAP'
$trans = Hit 'JJFB_E8C_FLAG_TRANSITION'
$fx = Hit 'JJFB_E8C_HELPER_FX'
$draw = Hit '\[JJFB_DRAW\]'
$net = Hit 'network|connect|http|socket|update_check'
$newPlat = Hit 'JJFB_PLAT_CENSUS\] code=0x(?!1E209|1 |10113|10102|10120|10140|10162|10165|10800)'
$tick600 = Hit 'FIRE_DONE tick=600\b'
$unchanged = $armed -and $snap -and -not $trans

$decision = 'UNKNOWN'
if ($draw) { $decision = 'DRAW_REACHED' }
elseif ($trans) { $decision = 'WAITING_FOR_EVENT' }  # narrowed further in report if needed
elseif ($net -and $lifeOk) { $decision = 'WAITING_FOR_NETWORK' }
elseif ($fx -and $unchanged -and (Hit 'HELPER_FX\][^\r\n]*watched_writes=0')) {
  $decision = 'PLATFORM_SIDE_EFFECT_MISSING'
}
elseif ($unchanged -and $lifeOk) { $decision = 'ROBOTOL_STATE_FLAG_NEVER_SET' }
elseif ($lifeOk) { $decision = 'WAITING_FOR_EVENT' }
else { $decision = 'BOOT_REGRESS' }

# Prefer NEVER_SET when flags stay 0 across many ticks
if ($unchanged -and $lifeOk -and -not $draw -and -not $trans) {
  $decision = 'ROBOTOL_STATE_FLAG_NEVER_SET'
}

@"
# Stage E8C — idle state exit

| Gate | Result |
|------|--------|
| target | $Target |
| flag_map | ``out/e8c_tmp/flag_map.json`` |
| watch_offsets | $offsets |
| idle watch armed | $(if ($armed) { 'yes' } else { 'no' }) |
| flag snap | $(if ($snap) { 'yes' } else { 'no' }) |
| flag transition | $(if ($trans) { 'yes' } else { 'no' }) |
| helper fx | $(if ($fx) { 'yes' } else { 'no' }) |
| lifecycle ok=1 | $(if ($lifeOk) { 'yes' } else { 'no' }) |
| DRAW | $(if ($draw) { 'yes' } else { 'no' }) |
| decision | $decision |

See ``out/e8c_tmp/flag_write_xref.md`` and consolidated ``reports/stage_e8c_verdict.md``.
Log: ``$dst``
"@ | Set-Content -Encoding utf8 (Join-Path $reportDir 'stage_e8c_jjfb_gate.md')

Write-Host "e8c decision=$decision log=$dst rc=$rc"
# Stash decision for verdict writer
$decision | Set-Content -Encoding utf8 (Join-Path $outDir 'decision.txt')
if ($draw) { exit 0 }
if ($lifeOk -and $armed) { exit 1 }
exit 2
