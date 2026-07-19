# Phase 6B-A: EXT Context Bootstrap & Entry Selection Evidence (observe-only).
param(
  [int]$Seconds = 12,
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_entry_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\module_registry.c') -Pattern '0x304558|0x2D8DE0|0x22548' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_live_bootstrap_stdout.txt'
$stderr = Join-Path $logDir 'phase6b_live_bootstrap_stderr.txt'
$report = Join-Path $logDir 'phase6b_bootstrap_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b.json'
$xtGwyOut = Join-Path $logDir 'phase6b_xt_gwy_stdout.txt'
$xtWxOut = Join-Path $logDir 'phase6b_xt_wxjwq_stdout.txt'

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

Write-Host "== Phase 6B-A JJFB bootstrap evidence Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_live_bootstrap_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'ext_entry_points' ($all -match '\[EXT_ENTRY_POINTS\]') 'EXT_ENTRY_POINTS missing'
Assert-Log 'dsm_entry_select' ($all -match '\[DSM_ENTRY_SELECT\]') 'DSM_ENTRY_SELECT missing'
Assert-Log 'bootstrap_classify' ($all -match '\[BOOTSTRAP_CLASSIFY\]') 'BOOTSTRAP_CLASSIFY missing'
Assert-Log 'header_entry_distinct' (
  $all -match 'header_entry_candidate=0x[0-9A-Fa-f]+' -and
  $all -match 'registered_helper=0x[0-9A-Fa-f]+'
) 'header/helper not logged separately'
Assert-Log 'field28_writer_status' (
  $all -match 'field28_writer=(SEEN|NONE_BEFORE_FAULT)'
) 'field28_writer status missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|mapped low address 0x28')
) 'fake context suspected'
Assert-Log 'no_low_va_map' (
  -not ($all -match 'uc_mem_map\(\s*0x0\s*,')
) 'low VA map'

$primary = 'D'
$entryClass = 'unknown'
$field28w = 'NONE_BEFORE_FAULT'
$dsmSource = '?'
$dsmMatch = '?'
$r0Source = '?'
if ($all -match '\[BOOTSTRAP_CLASSIFY\] entry_class=(\S+) primary=(\S+) field28_writer=(\S+)') {
  $entryClass = $Matches[1]; $primary = $Matches[2]; $field28w = $Matches[3]
}
if ($all -match '\[DSM_ENTRY_SELECT\] source=(\S+).*match=(\S+).*r0_source=(\S+)') {
  $dsmSource = $Matches[1]; $dsmMatch = $Matches[2]; $r0Source = $Matches[3]
}

# Cross-target gwy.mrp
$xtGwy = 'missing'
$xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
if (Test-Path (Join-Path $xtRoot 'gwy.mrp')) {
  Write-Host '== Phase 6B-A cross-target gwy.mrp =='
  Invoke-Live $xtGwyOut (Join-Path $logDir 'phase6b_xt_gwy_stderr.txt') $xtRoot 'gwy.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xg = ''
  if (Test-Path $xtGwyOut) { $xg = Get-Content $xtGwyOut -Raw -ErrorAction SilentlyContinue }
  if ($xg -match '\[DSM_ENTRY_SELECT\]') { $xtGwy = 'DSM_ENTRY_SELECT_seen' }
  elseif ($xg -match '\[EXT_ENTRY_POINTS\]|mrc_loader') { $xtGwy = 'loader_tags_no_dsm_select' }
  else { $xtGwy = 'ran_no_nested_select' }
  Assert-Log 'xt_gwy' ($xg.Length -gt 50) 'gwy.mrp log empty'
}

# Cross-target wxjwq.mrp
$xtWx = 'missing'
$wx = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
if (Test-Path $wx) {
  Write-Host '== Phase 6B-A cross-target wxjwq.mrp =='
  Invoke-Live $xtWxOut (Join-Path $logDir 'phase6b_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xw = ''
  if (Test-Path $xtWxOut) { $xw = Get-Content $xtWxOut -Raw -ErrorAction SilentlyContinue }
  if ($xw -match '\[DSM_ENTRY_SELECT\]') { $xtWx = 'DSM_ENTRY_SELECT_seen' }
  elseif ($xw -match 'mrc_loader|mmochat|EXT_ENTRY') { $xtWx = 'loader_tags_present' }
  else { $xtWx = 'ran_minimal' }
  Assert-Log 'xt_wxjwq' ($xw.Length -gt 50) 'wxjwq log empty'
}

$lines = @(
  'Phase 6B-A EXT Context Bootstrap & Entry Selection Evidence',
  "jjfb_sha256=$hash",
  "dsm_entry_select_source=$dsmSource",
  "dsm_entry_select_match=$dsmMatch",
  "dsm_r0_source=$r0Source",
  "entry_class=$entryClass",
  "primary_conclusion=$primary",
  "primary_legend=A_wrong_entry|B_missing_context_bootstrap|C_missing_prior_method|D_unknown",
  "field28_writer=$field28w",
  "cross_target_gwy=$xtGwy",
  "cross_target_wxjwq=$xtWx",
  'hypothesis_chunk_plus4_is_init_func=DOCUMENTED (mr_helper.h mrc_extChunk+0x04)',
  'hypothesis_image_plus8_is_load=DOCUMENTED (fixR9.c TestCom800)',
  'hypothesis_helper_from_c_function_new=DOCUMENTED+OBSERVED',
  'hypothesis_field28_sendAppEvent=probable_DOCUMENTED not_confirmed_no_writer',
  'v49_P_layout=HYPOTHESIS_only_not_copied',
  'fix_applied=none (no context create; no PlatformRegistry; no r0 poke)',
  'phase6b_b_gate=blocked_until_A_or_B_or_C_producer_proven'
)
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "primary=$primary entry_class=$entryClass dsm_match=$dsmMatch field28=$field28w"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_EXT_BOOTSTRAP complete'
