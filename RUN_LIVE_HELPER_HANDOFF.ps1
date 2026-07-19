# Phase 6B-A6: Registered Helper Handoff & Public Entry Resolution (observe-only).
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
  (Join-Path $Root 'src\runtime\ext_object_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\ext_loader.c') -Pattern '0x304558|0x2D8DE0|0x22548' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_a6_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a6_handoff_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a6.json'
$xtGwyOut = Join-Path $logDir 'phase6b_a6_xt_gwy_stdout.txt'
$xtWxOut = Join-Path $logDir 'phase6b_a6_xt_wxjwq_stdout.txt'

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

Write-Host "== Phase 6B-A6 JJFB helper handoff Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a6_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'cfunction_enter' ($all -match '\[CFUNCTION_NEW\] stage=ENTER') 'CFUNCTION_NEW ENTER missing'
Assert-Log 'cfunction_exit' ($all -match '\[CFUNCTION_NEW\] stage=EXIT') 'CFUNCTION_NEW EXIT missing'
Assert-Log 'handoff_or_none' (
  ($all -match '\[HELPER_HANDOFF\] stage=') -or ($all -match '\[HELPER_HANDOFF\] NONE')
) 'HELPER_HANDOFF missing'
Assert-Log 'record_or_prov' (
  ($all -match '\[DSM_RECORD_FIELD\]') -or ($all -match '\[DIRECT_TARGET_PROVENANCE\]')
) 'DSM_RECORD_FIELD / DIRECT_TARGET_PROVENANCE missing'
Assert-Log 'entry_nature' (
  ([regex]::Matches($all, '\[ENTRY_NATURE\]')).Count -ge 2
) 'need ENTRY_NATURE x2'
Assert-Log 'handoff_case' (
  $all -match '\[HANDOFF_CASE\] case=(RETURN_LOST|FIELD_NOT_UPDATED|WRONG_FIELD_READ|WRONG_HELPER_IDENTITY|MISSING_TRAMPOLINE|CROSS_TARGET_DISPROVES|UNKNOWN)'
) 'HANDOFF_CASE missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|PlatformRegistry')
) 'fake context / PlatformRegistry suspected'
Assert-Log 'no_target_overwrite' (
  -not ($all -match 'force_second_hop|overwrite_dsm_direct|redirect_blx_helper')
) 'second-hop overwrite suspected'

$hopCase = 'UNKNOWN'
$cm = [regex]::Matches($all, '\[HANDOFF_CASE\] case=([A-Z_]+)')
if ($cm.Count -gt 0) { $hopCase = $cm[$cm.Count - 1].Groups[1].Value }

$helperNat = '?'
$directNat = '?'
if ($all -match '\[ENTRY_NATURE\][^\r\n]*role=registered_helper nature=(\S+)') { $helperNat = $Matches[1] }
if ($all -match '\[ENTRY_NATURE\][^\r\n]*role=dsm_direct_target nature=(\S+)') { $directNat = $Matches[1] }

$helperEqRet = '?'
if ($all -match 'helper_eq_return_r0=([01])') { $helperEqRet = $Matches[1] }

$fieldEqHelper = '?'
if ($all -match '\[DSM_RECORD_FIELD\][^\r\n]*equals_helper=([01])') { $fieldEqHelper = $Matches[1] }

$xtGwy = 'missing'
$xtRel = 'unknown'
$xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
if (Test-Path (Join-Path $xtRoot 'gwy.mrp')) {
  Write-Host '== Phase 6B-A6 cross-target gwy.mrp =='
  Invoke-Live $xtGwyOut (Join-Path $logDir 'phase6b_a6_xt_gwy_stderr.txt') $xtRoot 'gwy.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xg = ''
  if (Test-Path $xtGwyOut) { $xg = Get-Content $xtGwyOut -Raw -ErrorAction SilentlyContinue }
  if ($xg -match '\[HANDOFF_CASE\]|\[CFUNCTION_NEW\]|\[SECOND_HOP\]') { $xtGwy = 'handoff_tags_seen' }
  elseif ($xg -match '\[DSM_ENTRY_SELECT\]') { $xtGwy = 'DSM_ENTRY_SELECT_seen' }
  else { $xtGwy = 'ran_minimal' }
  if ($xg -match 'equals_helper=1') { $xtRel = 'eq' }
  elseif ($xg -match '\[SECOND_HOP\][^\r\n]*target_relation=trampoline') { $xtRel = 'trampoline' }
  elseif ($xg -match '\[SECOND_HOP\]|\[DSM_DIRECT_TARGET\]') { $xtRel = 'ne' }
  Assert-Log 'xt_gwy' ($xg.Length -gt 50) 'gwy.mrp log empty'
}

