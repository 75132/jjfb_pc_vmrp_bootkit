# P14 JJFB: 11F00 text resolve + low-mem baseline, 180s x2
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
$ArchiveRoot = Join-Path $Root 'out\p14_text_ab'
$MatrixCsv = Join-Path $Reports 'JJFB_NEXT_SCREEN_MATRIX.csv'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot | Out-Null

function Get-Sha256([string]$Path) { (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant() }

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
  $env:JJFB_HELPER_2F68E4_TRACE = '1'
  $env:JJFB_LIFECYCLE_RECORD_TRACE = '1'
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
}

function Invoke-OneCell([int]$Rep, [int]$HoldSeconds) {
  $ts = Get-Date -Format 'yyyyMMdd_HHmmss'
  $runId = "p14_r${Rep}_$ts"
  $cellDir = Join-Path $ArchiveRoot "r${Rep}_$ts"
  New-Item -ItemType Directory -Force -Path $cellDir | Out-Null
  $vmLog = Join-Path $cellDir 'vm_stdout.txt'
  $vmErr = Join-Path $cellDir 'vm_stderr.txt'
  Set-ProductEnv $runId
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch -ResourceRoot (Join-Path $Root 'game_files\mythroad\240x320') | Out-Null
  Get-Process -Name 'JJFB_Launcher','main','main_gwy' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400
  $p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir -PassThru -RedirectStandardOutput $vmLog -RedirectStandardError $vmErr
  "{`"runtime_pid`":$($p.Id),`"run_id`":`"$runId`"}" | Set-Content (Join-Path $RunDir 'runtime_process.json') -Encoding utf8
  Write-Host "main_pid=$($p.Id) run_id=$runId"
  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) { Write-Host "early exit $($p.ExitCode)"; break }
  }
  try { $p.Refresh(); $aliveAtEnd = -not $p.HasExited } catch { $aliveAtEnd = $false }
  if ($aliveAtEnd) {
    "{`"runtime_pid`":$($p.Id),`"run_id`":`"$runId`"}" | Set-Content (Join-Path $RunDir 'runtime_process.json') -Encoding utf8
  }
  $gatePass = $false; $gateFail = ''; $layer1Sha = ''; $nonBlack = ''
  try {
    $gate = Test-JjfbLayer1Gate -RunDir $RunDir -ArchiveDir (Join-Path $cellDir 'gate') -Require240:$true
    $gatePass = [bool]$gate.pass
    if (-not $gatePass) { $gateFail = ($gate.fail_reasons -join ',') }
    if ($gate.bmp -and $gate.bmp.sha256) { $layer1Sha = $gate.bmp.sha256 }
    if ($gate.bmp -and $null -ne $gate.bmp.nonBlackPct) { $nonBlack = "$($gate.bmp.nonBlackPct)" }
  } catch { $gatePass = $false; $gateFail = $_.Exception.Message }
  if (Test-Path (Join-Path $RunDir 'runtime_progress.jsonl')) {
    Copy-Item (Join-Path $RunDir 'runtime_progress.jsonl') (Join-Path $cellDir 'runtime_progress.jsonl') -Force
  }
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
  Get-Process -Name 'main','JJFB_Launcher' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 600

  $progTxt = ''
  if (Test-Path (Join-Path $cellDir 'runtime_progress.jsonl')) {
    $progTxt = Get-Content (Join-Path $cellDir 'runtime_progress.jsonl') -Raw -ErrorAction SilentlyContinue
  }
  $scan = $progTxt
  if (Test-Path $vmLog) { $scan += "`n" + (Get-Content $vmLog -Raw -ErrorAction SilentlyContinue) }
  $members = @()
  [regex]::Matches($progTxt, '"details":"([^"]+\.bmp)"') | ForEach-Object { $members += $_.Groups[1].Value }
  $members = @($members | Select-Object -Unique)
  $textDrawn = [regex]::Matches($scan, 'PLATFORM_11F00\] handled=1').Count
  $textApp7 = [bool]($scan -match 'handled=1 app=0x7')
  $textHex = ''
  if ($scan -match 'handled=1 app=0x7 str_va=0x[0-9A-Fa-f]+.*?evidence') { }
  if ($scan -match 'app=0x7 code_obj=0x[0-9A-Fa-f]+.*?hex=([0-9A-Fa-f]+)') { $textHex = $Matches[1] }
  $row = [ordered]@{
    group = 'P14_text'
    rep = $Rep
    hold_s = $HoldSeconds
    resume_mode = 'direct_lr'
    layer1_pass = $(if ($gatePass) { 'PASS' } else { 'FAIL' })
    layer1_fail_reasons = $gateFail
    layer1_sha256 = $layer1Sha
    non_black_pct = $nonBlack
    drawfp = $(if ($progTxt -match 'drawfp_first_drawn') { 'YES' } else { 'NO' })
    resource_count = $members.Count
    resource_6th = $(if ($members.Count -ge 6) { 'YES' } else { 'NO' })
    members = ($members -join '|')
    alive_at_end = $(if ($aliveAtEnd) { 'YES' } else { 'NO' })
    text_11f00_handled = $textDrawn
    text_app7 = $(if ($textApp7) { 'YES' } else { 'NO' })
    text_app7_hex = $textHex
    hit_2e2520 = $(if ($scan -match '2E2520|2e2520|target=0x2E2521') { 'YES' } else { 'NO' })
    low_mem_map = $(if ($scan -match 'PLATFORM_LOW_MEM_MAP') { 'YES' } else { 'NO' })
    p3_fault = $(if ($scan -match 'P3_FAULT|READ_UNMAPPED') { 'YES' } else { 'NO' })
    main_exe_sha256 = (Get-Sha256 $MainExe)
    cell_dir = $cellDir
  }
  ($row | ConvertTo-Json -Depth 4) | Set-Content (Join-Path $cellDir 'cell_summary.json') -Encoding utf8
  Write-Host ("cell layer1={0} res={1} text7={2} hex={3} sha={4}" -f $row.layer1_pass, $members.Count, $row.text_app7, $textHex, $layer1Sha)
  return [pscustomobject]$row
}

$rows = @()
for ($r = 1; $r -le $Reps; $r++) {
  Write-Host "===== REP $r / $Reps ====="
  $rows += Invoke-OneCell -Rep $r -HoldSeconds $HoldSeconds
}
$headers = @('group','rep','hold_s','resume_mode','layer1_pass','layer1_fail_reasons','layer1_sha256','non_black_pct','drawfp','resource_count','resource_6th','members','alive_at_end','text_11f00_handled','text_app7','text_app7_hex','hit_2e2520','low_mem_map','p3_fault','main_exe_sha256','cell_dir')
$rows | Select-Object $headers | Export-Csv -Path $MatrixCsv -NoTypeInformation -Encoding UTF8
Write-Host "matrix -> $MatrixCsv"
$rows | Format-Table group,rep,layer1_pass,resource_count,text_app7,text_app7_hex,alive_at_end -AutoSize
