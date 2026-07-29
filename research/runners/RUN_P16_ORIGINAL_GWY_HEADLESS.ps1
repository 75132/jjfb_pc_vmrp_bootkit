# P16–P18: Original GWY Launcher Headless Rehost A/B/C matrix
param(
  [switch]$SkipBuild,
  [int]$DirectHold = 180,
  [int]$DirectReps = 2,
  [int]$HeadlessHold = 300,
  [int]$HeadlessReps = 2,
  [int]$StartGameHold = 300,
  [int]$StartGameReps = 2,
  [switch]$Quick
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) { $Root = Split-Path -Parent $Root }
Set-Location $Root
. (Join-Path $Root 'tools\JjfbLayer1Gate.ps1')
$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path
$Reports = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResearchDir = Join-Path $Root 'out\vmrp_research'
$MainExe = Join-Path $RunDir 'main.exe'
$ArchiveRoot = Join-Path $Root 'out\p16_original_gwy'
$MatrixCsv = Join-Path $Reports 'ORIGINAL_GWY_AB_MATRIX.csv'
$Identity = Join-Path $Reports 'P15_BUILD_IDENTITY.txt'
$ApiCsv = Join-Path $Reports 'ORIGINAL_GWY_API_MAP.csv'
$StackJson = Join-Path $Reports 'ORIGINAL_GWY_RUNTIME_STACK.json'
$Verdict = Join-Path $Reports 'ORIGINAL_GWY_BOOTSTRAP_REHOST.md'
$CatalogJson = Join-Path $Reports 'ORIGINAL_GWY_BOOTSTRAP_CATALOG.json'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot, (Join-Path $ArchiveRoot 'frames') | Out-Null

if ($Quick) {
  $DirectHold = 45; $DirectReps = 1
  $HeadlessHold = 60; $HeadlessReps = 1
  $StartGameHold = 60; $StartGameReps = 1
}

function Get-Sha256([string]$Path) {
  if (-not (Test-Path $Path)) { return '' }
  (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

function Clear-ProductEnv {
  @(
    'JJFB_BOOTSTRAP_MODE','JJFB_LAUNCH_PATH','JJFB_SHELL_CHAIN_MODE','JJFB_GWY_LAUNCHER_MODE',
    'JJFB_NATIVE_BOOT_FULL','JJFB_MEMBER_VIEW_PRIMARY','JJFB_EXTCHUNK_PROVIDER',
    'JJFB_ER_RW_BIND_RESTORE','JJFB_PACKAGE_SCOPED_CLOAD','JJFB_PRODUCT_DESCRIPTOR_DIRECT',
    'JJFB_ORIGINAL_API_MAP_CSV','JJFB_ORIGINAL_RUNTIME_STACK_JSON','JJFB_ORIGINAL_LOAD_GAMELIST',
    'JJFB_101AB_PROVIDER','JJFB_101AB_TRACE','JJFB_101AB_TRACE_DIR','JJFB_TEXT_PARAM0_XY'
  ) | ForEach-Object { Remove-Item "Env:$_" -ErrorAction SilentlyContinue }
}

if (-not $SkipBuild) {
  Write-Host '== Full build (launcher + Gwy product + GwyResearch) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP Gwy failed' }
  # Preserve product binary before research overwrites out\vmrp_run\main.exe
  Copy-Item -Force (Join-Path $RunDir 'main.exe') (Join-Path $RunDir 'main_product.exe')
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP GwyResearch failed' }
  Copy-Item -Force (Join-Path $ResearchDir 'main.exe') (Join-Path $RunDir 'main_research.exe')
  # Restore product main.exe for A cells
  if (Test-Path (Join-Path $RunDir 'main_product.exe')) {
    Copy-Item -Force (Join-Path $RunDir 'main_product.exe') (Join-Path $RunDir 'main.exe')
  }
}

$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$launcher = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
@(
  "clean_commit=$(git -C $Root rev-parse HEAD)"
  "main.exe=$(Get-Sha256 $MainExe)"
  "main_research.exe=$(Get-Sha256 (Join-Path $RunDir 'main_research.exe'))"
  "JJFB_Launcher.exe=$(Get-Sha256 $launcher)"
  "jjfb.mrp=$(Get-Sha256 $jjfb)"
  "recorded_utc=$((Get-Date).ToUniversalTime().ToString('o'))"
  "phase=P16_original_gwy_headless"
) | Set-Content -Path $Identity -Encoding utf8
Write-Host (Get-Content $Identity -Raw)

# Catalog
$gwy = Join-Path $Root 'build-i686\gwy_launcher.exe'
& $gwy original-catalog --root $ResourceRoot --out $CatalogJson
if ($LASTEXITCODE -ne 0) { throw 'original-catalog failed' }

$testExe = Join-Path $Root 'build-i686\test_original_gwy_bootstrap.exe'
if (Test-Path $testExe) {
  $env:GWY_FIXTURE_ROOT = $ResourceRoot
  & $testExe
  if ($LASTEXITCODE -ne 0) { throw 'test_original_gwy_bootstrap failed' }
}

function Set-CommonEnv([string]$RunId) {
  $shotDir = Join-Path $RunDir 'screenshots'
  New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
  $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
  $env:GWY_WINDOW_TITLE = 'JJFB Launcher'
  $env:JJFB_LAUNCH_SOURCE = 'jjfb_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_DRAWFP_BINDING = '1'
  $env:JJFB_PLATFORM_MRP_RESOURCE = '1'
  $env:JJFB_PLATFORM_10134_CONTRACT = '1'
  $env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'launcher_first_frame.bmp')
  $env:JJFB_PRODUCT_FFP_MODE = '1'
  $env:JJFB_PRODUCT_FFP_PHASE = 'event'
  $env:JJFB_PRODUCT_EVENT_CONTRACT = '1'
  $env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
  $env:JJFB_PATH_A_EVENT_CONTRACT = '1'
  $env:JJFB_RUNTIME_PROGRESS = '1'
  $env:JJFB_BOOT_SUCCESSOR_TRACE = '1'
  $env:JJFB_PRODUCT_TRACE_305E09 = '1'
  $env:JJFB_304BF0_RESUME_MODE = 'direct_lr'
  $env:JJFB_MAP_LOW_GUEST_MEM = '1'
  $env:JJFB_HELPER_2F68E4_TRACE = '1'
  $env:JJFB_LIFECYCLE_RECORD_TRACE = '1'
  $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
  $env:JJFB_B71_DISPATCH_TRACE = '1'
  $env:JJFB_101AB_TRACE = '1'
  $env:JJFB_101AB_PROVIDER = 'synthetic'
  $env:JJFB_101AB_TRACE_DIR = $ArchiveRoot
  $env:JJFB_ORIGINAL_API_MAP_CSV = $ApiCsv
  $env:JJFB_ORIGINAL_RUNTIME_STACK_JSON = $StackJson
  $env:GWY_RUNTIME_PROGRESS_PATH = (Join-Path $RunDir 'runtime_progress.jsonl')
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $Reports
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_SHELL_OFFLINE_NO_UPDATE = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update'
}

