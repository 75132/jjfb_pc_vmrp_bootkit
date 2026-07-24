# Task 8: Field parser exit predicate + nested scheduling closure.
param(
  [ValidateSet('A','B','C')][string]$Variant = 'B',
  [int]$HoldSeconds = 90
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$OutDir = Join-Path $Root "out\helper_2f68e4_task8\$Variant"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$Reports = Join-Path $Root 'reports'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'

if (-not (Test-Path $MainExe)) { throw "missing $MainExe" }

Write-Host '== static disasm 0x30A0E0..0x30A130 =='
python (Join-Path $Root 'tools\disasm_field_parser_loop.py')
if ($LASTEXITCODE -ne 0) { throw 'disasm_field_parser_loop failed' }

Write-Host '== rebuild vmrp with launcher_core (H2 + FP trace) =='
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }

Remove-Item (Join-Path $RunDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'helper_2f68e4_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'field_parser_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'stage_field_parser_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'path_a_handler_*') -ErrorAction SilentlyContinue

$runId = "fp8_${Variant}_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
$progress = Join-Path $RunDir 'runtime_progress.jsonl'
$stdout = Join-Path $OutDir 'stdout.txt'
$stderr = Join-Path $OutDir 'stderr.txt'
$vmLog = Join-Path $OutDir 'vm_stdout.txt'
Remove-Item $stdout,$stderr,$vmLog -ErrorAction SilentlyContinue

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
    $env:JJFB_FIELD_PARSER_TRACE = '1'
  }
  'B' {
    $env:JJFB_PLATFORM_10138_CONTRACT = '1'
    $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
    $env:JJFB_PATH_A_HANDLER_TRACE = '1'
    $env:JJFB_PLATFORM_10138_TRACE = '1'
    $env:JJFB_HELPER_2F68E4_TRACE = '1'
    $env:JJFB_FIELD_PARSER_TRACE = '1'
  }
  'C' {
    $env:JJFB_PLATFORM_10138_CONTRACT = '1'
    Remove-Item Env:JJFB_POST_DRAIN_GATE_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_PATH_A_HANDLER_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_PLATFORM_10138_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_HELPER_2F68E4_TRACE -ErrorAction SilentlyContinue
    Remove-Item Env:JJFB_FIELD_PARSER_TRACE -ErrorAction SilentlyContinue
  }
}

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== Task8 Variant $Variant run_id=$runId Hold=${HoldSeconds}s fp=$($env:JJFB_FIELD_PARSER_TRACE) =="

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
$sawFpEntry = $false
$sawFpLoop = $false

do {
  Start-Sleep -Seconds 3
  $all = ''
  if (Test-Path $vmLog) { $all = Get-Content $vmLog -Raw -ErrorAction SilentlyContinue }
  if (-not $all) { $all = '' }

  if (-not $sawHelperRunning -and ($all -match 'path_a_helper_running|\[H2_ENTER\]')) {
    $sawHelperRunning = $true
    Write-Host 'helper 0x2F68E4 active'
  }
  if (-not $sawFpEntry -and ($all -match '\[FP_ENTRY\]')) {
    $sawFpEntry = $true
    Write-Host 'field parser entry traced'
  }
  if (-not $sawFpLoop -and ($all -match '\[FP_LOOP\]')) {
    $sawFpLoop = $true
    Write-Host 'field parser loop traced'
  }
  if (-not $sawStableLoop -and ($all -match 'path_a_helper_stable_loop|\[H2_LOOP\] stable')) {
    $sawStableLoop = $true
    Write-Host 'stable helper loop classified'
  }
  if ($all -match '\[H2_FINALIZE\]|\[FP_FINALIZE\]|\[FFP_FINALIZE\]') {
    Write-Host 'finalize seen — extra 10s'
    Start-Sleep -Seconds 10
    break
  }
  if (-not $p.HasExited -and ((Get-Date) -gt $deadline.AddSeconds(-8))) {
    Write-Host 'hold ending — allow finalize via graceful close'
    $p.CloseMainWindow() | Out-Null
    Start-Sleep -Seconds 5
  }
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

if (-not $p.HasExited) {
  $p.CloseMainWindow() | Out-Null
  Start-Sleep -Seconds 3
}
if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 500
}
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue

if (Test-Path $vmLog) { Copy-Item -Force $vmLog $stdout }
Copy-Item $progress (Join-Path $OutDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Get-ChildItem $Reports -Filter 'helper_2f68e4_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Get-ChildItem $Reports -Filter 'field_parser_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Get-ChildItem $Reports -Filter 'stage_field_parser_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Get-ChildItem $Reports -Filter 'path_a_handler_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Copy-Item (Join-Path $Reports 'field_parser_loop_disasm.md') (Join-Path $OutDir 'field_parser_loop_disasm.md') -ErrorAction SilentlyContinue

Set-Content -Path (Join-Path $OutDir 'meta.txt') -Encoding utf8 -Value @"
variant=$Variant
run_id=$runId
fp_trace=$($env:JJFB_FIELD_PARSER_TRACE)
h2_trace=$($env:JJFB_HELPER_2F68E4_TRACE)
hold=$HoldSeconds
saw_helper_running=$sawHelperRunning
saw_fp_entry=$sawFpEntry
saw_fp_loop=$sawFpLoop
saw_stable_loop=$sawStableLoop
"@

Write-Host '--- key stdout ---'
if (Test-Path $stdout) {
  Select-String -Path $stdout -Pattern 'FP_|H2_|PAH_|nested_event|2E4066|2DADC4|H2_LOOP|FP_LOOP|FP_ENTRY|FP_EXIT|FP_R5|FP_SCHED|FP_FINALIZE' |
    Select-Object -First 140 | ForEach-Object { $_.Line }
}
Write-Host '--- progress ---'
if (Test-Path (Join-Path $OutDir 'runtime_progress.jsonl')) {
  Get-Content (Join-Path $OutDir 'runtime_progress.jsonl')
}
Write-Host "task8_variant_${Variant}_done"
