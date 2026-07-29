# P15–P17: 0x101AB transport/protocol closure + text visibility A/B
param(
  [switch]$SkipBuild,
  [int]$BaselineHold = 180,
  [int]$BaselineReps = 2,
  [int]$TextHold = 70,
  [int]$TextReps = 2,
  [int]$ProviderHold = 90,
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
$MainExe = Join-Path $RunDir 'main.exe'
$ArchiveRoot = Join-Path $Root 'out\p15_101ab'
$MatrixCsv = Join-Path $Reports 'P15_FRAME_MATRIX.csv'
$Identity = Join-Path $Reports 'P15_BUILD_IDENTITY.txt'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot, (Join-Path $ArchiveRoot 'frames') | Out-Null

if ($Quick) {
  $BaselineHold = 45; $BaselineReps = 1; $TextHold = 35; $TextReps = 1; $ProviderHold = 45
}

function Get-Sha256([string]$Path) {
  if (-not (Test-Path $Path)) { return '' }
  (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

if (-not $SkipBuild) {
  Write-Host '== Full build (no SkipBuild) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -NoLaunch -ResourceRoot (Join-Path $Root 'game_files\mythroad\240x320')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL prepare failed' }
}

$jjfb = Join-Path $Root 'game_files\mythroad\240x320\gwy\jjfb.mrp'
$launcher = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
@(
  "clean_commit=$(git -C $Root rev-parse HEAD)"
  "main.exe=$(Get-Sha256 $MainExe)"
  "JJFB_Launcher.exe=$(Get-Sha256 $launcher)"
  "jjfb.mrp=$(Get-Sha256 $jjfb)"
  "recorded_utc=$((Get-Date).ToUniversalTime().ToString('o'))"
) | Set-Content -Path $Identity -Encoding utf8
Write-Host (Get-Content $Identity -Raw)

# Unit test
$testExe = Join-Path $Root 'build-i686\test_platform_101ab_frame.exe'
if (Test-Path $testExe) {
  & $testExe
  if ($LASTEXITCODE -ne 0) { throw 'test_platform_101ab_frame failed' }
}

function Set-ProductEnv([string]$RunId, [string]$TextXy) {
  $ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
  $shotDir = Join-Path $RunDir 'screenshots'
  New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
  $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
  $env:GWY_WINDOW_TITLE = 'JJFB Launcher'
  $env:JJFB_LAUNCH_SOURCE = 'jjfb_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
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
  $env:GWY_RUNTIME_PROGRESS_PATH = (Join-Path $RunDir 'runtime_progress.jsonl')
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $Reports
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  if ($TextXy -eq '1') { $env:JJFB_TEXT_PARAM0_XY = '1' }
  else { Remove-Item Env:JJFB_TEXT_PARAM0_XY -ErrorAction SilentlyContinue }
}

function Invoke-OneCell([string]$Group, [int]$Rep, [int]$HoldSeconds, [string]$TextXy) {
  $ts = Get-Date -Format 'yyyyMMdd_HHmmss'
  $runId = "p15_${Group}_r${Rep}_$ts"
  $cellDir = Join-Path $ArchiveRoot "${Group}_r${Rep}_$ts"
  New-Item -ItemType Directory -Force -Path $cellDir | Out-Null
  $vmLog = Join-Path $cellDir 'vm_stdout.txt'
  $vmErr = Join-Path $cellDir 'vm_stderr.txt'
  Set-ProductEnv $runId $TextXy
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch -ResourceRoot (Join-Path $Root 'game_files\mythroad\240x320') | Out-Null
  Get-Process -Name 'JJFB_Launcher','main','main_gwy' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400
  if (Test-Path (Join-Path $RunDir 'runtime_progress.jsonl')) { Remove-Item (Join-Path $RunDir 'runtime_progress.jsonl') -Force }
  $p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir -PassThru -RedirectStandardOutput $vmLog -RedirectStandardError $vmErr
  "{`"runtime_pid`":$($p.Id),`"run_id`":`"$runId`"}" | Set-Content (Join-Path $RunDir 'runtime_process.json') -Encoding utf8
  Write-Host "group=$Group main_pid=$($p.Id) run_id=$runId hold=${HoldSeconds}s text_xy=$TextXy"
  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) { Write-Host "early exit $($p.ExitCode)"; break }
  }
  try { $p.Refresh(); $aliveAtEnd = -not $p.HasExited } catch { $aliveAtEnd = $false }
  $gatePass = $false; $gateFail = ''; $layer1Sha = ''
  try {
    $gate = Test-JjfbLayer1Gate -RunDir $RunDir -ArchiveDir (Join-Path $cellDir 'gate') -Require240:$true
    $gatePass = [bool]$gate.pass
    if (-not $gatePass) { $gateFail = ($gate.fail_reasons -join ',') }
    if ($gate.bmp -and $gate.bmp.sha256) { $layer1Sha = $gate.bmp.sha256 }
  } catch { $gatePass = $false; $gateFail = $_.Exception.Message }
  if (Test-Path (Join-Path $RunDir 'runtime_progress.jsonl')) {
    Copy-Item (Join-Path $RunDir 'runtime_progress.jsonl') (Join-Path $cellDir 'runtime_progress.jsonl') -Force
  }
  if (Test-Path (Join-Path $Reports 'P15_101AB_TRACE.csv')) {
    Copy-Item (Join-Path $Reports 'P15_101AB_TRACE.csv') (Join-Path $cellDir 'P15_101AB_TRACE.csv') -Force
  }
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Get-Process -Name 'main','JJFB_Launcher' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 600

  $scan = ''
  if (Test-Path $vmLog) {
    $raw = Get-Content $vmLog -Raw -ErrorAction SilentlyContinue
    if ($null -ne $raw) { $scan = [string]$raw }
  }
  if (Test-Path (Join-Path $cellDir 'runtime_progress.jsonl')) {
    $raw2 = Get-Content (Join-Path $cellDir 'runtime_progress.jsonl') -Raw -ErrorAction SilentlyContinue
    if ($null -ne $raw2) { $scan += "`n" + [string]$raw2 }
  }
  if ($null -eq $scan) { $scan = '' }

  $resCount = 0
  $members = @()
  [regex]::Matches($scan, '"details":"([^"]+\.bmp)"') | ForEach-Object { $members += $_.Groups[1].Value }
  [regex]::Matches($scan, 'member[=:]?\s*([A-Za-z0-9_!]+\.bmp)') | ForEach-Object { $members += $_.Groups[1].Value }
  [regex]::Matches($scan, 'PLATFORM_MRP_RES.*?path=([^\s]+)') | ForEach-Object { $members += $_.Groups[1].Value }
  [regex]::Matches($scan, 'resource_open.*?([A-Za-z0-9_!]+\.bmp)') | ForEach-Object { $members += $_.Groups[1].Value }
  $members = @($members | Where-Object { $_ } | Select-Object -Unique)
  if ($members.Count -gt 0) { $resCount = $members.Count }
  $textApp7 = [bool]($scan -match 'handled=1 app=0x7')
  $code15 = [bool]($scan -match 'P15_CODE15|event_code[=:]15|,15,')
  $e6c = [bool]($scan -match 'E6C_NATURAL_STORE|P15_E6C_NATURAL')
  $frames = ([regex]::Matches($scan, '\[P15_101AB\] frame_id=|PLATFORM_BUFFER_FILL\] code=0x101AB')).Count
  $codes = @()
  [regex]::Matches($scan, '\[P15_101AB\] frame_id=\d+.*?code=(\d+)') | ForEach-Object { $codes += $_.Groups[1].Value }
  if ($codes.Count -eq 0) {
    [regex]::Matches($scan, 'PLATFORM_BUFFER_FILL\] code=0x101AB') | ForEach-Object { $codes += '5' }
  }
  $codeSeq = ($codes -join '|')
  $transport = ''
  if ($scan -match 'transport=([A-Z_]+)') { $transport = $Matches[1] }
  $row = [ordered]@{
    group = $Group
    rep = $Rep
    hold_s = $HoldSeconds
    text_xy = $TextXy
    resume_mode = 'direct_lr'
    layer1_pass = $(if ($gatePass) { 'PASS' } else { 'FAIL' })
    layer1_sha = $layer1Sha
    resources = $resCount
    text_app7 = $(if ($textApp7) { 'YES' } else { 'NO' })
    alive_end = $(if ($aliveAtEnd) { 'YES' } else { 'NO' })
    frames_101ab = $frames
    event_codes = $codeSeq
    code15 = $(if ($code15) { 'YES' } else { 'NO' })
    e6c_natural = $(if ($e6c) { 'YES' } else { 'NO' })
    transport = $transport
    gate_fail = $gateFail
    run_id = $runId
    cell = $cellDir
  }
  $isNew = -not (Test-Path $MatrixCsv)
  $line = ($row.Values | ForEach-Object { if ($_ -match '[,\"\n]') { '"' + ($_ -replace '"','""') + '"' } else { "$_" } }) -join ','
  if ($isNew) {
    ($row.Keys -join ',') | Set-Content $MatrixCsv -Encoding utf8
  }
  Add-Content $MatrixCsv $line -Encoding utf8
  $row | ConvertTo-Json -Compress | Set-Content (Join-Path $cellDir 'cell_summary.json') -Encoding utf8
  return $row
}

# Baseline synthetic 180s x2
for ($r = 1; $r -le $BaselineReps; $r++) {
  [void](Invoke-OneCell -Group 'baseline' -Rep $r -HoldSeconds $BaselineHold -TextXy '0')
}

# Text A/B
for ($r = 1; $r -le $TextReps; $r++) {
  [void](Invoke-OneCell -Group 'textA_yx' -Rep $r -HoldSeconds $TextHold -TextXy '0')
  [void](Invoke-OneCell -Group 'textB_xy' -Rep $r -HoldSeconds $TextHold -TextXy '1')
}

# Provider path (still synthetic — no real queue producer closed yet)
[void](Invoke-OneCell -Group 'provider_synth' -Rep 1 -HoldSeconds $ProviderHold -TextXy '0')

Write-Host "P15 matrix -> $MatrixCsv"
Write-Host "P15 identity -> $Identity"