function Set-DirectEnv([string]$RunId) {
  Clear-ProductEnv
  Set-CommonEnv $RunId
  $env:JJFB_BOOTSTRAP_MODE = 'direct_boot'
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
}

function Set-HeadlessEnv([string]$RunId) {
  Clear-ProductEnv
  Set-CommonEnv $RunId
  $env:JJFB_BOOTSTRAP_MODE = 'original_headless'
  $env:JJFB_LAUNCH_PATH = 'gwy_original_headless'
  $env:JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
  $env:JJFB_EXTCHUNK_PROVIDER = 'gwy_shell'
  $env:JJFB_ER_RW_BIND_RESTORE = 'shell_core'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
}

function Set-StartGameEnv([string]$RunId) {
  Set-HeadlessEnv $RunId
  $env:JJFB_BOOTSTRAP_MODE = 'startgame_only'
  $env:JJFB_LAUNCH_PATH = 'gwy_original_headless'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
}

function Parse-Metrics([string]$LogPath, [string]$ShotPath) {
  $t = if (Test-Path $LogPath) { Get-Content -Raw $LogPath } else { '' }
  $bmp = ([regex]::Matches($t, '(?i)jjfbol[/\\][^\"\s]+\.bmp')).Count
  $uniqueBmp = ([regex]::Matches($t, '(?i)jjfbol[/\\][^\"\s]+\.bmp') | ForEach-Object { $_.Value.ToLowerInvariant() } | Sort-Object -Unique).Count
  $drawfp = ([regex]::Matches($t, 'DrawFP|DRAWFP|drawBitmap')).Count
  $codes = @()
  [regex]::Matches($t, 'event_code[=:]?\s*(?:0x)?([0-9A-Fa-f]+)|101AB[^\r\n]*code[=:]?\s*(\d+)') | ForEach-Object {
    $c = if ($_.Groups[1].Success) { $_.Groups[1].Value } else { $_.Groups[2].Value }
    if ($c) { $codes += $c }
  }
  $code15 = $t -match 'event_code[=:]?\s*15\b|code[=:]?\s*15\b|CODE15'
  $e6c = $t -match 'E6C|0xE6C|guest.?natural.?store'
  $apiStart = $t -match 'API_REGISTER[^\r\n]*lib\.startGame|SHELL_EXPORT[^\r\n]*lib\.startGame'
  $apiRun = $t -match 'lib\.runflashmrp|lib\.runapp'
  $stackPush = ([regex]::Matches($t, 'MRP_RUNTIME_STACK\] push')).Count
  $fault = $t -match 'UC_ERR|mem_fault|FATAL'
  $alive = $t -match 'emu_exit|mr_exit|JJFB_BOOTSTRAP\] finalize|SHELL_NATIVE_SUMMARY|GWY_SHELL_SUMMARY'
  $shotOk = (Test-Path $ShotPath) -and ((Get-Item $ShotPath).Length -gt 1000)
  [pscustomobject]@{
    resources = $uniqueBmp
    resources_raw = $bmp
    drawfp = $drawfp
    codes = ($codes | Select-Object -First 12) -join ';'
    code15 = [int]$code15
    e6c = [int]$e6c
    api_startgame = [int]$apiStart
    api_run = [int]$apiRun
    stack_depth = $stackPush
    fault = [int]$fault
    alive = [int]$alive
    shot = [int]$shotOk
  }
}

