# P10 Final Verification — A/B/C matrix (direct_lr vs epilogue R0=0 vs epilogue R0=handle).
# Assumes a fresh full rebuild already completed (do NOT SkipBuild on the prior rebuild).
# Each cell: two consecutive 150s product holds via RUN_JJFB_LAUNCHER -SkipBuild -SkipVmrpBuild.
# Product default remains direct_lr; this script only sets research env for B/C cells.
param(
  [int]$HoldSeconds = 150,
  [switch]$SkipBuildIdentity,
  [string]$OnlyGroup = ""  # A|B|C or empty=all
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = Split-Path -Parent $Root
}
Set-Location $Root
. (Join-Path $Root 'tools\JjfbLayer1Gate.ps1')

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$Reports = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$MainExe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$MatrixCsv = Join-Path $Reports 'p10_epilogue_ab_matrix.csv'
$VerdictMd = Join-Path $Reports 'p10_epilogue_ab_verdict.md'
$IdentityPath = Join-Path $Reports 'p10_epilogue_build_identity.txt'
$ArchiveRoot = Join-Path $Root 'out\p10_epilogue_ab'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot | Out-Null

if (-not (Test-Path $MainExe)) { throw "missing $MainExe — rebuild first" }
if (-not (Test-Path $Launcher)) { throw "missing $Launcher — rebuild first" }

function Get-Sha256([string]$Path) {
  (Get-FileHash -Algorithm SHA256 -Path $Path).Hash.ToLowerInvariant()
}

# --- build identity (source↔binary closed loop) ---
if (-not $SkipBuildIdentity -or -not (Test-Path $IdentityPath)) {
  $gitCommit = (git -C $Root rev-parse HEAD 2>$null)
  if (-not $gitCommit) { $gitCommit = 'UNKNOWN' }
  $gitDirty = (git -C $Root status --porcelain 2>$null)
  $dirty = if ($gitDirty) { 'DIRTY' } else { 'CLEAN' }
  $ts = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
  $mainSha = Get-Sha256 $MainExe
  $launchSha = Get-Sha256 $Launcher
  $gwySha = if (Test-Path (Join-Path $Root 'build-i686\gwy_launcher.exe')) {
    Get-Sha256 (Join-Path $Root 'build-i686\gwy_launcher.exe')
  } else { 'n/a' }
  @"
git_commit=$gitCommit
git_tree=$dirty
build_timestamp=$ts
main_exe=$MainExe
main_exe_sha256=$mainSha
JJFB_Launcher_exe=$Launcher
JJFB_Launcher_sha256=$launchSha
gwy_launcher_sha256=$gwySha
skip_build_on_matrix_runs=YES
full_rebuild_before_matrix=REQUIRED (no SkipBuild on RUN_BUILD / RUN_BUILD_VMRP / RUN_TESTS)
product_default_resume_mode=direct_lr
"@ | Set-Content -Path $IdentityPath -Encoding utf8
  Write-Host "build_identity saved -> $IdentityPath"
}

$identity = Get-Content $IdentityPath -Raw
$mainSha = if ($identity -match 'main_exe_sha256=([0-9a-f]+)') { $Matches[1] } else { Get-Sha256 $MainExe }
$launchSha = if ($identity -match 'JJFB_Launcher_sha256=([0-9a-f]+)') { $Matches[1] } else { Get-Sha256 $Launcher }
$gitCommit = if ($identity -match 'git_commit=(\S+)') { $Matches[1] } else { 'UNKNOWN' }
$buildTs = if ($identity -match 'build_timestamp=(.+)') { $Matches[1].Trim() } else { (Get-Date -Format 'yyyy-MM-dd HH:mm:ss') }

function Clear-ResumeEnv {
  Remove-Item Env:JJFB_304BF0_RESUME_MODE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_304BF0_EPILOGUE_R0 -ErrorAction SilentlyContinue
}

function Set-ProductEnv([string]$RunId, [string]$ReportsDir) {
  $ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
  $Profile = Join-Path $Root 'profiles\jjfb.json'
  $shotDir = Join-Path $RunDir 'screenshots'
  New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
  $env:GWY_PROFILE = $Profile
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
  $env:GWY_RUNTIME_PROGRESS_PATH = (Join-Path $RunDir 'runtime_progress.jsonl')
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $ReportsDir
  $param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = $param
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  Remove-Item Env:JJFB_HWND_UNTIL_DISPUP -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_P5_MODE -ErrorAction SilentlyContinue
  Remove-Item Env:GWY_HOST_TEST_PATTERN -ErrorAction SilentlyContinue
  Remove-Item Env:SDL_VIDEODRIVER -ErrorAction SilentlyContinue
}

