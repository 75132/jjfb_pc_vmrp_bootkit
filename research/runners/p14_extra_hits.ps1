# Append extra P14 natural hits to existing matrix (SkipBuild).
param([int]$Seconds = 70, [int]$MaxRuns = 4, [int]$NeedHits = 1)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $Root
$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$matrix = Join-Path $reportDir 'p14_userinfo_run_matrix.csv'
$mainSha = (Get-FileHash -Algorithm SHA256 -Path $exe).Hash.ToLowerInvariant()

function Clear-CaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } | ForEach-Object {
    Remove-Item -Path ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
  }
}
function Analyze-Vm([string]$vm) {
  $all = if (Test-Path $vm) { Get-Content $vm -Raw -EA SilentlyContinue } else { '' }
  $enterN = ([regex]::Matches($all, '\[MR_GETUSERINFO_ENTER\]')).Count
  $leaveWrite = ([regex]::Matches($all, '\[MR_GETUSERINFO_LEAVE\].*status=0 bytes_written=64')).Count
  $leaveNull = ([regex]::Matches($all, '\[MR_GETUSERINFO_LEAVE\].*note=null_ptr')).Count
  $leaveFail = ([regex]::Matches($all, '\[MR_GETUSERINFO_LEAVE\].*status=-1')).Count
  $unimpl = [bool]($all -match 'mr_getUserInfo\(\) Not yet implemented')
  $sleepN = ([regex]::Matches($all, '\[JJFB_MR_SLEEP\]')).Count
  $sleepUnimpl = [bool]($all -match 'mr_sleep\(\) Not yet implemented')
  $nextUnimpl = [regex]::Matches($all, '\[POST_CONT_UNIMPLEMENTED_API\] api=(\S+)|!!! (\S+)\(\) Not yet implemented') |
    ForEach-Object { if ($_.Groups[1].Success) { $_.Groups[1].Value } elseif ($_.Groups[2].Success) { $_.Groups[2].Value } } |
    Select-Object -Unique
  $owner = ''; $m = [regex]::Match($all, '\[MR_GETUSERINFO_ENTER\] call_id=1 .* owner_module=(\S+)'); if ($m.Success) { $owner = $m.Groups[1].Value }
  $note = ''; $nm = [regex]::Match($all, '\[MR_GETUSERINFO_LEAVE\] call_id=1 .* note=(\S+)'); if ($nm.Success) { $note = $nm.Groups[1].Value }
  $hit = ($enterN -gt 0 -and ($leaveWrite + $leaveFail) -gt 0 -and -not $unimpl)
  return [ordered]@{
    applicable = $(if ($enterN -gt 0) { 'HIT' } else { 'NOT_APPLICABLE' })
    hit = $hit; enter_n = $enterN; leave_write = $leaveWrite; leave_null = $leaveNull
    leave_note = $(if ($note) { $note } else { 'NONE' })
    unimpl = $unimpl; owner_module = $(if ($owner) { $owner } else { 'NONE' })
    case9_leave = [bool]($all -match 'CASE9_LEAVE ok=1|DELIVER_DONE ok=1.*handler=0x30D311')
    mmochat = [bool]($all -match 'module=mmochat\.ext.*state=REGISTERED')
    package_alert = [bool]($all -match '\[MR_GETUSERINFO_PACKAGE_ALERT\]')
    wxjwq_active = [bool]($all -match '\[MR_GETUSERINFO_ENTER\].*current_mrp=\S*wxjwq')
    sleep_n = $sleepN; sleep_unimpl = $sleepUnimpl
    dsm_reinit = ([regex]::Matches($all, 'initMemoryManager:')).Count
    next_unimpl = $(if ($nextUnimpl) { ($nextUnimpl -join '|') } else { 'none' })
    p3_fault = [bool]($all -match '\[P3_FAULT\]|FIRE_DONE ok=0')
  }
}

