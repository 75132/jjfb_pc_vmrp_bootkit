# Phase 6B-A8: CODE_IMAGE Entry ABI & mr_helper Event Path Separation (observe-only).
param(
  [int]$Seconds = 14,
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_module_entry_abi.c'),
  (Join-Path $Root 'src\runtime\ext_helper_handoff.c'),
  (Join-Path $Root 'src\runtime\ext_object_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\ext_loader.c') -Pattern '0x304558|0x2D8DE0|0x22548|0x303B92|0x304AED' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_a8_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a8_entry_abi_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a8.json'
$xtGwyOut = Join-Path $logDir 'phase6b_a8_xt_gwy_stdout.txt'
$xtWxOut = Join-Path $logDir 'phase6b_a8_xt_wxjwq_stdout.txt'

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

Write-Host "== Phase 6B-A8 JJFB CODE_IMAGE entry ABI Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a8_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'handoff_expected' (
  $all -match '\[HANDOFF_CASE\] case=FIELD_NOT_UPDATED status=EXPECTED_BY_CONTRACT causal=NO'
) 'FIELD_NOT_UPDATED EXPECTED_BY_CONTRACT missing'
Assert-Log 'chain_separation' (
  $all -match '\[CHAIN_SEPARATION\] registered_helper_ne_module_entry=EXPECTED'
) 'CHAIN_SEPARATION missing'
Assert-Log 'loadcode_contract' ($all -match '\[LOADCODE_CONTRACT\]') 'LOADCODE_CONTRACT missing'
Assert-Log 'module_entry_proto' ($all -match '\[MODULE_ENTRY_PROTO\]') 'MODULE_ENTRY_PROTO missing'
Assert-Log 'cfunction_p' ($all -match '\[CFUNCTION_P\] stage=ALLOC') 'CFUNCTION_P ALLOC missing'
Assert-Log 'entry_compare' ($all -match '\[ENTRY_COMPARE\]') 'ENTRY_COMPARE missing'
Assert-Log 'mr_helper_or_not_reached' (
  ($all -match '\[MR_HELPER\] stage=CALL_HELPER') -or
  ($all -match 'MR_HELPER_NOT_REACHED_BEFORE_MODULE_ENTRY_FAULT')
) 'MR_HELPER path tag missing'
Assert-Log 'blocker' (
  $all -match '\[MODULE_ENTRY_BLOCKER\] blocker=(MODULE_ENTRY_ABI_UNKNOWN|CODE_IMAGE_ENTRY_NULL_ARGUMENT)'
) 'MODULE_ENTRY_BLOCKER missing'
Assert-Log 'r0_robotol_unknown' (
  $all -match '\[R0_SEMANTICS\] hop=DSM_TO_ROBOTOL r0=0x0 role=UNKNOWN'
) 'DSM_TO_ROBOTOL r0 role should be UNKNOWN'
Assert-Log 'phase6b_b_gate_line' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b_gate missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|PlatformRegistry')
) 'fake context / PlatformRegistry suspected'
Assert-Log 'no_dispatch_overwrite' (
  -not ($all -match 'dsm_module_record_set_dispatch|force_dispatch_helper')
) 'dispatch field overwrite suspected'
Assert-Log 'root_line' ($all -match '\[MODULE_ENTRY_ROOT\] root=') 'MODULE_ENTRY_ROOT missing'

$root = 'UNKNOWN'
if ($all -match '\[MODULE_ENTRY_ROOT\] root=([A-Z_]+)') { $root = $Matches[1] }

$retClass = 'UNKNOWN'
if ($all -match '\[LOADCODE_CONTRACT\][^\r\n]*return_class=([A-Z_]+)') { $retClass = $Matches[1] }

$r0Assign = 'unknown'
if ($all -match '\[MODULE_ENTRY_PROTO\][^\r\n]*r0_assign=(\S+)') { $r0Assign = $Matches[1] }

$mrHelper = 'NOT_REACHED'
if ($all -match '\[MR_HELPER\] stage=CALL_HELPER') { $mrHelper = 'REACHED' }
elseif ($all -match 'MR_HELPER_NOT_REACHED_BEFORE_MODULE_ENTRY_FAULT') { $mrHelper = 'NOT_REACHED' }

$blocker = 'MODULE_ENTRY_ABI_UNKNOWN'
if ($all -match '\[MODULE_ENTRY_BLOCKER\] blocker=([A-Z_]+)') { $blocker = $Matches[1] }

