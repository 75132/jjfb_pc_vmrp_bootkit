# P14 — mr_getUserInfo natural hits (frozen product ABI)
param(
  [int]$DiagSeconds = 45,
  [int]$HitSeconds = 90,
  [int]$MaxRuns = 5,
  [int]$NeedHits = 3,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\p14'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$matrix = Join-Path $reportDir 'p14_userinfo_run_matrix.csv'
$identity = Join-Path $outDir 'p14_build_identity.txt'

New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

function Get-Sha([string]$p) {
  if (-not (Test-Path $p)) { return 'MISSING' }
  return (Get-FileHash -Algorithm SHA256 -Path $p).Hash.ToLowerInvariant()
}
function Clear-CaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } | ForEach-Object {
    Remove-Item -Path ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
  }
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
}

$sourceCommit = (git rev-parse HEAD).Trim()
$mainSha = Get-Sha $exe
$jjfbSha = Get-Sha $JJFB
$gwySha = Get-Sha $Launcher
$compiler = & gcc -dumpversion 2>$null
@"
source_commit=$sourceCommit
source_tree_clean=$(-not [bool]@(git status --porcelain -- 'src' 'include' 'third_party/vmrp_upstream/bridge.c' 'CMakeLists.txt' 'tests'))
build_time_utc=$((Get-Item $exe).LastWriteTimeUtc.ToString('o'))
main_exe_sha256=$mainSha
JJFB_Launcher_exe_sha256=$jjfbSha
gwy_launcher_exe_sha256=$gwySha
compiler=gcc-$compiler
product_default_return_mode=direct_lr
product_ffp_apply_abi=0
JJFB_PRODUCT_EVENT_CONTRACT=0
JJFB_FAMILY_4F_FOR_E6C=0
"@ | Set-Content $identity -Encoding utf8
Write-Host '=== P14 identity ==='; Get-Content $identity

function Invoke-NaturalRun([string]$tag, [int]$seconds) {
  Clear-CaseEnv
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
  $runId = ('{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $tag, (Get-Date), (Get-Random -Maximum 99999))
  $stdout = Join-Path $logDir ("{0}_stdout.txt" -f $tag)
  $stderr = Join-Path $logDir ("{0}_stderr.txt" -f $tag)
  $vmLog = Join-Path $logDir ("{0}_vmrp.txt" -f $tag)
  @($stdout, $stderr, $vmLog) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }
  $overlay = Join-Path $RunDir ("overlay_$runId")
  New-Item -ItemType Directory -Force -Path $overlay | Out-Null
  $param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'

  $env:GWY_PROFILE = $Profile
  $env:GWY_OVERLAY_ROOT = $overlay
  $env:GWY_PRODUCT_REPORTS_DIR = $reportDir
  $env:GWY_PRODUCT_RUN_ID = $runId
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_RUNAPP_NATIVE_ONLY = '0'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:JJFB_MODULE_REGISTRY_TRACE = '1'
  $env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
  $env:JJFB_MRC_INIT_TRACE = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_PRODUCT_P3_MODE = '1'
  $env:JJFB_PRODUCT_P4_MODE = '1'
  $env:JJFB_PRODUCT_FFP_MODE = '1'
  $env:JJFB_PRODUCT_FFP_PHASE = 'event'
  $env:JJFB_HWND_UNTIL_DISPUP = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = $param
  Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_EVENT_CONTRACT -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAMILY_4F_FOR_E6C -ErrorAction SilentlyContinue

  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $vmLog -RedirectStandardError $stderr
  $deadline = (Get-Date).AddSeconds($seconds)
  do {
    Start-Sleep -Seconds 2
    if (Test-Path $vmLog) {
      Get-Content $vmLog -Tail 800 -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8
    }
  } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
  if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 400
  }
  if (Test-Path $vmLog) { Get-Content $vmLog -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8 }
  return @{ runId = $runId; stdout = $stdout; vmLog = $vmLog; exit = $(if ($p.HasExited) { "$($p.ExitCode)" } else { 'killed' }) }
}

