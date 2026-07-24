# Task 6 Platform 0x10138 ABI + nested Path-A — A/B/C with stdout capture.
param(
  [ValidateSet('A','B','C')][string]$Variant = 'B',
  [int]$HoldSeconds = 55
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$OutDir = Join-Path $Root "out\p10138_task6\$Variant"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$Reports = Join-Path $Root 'reports'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'

if (-not (Test-Path $MainExe)) { throw "missing $MainExe" }

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null

Remove-Item (Join-Path $RunDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'path_a_handler_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'platform_10138_*') -ErrorAction SilentlyContinue

$runId = "p10138_${Variant}_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
$progress = Join-Path $RunDir 'runtime_progress.jsonl'
$stdout = Join-Path $OutDir 'stdout.txt'
$stderr = Join-Path $OutDir 'stderr.txt'
$vmLog = Join-Path $OutDir 'vm_stdout.txt'
Remove-Item $stdout,$stderr,$vmLog -ErrorAction SilentlyContinue

# Product env (full JJFB_Launcher apply_product_env parity).
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
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
Remove-Item Env:JJFB_HWND_UNTIL_DISPUP -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_P5_MODE -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_P5_ONE_SHOT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_HOST_TEST_PATTERN -ErrorAction SilentlyContinue

$env:JJFB_PRODUCT_FFP_MODE = '1'
$env:JJFB_PRODUCT_FFP_PHASE = 'event'
$env:JJFB_PRODUCT_EVENT_CONTRACT = '1'
$env:JJFB_PRODUCT_TRACE_305E09 = '1'
$env:JJFB_PRODUCT_TRACE_QUEUE_BOOTSTRAP = '1'
$env:JJFB_PRODUCT_TRACE_NODE_ALLOC = '1'
$env:JJFB_PRODUCT_TRACE_QUEUE_CONSUMER = '1'
$env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
$env:GWY_PRODUCT_RUN_ID = $runId
$env:GWY_PRODUCT_REPORTS_DIR = $Reports
$env:GWY_RUNTIME_PROGRESS_PATH = $progress
$env:JJFB_RUNTIME_PROGRESS = '1'
$env:JJFB_PATH_A_EVENT_CONTRACT = '1'

switch ($Variant) {
  'A' {
    # Baseline: Path-A framing on, but 0x10138 multi-out contract OFF (old STATUS-only).
    $env:JJFB_PLATFORM_10138_CONTRACT = '0'
    $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
    $env:JJFB_B71_DISPATCH_TRACE = '1'
    $env:JJFB_EVENT_OBJECT_TRACE = '1'
    $env:JJFB_PATH_A_HANDLER_TRACE = '1'
    $env:JJFB_PLATFORM_10138_TRACE = '1'
  }
  'B' {
    # Fix + Diagnostic.
    $env:JJFB_PLATFORM_10138_CONTRACT = '1'
    $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
    $env:JJFB_B71_DISPATCH_TRACE = '1'
    $env:JJFB_EVENT_OBJECT_TRACE = '1'
    $env:JJFB_PATH_A_HANDLER_TRACE = '1'
    $env:JJFB_PLATFORM_10138_TRACE = '1'
  }
  'C' {
    # Default Launcher: contract on, traces off.
    $env:JJFB_PLATFORM_10138_CONTRACT = '1'
    Remove-Item Env:JJFB_POST_DRAIN_GATE_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_B71_DISPATCH_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_EVENT_OBJECT_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_PATH_A_HANDLER_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_PLATFORM_10138_TRACE -ErrorAction SilentlyContinue
  }
}

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== Variant $Variant run_id=$runId Hold=${HoldSeconds}s =="
Write-Host "path_a=$($env:JJFB_PATH_A_EVENT_CONTRACT) 10138=$($env:JJFB_PLATFORM_10138_CONTRACT) pah=$($env:JJFB_PATH_A_HANDLER_TRACE) p10138=$($env:JJFB_PLATFORM_10138_TRACE)"

& $Launcher validate --root $ResourceRoot | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($HoldSeconds)
$sawHelperReturn = $false
$sawSuccessor = $false
$postHold = $null
do {
  Start-Sleep -Seconds 2
  $all = ''
  if (Test-Path $vmLog) { $all = Get-Content $vmLog -Raw -ErrorAction SilentlyContinue }
  if (-not $all) { $all = '' }

  if (-not $sawHelperReturn -and ($all -match 'path_a_helper_returned|\[PAH_RETURN\]|inside_2E4066')) {
    $sawHelperReturn = $true
    $postHold = (Get-Date).AddSeconds([Math]::Max(25, [int]($HoldSeconds * 0.45)))
    Write-Host "helper/handler progress — observe until $($postHold.ToString('HH:mm:ss'))"
  }
  if (-not $sawSuccessor -and ($all -match 'lifecycle_successor_entered|seen_2DADC4=1|inside_2DADC4|bl_2DADC4')) {
    $sawSuccessor = $true
    Write-Host 'lifecycle successor 0x2DADC4 seen'
  }
  if ($postHold -and (Get-Date) -ge $postHold) {
    Write-Host 'post-helper observation window complete'
    break
  }
  if ($all -match '\[PAH_POST_WINDOW\]') {
    Write-Host 'PAH_POST_WINDOW seen'
    break
  }
  if ($all -match '\[PAH_FINALIZE\].*PATH_A_HANDLER_FAULTED|milestone=GUEST_FAULT') {
    Write-Host 'fault stop'
    break
  }
  # Do not early-stop on bootstrap UC_MEM noise; only clear guest-fault milestone.
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 500
}
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

if (Test-Path $vmLog) { Copy-Item -Force $vmLog $stdout }
Copy-Item $progress (Join-Path $OutDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Get-ChildItem $Reports -Filter 'path_a_handler_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Get-ChildItem $Reports -Filter 'platform_10138_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force

Set-Content -Path (Join-Path $OutDir 'meta.txt') -Encoding utf8 -Value @"
variant=$Variant
run_id=$runId
path_a_contract=$($env:JJFB_PATH_A_EVENT_CONTRACT)
plat_10138_contract=$($env:JJFB_PLATFORM_10138_CONTRACT)
pah_trace=$($env:JJFB_PATH_A_HANDLER_TRACE)
p10138_trace=$($env:JJFB_PLATFORM_10138_TRACE)
hold=$HoldSeconds
expected_hash=$ExpectedHash
saw_helper_return=$sawHelperReturn
saw_successor=$sawSuccessor
"@

Write-Host '--- key stdout ---'
if (Test-Path $stdout) {
  Select-String -Path $stdout -Pattern 'PLATFORM_10138|P10138_|PAH_|path_a_helper|lifecycle_successor|2E4066|2DADC4|2F68E4|312A78|FAULT|RUNTIME_PROGRESS|PATH_A_CONTRACT' |
    Select-Object -First 100 | ForEach-Object { $_.Line }
}
Write-Host '--- progress ---'
if (Test-Path (Join-Path $OutDir 'runtime_progress.jsonl')) {
  Get-Content (Join-Path $OutDir 'runtime_progress.jsonl')
}
Write-Host "variant_${Variant}_done"
