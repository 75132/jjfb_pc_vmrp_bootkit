#!/usr/bin/env python3
from pathlib import Path

PS1 = """# P12 JJFB 180s x2 after low-mem map + family r2 fill fix
param([int]$HoldSeconds = 180, [int]$Reps = 2)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) { $Root = Split-Path -Parent $Root }
Set-Location $Root
. (Join-Path $Root 'tools\\JjfbLayer1Gate.ps1')
$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\\msys64\\mingw32\\bin' }
$env:Path = \"$MingwBin;C:\\msys64\\usr\\bin;\" + $env:Path
$Reports = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$ArchiveRoot = Join-Path $Root 'out\\p12_lowmem_ab'
$MatrixCsv = Join-Path $Reports 'JJFB_NEXT_SCREEN_MATRIX.csv'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot | Out-Null
if (-not (Test-Path $MainExe)) { throw \"missing $MainExe\" }

function Set-ProductEnv([string]$RunId) {
  $ResourceRoot = Join-Path $Root 'game_files\\mythroad\\240x320'
  $shotDir = Join-Path $RunDir 'screenshots'
  New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
  $env:GWY_PROFILE = Join-Path $Root 'profiles\\jjfb.json'
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
  $env:GWY_RUNTIME_PROGRESS_PATH = (Join-Path $RunDir 'runtime_progress.jsonl')
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $Reports
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  Remove-Item Env:JJFB_MAP_LOW_GUEST_MEM -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_304BF0_EPILOGUE_R0 -ErrorAction SilentlyContinue
  Remove-Item Env:GWY_HOST_TEST_PATTERN -ErrorAction SilentlyContinue
}

Write-Host 'Preparing resources...'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch -ResourceRoot (Join-Path $Root 'game_files\\mythroad\\240x320') | Out-Null

$matrixLines = New-Object System.Collections.Generic.List[string]
$matrixLines.Add('rep,layer1,resources,drawfp,frame_sha,alive,p3_fault,fault_pc,fault_addr,low_mem_map,sixth,notes')

for ($rep = 1; $rep -le $Reps; $rep++) {
  $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
  $runId = \"p12_r${rep}_$stamp\"
  $cellDir = Join-Path $ArchiveRoot (\"r{0}_{1}\" -f $rep, $stamp)
  New-Item -ItemType Directory -Force -Path $cellDir | Out-Null
  Set-ProductEnv $runId

  Remove-Item -Force `
    (Join-Path $RunDir 'runtime_progress.jsonl'),
    (Join-Path $RunDir 'runtime_process.json'),
    (Join-Path $RunDir 'screenshots\\launcher_first_frame.bmp'),
    (Join-Path $Reports 'p6_post_resource5_verdict.md'),
    (Join-Path $Reports 'p8_sp_invariant.csv') -ErrorAction SilentlyContinue

  Get-Process -Name 'JJFB_Launcher','main','main_gwy' -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400

  $vmLog = Join-Path $cellDir 'vm_stdout.txt'
  $vmErr = Join-Path $cellDir 'vm_stderr.txt'
  Write-Host \"==== P12 rep=$rep hold=${HoldSeconds}s ====\"

  $p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $vmLog -RedirectStandardError $vmErr
  $json = '{0}\"runtime_pid\":{1},\"run_id\":\"{2}\"{3}' -f '{', $p.Id, $runId, '}'
  Set-Content -Path (Join-Path $RunDir 'runtime_process.json') -Value $json -Encoding utf8

  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  $progress = Join-Path $RunDir 'runtime_progress.jsonl'
  $sawFrame = $false
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) { Write-Host 'main exited early'; break }
    if (-not $sawFrame -and (Test-Path $progress)) {
      $txt = Get-Content $progress -Raw -ErrorAction SilentlyContinue
      if ($txt -match 'drawfp_first_drawn|FIRST_REAL_FRAME') {
        $sawFrame = $true
        Write-Host 'first_frame_seen'
      }
    }
  }

  $alive = $false
  try { $p.Refresh(); $alive = -not $p.HasExited } catch { $alive = $false }

  $archive = Join-Path $cellDir 'gate'
  New-Item -ItemType Directory -Force -Path $archive | Out-Null
  $layer1 = 'FAIL'
  $frameSha = ''
  try {
    $gate = Test-JjfbLayer1Gate -RunDir $RunDir -ArchiveDir $archive -Require240:$true
    if ($gate.pass) { $layer1 = 'PASS' }
    if ($gate.bmp -and $gate.bmp.sha256) { $frameSha = [string]$gate.bmp.sha256 }
  } catch {
    Write-Host \"gate_exception: $($_.Exception.Message)\"
  }

  foreach ($f in @(
      'p8_sp_invariant.csv','p6_post_resource5_verdict.md','p6_post_resource5_timeline.csv',
      'p6_post_resource5_pc_histogram.csv','p7_family_event_abi.csv','p8_callsite_stack_contract_runtime.md'
    )) {
    $src = Join-Path $Reports $f
    if (Test-Path $src) { Copy-Item $src (Join-Path $cellDir $f) -Force }
  }
  if (Test-Path $progress) { Copy-Item $progress (Join-Path $cellDir 'runtime_progress.jsonl') -Force }

  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Start-Sleep -Milliseconds 600

  $scan = ''
  if (Test-Path $vmLog) { $scan = Get-Content $vmLog -Raw -ErrorAction SilentlyContinue }
  if (Test-Path $vmErr) { $scan += \"`n\" + (Get-Content $vmErr -Raw -ErrorAction SilentlyContinue) }

  $resCount = 0
  if (Test-Path $spCsv) {
    $memberNames = @(Import-Csv $spCsv | Where-Object { $_.tag -eq 'LOOKUP_CALLSITE' -and $_.member_name } |
      Select-Object -ExpandProperty member_name -Unique)
    $resCount = $memberNames.Count
  }
  $verdictPath = Join-Path $cellDir 'p6_post_resource5_verdict.md'
  if (Test-Path $verdictPath) {
    $v = Get-Content $verdictPath -Raw
    if ($v -match 'Resources completed:\\s*(\\d+)') { $resCount = [int]$Matches[1] }
  }

  $drawfp = if ($sawFrame) { 'YES' } else { 'NO' }
  $p3 = if ($scan -match '\\[P3_FAULT\\]') { 'YES' } else { 'NO' }
  $faultPc = ''
  $faultAddr = ''
  if ($scan -match 'pc=0x2D960E') {
    $faultPc = '0x2D960E'
    if ($scan -match 'addr=0x([0-9A-Fa-f]+)') { $faultAddr = '0x' + $Matches[1] }
    $p3 = 'YES'
  } elseif ($scan -match '\\[P3_FAULT\\][^\\n]*pc=0x([0-9A-Fa-f]+)[^\\n]*addr=0x([0-9A-Fa-f]+)') {
    $faultPc = '0x' + $Matches[1]
    $faultAddr = '0x' + $Matches[2]
    $p3 = 'YES'
  }
  $lowMap = if ($scan -match 'PLATFORM_LOW_MEM_MAP\\] op=MAP') { 'YES' } else { 'NO' }
  $sixth = if ($resCount -ge 6) { 'YES' } else { 'NO' }
  $aliveStr = if ($alive) { 'YES' } else { 'NO' }

  $rows += \"$rep,$layer1,$resCount,$drawfp,$frameSha,$aliveStr,$p3,$faultPc,$faultAddr,$lowMap,$sixth,direct_lr+lowmem\"
  Write-Host \"LAYER1=$layer1 res=$resCount sixth=$sixth p3=$p3 lowmap=$lowMap\"
}

$rows | Set-Content -Encoding utf8 $MatrixCsv
Write-Host \"matrix -> $MatrixCsv\"
$rows | ForEach-Object { Write-Host $_ }
"""