function Analyze-Log([string]$path) {
  $all = if (Test-Path $path) { Get-Content $path -Raw -EA SilentlyContinue } else { '' }
  # Deduplicate: runner appends overlapping tails into stdout.
  $vmOnly = if ($path -match '_stdout\.txt$') {
    $vm = $path -replace '_stdout\.txt$', '_vmrp.txt'
    if (Test-Path $vm) { Get-Content $vm -Raw -EA SilentlyContinue } else { $all }
  } else { $all }
  $enterN = ([regex]::Matches($vmOnly, '\[MR_GETUSERINFO_ENTER\]')).Count
  $leaveWrite = ([regex]::Matches($vmOnly, '\[MR_GETUSERINFO_LEAVE\].*status=0 bytes_written=64')).Count
  $leaveNull = ([regex]::Matches($vmOnly, '\[MR_GETUSERINFO_LEAVE\].*note=null_ptr')).Count
  $leaveFail = ([regex]::Matches($vmOnly, '\[MR_GETUSERINFO_LEAVE\].*status=-1')).Count
  $unimpl = [bool]($vmOnly -match 'mr_getUserInfo\(\) Not yet implemented')
  $pkgAlert = [bool]($vmOnly -match '\[MR_GETUSERINFO_PACKAGE_ALERT\]')
  $wxActive = [bool]($vmOnly -match '\[MR_GETUSERINFO_ENTER\].*current_mrp=\S*wxjwq')
  $sleepN = ([regex]::Matches($vmOnly, '\[JJFB_MR_SLEEP\]')).Count
  $sleepUnimpl = [bool]($vmOnly -match 'mr_sleep\(\) Not yet implemented')
  $nextUnimpl = [regex]::Matches($vmOnly, '\[POST_CONT_UNIMPLEMENTED_API\] api=(\S+)|!!! (\S+)\(\) Not yet implemented') |
    ForEach-Object {
      if ($_.Groups[1].Success) { $_.Groups[1].Value }
      elseif ($_.Groups[2].Success) { $_.Groups[2].Value }
    } | Select-Object -Unique
  $owner = ''
  $m = [regex]::Match($vmOnly, '\[MR_GETUSERINFO_ENTER\] call_id=1 .* owner_module=(\S+)')
  if ($m.Success) { $owner = $m.Groups[1].Value }
  $sha = ''
  $ms = [regex]::Match($vmOnly, '\[MR_GETUSERINFO_LEAVE\] call_id=1 .* blob_sha256=([0-9a-f]{64})')
  if ($ms.Success) { $sha = $ms.Groups[1].Value }
  $note = ''
  $nm = [regex]::Match($vmOnly, '\[MR_GETUSERINFO_LEAVE\] call_id=1 .* note=(\S+)')
  if ($nm.Success) { $note = $nm.Groups[1].Value }
  # Contract hit: real ENTER+LEAVE and not unimplemented (NULL→MR_FAILED is valid per dsm.c).
  $hit = ($enterN -gt 0 -and ($leaveWrite + $leaveFail) -gt 0 -and -not $unimpl)
  $applicable = if ($enterN -gt 0) { 'HIT' } else { 'NOT_APPLICABLE' }
  return [ordered]@{
    applicable = $applicable
    hit = $hit
    case9_leave = [bool]($vmOnly -match 'CASE9_LEAVE ok=1|DELIVER_DONE ok=1.*handler=0x30D311')
    mmochat = [bool]($vmOnly -match 'module=mmochat\.ext.*state=REGISTERED')
    enter_n = $enterN
    leave_write = $leaveWrite
    leave_null = $leaveNull
    leave_fail = $leaveFail
    leave_note = $(if ($note) { $note } else { 'NONE' })
    unimpl = $unimpl
    owner_module = $(if ($owner) { $owner } else { 'NONE' })
    blob_sha256 = $(if ($sha) { $sha } else { 'NONE' })
    package_alert = $pkgAlert
    wxjwq_active = $wxActive
    sleep_n = $sleepN
    sleep_unimpl = $sleepUnimpl
    dsm_reinit = ([regex]::Matches($vmOnly, 'initMemoryManager:')).Count
    next_unimpl = $(if ($nextUnimpl) { ($nextUnimpl -join '|') } else { 'none' })
    p3_fault = [bool]($vmOnly -match '\[P3_FAULT\]|FIRE_DONE ok=0')
  }
}