$gate = 'blocked'
if ($all -match 'phase6b_b_gate=(open|blocked)') { $gate = $Matches[1] }

$xtGwy = 'missing'
$xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
if (Test-Path (Join-Path $xtRoot 'gwy.mrp')) {
  Write-Host '== Phase 6B-A8 cross-target gwy.mrp =='
  Invoke-Live $xtGwyOut (Join-Path $logDir 'phase6b_a8_xt_gwy_stderr.txt') $xtRoot 'gwy.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xg = ''
  if (Test-Path $xtGwyOut) { $xg = Get-Content $xtGwyOut -Raw -ErrorAction SilentlyContinue }
  if ($xg -match '\[LOADCODE_CONTRACT\]|\[MODULE_ENTRY_PROTO\]|\[CHAIN_SEPARATION\]') {
    $xtGwy = 'entry_abi_tags_seen'
  } elseif ($xg -match '\[SECOND_HOP\]|\[CFUNCTION_NEW\]') {
    $xtGwy = 'partial'
  } else {
    $xtGwy = 'ran_minimal'
  }
  Assert-Log 'xt_gwy' ($xg.Length -gt 50) 'gwy.mrp log empty'
}

$xtWx = 'missing'
$wx = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
if (Test-Path $wx) {
  Write-Host '== Phase 6B-A8 cross-target wxjwq.mrp =='
  Invoke-Live $xtWxOut (Join-Path $logDir 'phase6b_a8_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xw = ''
  if (Test-Path $xtWxOut) { $xw = Get-Content $xtWxOut -Raw -ErrorAction SilentlyContinue }
  if ($xw -match '\[LOADCODE_CONTRACT\]|\[MODULE_ENTRY_PROTO\]|\[CHAIN_SEPARATION\]') {
    $xtWx = 'entry_abi_tags_seen'
  } elseif ($xw -match '\[SECOND_HOP\]|\[CFUNCTION_NEW\]') {
    $xtWx = 'partial'
  } else {
    $xtWx = 'ran_minimal'
  }
  Assert-Log 'xt_wxjwq' ($xw.Length -gt 50) 'wxjwq log empty'
}

$answers = @(
  "Q1_loadcode_return_class=$retClass",
  ("Q2_module_entry_proto=" + $(if ($all -match '\[MODULE_ENTRY_PROTO\]') { 'emitted' } else { 'missing' })),
  "Q3_r0_assign=$r0Assign",
  "Q4_null_r0_allowed=UNKNOWN",
  ("Q5_cfunction_p_lifecycle=" + $(if ($all -match '\[CFUNCTION_P\] stage=ALLOC') { 'ALLOC_seen' } else { 'missing' })),
  "Q6_mr_helper=$mrHelper",
  ("Q7_fault_outside_mr_helper=" + $(if ($mrHelper -eq 'NOT_REACHED') { 'yes' } else { 'no_helper_reached_before_fault' })),
  "Q8_xt_gwy=$xtGwy",
  "Q9_root=$root",
  "Q10_no_guest_mutation=yes",
  "Q11_phase6b_b_gate=$gate",
  "Q12_blocker=$blocker"
)

$lines = @(
  'Phase 6B-A8 CODE_IMAGE Entry ABI & mr_helper Path Separation',
  "jjfb_sha256=$hash",
  "blocker=$blocker",
  "root=$root",
  'root_legend=MISSING_MODULE_ENTRY_CONTEXT|MISSING_PRE_ENTRY_INITIALIZATION|LOADCODE_RETURN_MISINTERPRETED|WRONG_MODULE_ENTRY_LIFECYCLE|MRC_FUNCTION_P_ENTRY_ARGUMENT|UNKNOWN',
  "loadcode_return_class=$retClass",
  "r0_assign=$r0Assign",
  "mr_helper=$mrHelper",
  "FIELD_NOT_UPDATED=EXPECTED_BY_CONTRACT causal=NO",
  "registered_helper_ne_module_entry=EXPECTED",
  "cross_target_gwy=$xtGwy",
  "cross_target_wxjwq=$xtWx",
  'fix_applied=none (no DSM dispatch write; no r0 fill; no PlatformRegistry; no fake context)',
  "phase6b_b_gate=$gate"
) + $answers
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "root=$root blocker=$blocker ret=$retClass mr_helper=$mrHelper gate=$gate"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a8 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_MODULE_ENTRY_ABI complete'
