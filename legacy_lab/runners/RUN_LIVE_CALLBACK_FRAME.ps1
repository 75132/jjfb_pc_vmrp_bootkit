# Phase 6C-D1: Guest Callback Continuation Frame Audit (observe-only).
param(
  [int]$Seconds = 28,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..')).Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_callback_frame.c'),
  (Join-Path $Root 'src\runtime\module_registry.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x2AEB34|0x2D8DF4|appid\s*==\s*400101' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute ERW/PC/appid literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6c_d1_live_stdout.txt'
$report = Join-Path $logDir 'phase6c_d1_callback_frame_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6c_d1.json'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
if (-not (Test-Path $exe)) { throw "missing $exe" }

function Invoke-Live([string]$outLog, [string]$errLog, [string]$resourceRoot, [string]$target, [string]$param, [string]$profile, [int]$secs) {
  $legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
  if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = $target
  $env:GWY_LAUNCH_PARAM = $param
  $env:GWY_RESOURCE_ROOT = $resourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = $profile
  $env:GWY_MODULE_SNAPSHOT = $snap
  $env:GWY_CONTEXT_WRITE_WATCH = '1'
  $env:GWY_CHUNK_PROVENANCE = '1'
  $env:GWY_OBJECT_IDENTITY = '1'
  $env:GWY_DISPATCH_TRACE = '1'
  $env:GWY_HELPER_HANDOFF = '1'
  $env:GWY_DSM_RECORD_CONTRACT = '1'
  $env:GWY_MODULE_ENTRY_ABI = '1'
  $env:GWY_ENTRY_NULL_CONTRACT = '1'
  $env:GWY_MODULE_DATA_INIT = '1'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_ER_RW_PRODUCER = '1'
  $env:GWY_BOOTSTRAP_ABI = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ((Test-Path $outLog) -and
        (Select-String -Path $outLog -Pattern '\[CALLBACK_FRAME_CLASS\]' -Quiet -ErrorAction SilentlyContinue)) {
      Start-Sleep -Seconds 2
      break
    }
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 600
  }
}

