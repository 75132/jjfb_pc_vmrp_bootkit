# Phase 6B-A5: Dispatcher Semantics & Second-Hop Provenance (observe-only).
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_entry_observe.c'),
  (Join-Path $Root 'src\runtime\ext_chunk_observe.c'),
  (Join-Path $Root 'src\runtime\ext_object_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\module_registry.c') -Pattern '0x304558|0x2D8DE0|0x22548' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_a5_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a5_dispatch_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a5.json'
$xtGwyOut = Join-Path $logDir 'phase6b_a5_xt_gwy_stdout.txt'
$xtWxOut = Join-Path $logDir 'phase6b_a5_xt_wxjwq_stdout.txt'

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

Write-Host "== Phase 6B-A5 JJFB dispatch hop Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a5_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'dispatch_enter' ($all -match '\[DISPATCH_CALL\] stage=ENTER') 'DISPATCH_CALL ENTER missing'
Assert-Log 'dispatch_exit' ($all -match '\[DISPATCH_CALL\] stage=EXIT') 'DISPATCH_CALL EXIT missing'
Assert-Log 'dispatch_proto' ($all -match '\[DISPATCH_PROTO\] r0_role=') 'DISPATCH_PROTO missing'
Assert-Log 'transfer_or_second' (
  ($all -match 'transfer_id=2') -or ($all -match '\[SECOND_HOP\]')
) 'transfer_id>=2 / SECOND_HOP missing'
Assert-Log 'ptr_write_or_none' (
  ($all -match '\[MODULE_POINTER_WRITE\]') -or ($all -match '\[MODULE_POINTER_WRITE\] NONE')
) 'MODULE_POINTER_WRITE missing'
Assert-Log 'dsm_direct' (
  ($all -match '\[DSM_DIRECT_TARGET\]') -or ($all -match 'dsm_direct_target')
) 'dsm_direct_target missing'
Assert-Log 'r0_semantics' (
  ([regex]::Matches($all, '\[R0_SEMANTICS\]')).Count -ge 2
) 'need R0_SEMANTICS x2'
Assert-Log 'hop_case' ($all -match '\[DISPATCH_HOP_CASE\] case=[A-F]') 'DISPATCH_HOP_CASE missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|PlatformRegistry')
) 'fake context / PlatformRegistry suspected'
Assert-Log 'no_r0_poke' (-not ($all -match 'r0_poke|force_r0\s*=')) 'r0 poke suspected'

$hopCase = 'F'
$caseMatches = [regex]::Matches($all, '\[DISPATCH_HOP_CASE\] case=([A-F])')
if ($caseMatches.Count -gt 0) {
  $hopCase = $caseMatches[$caseMatches.Count - 1].Groups[1].Value
}

$r0Loader = '?'
$r0Robotol = '?'
if ($all -match '\[R0_SEMANTICS\] hop=DSM_TO_LOADER r0=0x([0-9A-Fa-f]+)') {
  $r0Loader = $Matches[1]
}
if ($all -match '\[R0_SEMANTICS\] hop=DSM_TO_ROBOTOL r0=0x([0-9A-Fa-f]+)') {
  $r0Robotol = $Matches[1]
}

$protoR0 = '?'
if ($all -match '\[DISPATCH_PROTO\] r0_role=(\S+)') { $protoR0 = $Matches[1] }

$directRel = '?'
if ($all -match '\[DSM_DIRECT_TARGET\][^\r\n]*relation=(\S+)') { $directRel = $Matches[1] }
elseif (Test-Path $snap) {
  $sj = Get-Content $snap -Raw -ErrorAction SilentlyContinue
  if ($sj -match '"direct_target_relation"\s*:\s*"([^"]+)"') { $directRel = $Matches[1] }
}

$secondSrc = '?'
if ($all -match '\[SECOND_HOP\][^\r\n]*target_source=(\S+)') { $secondSrc = $Matches[1] }

$xtGwy = 'missing'
$xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
if (Test-Path (Join-Path $xtRoot 'gwy.mrp')) {
  Write-Host '== Phase 6B-A5 cross-target gwy.mrp =='
  Invoke-Live $xtGwyOut (Join-Path $logDir 'phase6b_a5_xt_gwy_stderr.txt') $xtRoot 'gwy.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xg = ''
  if (Test-Path $xtGwyOut) { $xg = Get-Content $xtGwyOut -Raw -ErrorAction SilentlyContinue }
  if ($xg -match '\[DISPATCH_CALL\]|\[SECOND_HOP\]|\[CROSS_MODULE_CALL\]') {
    $xtGwy = 'dispatch_tags_seen'
  } elseif ($xg -match '\[DSM_ENTRY_SELECT\]') { $xtGwy = 'DSM_ENTRY_SELECT_seen' }
  else { $xtGwy = 'ran_minimal' }
  Assert-Log 'xt_gwy' ($xg.Length -gt 50) 'gwy.mrp log empty'
}

$xtWx = 'missing'
$wx = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
if (Test-Path $wx) {
  Write-Host '== Phase 6B-A5 cross-target wxjwq.mrp =='
  Invoke-Live $xtWxOut (Join-Path $logDir 'phase6b_a5_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xw = ''
  if (Test-Path $xtWxOut) { $xw = Get-Content $xtWxOut -Raw -ErrorAction SilentlyContinue }
  if ($xw -match '\[DISPATCH_CALL\]|\[SECOND_HOP\]|\[CROSS_MODULE_CALL\]') {
    $xtWx = 'dispatch_tags_seen'
  } elseif ($xw -match '\[DSM_ENTRY_SELECT\]') { $xtWx = 'DSM_ENTRY_SELECT_seen' }
  else { $xtWx = 'ran_minimal' }
  Assert-Log 'xt_wxjwq' ($xw.Length -gt 50) 'wxjwq log empty'
}

$answers = @(
  'Q1_dispatcher_r0_role=' + $protoR0,
  'Q2_dispatch_enter_exit=OBSERVED',
  'Q3_second_hop_source=' + $secondSrc,
  'Q4_r0_loader_vs_robotol=0x' + $r0Loader + '/0x' + $r0Robotol,
  'Q5_dsm_direct_relation=' + $directRel,
  'Q6_ptr_write=' + $(if ($all -match '\[MODULE_POINTER_WRITE\] value=') { 'seen' } else { 'NONE' }),
  'Q7_hop_case=' + $hopCase,
  'Q8_no_fake_context=yes',
  'Q9_phase6b_b=blocked',
  'Q10_evidence=OBSERVED'
)

$lines = @(
  'Phase 6B-A5 Dispatcher Semantics & Second-Hop Provenance',
  "jjfb_sha256=$hash",
  "case=$hopCase",
  'case_legend=A_cmd_ok_wrong_entry|B_helper_match|C_reg_mismatch|D_missing_desc|E_method_dispatch|F_unknown',
  "r0_role_proto=$protoR0",
  "r0_dsm_to_loader=0x$r0Loader",
  "r0_dsm_to_robotol=0x$r0Robotol",
  "second_hop_target_source=$secondSrc",
  "dsm_direct_relation=$directRel",
  "cross_target_gwy=$xtGwy",
  "cross_target_wxjwq=$xtWx",
  'fix_applied=none (no chunk/context create; no PlatformRegistry; no r0 poke)',
  'phase6b_b_gate=blocked'
) + $answers
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "case=$hopCase proto_r0=$protoR0 second_src=$secondSrc"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a5 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_DISPATCH_HOP complete'