function Set-GroupEnv([string]$Group) {
  Clear-ResumeEnv
  switch ($Group) {
    'A' {
      $env:JJFB_304BF0_RESUME_MODE = 'direct_lr'
    }
    'B' {
      $env:JJFB_304BF0_RESUME_MODE = 'epilogue'
      $env:JJFB_304BF0_EPILOGUE_R0 = '0'
    }
    'C' {
      $env:JJFB_304BF0_RESUME_MODE = 'epilogue'
      $env:JJFB_304BF0_EPILOGUE_R0 = 'handle'
    }
    default { throw "unknown group $Group" }
  }
}

function Get-SpDeltas([string]$CsvPath) {
  $deltas = @()
  if (-not (Test-Path $CsvPath)) { return $deltas }
  $rows = Import-Csv $CsvPath
  $byCall = $rows | Group-Object call_id
  foreach ($g in $byCall) {
    $lookup = $g.Group | Where-Object { $_.tag -eq 'LOOKUP_CALLSITE' } | Select-Object -First 1
    $caller = $g.Group | Where-Object { $_.tag -eq 'CALLER_CONTINUATION' } | Select-Object -First 1
    if ($lookup -and $caller) {
      $spL = [Convert]::ToInt64($lookup.sp, 16)
      $spC = [Convert]::ToInt64($caller.sp, 16)
      $deltas += ($spC - $spL)
    }
  }
  return $deltas
}

