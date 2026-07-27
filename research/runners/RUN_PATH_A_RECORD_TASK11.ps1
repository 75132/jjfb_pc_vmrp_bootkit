# Task 11: Restore Path-A lifecycle record (generation-scoped).
# A: WITH_RECORD=0 → empty body, 2DADC4 without 30ED2C (baseline white-screen)
# B: WITH_RECORD=1 → one-record, expect 30ED2C path
# C: env unset → profile default (= B once per generation)
param(
  [ValidateSet('A','B','C')][string]$Variant = 'B',
  [int]$HoldSeconds = 75,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$OutDir = Join-Path $Root "out\path_a_record_task11\$Variant"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$Reports = Join-Path $Root 'reports'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'

if (-not (Test-Path $MainExe)) { throw "missing $MainExe — build first" }

if (-not $SkipBuild) {
  Write-Host '== rebuild vmrp with launcher_core (Path-A response) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
}

Remove-Item (Join-Path $RunDir 'runtime_progress.jsonl') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'lifecycle_record_trace.md') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'helper_2f68e4_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'field_parser_*') -ErrorAction SilentlyContinue
Remove-Item (Join-Path $Reports 'path_a_handler_*') -ErrorAction SilentlyContinue

$runId = "pa11_${Variant}_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
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
$env:JJFB_PLATFORM_MEMCPY_IMPORT = '1'
$env:JJFB_FIELD_STREAM_CONTRACT = '0'
$env:GWY_PRODUCT_RUN_ID = $runId
$env:GWY_PRODUCT_REPORTS_DIR = $Reports
$env:GWY_RUNTIME_PROGRESS_PATH = $progress
$env:JJFB_RUNTIME_PROGRESS = '1'
$env:JJFB_POST_DRAIN_GATE_TRACE = '1'
$env:JJFB_PATH_A_HANDLER_TRACE = '1'
$env:JJFB_PLATFORM_10138_TRACE = '1'
$env:JJFB_HELPER_2F68E4_TRACE = '1'
$env:JJFB_FIELD_PARSER_TRACE = '1'
$env:JJFB_LIFECYCLE_RECORD_TRACE = '1'

switch ($Variant) {
  'A' {
    $env:JJFB_101AB_WITH_RECORD = '0'
  }
  'B' {
    $env:JJFB_101AB_WITH_RECORD = '1'
  }
  'C' {
    Remove-Item Env:JJFB_101AB_WITH_RECORD -ErrorAction SilentlyContinue
  }
}

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== Task11 Variant $Variant run_id=$runId Hold=${HoldSeconds}s with_rec=$($env:JJFB_101AB_WITH_RECORD) =="

& $Launcher validate --root $ResourceRoot | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($HoldSeconds)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Seconds 2
}
if (-not $p.HasExited) {
  Write-Host "hold elapsed — stopping pid=$($p.Id)"
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 500
}

Copy-Item $vmLog $stdout -Force -ErrorAction SilentlyContinue
if (Test-Path $progress) {
  Copy-Item $progress (Join-Path $OutDir 'runtime_progress.jsonl') -Force
}
if (Test-Path (Join-Path $Reports 'lifecycle_record_trace.md')) {
  Copy-Item (Join-Path $Reports 'lifecycle_record_trace.md') (Join-Path $OutDir 'lifecycle_record_trace.md') -Force
}

$meta = @"
run_id=$runId
variant=$Variant
with_rec=$($env:JJFB_101AB_WITH_RECORD)
import=$($env:JJFB_PLATFORM_MEMCPY_IMPORT)
fsc=$($env:JJFB_FIELD_STREAM_CONTRACT)
lrt=$($env:JJFB_LIFECYCLE_RECORD_TRACE)
hold=$HoldSeconds
"@
Set-Content -Path (Join-Path $OutDir 'meta.txt') -Value $meta -Encoding UTF8

Write-Host '== milestone grep =='
Select-String -Path $vmLog -Pattern 'PLATFORM_BUFFER_FILL|PATH_A_RESPONSE|LRT_|B71_NATURALLY|30ED2C|2DADC4|2F68E4|2E4066|0x94E40|r5=0x7374|FP_MILESTONE' |
  Select-Object -First 80 | ForEach-Object { $_.Line }

Write-Host "out=$OutDir"