function Invoke-OneCell([string]$Group, [int]$Rep, [int]$HoldSeconds, [string]$Mode, [string]$ExeName) {
  $ts = Get-Date -Format 'yyyyMMdd_HHmmss'
  $runId = "p16_${Group}_r${Rep}_$ts"
  $cellDir = Join-Path $ArchiveRoot "${Group}_r${Rep}_$ts"
  New-Item -ItemType Directory -Force -Path $cellDir | Out-Null
  $vmLog = Join-Path $cellDir 'vm_stdout.txt'
  $vmErr = Join-Path $cellDir 'vm_stderr.txt'
  if ($Mode -eq 'direct') { Set-DirectEnv $runId }
  elseif ($Mode -eq 'startgame') { Set-StartGameEnv $runId }
  else { Set-HeadlessEnv $runId }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch -ResourceRoot $ResourceRoot | Out-Null
  Get-Process -Name 'JJFB_Launcher','main','main_gwy','main_research' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400
  $exe = Join-Path $RunDir $ExeName
  if (-not (Test-Path $exe)) { $exe = $MainExe }
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -RedirectStandardOutput $vmLog -RedirectStandardError $vmErr -PassThru
  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 2 }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
  }
  Start-Sleep -Milliseconds 500
  $shotSrc = Join-Path $RunDir 'screenshots\launcher_first_frame.bmp'
  $shotDst = Join-Path $cellDir 'first_frame.bmp'
  if (Test-Path $shotSrc) { Copy-Item -Force $shotSrc $shotDst }
  $m = Parse-Metrics $vmLog $shotDst
  if (Test-Path $ApiCsv) { Copy-Item -Force $ApiCsv (Join-Path $cellDir 'ORIGINAL_GWY_API_MAP.csv') }
  if (Test-Path $StackJson) { Copy-Item -Force $StackJson (Join-Path $cellDir 'ORIGINAL_GWY_RUNTIME_STACK.json') }
  [pscustomobject]@{
    group = $Group; rep = $Rep; hold = $HoldSeconds; run_id = $runId; cell = $cellDir
    resources = $m.resources; drawfp = $m.drawfp; codes = $m.codes; code15 = $m.code15
    e6c = $m.e6c; api_startgame = $m.api_startgame; api_run = $m.api_run
    stack_depth = $m.stack_depth; fault = $m.fault; alive = $m.alive; shot = $m.shot
    exit_code = $(if ($p.HasExited) { $p.ExitCode } else { -1 })
  }
}

$rows = @()
Write-Host '== A direct_boot =='
1..$DirectReps | ForEach-Object {
  $rows += Invoke-OneCell 'A_direct_boot' $_ $DirectHold 'direct' 'main.exe'
}
Write-Host '== B original_headless =='
1..$HeadlessReps | ForEach-Object {
  $rows += Invoke-OneCell 'B_original_headless' $_ $HeadlessHold 'headless' 'main_research.exe'
}
Write-Host '== C startgame_only =='
1..$StartGameReps | ForEach-Object {
  $rows += Invoke-OneCell 'C_startgame_only' $_ $StartGameHold 'startgame' 'main_research.exe'
}

$hdr = 'group,rep,hold,resources,drawfp,101ab_codes,code15,e6c,api_startgame,api_run,stack_depth,fault,alive,shot,exit_code,run_id,cell'
$lines = @($hdr)
foreach ($r in $rows) {
  $lines += ('{0},{1},{2},{3},{4},"{5}",{6},{7},{8},{9},{10},{11},{12},{13},{14},{15},{16}' -f `
    $r.group,$r.rep,$r.hold,$r.resources,$r.drawfp,$r.codes,$r.code15,$r.e6c,$r.api_startgame,$r.api_run,`
    $r.stack_depth,$r.fault,$r.alive,$r.shot,$r.exit_code,$r.run_id,$r.cell)
}
$lines | Set-Content -Path $MatrixCsv -Encoding utf8