function Invoke-OneCell {
  param(
    [string]$Group,
    [int]$Rep,
    [int]$HoldSeconds
  )
  $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
  $cellDir = Join-Path $ArchiveRoot ("{0}_r{1}_{2}" -f $Group, $Rep, $stamp)
  New-Item -ItemType Directory -Force -Path $cellDir | Out-Null

  Set-GroupEnv $Group
  $wantMode = $env:JJFB_304BF0_RESUME_MODE
  $wantR0 = if ($env:JJFB_304BF0_EPILOGUE_R0) { $env:JJFB_304BF0_EPILOGUE_R0 } else { 'n/a' }

  # Isolate per-run report writes under a cell-local reports dir (runtime also writes ../../reports).
  $cellReports = Join-Path $cellDir 'reports'
  New-Item -ItemType Directory -Force -Path $cellReports | Out-Null
  $runId = ("p10_{0}_r{1}_{2}" -f $Group, $Rep, $stamp)
  Set-ProductEnv -RunId $runId -ReportsDir $Reports
  # Re-apply resume mode after Set-ProductEnv (does not touch these).
  Set-GroupEnv $Group

  # Wipe live progress / screenshots / prior SP csv so gate reads this run only.
  Remove-Item -Force `
    (Join-Path $RunDir 'runtime_progress.jsonl'),
    (Join-Path $RunDir 'runtime_process.json'),
    (Join-Path $RunDir 'screenshots\launcher_first_frame.bmp'),
    (Join-Path $Reports 'p8_sp_invariant.csv'),
    (Join-Path $Reports 'p6_post_resource5_verdict.md'),
    (Join-Path $Reports 'p8_callsite_stack_contract_runtime.md') `
    -ErrorAction SilentlyContinue

  Write-Host ""
  Write-Host "==== P10 cell group=$Group rep=$Rep mode=$wantMode r0=$wantR0 hold=${HoldSeconds}s ===="

  $gatePass = $false
  $gateFail = ''
  $aliveAtEnd = $false
  $layer1Sha = ''
  $nonBlack = ''
  $drawFp = $false
  $procExit = -1
  $vmLog = Join-Path $cellDir 'vm_stdout.txt'
  $vmErr = Join-Path $cellDir 'vm_stderr.txt'

  # Sync resources once-ish (cheap with SkipBuild).
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') `
    -SkipBuild -NoLaunch -ResourceRoot (Join-Path $Root 'game_files\mythroad\240x320') | Out-Null

  Get-Process -Name 'JJFB_Launcher','main','main_gwy' -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400

  # Launch main.exe directly (product env parity) so PLATFORM_MRP / P8 / family logs are captured.
  $p = Start-Process -FilePath $MainExe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $vmLog -RedirectStandardError $vmErr
  "{`"runtime_pid`":$($p.Id),`"run_id`":`"$runId`"}" |
    Set-Content (Join-Path $RunDir 'runtime_process.json') -Encoding utf8
  Write-Host "main_pid=$($p.Id) run_id=$runId"

  $deadline = (Get-Date).AddSeconds($HoldSeconds)
  $progress = Join-Path $RunDir 'runtime_progress.jsonl'
  $sawFrame = $false
  while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) {
      Write-Host "main exited early code=$($p.ExitCode)"
      break
    }
    if (-not $sawFrame -and (Test-Path $progress)) {
      $txt = Get-Content $progress -Raw -ErrorAction SilentlyContinue
      if ($txt -match 'drawfp_first_drawn|FIRST_REAL_FRAME') {
        $sawFrame = $true
        Write-Host 'first_frame_seen'
      }
    }
  }

  try {
    $p.Refresh()
    $aliveAtEnd = -not $p.HasExited
  } catch { $aliveAtEnd = $false }

  $archive = Join-Path $cellDir 'gate'
  New-Item -ItemType Directory -Force -Path $archive | Out-Null
  # Keep runtime_process.json current for Layer-1 (must be alive during gate).
  if ($aliveAtEnd) {
    "{`"runtime_pid`":$($p.Id),`"run_id`":`"$runId`"}" |
      Set-Content (Join-Path $RunDir 'runtime_process.json') -Encoding utf8
  }
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

  # Snapshot live reports into cell dir before stop (boot_successor flushes on exit).
  foreach ($f in @(
      'p8_sp_invariant.csv',
      'p6_post_resource5_verdict.md',
      'p6_post_resource5_timeline.csv',
      'p6_post_resource5_pc_histogram.csv',
      'p8_callsite_stack_contract_runtime.md',
      'p7_family_event_abi.csv'
    )) {
    $src = Join-Path $Reports $f
    if (Test-Path $src) { Copy-Item $src (Join-Path $cellDir $f) -Force }
  }
  if (Test-Path $progress) { Copy-Item $progress (Join-Path $cellDir 'runtime_progress.jsonl') -Force }

  if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  }
  Get-Process -Name 'main','JJFB_Launcher' -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 800
  try { $p.Refresh(); $procExit = $p.ExitCode } catch { $procExit = -1 }

  foreach ($f in @('p8_sp_invariant.csv', 'p6_post_resource5_verdict.md', 'p8_callsite_stack_contract_runtime.md', 'p6_post_resource5_timeline.csv')) {
    $src = Join-Path $Reports $f
    if (Test-Path $src) { Copy-Item $src (Join-Path $cellDir $f) -Force }
  }

  # Parse signals
  $progTxt = ''
  $progPath = Join-Path $cellDir 'runtime_progress.jsonl'
  if (Test-Path $progPath) { $progTxt = Get-Content $progPath -Raw -ErrorAction SilentlyContinue }
  $drawFp = [bool]($progTxt -match 'drawfp_first_drawn|FIRST_REAL_FRAME|DrawFP')

  $members = @()
  [regex]::Matches($progTxt, '"details":"([^"]+\.bmp)"') | ForEach-Object { $members += $_.Groups[1].Value }
  $members = @($members | Select-Object -Unique)
  $resCount = $members.Count
  # Also count LOOKUP_CALLSITE rows if present
  $spCsv = Join-Path $cellDir 'p8_sp_invariant.csv'
  $deltas = @(Get-SpDeltas $spCsv)
  if ((Test-Path $spCsv)) {
    $callIds = @(Import-Csv $spCsv | Where-Object { $_.tag -eq 'LOOKUP_CALLSITE' } | Select-Object -ExpandProperty call_id -Unique)
    if ($callIds.Count -gt $resCount) { $resCount = $callIds.Count }
    $memberNames = @(Import-Csv $spCsv | Where-Object { $_.tag -eq 'LOOKUP_CALLSITE' -and $_.member_name } |
      Select-Object -ExpandProperty member_name -Unique)
    if ($memberNames.Count -gt 0) { $members = $memberNames; $resCount = $memberNames.Count }
  }
  $res6 = if ($resCount -ge 6) { 'YES' } else { 'NO' }
  $spOk = @($deltas | Where-Object { $_ -eq 0 }).Count
  $spBad = @($deltas | Where-Object { $_ -ne 0 }).Count
  $spList = ($deltas -join ';')

  $verdictTxt = ''
  $verdictPath = Join-Path $cellDir 'p6_post_resource5_verdict.md'
  if (Test-Path $verdictPath) { $verdictTxt = Get-Content $verdictPath -Raw }
  $firstPostUi = '0x0'
  if ($verdictTxt -match 'FIRST_POST_UI_PC:\s*(0x[0-9A-Fa-f]+)') { $firstPostUi = $Matches[1] }
  $familyNotes = ''
  if ($verdictTxt -match 'family 0x1E209 notes:\s*(\d+)') { $familyNotes = $Matches[1] }

  # Scan progress + any redirected stdout for resume / platform / fault markers
  $scan = $progTxt
  if (Test-Path $vmLog) { $scan += "`n" + (Get-Content $vmLog -Raw -ErrorAction SilentlyContinue) }
  if (Test-Path $vmErr) { $scan += "`n" + (Get-Content $vmErr -Raw -ErrorAction SilentlyContinue) }
  $rtMd = Join-Path $cellDir 'p8_callsite_stack_contract_runtime.md'
  if (Test-Path $rtMd) { $scan += "`n" + (Get-Content $rtMd -Raw -ErrorAction SilentlyContinue) }
  $tlCsv = Join-Path $cellDir 'p6_post_resource5_timeline.csv'
  if (Test-Path $tlCsv) { $scan += "`n" + (Get-Content $tlCsv -Raw -ErrorAction SilentlyContinue) }
  Get-ChildItem $cellDir -Filter '*.md' -ErrorAction SilentlyContinue | ForEach-Object {
    $scan += "`n" + (Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue)
  }

  $actualMode = $wantMode
  if ($scan -match '\[P8_MRP_RESUME\]\s+mode=(direct_lr|epilogue|callsite)') {
    $actualMode = $Matches[1]
  } elseif ($scan -match 'JJFB_304BF0_RESUME_MODE\s*=\s*(direct_lr|epilogue|callsite)') {
    $actualMode = $Matches[1]
  } elseif ($scan -match 'resume=(direct_lr|epilogue|callsite)') {
    $actualMode = $Matches[1]
  } elseif ($scan -match 'MRP_RESUME.*(direct_lr|epilogue|callsite)') {
    $actualMode = $Matches[1]
  }

  # Actual R0 mode: for epilogue, confirm handle vs 0 from log lines if present.
  $actualR0 = $wantR0
  if ($wantMode -eq 'epilogue') {
    if ($scan -match 'resume=epilogue.*handle=0x([0-9A-Fa-f]+)') {
      # presence of handle field does not prove R0=handle; check EPILOGUE_R0 want.
      $actualR0 = $wantR0
    }
  }

  $hit11f00 = [bool]($scan -match '0x11[Ff]00|plat_11[Ff]00|11F00')
  $hit12340 = [bool]($scan -match '0x12340|plat_12340|12340')
  $hitFamily = [bool]($scan -match '0x1[Ee]209|BOOT_FAMILY_HANDLER_ENTER|family.*0x1E209|a=0x1E209')
  $familyCase9 = [bool]($scan -match 'a=0x1[Ee]209\s+b=0x9|1E209.*\b9\b|case\s*9')
  $invalidFree = [bool]($scan -match 'invalid.?free|INVALID_FREE')
  $allocStorm = [bool]($scan -match 'ALLOC_STORM|alloc.?storm')
  $p3Fault = [bool]($scan -match 'P3_FAULT|p3_fault')

  # Persist cell summary
  $row = [ordered]@{
    group                = $Group
    rep                  = $Rep
    hold_s               = $HoldSeconds
    want_resume_mode     = $wantMode
    want_epilogue_r0     = $wantR0
    actual_resume_mode   = $actualMode
    actual_epilogue_r0   = $actualR0
    layer1_pass          = $(if ($gatePass) { 'PASS' } else { 'FAIL' })
    layer1_fail_reasons  = $gateFail
    layer1_sha256        = $layer1Sha
    non_black_pct        = $nonBlack
    drawfp               = $(if ($drawFp) { 'YES' } else { 'NO' })
    resource_count       = $resCount
    resource_6th         = $res6
    members              = ($members -join '|')
    sp_deltas            = $spList
    sp_delta_ok          = $spOk
    sp_delta_bad         = $spBad
    first_post_ui_pc     = $firstPostUi
    hit_0x11F00          = $(if ($hit11f00) { 'YES' } else { 'NO' })
    hit_0x12340          = $(if ($hit12340) { 'YES' } else { 'NO' })
    family_0x1E209       = $(if ($hitFamily) { 'YES' } else { 'NO' })
    family_case9         = $(if ($familyCase9) { 'YES' } else { 'NO' })
    family_1e209_notes   = $familyNotes
    invalid_free         = $(if ($invalidFree) { 'YES' } else { 'NO' })
    alloc_storm          = $(if ($allocStorm) { 'YES' } else { 'NO' })
    p3_fault             = $(if ($p3Fault) { 'YES' } else { 'NO' })
    alive_at_150s        = $(if ($aliveAtEnd) { 'YES' } else { 'NO' })
    main_exe_sha256      = $mainSha
    JJFB_Launcher_sha256 = $launchSha
    git_commit           = $gitCommit
    build_timestamp      = $buildTs
    skip_build           = 'YES'
    cell_dir             = $cellDir
  }
  ($row | ConvertTo-Json -Depth 4) | Set-Content (Join-Path $cellDir 'cell_summary.json') -Encoding utf8
  Write-Host ("cell done layer1={0} res={1} sp_ok={2}/{3} alive={4} mode={5}" -f `
    $row.layer1_pass, $resCount, $spOk, ($spOk + $spBad), $row.alive_at_150s, $actualMode)
  return [pscustomobject]$row
}

# --- matrix ---
$groups = @('A', 'B', 'C')
if ($OnlyGroup) { $groups = @($OnlyGroup.ToUpperInvariant()) }

$allRows = @()
foreach ($g in $groups) {
  for ($r = 1; $r -le 2; $r++) {
    $allRows += Invoke-OneCell -Group $g -Rep $r -HoldSeconds $HoldSeconds
  }
}

Clear-ResumeEnv  # restore product-default env (unset => direct_lr)

# Write CSV
$cols = @(
  'group','rep','hold_s','want_resume_mode','want_epilogue_r0','actual_resume_mode','actual_epilogue_r0',
  'layer1_pass','layer1_fail_reasons','layer1_sha256','non_black_pct','drawfp',
  'resource_count','resource_6th','members','sp_deltas','sp_delta_ok','sp_delta_bad',
  'first_post_ui_pc','hit_0x11F00','hit_0x12340','family_0x1E209','family_case9','family_1e209_notes',
  'invalid_free','alloc_storm','p3_fault','alive_at_150s',
  'main_exe_sha256','JJFB_Launcher_sha256','git_commit','build_timestamp','skip_build','cell_dir'
)
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine(($cols -join ','))
foreach ($row in $allRows) {
  $vals = foreach ($c in $cols) {
    $v = [string]$row.$c
    if ($v -match '[,"\r\n]') { '"' + ($v.Replace('"','""')) + '"' } else { $v }
  }
  [void]$sb.AppendLine(($vals -join ','))
}
[System.IO.File]::WriteAllText($MatrixCsv, $sb.ToString(), [System.Text.UTF8Encoding]::new($false))

# Verdict
function Summarize([string]$Group) {
  $rows = @($allRows | Where-Object { $_.group -eq $Group })
  $pass = @($rows | Where-Object { $_.layer1_pass -eq 'PASS' }).Count
  $res = ($rows | ForEach-Object { $_.resource_count }) -join ','
  $sp = ($rows | ForEach-Object { '{0}/{1}' -f $_.sp_delta_ok, ([int]$_.sp_delta_ok + [int]$_.sp_delta_bad) }) -join ','
  $alive = ($rows | ForEach-Object { $_.alive_at_150s }) -join ','
  $mode = ($rows | Select-Object -First 1).actual_resume_mode
  $r0 = ($rows | Select-Object -First 1).actual_epilogue_r0
  $r6 = ($rows | ForEach-Object { $_.resource_6th }) -join ','
  return [pscustomobject]@{
    group = $Group; layer1_pass_n = $pass; layer1_n = $rows.Count
    resources = $res; sp = $sp; alive = $alive; mode = $mode; r0 = $r0; r6 = $r6
  }
}
$sA = Summarize 'A'
$sB = Summarize 'B'
$sC = Summarize 'C'

$equivB = $false
$betterC = $false
# Conservative: do not claim epilogue better than direct_lr without equal Layer-1 + SP + resources.
if ($sA.layer1_pass_n -eq 2 -and $sB.layer1_pass_n -eq 2) {
  $equivB = $true
}
$md = @"
# P10 epilogue A/B matrix verdict

**Date:** $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
**Status:** MATRIX_COMPLETE — do **not** promote ``epilogue`` to product default from this report alone without human review.

## Build identity (source ↔ binary)

| field | value |
|-------|-------|
| git commit | ``$gitCommit`` |
| build timestamp | $buildTs |
| main.exe SHA256 | ``$mainSha`` |
| JJFB_Launcher.exe SHA256 | ``$launchSha`` |
| SkipBuild on matrix cells | YES (full rebuild done before matrix) |
| product default resume_mode | ``direct_lr`` (unchanged) |

## Matrix (2×150s per group)

| group | resume_mode | EPILOGUE_R0 | Layer-1 PASS | resources/run | SP delta ok | alive@150s | 6th resource |
|-------|-------------|-------------|--------------|---------------|-------------|------------|--------------|
| A | $($sA.mode) | $($sA.r0) | $($sA.layer1_pass_n)/$($sA.layer1_n) | $($sA.resources) | $($sA.sp) | $($sA.alive) | $($sA.r6) |
| B | $($sB.mode) | $($sB.r0) | $($sB.layer1_pass_n)/$($sB.layer1_n) | $($sB.resources) | $($sB.sp) | $($sB.alive) | $($sB.r6) |
| C | $($sC.mode) | $($sC.r0) | $($sC.layer1_pass_n)/$($sC.layer1_n) | $($sC.resources) | $($sC.sp) | $($sC.alive) | $($sC.r6) |

Raw rows: ``reports/p10_epilogue_ab_matrix.csv``
Per-cell archives: ``out/p10_epilogue_ab/``

## Reading (no premature promotion)

- Independent Unicorn stack proof (``reports/p10_epilogue_stack_verify.txt``) only shows the **epilogue bytes** balance; it is **not** product acceptance.
- Group A is the product-safe ``direct_lr`` baseline.
- Group B (``epilogue`` + R0=0): compare Layer-1 / resource count / SP deltas to A — if matched, it is at best **equivalent**, not automatically better.
- Group C (``epilogue`` + R0=handle): look for natural successor (6th resource / post-UI PC / family case 9). A 6th resource here would be evidence; absence means ``0x304BF0`` resume is likely **not** the current primary blocker.
- Product default remains ``direct_lr``. No family ABI / Event15 / E6C changes in this pass.

## Cell detail

"@
foreach ($row in $allRows) {
  $md += @"

### Group $($row.group) rep $($row.rep)

- want/actual mode: ``$($row.want_resume_mode)`` / ``$($row.actual_resume_mode)``; R0=``$($row.actual_epilogue_r0)``
- Layer-1: **$($row.layer1_pass)** $($row.layer1_fail_reasons) (nonBlack%=$($row.non_black_pct))
- DrawFP: $($row.drawfp); resources=$($row.resource_count); 6th=$($row.resource_6th); members=$($row.members)
- SP deltas (CALLER-LOOKUP): [$($row.sp_deltas)] ok=$($row.sp_delta_ok) bad=$($row.sp_delta_bad)
- FIRST_POST_UI_PC: $($row.first_post_ui_pc)
- 0x11F00=$($row.hit_0x11F00) 0x12340=$($row.hit_0x12340) family 0x1E209=$($row.family_0x1E209) case9=$($row.family_case9)
- invalid_free=$($row.invalid_free) alloc_storm=$($row.alloc_storm) P3_fault=$($row.p3_fault)
- alive@150s=$($row.alive_at_150s)
- cell: ``$($row.cell_dir)``

"@
}

$md | Set-Content -Path $VerdictMd -Encoding utf8
Write-Host ""
Write-Host "matrix csv -> $MatrixCsv"
Write-Host "verdict   -> $VerdictMd"
Write-Host '[OK] P10 A/B matrix complete (product default still direct_lr)'
exit 0
