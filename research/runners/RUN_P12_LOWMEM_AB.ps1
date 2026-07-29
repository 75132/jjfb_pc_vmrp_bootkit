# P12/P13 JJFB 180s x2 after low-mem map + family r2 fill fix
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

function Get-Sha256([string]$Path) {
  (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

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
  $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
  $env:JJFB_B71_DISPATCH_TRACE = '1'
  $env:GWY_RUNTIME_PROGRESS_PATH = (Join-Path $RunDir 'runtime_progress.jsonl')
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $Reports
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  Remove-Item Env:JJFB_MAP_LOW_GUEST_MEM -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_HWND_UNTIL_DISPUP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_P5_MODE -ErrorAction SilentlyContinue
  Remove-Item Env:GWY_HOST_TEST_PATTERN -ErrorAction SilentlyContinue
  Remove-Item Env:SDL_VIDEODRIVER -ErrorAction SilentlyContinue
}

function Invoke-OneCell([int]$Rep, [int]$HoldSeconds) {
  $ts = Get-Date -Format 'yyyyMMdd_HHmmss'
  $runId = "p12_r${Rep}_$ts"
  $cellDir = Join-Path $ArchiveRoot "r${Rep}_$ts"
  New-Item -ItemType Directory -Force -Path $cellDir | Out-Null
  $vmLog = Join-Path $cellDir 'vm_stdout.txt'
  $vmErr = Join-Path $cellDir 'vm_stderr.txt'

  Set-ProductEnv $runId
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') `
    -SkipBuild -NoLaunch -ResourceRoot (Join-Path $Root 'game_files\mythroad\240x320') | Out-Null
  Get-Process -Name 'JJFB_Launcher','main','main_gwy' -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400

  $p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $vmLog -RedirectStandardError $vmErr
  "{`"runtime_pid`":$($p.Id),`"run_id`":`"$runId`"}" |
    Set-Content (Join-Path $RunDir 'runtime_process.json') -Encoding utf8
  Write-Host "main_pid=$($p.Id) run_id=$runId hold=$HoldSeconds"

  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  $progress = Join-Path $RunDir 'runtime_progress.jsonl'
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) { Write-Host "main exited early code=$($p.ExitCode)"; break }
  }

  try { $p.Refresh(); $aliveAtEnd = -not $p.HasExited } catch { $aliveAtEnd = $false }
  $archive = Join-Path $cellDir 'gate'
  if ($aliveAtEnd) {
    "{`"runtime_pid`":$($p.Id),`"run_id`":`"$runId`"}" |
      Set-Content (Join-Path $RunDir 'runtime_process.json') -Encoding utf8
  }
  $gatePass = $false; $gateFail = ''; $layer1Sha = ''; $nonBlack = ''
  try {
    $gate = Test-JjfbLayer1Gate -RunDir $RunDir -ArchiveDir $archive -Require240:$true
    $gatePass = [bool]$gate.pass
    if (-not $gatePass) { $gateFail = ($gate.fail_reasons -join ',') }
    if ($gate.bmp -and $gate.bmp.sha256) { $layer1Sha = $gate.bmp.sha256 }
    if ($gate.bmp -and $null -ne $gate.bmp.nonBlackPct) { $nonBlack = "$($gate.bmp.nonBlackPct)" }
  } catch {
    $gatePass = $false
    $gateFail = "gate_exception:$($_.Exception.Message)"
  }

  foreach ($f in @(
      'p8_sp_invariant.csv','p6_post_resource5_verdict.md','p6_post_resource5_timeline.csv',
      'p6_post_resource5_pc_histogram.csv','p8_callsite_stack_contract_runtime.md','p7_family_event_abi.csv',
      'product_b71_dispatch_timeline.md'
    )) {
    $src = Join-Path $Reports $f
    if (Test-Path $src) { Copy-Item $src (Join-Path $cellDir $f) -Force }
  }
  if (Test-Path $progress) { Copy-Item $progress (Join-Path $cellDir 'runtime_progress.jsonl') -Force }

  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Get-Process -Name 'main','JJFB_Launcher' -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 800

  $progTxt = ''
  $progPath = Join-Path $cellDir 'runtime_progress.jsonl'
  if (Test-Path $progPath) { $progTxt = Get-Content $progPath -Raw -ErrorAction SilentlyContinue }
  $drawFp = [bool]($progTxt -match 'drawfp_first_drawn|FIRST_REAL_FRAME|DrawFP')
  $members = @()
  [regex]::Matches($progTxt, '"details":"([^"]+\.bmp)"') | ForEach-Object { $members += $_.Groups[1].Value }
  $members = @($members | Select-Object -Unique)
  $resCount = $members.Count
  $spCsv = Join-Path $cellDir 'p8_sp_invariant.csv'
  if (Test-Path $spCsv) {
    $memberNames = @(Import-Csv $spCsv | Where-Object { $_.tag -eq 'LOOKUP_CALLSITE' -and $_.member_name } |
      Select-Object -ExpandProperty member_name -Unique)
    if ($memberNames.Count -gt 0) { $members = $memberNames; $resCount = $memberNames.Count }
  }
  $scan = $progTxt
  if (Test-Path $vmLog) { $scan += "`n" + (Get-Content $vmLog -Raw -ErrorAction SilentlyContinue) }
  $p3Fault = [bool]($scan -match 'P3_FAULT|p3_fault|READ_UNMAPPED|UC_ERR')
  $hit2e2520 = [bool]($scan -match '2E2520|2e2520|target=0x2E2521')
  $bcsDefault = [bool]($scan -match '2E4194|BCS|index_out')
  $lowMem = [bool]($scan -match 'PLATFORM_LOW_MEM_MAP')
  $fault1e205 = [bool]($scan -match 'addr=0x1E205|0x1E205')

  $row = [ordered]@{
    group = 'P12_lowmem'
    rep = $Rep
    hold_s = $HoldSeconds
    resume_mode = 'direct_lr'
    layer1_pass = $(if ($gatePass) { 'PASS' } else { 'FAIL' })
    layer1_fail_reasons = $gateFail
    layer1_sha256 = $layer1Sha
    non_black_pct = $nonBlack
    drawfp = $(if ($drawFp) { 'YES' } else { 'NO' })
    resource_count = $resCount
    resource_6th = $(if ($resCount -ge 6) { 'YES' } else { 'NO' })
    members = ($members -join '|')
    alive_at_end = $(if ($aliveAtEnd) { 'YES' } else { 'NO' })
    p3_fault = $(if ($p3Fault) { 'YES' } else { 'NO' })
    fault_1e205 = $(if ($fault1e205) { 'YES' } else { 'NO' })
    low_mem_map = $(if ($lowMem) { 'YES' } else { 'NO' })
    hit_2e2520 = $(if ($hit2e2520) { 'YES' } else { 'NO' })
    bcs_default = $(if ($bcsDefault) { 'YES' } else { 'NO' })
    main_exe_sha256 = (Get-Sha256 $MainExe)
    cell_dir = $cellDir
  }
  ($row | ConvertTo-Json -Depth 4) | Set-Content (Join-Path $cellDir 'cell_summary.json') -Encoding utf8
  Write-Host ("cell done layer1={0} res={1} alive={2} 2e2520={3} fault1e205={4}" -f `
    $row.layer1_pass, $resCount, $row.alive_at_end, $row.hit_2e2520, $row.fault_1e205)
  return [pscustomobject]$row
}

$rows = @()
for ($r = 1; $r -le $Reps; $r++) {
  Write-Host "===== REP $r / $Reps ====="
  $rows += Invoke-OneCell -Rep $r -HoldSeconds $HoldSeconds
}

$headers = @('group','rep','hold_s','resume_mode','layer1_pass','layer1_fail_reasons','layer1_sha256',
  'non_black_pct','drawfp','resource_count','resource_6th','members','alive_at_end','p3_fault',
  'fault_1e205','low_mem_map','hit_2e2520','bcs_default','main_exe_sha256','cell_dir')
$rows | Select-Object $headers | Export-Csv -Path $MatrixCsv -NoTypeInformation -Encoding UTF8
Write-Host "matrix -> $MatrixCsv"
$rows | Format-Table group,rep,layer1_pass,resource_count,resource_6th,alive_at_end,fault_1e205,hit_2e2520 -AutoSize