'run_kind,run_id,main_sha,applicable,hit,case9_leave,mmochat,enter_n,leave_write,leave_null,leave_note,unimpl,owner_module,blob_sha256,package_alert,wxjwq_active,sleep_n,sleep_unimpl,dsm_reinit,next_unimpl,p3_fault,proc' |
  Set-Content $matrix -Encoding utf8

function Add-MatrixRow($kind, $runId, $a, $proc) {
  $row = @(
    $kind, $runId, $mainSha,
    $a.applicable,
    $(if ($a.hit) { 'yes' } else { 'no' }),
    $(if ($a.case9_leave) { 'yes' } else { 'no' }),
    $(if ($a.mmochat) { 'yes' } else { 'no' }),
    $a.enter_n, $a.leave_write, $a.leave_null, $a.leave_note,
    $(if ($a.unimpl) { 'yes' } else { 'no' }),
    $a.owner_module, $a.blob_sha256,
    $(if ($a.package_alert) { 'yes' } else { 'no' }),
    $(if ($a.wxjwq_active) { 'yes' } else { 'no' }),
    $a.sleep_n,
    $(if ($a.sleep_unimpl) { 'yes' } else { 'no' }),
    $a.dsm_reinit,
    $a.next_unimpl,
    $(if ($a.p3_fault) { 'yes' } else { 'no' }),
    $proc
  ) -join ','
  Add-Content $matrix $row -Encoding utf8
}

Write-Host '==== P14 DIAG ===='
$diag = Invoke-NaturalRun -tag 'p14_diag' -seconds $DiagSeconds
$da = Analyze-Log $diag.stdout
$da.GetEnumerator() | ForEach-Object { Write-Host ("  {0}={1}" -f $_.Key, $_.Value) }
$da | ConvertTo-Json | Set-Content (Join-Path $outDir 'p14_diag_summary.json') -Encoding utf8
Add-MatrixRow 'diag' $diag.runId $da $diag.exit

$hits = @()
if ($da.hit) { $hits += $da }

$run = 1
while ($hits.Count -lt $NeedHits -and $run -le $MaxRuns) {
  Write-Host ("==== P14 HIT RUN {0}/{1} (have {2}/{3}) ====" -f $run, $MaxRuns, $hits.Count, $NeedHits)
  if ((Get-Sha $exe) -ne $mainSha) { throw 'binary drift' }
  $r = Invoke-NaturalRun -tag ("p14_hit{0}" -f $run) -seconds $HitSeconds
  $a = Analyze-Log $r.stdout
  Add-MatrixRow ("hit$run") $r.runId $a $r.exit
  Write-Host ("  applicable={0} hit={1} enter={2} leave_null={3} sleep_n={4} next={5}" -f $a.applicable, $a.hit, $a.enter_n, $a.leave_null, $a.sleep_n, $a.next_unimpl)
  if ($a.hit) { $hits += $a }
  $run++
}

Write-Host ("P14 hits={0}/{1} matrix={2} identity={3}" -f $hits.Count, $NeedHits, $matrix, $identity)
if ($hits.Count -lt $NeedHits) {
  Write-Host '[P14] fewer than NeedHits — matrix still written; classify remaining as NOT_APPLICABLE in verdict'
}
