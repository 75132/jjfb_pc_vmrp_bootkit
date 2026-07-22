# Stage E8D: flag source / 10165 enqueue provenance (no flag force, no 1E209 ret mutation)
param(
  [int]$Seconds = 80,
  [string]$Target = 'gwy/jjfb.mrp',
  [string]$Candidate = 'ZERO_ARGS',
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

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8d_tmp'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$rob = $null
@(
  'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext',
  'out\e8c_tmp\jjfb_ext\robotol.ext',
  'out\e8d_tmp\jjfb_ext\robotol.ext'
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

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}

Write-Host "== E8D write xref v2 =="
python (Join-Path $Root 'tools\e8d_flag_write_xref.py') --ext $rob --flag-map $flagMap `
  -o (Join-Path $outDir 'flag_write_xref_v2.md') `
  --class-out (Join-Path $outDir 'writer_class.md')

# Disasm live 10165 handler from prior logs / known registration VA 0x30D2F9
Write-Host "== E8D 10165 disasm =="
python (Join-Path $Root 'tools\handler_dual_disasm.py') --ext $rob --code-base 0x2D8DF4 `
  --handler 0x30D2F8 --before 0x20 --after 0x100 -o (Join-Path $outDir 'handler_10165_disasm.txt')

$map = Get-Content $flagMap -Raw | ConvertFrom-Json
$offsets = $map.watch_offsets_csv
Write-Host "JJFB_E8C_WATCH_OFFSETS=$offsets"

$env:JJFB_E8D_MODE = '1'
$env:JJFB_E8C_IDLE_WATCH = '1'
$env:JJFB_E8C_WATCH_OFFSETS = $offsets
$env:JJFB_E8D_EARLY_WATCH = '1'
$env:JJFB_E8D_ERW_DIFF = '1'
$env:JJFB_E8D_10165_PROBE = '1'
$env:JJFB_E8D_10165_CANDIDATE = $Candidate

$env:JJFB_PLAT_CENSUS = '1'
$env:JJFB_PLAT_1E209_TRACE = '0'
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
$env:JJFB_MODULE_REGISTRY_TRACE = '1'
$env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
$env:JJFB_MRC_INIT_TRACE = '1'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_PLAT_RET0_TRACE = '1'
$env:JJFB_LIFECYCLE_EVENT_TRACE = '1'
$env:JJFB_E7_LIFECYCLE_MODE = '1'
$env:JJFB_POST_START_SCHEDULER_TRACE = '1'
$env:JJFB_GAME_SELF_PATCH = '0'
$env:GWY_MODULE_R9_SWITCH = '1'
$env:GWY_CALLBACK_FRAME = '1'
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'

Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8B_MODE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_E8C_MODE -ErrorAction SilentlyContinue
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
$dst = Join-Path $logDir 'stage_e8d_jjfb_stdout.txt'
if (Test-Path $src) { Copy-Item -Force $src $dst }

$all = ''
if (Test-Path $dst) { $all = [System.IO.File]::ReadAllText($dst) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$lifeOk = Hit 'JJFB_LIFECYCLE\] op=FIRE_DONE tick=.*ok=1'
$armed = Hit 'JJFB_E8C_IDLE_WATCH\] armed=1'
$trans = Hit 'JJFB_E8C_FLAG_TRANSITION'
$fire65 = Hit 'JJFB_E8D_10165_FIRE'
$fire65Done = Hit 'JJFB_E8D_10165_FIRE_DONE'
$map65 = Hit 'JJFB_E8D_HANDLER_MAP\] code=0x10165'
$diff = Hit 'JJFB_E8D_ERW_DIFF'
$draw = Hit '\[JJFB_DRAW\]'
$flagsAfterProbe = Hit 'JJFB_E8D_ERW_FLAGS\] stage=after_10165_probe[^\r\n]*0xC44=0x1|0xC9D=0x1|0xCF5=0x1'
$probeNoWrite = $fire65Done -and -not $trans -and -not $flagsAfterProbe

$decision = 'UNKNOWN'
if ($draw) { $decision = 'DRAW_REACHED' }
elseif ($trans -or $flagsAfterProbe) {
  if ($fire65) { $decision = 'MISSING_10165_EVENT_DELIVERY' }  # probe proved path can write — wait, if probe caused write it's FLAG_WRITER_REACHED
  else { $decision = 'FLAG_WRITER_REACHED_NEXT_GAP' }
}
elseif ($fire65 -and $probeNoWrite -and $map65) {
  # 10165 registered + fired ZERO_ARGS but flags still 0 → missing real event payload / ABI
  $decision = 'MISSING_10165_EVENT_DELIVERY'
}
elseif ($diff -and (Hit 'JJFB_E8D_ERW_FLAGS\] stage=after_mrc_init[^\r\n]*0xC9D=0x0') -and $lifeOk) {
  $decision = 'MISSING_LOADER_INIT_SIDE_EFFECT'
}
elseif ($lifeOk) { $decision = 'MISSING_PLATFORM_SIDE_EFFECT' }
else { $decision = 'BOOT_REGRESS' }

# Prefer 10165 missing when map+fire+no flag change (primary hypothesis)
if ($map65 -and $fire65Done -and -not $trans -and -not $draw) {
  $decision = 'MISSING_10165_EVENT_DELIVERY'
}

@"
# Stage E8D — flag source / event delivery

| Gate | Result |
|------|--------|
| candidate | $Candidate |
| armed | $(if ($armed) { 'yes' } else { 'no' }) |
| erw diff | $(if ($diff) { 'yes' } else { 'no' }) |
| handler map 10165 | $(if ($map65) { 'yes' } else { 'no' }) |
| 10165 fire | $(if ($fire65) { 'yes' } else { 'no' }) |
| flag transition | $(if ($trans) { 'yes' } else { 'no' }) |
| DRAW | $(if ($draw) { 'yes' } else { 'no' }) |
| decision | $decision |

Artifacts: ``out/e8d_tmp/`` Log: ``$dst``
"@ | Set-Content -Encoding utf8 (Join-Path $reportDir 'stage_e8d_jjfb_gate.md')

$decision | Set-Content -Encoding utf8 (Join-Path $outDir 'decision.txt')
Write-Host "e8d decision=$decision log=$dst rc=$rc"
if ($draw) { exit 0 }
if ($lifeOk) { exit 1 }
exit 2
