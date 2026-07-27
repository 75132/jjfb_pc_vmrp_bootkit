# Task 7: 0x2F68E4 sparse helper trace — A/B/C regression.
param(
  [ValidateSet('A','B','C')][string]$Variant = 'B',
  [int]$HoldSeconds = 90
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$OutDir = Join-Path $Root "out\helper_2f68e4_task7\$Variant"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$Reports = Join-Path $Root 'reports'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'

if (-not (Test-Path $MainExe)) { throw "missing $MainExe" }

Write-Host '== rebuild vmrp with launcher_core (includes H2 trace) =='
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }

Remove-Item (Join-Path $RunDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'helper_2f68e4_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'path_a_handler_*') -ErrorAction SilentlyContinue

$runId = "h2_${Variant}_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
$progress = Join-Path $RunDir 'runtime_progress.jsonl'
$stdout = Join-Path $OutDir 'stdout.txt'
$stderr = Join-Path $OutDir 'stderr.txt'
$vmLog = Join-Path $OutDir 'vm_stdout.txt'
Remove-Item $stdout,$stderr,$vmLog -ErrorAction SilentlyContinue

# Product env parity with JJFB_Launcher + task7 traces.
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
$env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
$env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
$env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
$env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
$env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
$env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
$env:GWY_MODULE_R9_SWITCH = '1'
$env:GWY_CALLBACK_FRAME = '1'
$env:JJFB_E5_SCHEDULER_MODE = '1'
$env:JJFB_PRODUCT_FFP_MODE = '1'
$env:JJFB_PRODUCT_FFP_PHASE = 'event'
$env:JJFB_PRODUCT_EVENT_CONTRACT = '1'
$env:JJFB_PRODUCT_TRACE_305E09 = '1'
$env:JJFB_PRODUCT_TRACE_QUEUE_BOOTSTRAP = '1'
$env:JJFB_PRODUCT_TRACE_NODE_ALLOC = '1'
$env:JJFB_PRODUCT_TRACE_QUEUE_CONSUMER = '1'
$env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
$env:JJFB_PATH_A_EVENT_CONTRACT = '1'
$env:GWY_PRODUCT_RUN_ID = $runId
$env:GWY_PRODUCT_REPORTS_DIR = $Reports
$env:GWY_RUNTIME_PROGRESS_PATH = $progress
$env:JJFB_RUNTIME_PROGRESS = '1'

switch ($Variant) {
  'A' {
    $env:JJFB_PLATFORM_10138_CONTRACT = '1'
    $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
    $env:JJFB_PATH_A_HANDLER_TRACE = '1'
    $env:JJFB_PLATFORM_10138_TRACE = '1'
    $env:JJFB_HELPER_2F68E4_TRACE = '1'
  }
  'B' {
    $env:JJFB_PLATFORM_10138_CONTRACT = '1'
    $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
    $env:JJFB_PATH_A_HANDLER_TRACE = '1'
    $env:JJFB_PLATFORM_10138_TRACE = '1'
    $env:JJFB_HELPER_2F68E4_TRACE = '1'
  }
  'C' {
    $env:JJFB_PLATFORM_10138_CONTRACT = '1'
    Remove-Item Env:JJFB_POST_DRAIN_GATE_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_PATH_A_HANDLER_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_PLATFORM_10138_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_HELPER_2F68E4_TRACE -ErrorAction SilentlyContinue
  }
}

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== Task7 Variant $Variant run_id=$runId Hold=${HoldSeconds}s h2=$($env:JJFB_HELPER_2F68E4_TRACE) =="

& $Launcher validate --root $ResourceRoot | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($HoldSeconds)
$sawHelperRunning = $false
$sawStableLoop = $false
$sawHelperReturn = $false
$sawSuccessor = $false

do {
  Start-Sleep -Seconds 3
  $all = ''
  if (Test-Path $vmLog) { $all = Get-Content $vmLog -Raw -ErrorAction SilentlyContinue }
  if (-not $all) { $all = '' }

  if (-not $sawHelperRunning -and ($all -match 'path_a_helper_running|\[H2_ENTER\]')) {
    $sawHelperRunning = $true
    Write-Host 'helper 0x2F68E4 sparse trace active'
  }
  if (-not $sawStableLoop -and ($all -match 'path_a_helper_stable_loop|\[H2_LOOP\] stable')) {
    $sawStableLoop = $true
    Write-Host 'stable helper loop classified'
  }
  if (-not $sawHelperReturn -and ($all -match 'path_a_helper_returned|\[H2_LEAVE\]|\[PAH_RETURN\]|inside_2E4066')) {
    $sawHelperReturn = $true
    Write-Host 'helper returned or reached 0x2E4066'
  }
  if (-not $sawSuccessor -and ($all -match 'lifecycle_successor_entered|inside_2DADC4|bl_2DADC4')) {
    $sawSuccessor = $true
    Write-Host 'lifecycle successor 0x2DADC4 seen'
  }
  if ($all -match '\[H2_FINALIZE\]|\[PAH_FINALIZE\]|\[FFP_FINALIZE\]') {
    Write-Host 'finalize seen — extra 10s'
    Start-Sleep -Seconds 10
    break
  }
  if ($all -match 'NESTED_EVENT_SCHEDULING_DEADLOCK|\[H2_DEADLOCK\]') {
    Write-Host 'scheduling deadlock classified'
    break
  }
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 500
}
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

if (Test-Path $vmLog) { Copy-Item -Force $vmLog $stdout }
Copy-Item $progress (Join-Path $OutDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Get-ChildItem $Reports -Filter 'helper_2f68e4_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Get-ChildItem $Reports -Filter 'path_a_handler_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force

Set-Content -Path (Join-Path $OutDir 'meta.txt') -Encoding utf8 -Value @"
variant=$Variant
run_id=$runId
h2_trace=$($env:JJFB_HELPER_2F68E4_TRACE)
plat_10138_contract=$($env:JJFB_PLATFORM_10138_CONTRACT)
hold=$HoldSeconds
saw_helper_running=$sawHelperRunning
saw_stable_loop=$sawStableLoop
saw_helper_return=$sawHelperReturn
saw_successor=$sawSuccessor
"@

Write-Host '--- key stdout ---'
if (Test-Path $stdout) {
  Select-String -Path $stdout -Pattern 'H2_|PAH_ENTER|PAH_RETURN|path_a_helper|nested_event|2E4066|2DADC4|2F68E4|H2_FINALIZE|H2_LOOP|H2_DEADLOCK|H2_SNAP|PLATFORM_10138|DispUp|resource_request' |
    Select-Object -First 120 | ForEach-Object { $_.Line }
}
Write-Host '--- progress ---'
if (Test-Path (Join-Path $OutDir 'runtime_progress.jsonl')) {
  Get-Content (Join-Path $OutDir 'runtime_progress.jsonl')
}
Write-Host "task7_variant_${Variant}_done"
