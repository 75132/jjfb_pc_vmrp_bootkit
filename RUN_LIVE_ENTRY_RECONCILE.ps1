# Phase 6B-A2: Chunk Init Entry Reconciliation (observe-only).
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
$stdout = Join-Path $logDir 'phase6b_a2_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a2_reconcile_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a2.json'
$xtGwyOut = Join-Path $logDir 'phase6b_a2_xt_gwy_stdout.txt'
$xtWxOut = Join-Path $logDir 'phase6b_a2_xt_wxjwq_stdout.txt'

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
  $env:GWY_ENTRY_RECONCILE = '1'
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

Write-Host "== Phase 6B-A2 JJFB entry reconcile Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a2_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'entry_reconcile' ($all -match '\[ENTRY_RECONCILE\]') 'ENTRY_RECONCILE missing'
Assert-Log 'dsm_entry_select' ($all -match '\[DSM_ENTRY_SELECT\]') 'DSM_ENTRY_SELECT missing'
Assert-Log 'chunk_field_04_status' (
  $all -match 'chunk_field_04_writer=(SEEN|NONE_BEFORE_SELECT)|\[CHUNK_FIELD_04_WRITE\]'
) 'chunk init writer status missing'
Assert-Log 'header_candidate' ($all -match 'header_entry_candidate=0x') 'header_entry_candidate missing'
Assert-Log 'candidate_disasm' ($all -match '\[ENTRY_CANDIDATE_DISASM\]|\[ENTRY_CANDIDATE_CLASS\]') 'candidate disasm missing'
Assert-Log 'reconcile_case' ($all -match '\[ENTRY_RECONCILE_CASE\] case=[A-F]') 'ENTRY_RECONCILE_CASE missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|mapped low address 0x28')
) 'fake context suspected'
Assert-Log 'no_low_va_map' (
  -not ($all -match 'uc_mem_map\(\s*0x0\s*,')
) 'low VA map'

$reconCase = 'F'
$relation = 'UNKNOWN'
$chunkWriter = 'NONE_BEFORE_SELECT'
$dsmMatch = '?'
$headerNorm = '?'
$chunkNorm = '?'
$helperNorm = '?'
# Prefer last ENTRY_RECONCILE_CASE (nested robotol/mrc_loader over DSM).
$caseMatches = [regex]::Matches($all, '\[ENTRY_RECONCILE_CASE\] case=([A-F]) relation=(\S+) chunk_field_04_writer=(\S+)')
if ($caseMatches.Count -gt 0) {
  $last = $caseMatches[$caseMatches.Count - 1]
  $reconCase = $last.Groups[1].Value
  $relation = $last.Groups[2].Value
  $chunkWriter = $last.Groups[3].Value
}
# Prefer robotol ENTRY_RECONCILE_CASE when present.
if ($all -match '\[ENTRY_RECONCILE_CASE\] case=([A-F]) relation=(\S+) chunk_field_04_writer=(\S+)[^\r\n]*module=robotol') {
  $reconCase = $Matches[1]; $relation = $Matches[2]; $chunkWriter = $Matches[3]
}
if ($all -match '\[DSM_ENTRY_SELECT\].*match=(\S+)') { $dsmMatch = $Matches[1] }
# Prefer robotol ENTRY_RECONCILE norms when present.
if ($all -match '\[ENTRY_RECONCILE\] module=robotol\.ext[^\r\n]*header_norm=0x([0-9A-Fa-f]+)[^\r\n]*chunk_field_04_norm=0x([0-9A-Fa-f]+)[^\r\n]*helper_norm=0x([0-9A-Fa-f]+)') {
  $headerNorm = $Matches[1]; $chunkNorm = $Matches[2]; $helperNorm = $Matches[3]
} elseif ($all -match '\[ENTRY_RECONCILE\].*header_norm=0x([0-9A-Fa-f]+).*chunk_field_04_norm=0x([0-9A-Fa-f]+).*helper_norm=0x([0-9A-Fa-f]+)') {
  $headerNorm = $Matches[1]; $chunkNorm = $Matches[2]; $helperNorm = $Matches[3]
}

$xtGwy = 'missing'
$xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
if (Test-Path (Join-Path $xtRoot 'gwy.mrp')) {
  Write-Host '== Phase 6B-A2 cross-target gwy.mrp =='
  Invoke-Live $xtGwyOut (Join-Path $logDir 'phase6b_a2_xt_gwy_stderr.txt') $xtRoot 'gwy.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xg = ''
  if (Test-Path $xtGwyOut) { $xg = Get-Content $xtGwyOut -Raw -ErrorAction SilentlyContinue }
  if ($xg -match '\[ENTRY_RECONCILE\]') { $xtGwy = 'ENTRY_RECONCILE_seen' }
  elseif ($xg -match '\[DSM_ENTRY_SELECT\]') { $xtGwy = 'DSM_ENTRY_SELECT_seen' }
  else { $xtGwy = 'ran_minimal' }
  Assert-Log 'xt_gwy' ($xg.Length -gt 50) 'gwy.mrp log empty'
}

$xtWx = 'missing'
$wx = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
if (Test-Path $wx) {
  Write-Host '== Phase 6B-A2 cross-target wxjwq.mrp =='
  Invoke-Live $xtWxOut (Join-Path $logDir 'phase6b_a2_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xw = ''
  if (Test-Path $xtWxOut) { $xw = Get-Content $xtWxOut -Raw -ErrorAction SilentlyContinue }
  if ($xw -match '\[ENTRY_RECONCILE\]') { $xtWx = 'ENTRY_RECONCILE_seen' }
  elseif ($xw -match '\[DSM_ENTRY_SELECT\]') { $xtWx = 'DSM_ENTRY_SELECT_seen' }
  else { $xtWx = 'ran_minimal' }
  Assert-Log 'xt_wxjwq' ($xw.Length -gt 50) 'wxjwq log empty'
}

$lines = @(
  'Phase 6B-A2 Chunk Init Entry Reconciliation',
  "jjfb_sha256=$hash",
  "case=$reconCase",
  'case_legend=A_thumb_norm|B_trampoline|C_image_plus8_wrong|D_chunk_write_wrong|E_chunk_uninit|F_unknown',
  "relation=$relation",
  "chunk_field_04_writer=$chunkWriter",
  "dsm_match=$dsmMatch",
  "header_norm=0x$headerNorm",
  "chunk_field_04_norm=0x$chunkNorm",
  "helper_norm=0x$helperNorm",
  "cross_target_gwy=$xtGwy",
  "cross_target_wxjwq=$xtWx",
  'image_plus8_meaning=DOCUMENTED mr_load_c_function/init_func candidate (fixR9+TestCom800+mrc_extLoad)',
  'registry_field=header_entry_candidate (not assumed DSM target)',
  'fix_applied=none (no context create; no PlatformRegistry; no r0 poke)',
  'phase6b_b_gate=blocked'
)
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "case=$reconCase relation=$relation chunk_writer=$chunkWriter dsm_match=$dsmMatch"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a2 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_ENTRY_RECONCILE complete'
