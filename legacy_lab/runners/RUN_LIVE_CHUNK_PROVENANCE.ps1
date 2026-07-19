# Phase 6B-A3: Chunk Construction Provenance (observe-only).
param(
  [int]$Seconds = 12,
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
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\module_registry.c') -Pattern '0x304558|0x2D8DE0|0x22548' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_a3_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a3_chunk_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a3.json'
$xtGwyOut = Join-Path $logDir 'phase6b_a3_xt_gwy_stdout.txt'
$xtWxOut = Join-Path $logDir 'phase6b_a3_xt_wxjwq_stdout.txt'

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
  # Heavy disasm off by default (A2 reconcile timeout avoidance).
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

Write-Host "== Phase 6B-A3 JJFB chunk provenance Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a3_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'chunk_record' ($all -match '\[CHUNK_RECORD\]') 'ExtChunkRecord missing'
Assert-Log 'chunk_snapshot' ($all -match '\[CHUNK_SNAPSHOT\]') 'CHUNK_SNAPSHOT missing'
Assert-Log 'chunk_write_or_none' (
  $all -match '\[CHUNK_WRITE\]|note=NONE_BEFORE_SELECT'
) 'CHUNK_WRITE or NONE missing'
Assert-Log 'entry_transfer' ($all -match '\[ENTRY_TRANSFER\]') 'ENTRY_TRANSFER missing'
Assert-Log 'chunk_ptr_or_field' (
  $all -match '\[CHUNK_PTR_TRACE\]|\[CHUNK_FIELD_04\]|chunk_field_04='
) 'chunk ptr/field typing missing'
Assert-Log 'provenance_case' ($all -match '\[CHUNK_PROVENANCE_CASE\] case=[A-G]') 'CHUNK_PROVENANCE_CASE missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|mapped low address 0x28|PlatformRegistry')
) 'fake context / PlatformRegistry suspected'
Assert-Log 'no_r0_poke' (-not ($all -match 'r0_poke|force_r0\s*=')) 'r0 poke suspected'

$provCase = 'G'
$caseMatches = [regex]::Matches($all, '\[CHUNK_PROVENANCE_CASE\] case=([A-G])')
if ($caseMatches.Count -gt 0) {
  $provCase = $caseMatches[$caseMatches.Count - 1].Groups[1].Value
}

$xtGwy = 'missing'
$xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
if (Test-Path (Join-Path $xtRoot 'gwy.mrp')) {
  Write-Host '== Phase 6B-A3 cross-target gwy.mrp =='
  Invoke-Live $xtGwyOut (Join-Path $logDir 'phase6b_a3_xt_gwy_stderr.txt') $xtRoot 'gwy.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xg = ''
  if (Test-Path $xtGwyOut) { $xg = Get-Content $xtGwyOut -Raw -ErrorAction SilentlyContinue }
  if ($xg -match '\[CHUNK_PROVENANCE_CASE\]|\[CHUNK_SNAPSHOT\]|\[ENTRY_TRANSFER\]') {
    $xtGwy = 'provenance_tags_seen'
  } elseif ($xg -match '\[DSM_ENTRY_SELECT\]') { $xtGwy = 'DSM_ENTRY_SELECT_seen' }
  else { $xtGwy = 'ran_minimal' }
  Assert-Log 'xt_gwy' ($xg.Length -gt 50) 'gwy.mrp log empty'
}

$xtWx = 'missing'
$wx = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
if (Test-Path $wx) {
  Write-Host '== Phase 6B-A3 cross-target wxjwq.mrp =='
  Invoke-Live $xtWxOut (Join-Path $logDir 'phase6b_a3_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xw = ''
  if (Test-Path $xtWxOut) { $xw = Get-Content $xtWxOut -Raw -ErrorAction SilentlyContinue }
  if ($xw -match '\[CHUNK_PROVENANCE_CASE\]|\[CHUNK_SNAPSHOT\]|\[ENTRY_TRANSFER\]') {
    $xtWx = 'provenance_tags_seen'
  } elseif ($xw -match '\[DSM_ENTRY_SELECT\]') { $xtWx = 'DSM_ENTRY_SELECT_seen' }
  else { $xtWx = 'ran_minimal' }
  Assert-Log 'xt_wxjwq' ($xw.Length -gt 50) 'wxjwq log empty'
}

$lines = @(
  'Phase 6B-A3 Chunk Construction Provenance',
  "jjfb_sha256=$hash",
  "case=$provCase",
  'case_legend=A_observer_late|B_block_copy|C_static_template|D_wrong_identity|E_missing_bootstrap|F_corrupt_field|G_unknown',
  "cross_target_gwy=$xtGwy",
  "cross_target_wxjwq=$xtWx",
  'header_entry_candidate=observe_compare_only',
  'chunk_field_04=raw_last_seen_chunk_plus4',
  'fix_applied=none (no chunk/context create; no PlatformRegistry; no r0 poke)',
  'phase6b_b_gate=only_Case_E_with_DOCUMENTED_or_CROSS_TARGET'
)
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "case=$provCase xt_gwy=$xtGwy xt_wx=$xtWx"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a3 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_CHUNK_PROVENANCE complete'
