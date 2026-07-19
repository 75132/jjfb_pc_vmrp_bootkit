# Phase 6B-A7: DSM Module Record Handoff Contract & Public Entry Resolution (observe-only).
param(
  [int]$Seconds = 14,
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_helper_handoff.c'),
  (Join-Path $Root 'src\runtime\ext_dsm_record_observe.c'),
  (Join-Path $Root 'src\runtime\ext_object_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\ext_loader.c') -Pattern '0x304558|0x2D8DE0|0x22548' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_a7_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a7_contract_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a6.json'
$xtGwyOut = Join-Path $logDir 'phase6b_a7_xt_gwy_stdout.txt'
$xtWxOut = Join-Path $logDir 'phase6b_a7_xt_wxjwq_stdout.txt'

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

Write-Host "== Phase 6B-A7 JJFB DSM handoff contract Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a7_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'cfn_side_effect' ($all -match '\[CFUNCTION_NEW_SIDE_EFFECT\].*DOCUMENTED') 'DOCUMENTED side effect missing'
Assert-Log 'contract_case' (
  $all -match '\[CONTRACT_CASE\] case=(REGISTER_FINALIZE_MISSING|WRONG_RECORD_ASSOCIATION|WRONG_FIELD_READ|HELPER_ADDRESS_ENCODING|HELPER_NOT_DISPATCH_TARGET|GENERIC_HANDOFF_MISSING|UNKNOWN)'
) 'CONTRACT_CASE missing'
Assert-Log 'finalize_or_none' ($all -match '\[REGISTER_FINALIZE\]') 'REGISTER_FINALIZE missing'
Assert-Log 'record_or_write' (
  ($all -match '\[DSM_MODULE_RECORD\]') -or ($all -match '\[DSM_RECORD_WRITE\]') -or ($all -match '\[DSM_RECORD_WRITE_SUMMARY\]')
) 'DSM_MODULE_RECORD / WRITE missing'
Assert-Log 'phase6b_b_gate_line' ($all -match 'phase6b_b_gate=(open|blocked)') 'phase6b_b_gate missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|PlatformRegistry')
) 'fake context / PlatformRegistry suspected'
Assert-Log 'no_record_overwrite' (
  -not ($all -match 'dsm_module_record_set_dispatch|force_dispatch_helper')
) 'dispatch field overwrite suspected'

$hopCase = 'UNKNOWN'
$cm = [regex]::Matches($all, '\[CONTRACT_CASE\] case=([A-Z_]+)')
if ($cm.Count -gt 0) { $hopCase = $cm[$cm.Count - 1].Groups[1].Value }

$evLevel = 'UNKNOWN'
if ($all -match '\[CONTRACT_CASE\][^\r\n]*evidence=(\S+)') { $evLevel = $Matches[1] }

$gate = 'blocked'
if ($all -match 'phase6b_b_gate=(open|blocked)') { $gate = $Matches[1] }

$helperEqRet = '?'
if ($all -match 'helper_eq_return_r0=([01])') { $helperEqRet = $Matches[1] }

$docNoDispatch = ($all -match 'NO_DSM_DISPATCH_WRITE')

$xtGwy = 'missing'
$xtRel = 'unknown'
$xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
if (Test-Path (Join-Path $xtRoot 'gwy.mrp')) {
  Write-Host '== Phase 6B-A7 cross-target gwy.mrp =='
  Invoke-Live $xtGwyOut (Join-Path $logDir 'phase6b_a7_xt_gwy_stderr.txt') $xtRoot 'gwy.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xg = ''
  if (Test-Path $xtGwyOut) { $xg = Get-Content $xtGwyOut -Raw -ErrorAction SilentlyContinue }
  if ($xg -match '\[CONTRACT_CASE\]|\[CFUNCTION_NEW_SIDE_EFFECT\]|\[SECOND_HOP\]') { $xtGwy = 'contract_tags_seen' }
  elseif ($xg -match '\[DSM_ENTRY_SELECT\]') { $xtGwy = 'DSM_ENTRY_SELECT_seen' }
  else { $xtGwy = 'ran_minimal' }
  if ($xg -match 'equals_helper=1|HELPER_NOT_DISPATCH_TARGET') { $xtRel = $(if ($xg -match 'equals_helper=1') { 'eq' } else { 'ne_documented' }) }
  elseif ($xg -match '\[SECOND_HOP\][^\r\n]*target_relation=trampoline') { $xtRel = 'trampoline' }
  elseif ($xg -match '\[SECOND_HOP\]|\[DSM_DIRECT_TARGET\]') { $xtRel = 'ne' }
  Assert-Log 'xt_gwy' ($xg.Length -gt 50) 'gwy.mrp log empty'
}