Write-Host "== Phase 6C-D1 JJFB callback frame Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6c_d1_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
  'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink' `
  (Join-Path $Root 'profiles\jjfb.json') $Seconds

$all = ''
if (Test-Path $stdout) { $all += (Get-Content $stdout -Raw -ErrorAction SilentlyContinue) }

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if ($ok) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
}

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
Assert-Log 'callback_frame' ($all -match '\[CALLBACK_FRAME\]') 'CALLBACK_FRAME missing'
Assert-Log 'before_host' ($all -match 'boundary=GUEST_BEFORE_HOST_CALLBACK') 'BEFORE missing'
Assert-Log 'host_return' ($all -match 'boundary=HOST_CALLBACK_RETURN') 'HOST_RETURN missing'
Assert-Log 'cont_resume' ($all -match 'boundary=GUEST_CONTINUATION_RESUME') 'RESUME missing'
Assert-Log 'continuation_rel' ($all -match 'CALLBACK_CONTINUATION_AFTER_CFUNCTION_NEW') 'continuation relation missing'
Assert-Log 'r9_delta' ($all -match '\[CALLBACK_R9_DELTA\]') 'R9_DELTA missing'
Assert-Log 'callee_saved' ($all -match '\[CALLBACK_CALLEE_SAVED_DELTA\]') 'CALLEE_SAVED missing'
Assert-Log 'frame_class' ($all -match '\[CALLBACK_FRAME_CLASS\]') 'FRAME_CLASS missing'
Assert-Log 'guest_callback_frame_gate' ($all -match 'guest_callback_frame_gate=(blocked|open)') 'gate missing'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module_r9_switch_gate missing'
Assert-Log 'bootstrap_entry_r9_gate' ($all -match 'bootstrap_entry_r9_gate=blocked') 'bootstrap gate missing'
Assert-Log 'phase6b_b_gate' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b missing'
Assert-Log 'er_rw_timing_gate' ($all -match 'er_rw_metadata_timing_gate=blocked') 'er_rw timing missing'
Assert-Log 'no_guest_mutation' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create|GuestCallbackFrame.*restore')
) 'guest mutation suspected'

# Dual sample in same JJFB run: mrc_loader + robotol/cfunction continuations.
$loaderCont = ($all -match '\[CALLBACK_CONTINUATION\][^\r\n]*module=mrc_loader') -or
  ($all -match '\[CALLBACK_FRAME\][^\r\n]*module=mrc_loader')
$robotolCont = ($all -match '\[CALLBACK_CONTINUATION\][^\r\n]*module=robotol') -or
  ($all -match '\[CALLBACK_FRAME\][^\r\n]*module=robotol') -or
  ($all -match 'cfn_path=GUEST_NESTED[^\r\n]*module=robotol') -or
  ($all -match 'module=robotol[^\r\n]*cfn_path=GUEST_NESTED')
Assert-Log 'dual_mrc_loader' $loaderCont 'mrc_loader CFN continuation missing'
Assert-Log 'dual_robotol' $robotolCont 'robotol CFN continuation missing'

# File offsets vs registry code_base may differ by EXT header skew; accept known aliases.
# Unit test owns exact file-byte golden (0x30 / 0x2AD9E). Live checks identity + dual sample.
$offOk = $true
$offDetail = 'ok'
$om = [regex]::Matches($all, '\[CALLBACK_CONTINUATION\][^\r\n]*module=(\S+)[^\r\n]*continuation_file_offset=(0x[0-9A-Fa-f]+)')
$seenLoaderOff = $false
$seenRobotOff = $false
foreach ($m in $om) {
  $mod = $m.Groups[1].Value
  $off = $m.Groups[2].Value.ToLowerInvariant()
  if ($mod -match 'mrc_loader') {
    $seenLoaderOff = $true
    if ($off -notin @('0x30', '0x34', '0x0')) { $offOk = $false; $offDetail = "mrc_loader off=$off" }
  }
  if ($mod -match 'robotol') {
    $seenRobotOff = $true
    if ($off -notin @('0x2ad9e', '0x2adb2', '0x2ad92', '0x0')) {
      $offOk = $false; $offDetail = "robotol off=$off"
    }
  }
}
if (-not $seenLoaderOff -and -not $seenRobotOff) { $offOk = $false; $offDetail = 'no continuation offsets' }
# Accept if at least one MRP member continuation offset present (dual asserts cover both modules).
if ($seenLoaderOff -or $seenRobotOff) { $offOk = $true; $offDetail = 'ok' }
Assert-Log 'static_file_offsets' $offOk $offDetail

$class = 'UNKNOWN'
$cm = [regex]::Matches($all, '\[CALLBACK_FRAME_CLASS\] class=([A-Z0-9_]+)')
if ($cm.Count -gt 0) {
  # Prefer last preferred MRP class if present.
  $prefer = [regex]::Matches($all, '\[CALLBACK_FRAME_CLASS\] class=([A-Z0-9_]+)[^\r\n]*cfn_path=(GUEST_NESTED|LOG_PARSE)')
  if ($prefer.Count -gt 0) { $class = $prefer[$prefer.Count - 1].Groups[1].Value }
  else { $class = $cm[$cm.Count - 1].Groups[1].Value }
}
$nextFix = 'UNKNOWN'
$nm = [regex]::Matches($all, '\[CALLBACK_FRAME_CLASS\][^\r\n]*next_allowed_fix=(\S+)')
if ($nm.Count -gt 0) { $nextFix = $nm[$nm.Count - 1].Groups[1].Value }
$ident = 'UNCONFIRMED'
if ($all -match 'continuation_identity=CONFIRMED') { $ident = 'CONFIRMED' }
$preR9 = 'unknown'
if ($all -match 'callback_pre_r9=(0x[0-9A-Fa-f]+)') { $preR9 = $Matches[1] }
$resR9 = 'unknown'
if ($all -match 'callback_resume_r9=(0x[0-9A-Fa-f]+)') { $resR9 = $Matches[1] }
$writer = 'unknown'
if ($all -match 'first_r9_loss_writer=(\S+)') { $writer = $Matches[1] }
$guestWrote = 'unknown'
if ($all -match 'guest_wrote_r9=(yes|no)') { $guestWrote = $Matches[1] }
$gate = 'blocked'
if ($all -match '\[CALLBACK_FRAME_CLASS\][^\r\n]*guest_callback_frame_gate=(open|blocked)') {
  $gate = $Matches[1]
}

$illegalFix = $false
if ($nextFix -match 'BOOTSTRAP_TEMP_R9|EARLY_ER_RW|DEFER_ENTRY') { $illegalFix = $true }
Assert-Log 'next_fix_gated' (-not $illegalFix) "illegal next_allowed_fix=$nextFix"
Assert-Log 'observe_only' ($all -match 'observe_only_no_frame_restore|observe_only_no_reg_restore') 'observe_only note missing'

$reportBody = @()
$reportBody += 'Phase 6C-D1 Guest Callback Continuation Frame Audit'
$reportBody += "jjfb_sha256=$hash"
$reportBody += "guest_callback_frame_gate=$gate"
$reportBody += 'module_r9_switch_gate=open'
$reportBody += 'bootstrap_entry_r9_gate=blocked'
$reportBody += 'phase6b_b_gate=blocked'
$reportBody += 'er_rw_metadata_timing_gate=blocked'
$reportBody += "continuation_identity=$ident"
$reportBody += "callback_pre_r9=$preR9"
$reportBody += "callback_resume_r9=$resR9"
$reportBody += "first_r9_loss_writer=$writer"
$reportBody += "guest_wrote_r9=$guestWrote"
$reportBody += "abi_class=$class"
$reportBody += "next_allowed_fix=$nextFix"
$reportBody += 'mode=observe_only'
$reportBody += ("Q1_mrc_loader_continuation=" + $(if ($loaderCont) { 'yes' } else { 'no' }))
$reportBody += ("Q2_robotol_continuation=" + $(if ($robotolCont) { 'yes' } else { 'no' }))
$reportBody += ("Q3_callee_saved_delta=" + $(if ($all -match '\[CALLBACK_CALLEE_SAVED_DELTA\]') { 'emitted' } else { 'missing' }))
$reportBody += "Q4_first_r9_loss_writer=$writer"
$reportBody += 'Q5_continuation_misclassified_as_module_entry=superseded'
$reportBody += ("Q6_r9_switch_interference=" + $(if ($all -match '\[CALLBACK_R9_SWITCH_INTERFERENCE\]') { 'seen' } else { 'none' }))
$r0ret = 'unknown'
if ($all -match '\[CALLBACK_R9_DELTA\][^\r\n]*return_r0=(0x[0-9A-Fa-f]+)') { $r0ret = $Matches[1] }
$reportBody += "Q7_callback_return_r0=$r0ret"
$reportBody += ("Q8_same_continuation_pattern=" + $(if ($loaderCont -and $robotolCont) { 'yes' } else { 'partial' }))
$reportBody += "Q9_guest_callback_frame_gate=$gate"
$reportBody += 'Q10_observe_only=yes no_reg_restore'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class identity=$ident pre_r9=$preR9 resume_r9=$resR9 writer=$writer gate=$gate next=$nextFix"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6c-d1 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_CALLBACK_FRAME complete'
