# Phase 6C-B: ER_RW Producer Timing & First-Entry Ordering (observe-only).
param(
  [int]$Seconds = 20,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_er_rw_producer.c'),
  (Join-Path $Root 'src\runtime\module_r9_switch.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x2D8DE0|appid\s*==\s*400101' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute ERW/PC/appid literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6c_b_live_stdout.txt'
$report = Join-Path $logDir 'phase6c_b_er_rw_timing_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6c_b.json'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
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
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  Start-Sleep -Seconds $secs
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 400
  }
}

Write-Host "== Phase 6C-B JJFB ER_RW producer timing Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6c_b_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'er_rw_timeline' ($all -match '\[ER_RW_TIMELINE\]') 'ER_RW_TIMELINE missing'
Assert-Log 'cfunction_load_contract' ($all -match '\[CFUNCTION_LOAD_CONTRACT\]') 'CFUNCTION_LOAD_CONTRACT missing'
Assert-Log 'r9_switch_blocked_outcome' ($all -match '\[R9_SWITCH_BLOCKED_OUTCOME\]') 'R9_SWITCH_BLOCKED_OUTCOME missing'
Assert-Log 'entry_executed_with_caller_r9' (
  $all -match 'SWITCH_BLOCKED_ENTRY_EXECUTED_WITH_CALLER_R9'
) 'EXECUTED_WITH_CALLER_R9 missing'
Assert-Log 'entry_not_deferred' ($all -match 'entry_deferred=no') 'entry_deferred=no missing'
Assert-Log 'pre_entry_meta' ($all -match '\[ER_RW_PRE_ENTRY_META\]') 'ER_RW_PRE_ENTRY_META missing'
Assert-Log 'entry_identity' ($all -match '\[ENTRY_IDENTITY\]') 'ENTRY_IDENTITY missing'
Assert-Log 'er_rw_timing_class' ($all -match '\[ER_RW_TIMING_CLASS\]') 'ER_RW_TIMING_CLASS missing'
Assert-Log 'er_rw_metadata_timing_gate' (
  $all -match 'er_rw_metadata_timing_gate=blocked'
) 'er_rw_metadata_timing_gate missing'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module_r9_switch_gate missing'
Assert-Log 'phase6b_b_gate' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b_gate missing'
Assert-Log 'no_guest_mutation' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create')
) 'guest mutation suspected'
Assert-Log 'switch_not_entry_fail_closed' (
  $all -match 'switch_fail_closed_not_entry_fail_closed'
) 'switch_fail_closed note missing'