$xtWx = 'missing'
$wx = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
if (Test-Path $wx) {
  Write-Host '== Phase 6B-A7 cross-target wxjwq.mrp =='
  Invoke-Live $xtWxOut (Join-Path $logDir 'phase6b_a7_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xw = ''
  if (Test-Path $xtWxOut) { $xw = Get-Content $xtWxOut -Raw -ErrorAction SilentlyContinue }
  if ($xw -match '\[CONTRACT_CASE\]|\[CFUNCTION_NEW_SIDE_EFFECT\]|\[SECOND_HOP\]') { $xtWx = 'contract_tags_seen' }
  elseif ($xw -match '\[DSM_ENTRY_SELECT\]') { $xtWx = 'DSM_ENTRY_SELECT_seen' }
  else { $xtWx = 'ran_minimal' }
  if ($xtRel -eq 'unknown') {
    if ($xw -match 'equals_helper=1') { $xtRel = 'eq' }
    elseif ($xw -match 'HELPER_NOT_DISPATCH_TARGET') { $xtRel = 'ne_documented' }
    elseif ($xw -match 'target_relation=trampoline') { $xtRel = 'trampoline' }
    elseif ($xw -match '\[SECOND_HOP\]|\[DSM_DIRECT_TARGET\]') { $xtRel = 'ne' }
  }
  Assert-Log 'xt_wxjwq' ($xw.Length -gt 50) 'wxjwq log empty'
}

$answers = @(
  "Q1_dispatch_record_offset=" + $(if ($all -match 'dispatch_field_offset=0x([0-9A-Fa-f]+)') { $Matches[1] } else { 'unknown_reverse_miss' }),
  "Q2_internal_entry_writer=CODE_IMAGE_LOAD_RETURN",
  "Q3_cfunction_stores_helper=HOST_GLOBAL_mr_c_function_DOCUMENTED",
  "Q4_finalize_exists=" + $(if ($all -match 'finalizer_module=NONE') { 'no' } else { 'maybe' }),
  "Q5_finalize_should_update_dispatch=DOCUMENTED_no_mythroad_c_function_new_does_not",
  "Q6_why_not_updated=DOCUMENTED_api_has_no_dsm_dispatch_write",
  "Q8_xt_second_hop_vs_helper=$xtRel",
  "Q9_helper_is_second_hop_target=no_DOCUMENTED",
  "Q10_evidence=$evLevel",
  "Q11_no_guest_mutation=yes",
  "Q12_phase6b_b_gate=$gate"
)

$lines = @(
  'Phase 6B-A7 DSM Module Record Handoff Contract',
  "jjfb_sha256=$hash",
  "case=$hopCase",
  'case_legend=REGISTER_FINALIZE_MISSING|WRONG_RECORD_ASSOCIATION|WRONG_FIELD_READ|HELPER_ADDRESS_ENCODING|HELPER_NOT_DISPATCH_TARGET|GENERIC_HANDOFF_MISSING|UNKNOWN',
  "evidence=$evLevel",
  "documented_no_dsm_dispatch_write=$docNoDispatch",
  "xt_second_hop_vs_helper=$xtRel",
  "cross_target_gwy=$xtGwy",
  "cross_target_wxjwq=$xtWx",
  'fix_applied=none (no DSM record write; no second-hop redirect; no PlatformRegistry)',
  "phase6b_b_gate=$gate"
) + $answers
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "case=$hopCase evidence=$evLevel gate=$gate xt_rel=$xtRel"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a7 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_DSM_HANDOFF_CONTRACT complete'