# Ensure API/stack exist even if no runtime emit
if (-not (Test-Path $ApiCsv)) {
  "api_name,function_pointer,wrapper_pointer,string_va,context,owner_module,R9,generation,kind,registered`n" |
    Set-Content -Path $ApiCsv -Encoding utf8
}
if (-not (Test-Path $StackJson)) {
  '{"depth":0,"nested_jjfb_intercepted":false,"frames":[],"note":"no runtime stack captured"}' |
    Set-Content -Path $StackJson -Encoding utf8
}

$bRows = @($rows | Where-Object { $_.group -like 'B_*' })
$cRows = @($rows | Where-Object { $_.group -like 'C_*' })
$aRows = @($rows | Where-Object { $_.group -like 'A_*' })
$maxRes = ($rows | Measure-Object -Property resources -Maximum).Maximum
$gain = ($bRows + $cRows | Where-Object { $_.resources -gt 5 -or $_.code15 -eq 1 -or $_.e6c -eq 1 }).Count -gt 0
$aRes = if ($aRows) { ($aRows | Measure-Object -Property resources -Average).Average } else { 0 }
$bRes = if ($bRows) { ($bRows | Measure-Object -Property resources -Average).Average } else { 0 }

$md = @"
# ORIGINAL GWY Bootstrap Rehost (P16–P18)

## Verdict
$(if ($gain) { 'PARTIAL/GAIN — headless/startGame cell met at least one success signal.' } else { 'NO FIRST-SCREEN GAIN vs direct_boot — parent bootstrap ran for observation; missing remote or local contract step identified below.' })

## Baseline
- P15 frozen at commit recorded in ``reports/P15_BUILD_IDENTITY.txt``
- ``JJFB_304BF0_RESUME_MODE=direct_lr`` kept as product-safe baseline
- No code15 synthesis, no E6C force-write, no forged next screen

## Catalog
- Built via ``gwy_launcher original-catalog``
- Output: ``reports/ORIGINAL_GWY_BOOTSTRAP_CATALOG.json`` (supporting)
- Required packages: gwy.mrp, gbrwcore, gbrwshell, font, jjfb, jjfbol, cfg.bin

## Modes
| Mode | Env | Launch target |
|---|---|---|
| A direct_boot | ``JJFB_BOOTSTRAP_MODE=direct_boot`` | gwy/jjfb.mrp |
| B original_headless | ``JJFB_BOOTSTRAP_MODE=original_headless`` | gwy/gbrwcore.mrp → gbrwshell |
| C startgame_only | ``JJFB_BOOTSTRAP_MODE=startgame_only`` | gwy/gbrwcore.mrp (observe API_REGISTER) |

## Matrix summary
- A avg unique BMP resources: $([math]::Round($aRes,2))
- B avg unique BMP resources: $([math]::Round($bRes,2))
- Max resources any cell: $maxRes
- Full CSV: ``reports/ORIGINAL_GWY_AB_MATRIX.csv``

## 0x101AB comparison
- Product path still uses ``SYNTHETIC_CODE5_COMPAT`` unless a natural parent producer appears
- If parent naturally enqueues code15, frames must enter TRANSPORT_QUEUE without rewrite
- This run: code15 seen in any B/C cell = $(if (($bRows+$cRows | Where-Object code15 -eq 1).Count -gt 0) { 'YES' } else { 'NO' })

## API map / runtime stack
- ``reports/ORIGINAL_GWY_API_MAP.csv`` — runtime ``API_REGISTER`` (string VA first; entry PC only when observed)
- ``reports/ORIGINAL_GWY_RUNTIME_STACK.json`` — parent/child frames; parent must survive jjfb nested start

## Blockers (if no gain)
1. Original ``lib.startGame`` may still require live version/network contracts beyond local ``no_update`` stub
2. Nested ``runflashmrp/runapp`` → jjfb may not fire without gamelist cfg-select producer
3. Parent ``0x101AB`` producer may be network-backed; local gbrwcore alone may only prepare shared libs

## Policy
- ``direct_boot`` remains the safe JJFB baseline
- Do not hardcode startGame entry addresses — use registered pointers only
"@
$md | Set-Content -Path $Verdict -Encoding utf8
Write-Host "Wrote $Verdict"
Write-Host "Wrote $MatrixCsv"
Write-Host '[OK] P16–P18 original GWY headless matrix complete'