$got = 0
for ($i = 1; $i -le $MaxRuns -and $got -lt $NeedHits; $i++) {
  Clear-CaseEnv
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
  $tag = "p14_x$i"
  $runId = ('{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $tag, (Get-Date), (Get-Random -Maximum 99999))
  $vmLog = Join-Path $logDir ("{0}_vmrp.txt" -f $tag)
  $stderr = Join-Path $logDir ("{0}_stderr.txt" -f $tag)
  Remove-Item -Force $vmLog, $stderr -EA SilentlyContinue
  $overlay = Join-Path $RunDir ("overlay_$runId")
  New-Item -ItemType Directory -Force -Path $overlay | Out-Null
  $param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_PROFILE = $Profile; $env:GWY_OVERLAY_ROOT = $overlay
  $env:GWY_PRODUCT_REPORTS_DIR = $reportDir; $env:GWY_PRODUCT_RUN_ID = $runId
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'; $env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'; $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_RUNAPP_NATIVE_ONLY = '0'; $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'; $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'; $env:JJFB_MODULE_REGISTRY_TRACE = '1'
  $env:JJFB_ROBOTOL_ENTRY_TRACE = '1'; $env:JJFB_MRC_INIT_TRACE = '1'; $env:JJFB_GAME_SELF_PATCH = '0'
  $env:GWY_MODULE_R9_SWITCH = '1'; $env:GWY_CALLBACK_FRAME = '1'; $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_PRODUCT_P3_MODE = '1'; $env:JJFB_PRODUCT_P4_MODE = '1'; $env:JJFB_PRODUCT_FFP_MODE = '1'
  $env:JJFB_PRODUCT_FFP_PHASE = 'event'; $env:JJFB_HWND_UNTIL_DISPUP = '1'; $env:JJFB_VISIBLE_WINDOW = '1'
  $env:JJFB_E9B_MODE = '1'; $env:JJFB_DISPLAY_FIRST = '1'; $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_LAUNCH = '1'; $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'; $env:GWY_LAUNCH_PARAM = $param
  Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI, Env:JJFB_PRODUCT_EVENT_CONTRACT, Env:JJFB_FAMILY_4F_FOR_E6C -EA SilentlyContinue
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru -RedirectStandardOutput $vmLog -RedirectStandardError $stderr
  $deadline = (Get-Date).AddSeconds($Seconds)
  do { Start-Sleep -Seconds 2 } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -EA SilentlyContinue; Start-Sleep -Milliseconds 400 }
  $a = Analyze-Vm $vmLog
  $row = @(
    "extra$i", $runId, $mainSha, $a.applicable, $(if ($a.hit) { 'yes' } else { 'no' }),
    $(if ($a.case9_leave) { 'yes' } else { 'no' }), $(if ($a.mmochat) { 'yes' } else { 'no' }),
    $a.enter_n, $a.leave_write, $a.leave_null, $a.leave_note,
    $(if ($a.unimpl) { 'yes' } else { 'no' }), $a.owner_module, 'NONE',
    $(if ($a.package_alert) { 'yes' } else { 'no' }), $(if ($a.wxjwq_active) { 'yes' } else { 'no' }),
    $a.sleep_n, $(if ($a.sleep_unimpl) { 'yes' } else { 'no' }), $a.dsm_reinit, $a.next_unimpl,
    $(if ($a.p3_fault) { 'yes' } else { 'no' }), $(if ($p.HasExited) { "$($p.ExitCode)" } else { 'killed' })
  ) -join ','
  Add-Content $matrix $row -Encoding utf8
  Write-Host ("extra{0}: applicable={1} hit={2} enter={3} sleep={4} next={5}" -f $i, $a.applicable, $a.hit, $a.enter_n, $a.sleep_n, $a.next_unimpl)
  if ($a.hit) { $got++ }
}
Write-Host ("extra hits gained={0} matrix={1}" -f $got, $matrix)