# Cross-target wxjwq
$xtOut = Join-Path $logDir 'phase6c_b_xt_wxjwq_stdout.txt'
$xtPath = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
$xtPattern = 'ABSENT'
if (Test-Path $xtPath) {
  Write-Host '== Phase 6C-B cross-target wxjwq =='
  Invoke-Live $xtOut (Join-Path $logDir 'phase6c_b_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xt = ''
  if (Test-Path $xtOut) { $xt = Get-Content $xtOut -Raw -ErrorAction SilentlyContinue }
  Assert-Log 'xt_wxjwq' ($xt.Length -gt 50) 'wxjwq log empty'
  if ($xt -match '\[XT_ER_RW_ORDER\]|\[ER_RW_TIMELINE\]|\[R9_SWITCH_BLOCKED_OUTCOME\]') {
    $xtPattern = 'PARTIAL'
  }
  if ($xt -match '\[XT_ER_RW_ORDER\][^\r\n]*pattern=([ABC]|PARTIAL|UNKNOWN)') {
    $xtPattern = $Matches[1]
  }
} else {
  Assert-Log 'xt_wxjwq' $true 'skipped missing'
}

$class = 'UNKNOWN'
# Prefer focus nested EXT (robotol) over loader/DSM class lines.
$cm = [regex]::Matches($all, '\[ER_RW_TIMING_CLASS\] class=([A-Z0-9_]+)[^\r\n]*module=robotol\.ext')
if ($cm.Count -eq 0) {
  $cm = [regex]::Matches($all, '\[ER_RW_TIMING_CLASS\] class=([A-Z0-9_]+)')
}
if ($cm.Count -gt 0) { $class = $cm[$cm.Count - 1].Groups[1].Value }
$nextFix = 'UNKNOWN'
$nm = [regex]::Matches($all, '\[ER_RW_TIMING_CLASS\][^\r\n]*module=robotol\.ext[^\r\n]*next_allowed_fix=(\S+)')
if ($nm.Count -eq 0) {
  $nm = [regex]::Matches($all, '\[ER_RW_TIMING_CLASS\][^\r\n]*next_allowed_fix=(\S+)')
}
if ($nm.Count -gt 0) { $nextFix = $nm[$nm.Count - 1].Groups[1].Value }
$identity = 'UNKNOWN'
$im = [regex]::Matches($all, '\[ENTRY_IDENTITY\][^\r\n]*module=robotol\.ext[^\r\n]*identity=(\S+)')
if ($im.Count -eq 0) {
  $im = [regex]::Matches($all, '\[ENTRY_IDENTITY\][^\r\n]*identity=(\S+)')
}
if ($im.Count -gt 0) { $identity = $im[$im.Count - 1].Groups[1].Value }
$outcome = 'none'
if ($all -match 'SWITCH_BLOCKED_ENTRY_EXECUTED_WITH_CALLER_R9') {
  $outcome = 'SWITCH_BLOCKED_ENTRY_EXECUTED_WITH_CALLER_R9'
} elseif ($all -match 'SWITCH_BLOCKED_ENTRY_DEFERRED') {
  $outcome = 'SWITCH_BLOCKED_ENTRY_DEFERRED'
}
$producer = 'unknown'
$pm = [regex]::Matches($all, '\[ER_RW_PRODUCER\][^\r\n]*module=robotol\.ext[^\r\n]*producer_kind=(\S+)')
if ($pm.Count -gt 0) { $producer = $pm[$pm.Count - 1].Groups[1].Value }
elseif ($all -match 'module=robotol\.ext[^\r\n]*er_rw_publish_seq=0') { $producer = 'NONE_BEFORE_ENTRY' }
$relation = 'unknown'
$rm2 = [regex]::Matches($all, '\[ER_RW_PRODUCER\][^\r\n]*module=robotol\.ext[\s\S]{0,400}?\[ER_RW_BIRTH_CORR\][^\r\n]*relation=(\S+)')
if ($rm2.Count -gt 0) {
  $relation = $rm2[$rm2.Count - 1].Groups[1].Value
} elseif ($class -eq 'BOOTSTRAP_ENTRY_PRECEDES_ER_RW') {
  # Callee ER_RW not published at first entry (DSM ER_RW must not count as robotol birth).
  $relation = 'AFTER_ENTRY'
  if ($producer -eq 'unknown') { $producer = 'NONE_BEFORE_ENTRY' }
} elseif ($class -eq 'ER_RW_METADATA_PUBLICATION_LATE') {
  $relation = 'BEFORE_ENTRY'
}
$faultPath = 'none'
$fm = [regex]::Matches($all, '\[ER_RW_FAULT_PATH\][^\r\n]*path=(\S+)')
if ($fm.Count -gt 0) { $faultPath = $fm[$fm.Count - 1].Groups[1].Value }

# Forbid opening EARLY/DEFER without DOCUMENTED|CROSS_TARGET on the class line
$illegalFix = $false
if ($nextFix -match 'EARLY_ER_RW_REGISTRY_PUBLISH|DEFER_ENTRY_UNTIL_ER_RW_READY') {
  $gev = ''
  $gm = [regex]::Matches($all, '\[ER_RW_TIMING_CLASS\][^\r\n]*gate_evidence=(\S+)')
  if ($gm.Count -gt 0) { $gev = $gm[$gm.Count - 1].Groups[1].Value }
  if ($gev -notmatch 'DOCUMENTED|CROSS_TARGET') { $illegalFix = $true }
}
Assert-Log 'next_fix_gated' (-not $illegalFix) "illegal next_allowed_fix=$nextFix"

$answers = @(
  "Q1_producer_kind=$producer",
  "Q2_er_rw_before_entry=" + $(if ($relation -eq 'BEFORE_ENTRY') { 'yes' } elseif ($relation -eq 'AFTER_ENTRY') { 'no' } else { 'unknown' }),
  "Q3_cfunction_load_contract=emitted",
  "Q4_entry_triggers_er_rw=" + $(if ($class -eq 'BOOTSTRAP_ENTRY_PRECEDES_ER_RW') { 'likely_yes' } elseif ($relation -eq 'AFTER_ENTRY') { 'likely' } else { 'unknown' }),
  "Q5_entry_identity=$identity",
  "Q6_blocked_outcome=$outcome",
  "Q7_effective_r9_after_block=" + $(
    if ($all -match '\[R9_SWITCH_BLOCKED_OUTCOME\][^\r\n]*effective_r9=(0x[0-9A-Fa-f]+)') { $Matches[1] } else { 'missing' }
  ),
  "Q8_fault_path=$faultPath",
  "Q9_xt_pattern=$xtPattern",
  "Q10_class=$class next_allowed_fix=$nextFix",
  "Q11_observe_only=yes",
  "Q12_gates=er_rw_metadata_timing_gate=blocked;module_r9_switch_gate=open;phase6b_b_gate=blocked"
)

$lines = @(
  'Phase 6C-B ER_RW Producer Timing & First-Entry Ordering',
  "jjfb_sha256=$hash",
  'er_rw_metadata_timing_gate=blocked',
  'module_r9_switch_gate=open',
  'phase6b_b_gate=blocked',
  "timing_class=$class",
  "entry_identity=$identity",
  "blocked_outcome=$outcome",
  "birth_relation=$relation",
  "producer_kind=$producer",
  "fault_path=$faultPath",
  "next_allowed_fix=$nextFix",
  "xt_pattern=$xtPattern",
  'mode=observe_only'
) + $answers
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "class=$class outcome=$outcome relation=$relation next=$nextFix"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6c-b failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_ER_RW_PRODUCER complete'