out = Path("research/runners/RUN_P12_LOWMEM_AB.ps1")
# Extract only if we accidentally wrapped - write PS1 content directly
# The PS1 variable above still has python-ish escaping issues from nested quotes.
# Write a minimal clean runner instead.
PS1_CLEAN = r'''# P12 JJFB 180s x2 after low-mem map + family r2 fill fix
param([int]$HoldSeconds = 180, [int]$Reps = 2)
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
$ArchiveRoot = Join-Path $Root 'out\p12_lowmem_ab'
$MatrixCsv = Join-Path $Reports 'JJFB_NEXT_SCREEN_MATRIX.csv'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot | Out-Null
if (-not (Test-Path $MainExe)) { throw "missing $MainExe" }

function Set-ProductEnv([string]$RunId) {
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
  $env:GWY_RUNTIME_PROGRESS_PATH = (Join-Path $RunDir 'runtime_progress.jsonl')
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $Reports
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  Remove-Item Env:JJFB_MAP_LOW_GUEST_MEM -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_304BF0_EPILOGUE_R0 -ErrorAction SilentlyContinue
  Remove-Item Env:GWY_HOST_TEST_PATTERN -ErrorAction SilentlyContinue
}

Write-Host 'Preparing resources...'
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch -ResourceRoot (Join-Path $Root 'game_files\mythroad\240x320') | Out-Null

$matrixLines = New-Object System.Collections.Generic.List[string]
$matrixLines.Add('rep,layer1,resources,drawfp,frame_sha,alive,p3_fault,fault_pc,fault_addr,low_mem_map,sixth,notes')

for ($rep = 1; $rep -le $Reps; $rep++) {
  $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
  $runId = "p12_r${rep}_$stamp"
  $cellDir = Join-Path $ArchiveRoot ("r{0}_{1}" -f $rep, $stamp)
  New-Item -ItemType Directory -Force -Path $cellDir | Out-Null
  Set-ProductEnv $runId

  Remove-Item -Force `
    (Join-Path $RunDir 'runtime_progress.jsonl'),
    (Join-Path $RunDir 'runtime_process.json'),
    (Join-Path $RunDir 'screenshots\launcher_first_frame.bmp'),
    (Join-Path $Reports 'p6_post_resource5_verdict.md'),
    (Join-Path $Reports 'p8_sp_invariant.csv') -ErrorAction SilentlyContinue

  Get-Process -Name 'JJFB_Launcher','main','main_gwy' -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400

  $vmLog = Join-Path $cellDir 'vm_stdout.txt'
  $vmErr = Join-Path $cellDir 'vm_stderr.txt'
  Write-Host "==== P12 rep=$rep hold=${HoldSeconds}s ===="

  $p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $vmLog -RedirectStandardError $vmErr
  $json = '{"runtime_pid":' + $p.Id + ',"run_id":"' + $runId + '"}'
  Set-Content -Path (Join-Path $RunDir 'runtime_process.json') -Value $json -Encoding utf8

  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  $progress = Join-Path $RunDir 'runtime_progress.jsonl'
  $sawFrame = $false
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) { Write-Host 'main exited early'; break }
    if (-not $sawFrame -and (Test-Path $progress)) {
      $txt = Get-Content $progress -Raw -ErrorAction SilentlyContinue
      if ($txt -match 'drawfp_first_drawn|FIRST_REAL_FRAME') {
        $sawFrame = $true
        Write-Host 'first_frame_seen'
      }
    }
  }

  $alive = $false
  try { $p.Refresh(); $alive = -not $p.HasExited } catch { $alive = $false }

  $archive = Join-Path $cellDir 'gate'
  New-Item -ItemType Directory -Force -Path $archive | Out-Null
  $layer1 = 'FAIL'
  $frameSha = ''
  try {
    $gate = Test-JjfbLayer1Gate -RunDir $RunDir -ArchiveDir $archive -Require240:$true
    if ($gate.pass) { $layer1 = 'PASS' }
    if ($gate.bmp -and $gate.bmp.sha256) { $frameSha = [string]$gate.bmp.sha256 }
  } catch {
    Write-Host "gate_exception: $($_.Exception.Message)"
  }

  foreach ($f in @(
      'p8_sp_invariant.csv','p6_post_resource5_verdict.md','p6_post_resource5_timeline.csv',
      'p6_post_resource5_pc_histogram.csv','p7_family_event_abi.csv','p8_callsite_stack_contract_runtime.md'
    )) {
    $src = Join-Path $Reports $f
    if (Test-Path $src) { Copy-Item $src (Join-Path $cellDir $f) -Force }
  }
  if (Test-Path $progress) { Copy-Item $progress (Join-Path $cellDir 'runtime_progress.jsonl') -Force }

  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Start-Sleep -Milliseconds 600

  $scan = ''
  if (Test-Path $vmLog) { $scan = Get-Content $vmLog -Raw -ErrorAction SilentlyContinue }
  if (Test-Path $vmErr) { $scan += "`n" + (Get-Content $vmErr -Raw -ErrorAction SilentlyContinue) }

  $resCount = 0
  if (Test-Path $spCsv) {
    $memberNames = @(Import-Csv $spCsv | Where-Object { $_.tag -eq 'LOOKUP_CALLSITE' -and $_.member_name } |
      Select-Object -ExpandProperty member_name -Unique)
    $resCount = $memberNames.Count
  }
  $verdictPath = Join-Path $cellDir 'p6_post_resource5_verdict.md'
  if (Test-Path $verdictPath) {
    $v = Get-Content $verdictPath -Raw
    if ($v -match 'Resources completed:\s*(\d+)') { $resCount = [int]$Matches[1] }
  }

  $drawfp = if ($sawFrame) { 'YES' } else { 'NO' }
  $p3 = if ($scan -match '\[P3_FAULT\]') { 'YES' } else { 'NO' }
  $faultPc = ''
  $faultAddr = ''
  if ($scan -match 'pc=0x2D960E') {
    $faultPc = '0x2D960E'
    if ($scan -match 'addr=0x([0-9A-Fa-f]+)') { $faultAddr = '0x' + $Matches[1] }
    $p3 = 'YES'
  } elseif ($scan -match '\[P3_FAULT\][^\n]*pc=0x([0-9A-Fa-f]+)[^\n]*addr=0x([0-9A-Fa-f]+)') {
    $faultPc = '0x' + $Matches[1]
    $faultAddr = '0x' + $Matches[2]
    $p3 = 'YES'
  }
  $lowMap = if ($scan -match 'PLATFORM_LOW_MEM_MAP\] op=MAP') { 'YES' } else { 'NO' }
  $sixth = if ($resCount -ge 6) { 'YES' } else { 'NO' }
  $aliveStr = if ($alive) { 'YES' } else { 'NO' }

  $rows += "$rep,$layer1,$resCount,$drawfp,$frameSha,$aliveStr,$p3,$faultPc,$faultAddr,$lowMap,$sixth,direct_lr+lowmem"
  Write-Host "LAYER1=$layer1 res=$resCount sixth=$sixth p3=$p3 lowmap=$lowMap"
}

$rows | Set-Content -Encoding utf8 $MatrixCsv
Write-Host "matrix -> $MatrixCsv"
$rows | ForEach-Object { Write-Host $_ }
'''

Path('research/runners/RUN_P12_LOWMEM_AB.ps1').write_text(PS1, encoding='utf-8')
print('OK', Path('research/runners/RUN_P12_LOWMEM_AB.ps1').stat().st_size)
print(Path('research/runners/RUN_P12_LOWMEM_AB.ps1').read_text(encoding='utf-8')[:60])
