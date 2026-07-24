# Task 9: Field stream length/cursor provenance + Path-A inner copy contract.
param(
  [ValidateSet('A','B','C')][string]$Variant = 'B',
  [int]$HoldSeconds = 75
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$OutDir = Join-Path $Root "out\field_stream_task9\$Variant"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$Reports = Join-Path $Root 'reports'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'

if (-not (Test-Path $MainExe)) { throw "missing $MainExe" }

Write-Host '== static disasm field parser length path =='
python (Join-Path $Root 'tools\disasm_field_parser_loop.py')
if ($LASTEXITCODE -ne 0) { throw 'disasm_field_parser_loop failed' }

Write-Host '== rebuild vmrp with launcher_core (FSC + FP) =='
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }

Remove-Item (Join-Path $RunDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'helper_2f68e4_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'field_parser_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'field_stream_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'stage_field_parser_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'path_a_handler_*') -ErrorAction SilentlyContinue

$runId = "fs9_${Variant}_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
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
$env:JJFB_PLATFORM_10138_CONTRACT = '1'
$env:GWY_PRODUCT_RUN_ID = $runId
$env:GWY_PRODUCT_REPORTS_DIR = $Reports
$env:GWY_RUNTIME_PROGRESS_PATH = $progress
$env:JJFB_RUNTIME_PROGRESS = '1'

switch ($Variant) {
  'A' {
    # Contract OFF + diagnostic: must reproduce r5=0x7374 / long copy loop
    $env:JJFB_FIELD_STREAM_CONTRACT = '0'
    $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
    $env:JJFB_PATH_A_HANDLER_TRACE = '1'
    $env:JJFB_PLATFORM_10138_TRACE = '1'
    $env:JJFB_HELPER_2F68E4_TRACE = '1'
    $env:JJFB_FIELD_PARSER_TRACE = '1'
  }
  'B' {
    # Contract ON + diagnostic: expect BE(-1) inner, helper return, 0x2E4066
    $env:JJFB_FIELD_STREAM_CONTRACT = '1'
    $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
    $env:JJFB_PATH_A_HANDLER_TRACE = '1'
    $env:JJFB_PLATFORM_10138_TRACE = '1'
    $env:JJFB_HELPER_2F68E4_TRACE = '1'
    $env:JJFB_FIELD_PARSER_TRACE = '1'
  }
  'C' {
    # Default launcher-like: contract default ON, quiet traces
    Remove-Item Env:JJFB_FIELD_STREAM_CONTRACT -ErrorAction SilentlyContinue
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

Write-Host "== Task9 Variant $Variant run_id=$runId Hold=${HoldSeconds}s fsc=$($env:JJFB_FIELD_STREAM_CONTRACT) fp=$($env:JJFB_FIELD_PARSER_TRACE) =="

& $Launcher validate --root $ResourceRoot | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($HoldSeconds)
$sawCopy = $false
$sawRepair = $false
$sawHelperRet = $false
$sawE4066 = $false
$sawDadc4 = $false
$sawBadR5 = $false
$sawFpLoop = $false

do {
  Start-Sleep -Seconds 3
  $all = ''
  if (Test-Path $vmLog) { $all = Get-Content $vmLog -Raw -ErrorAction SilentlyContinue }
  if (-not $all) { $all = '' }

  if (-not $sawCopy -and ($all -match '\[FSC_COPY\]')) {
    $sawCopy = $true
    Write-Host 'framing copy site traced'
  }
  if (-not $sawRepair -and ($all -match 'repaired=1|field_stream_copy_repaired')) {
    $sawRepair = $true
    Write-Host 'field stream copy repaired'
  }
  if (-not $sawBadR5 -and ($all -match 'new=0x7374|r5=0x7374')) {
    $sawBadR5 = $true
    Write-Host 'BAD r5=0x7374 observed'
  }
  if (-not $sawFpLoop -and ($all -match '\[FP_LOOP\]')) {
    $sawFpLoop = $true
    Write-Host 'field parser long loop'
  }
  if (-not $sawHelperRet -and ($all -match 'helper_2f68e4_returned|helper_2F68E4_return')) {
    $sawHelperRet = $true
    Write-Host 'helper 0x2F68E4 returned'
  }
  if (-not $sawE4066 -and ($all -match '0x2E4066|path_a_after_helper')) {
    $sawE4066 = $true
    Write-Host 'entered 0x2E4066'
  }
  if (-not $sawDadc4 -and ($all -match '0x2DADC4|path_a_lifecycle')) {
    $sawDadc4 = $true
    Write-Host 'entered 0x2DADC4'
  }
  if ($all -match '\[FP_FINALIZE\]|\[H2_FINALIZE\]|\[FFP_FINALIZE\]') {
    Write-Host 'finalize seen — extra 8s'
    Start-Sleep -Seconds 8
    break
  }
  if (-not $p.HasExited -and ((Get-Date) -gt $deadline.AddSeconds(-8))) {
    Write-Host 'hold ending — graceful close'
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
Get-ChildItem $Reports -Filter 'field_stream_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Get-ChildItem $Reports -Filter 'stage_field_parser_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force
Get-ChildItem $Reports -Filter 'path_a_handler_*' -ErrorAction SilentlyContinue | Copy-Item -Destination $OutDir -Force

Set-Content -Path (Join-Path $OutDir 'meta.txt') -Encoding utf8 -Value @"
variant=$Variant
run_id=$runId
fsc=$($env:JJFB_FIELD_STREAM_CONTRACT)
fp_trace=$($env:JJFB_FIELD_PARSER_TRACE)
hold=$HoldSeconds
saw_copy=$sawCopy
saw_repair=$sawRepair
saw_bad_r5=$sawBadR5
saw_fp_loop=$sawFpLoop
saw_helper_ret=$sawHelperRet
saw_2e4066=$sawE4066
saw_2dadc4=$sawDadc4
"@

Write-Host '--- key stdout ---'
if (Test-Path $stdout) {
  Select-String -Path $stdout -Pattern 'FSC_|FP_|H2_|PAH_INNER|PAH_ENTER|2E4066|2DADC4|field_stream|helper_2f68e4|FP_FINALIZE|FP_MILESTONE|FP_LEN|FP_ENTRY|FP_R5' |
    Select-Object -First 160 | ForEach-Object { $_.Line }
}
Write-Host '--- progress ---'
if (Test-Path (Join-Path $OutDir 'runtime_progress.jsonl')) {
  Get-Content (Join-Path $OutDir 'runtime_progress.jsonl')
}
Write-Host "task9_variant_${Variant}_done"