$xtWx = 'missing'
$wx = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
if (Test-Path $wx) {
  Write-Host '== Phase 6B-A6 cross-target wxjwq.mrp =='
  Invoke-Live $xtWxOut (Join-Path $logDir 'phase6b_a6_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xw = ''
  if (Test-Path $xtWxOut) { $xw = Get-Content $xtWxOut -Raw -ErrorAction SilentlyContinue }
  if ($xw -match '\[HANDOFF_CASE\]|\[CFUNCTION_NEW\]|\[SECOND_HOP\]') { $xtWx = 'handoff_tags_seen' }
  elseif ($xw -match '\[DSM_ENTRY_SELECT\]') { $xtWx = 'DSM_ENTRY_SELECT_seen' }
  else { $xtWx = 'ran_minimal' }
  if ($xtRel -eq 'unknown') {
    if ($xw -match 'equals_helper=1') { $xtRel = 'eq' }
    elseif ($xw -match 'target_relation=trampoline') { $xtRel = 'trampoline' }
    elseif ($xw -match '\[SECOND_HOP\]|\[DSM_DIRECT_TARGET\]') { $xtRel = 'ne' }
  }
  Assert-Log 'xt_wxjwq' ($xw.Length -gt 50) 'wxjwq log empty'
}

$answers = @(
  "Q1_cfunction_return_semantics=helper_is_input_eq_return_r0=$helperEqRet",
  "Q2_helper_handoff=" + $(if ($all -match '\[HELPER_HANDOFF\] stage=WRITE') { 'WRITE' } elseif ($all -match '\[HELPER_HANDOFF\] stage=RETURN') { 'RETURN' } elseif ($all -match '\[HELPER_HANDOFF\] NONE') { 'NONE' } else { '?' }),
  "Q3_dsm_record_field_eq_helper=$fieldEqHelper",
  "Q4_direct_provenance=" + $(if ($all -match '\[DIRECT_TARGET_PROVENANCE\][^\r\n]*source_kind=(\S+)') { $Matches[1] } else { '?' }),
  "Q5_helper_should_replace_field=hypothesis_FIELD_NOT_UPDATED",
  "Q6_helper_nature=$helperNat",
  "Q7_direct_nature=$directNat",
  "Q8_xt_second_hop_vs_helper=$xtRel",
  "Q9_handoff_case=$hopCase",
  'Q10_no_guest_mutation=yes'
)

$lines = @(
  'Phase 6B-A6 Registered Helper Handoff & Public Entry Resolution',
  "jjfb_sha256=$hash",
  "case=$hopCase",
  'case_legend=RETURN_LOST|FIELD_NOT_UPDATED|WRONG_FIELD_READ|WRONG_HELPER_IDENTITY|MISSING_TRAMPOLINE|CROSS_TARGET_DISPROVES|UNKNOWN',
  "helper_nature=$helperNat",
  "direct_nature=$directNat",
  "helper_eq_return_r0=$helperEqRet",
  "field_eq_helper=$fieldEqHelper",
  "xt_second_hop_vs_helper=$xtRel",
  "cross_target_gwy=$xtGwy",
  "cross_target_wxjwq=$xtWx",
  'fix_applied=none (no second-hop overwrite; no context; no PlatformRegistry)',
  'phase6b_b_gate=blocked'
) + $answers
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "case=$hopCase helper_nat=$helperNat direct_nat=$directNat xt_rel=$xtRel"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a6 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_HELPER_HANDOFF complete'
