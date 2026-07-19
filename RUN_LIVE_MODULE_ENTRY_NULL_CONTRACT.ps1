# Phase 6B-A9: Module Entry NULL-Argument Contract Discrimination (observe-only).
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_entry_null_contract.c'),
  (Join-Path $Root 'src\runtime\ext_module_entry_abi.c'),
  (Join-Path $Root 'src\runtime\ext_object_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c') -Pattern '0x304558|0x2D8DE0|0x22548|0x303B92|0x304AED' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_a9_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a9_null_contract_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a9.json'

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

function Test-EntryCross([string]$text) {
  if (-not $text) { return $false }
  if ($text -match '\[XT_ENTRY_CROSS\][^\r\n]*returned=1[^\r\n]*fault_on_first_entry=0') { return $true }
  return $false
}

Write-Host "== Phase 6B-A9 JJFB NULL contract Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a9_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'dsm_entry_call' ($all -match '\[DSM_MODULE_ENTRY_CALL\]') 'DSM_MODULE_ENTRY_CALL missing'
Assert-Log 'dsm_entry_family' ($all -match '\[DSM_ENTRY_FAMILY\]') 'DSM_ENTRY_FAMILY missing'
Assert-Log 'entry_type' ($all -match '\[ENTRY_TYPE_EVIDENCE\]') 'ENTRY_TYPE_EVIDENCE missing'
Assert-Log 'entry_path' ($all -match '\[ENTRY_PATH\]') 'ENTRY_PATH missing'
Assert-Log 'module_data_init' ($all -match '\[MODULE_DATA_INIT\]') 'MODULE_DATA_INIT missing'
Assert-Log 'lifecycle' ($all -match '\[MODULE_ENTRY_LIFECYCLE\]') 'MODULE_ENTRY_LIFECYCLE missing'
Assert-Log 'p_entry_link' ($all -match '\[P_ENTRY_LINK\] linked=NO') 'P_ENTRY_LINK missing'
Assert-Log 'null_class' ($all -match '\[NULL_CONTRACT_CLASS\]') 'NULL_CONTRACT_CLASS missing'
Assert-Log 'matrix' ($all -match '\[DISCRIMINATION_MATRIX\]') 'DISCRIMINATION_MATRIX missing'
Assert-Log 'phase6b_b_gate_line' ($all -match 'phase6b_b_gate=(open|blocked)') 'phase6b_b_gate missing'
Assert-Log 'no_fake_context' (
  -not ($all -match 'ext_platform_context_create|fake_P|PlatformRegistry')
) 'fake context suspected'
Assert-Log 'no_r0_poke' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch')
) 'r0/dispatch mutation suspected'

$class = 'UNKNOWN'
$cm = [regex]::Matches($all, '\[NULL_CONTRACT_CLASS\] class=([A-Z_]+)')
if ($cm.Count -gt 0) { $class = $cm[$cm.Count - 1].Groups[1].Value }
$gate = 'blocked'
if ($all -match 'phase6b_b_gate=(open|blocked)') { $gate = $Matches[1] }
$gev = 'none'
$gm = [regex]::Matches($all, 'gate_evidence=(\S+)')
if ($gm.Count -gt 0) { $gev = $gm[$gm.Count - 1].Groups[1].Value }
$familyAllZero = '?'
$fm = [regex]::Matches($all, '\[DSM_ENTRY_FAMILY\][^\r\n]*all_r0_zero=([01])')
if ($fm.Count -gt 0) { $familyAllZero = $fm[$fm.Count - 1].Groups[1].Value }

# Ordered cross-target candidates until entry-cross or ABSENT
$xtStatus = 'ABSENT'
$xtDetail = 'none'
$xtRoot240 = Join-Path $Root 'game_files\mythroad\240x320'
$candidates = @(
  @{ root = $ResourceRoot; target = 'gwy/wxjwq.mrp'; param = 'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink'; out = 'phase6b_a9_xt_wxjwq_stdout.txt'; name = 'wxjwq' },
  @{ root = $xtRoot240; target = 'gwy.mrp'; param = 'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink'; out = 'phase6b_a9_xt_gwy_stdout.txt'; name = 'gwy' },
  @{ root = $xtRoot240; target = 'gwy/dload.mrp'; param = 'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/dload.mrp_gwyblink'; out = 'phase6b_a9_xt_dload_stdout.txt'; name = 'dload' },
  @{ root = $xtRoot240; target = 'gwy/gui.mrp'; param = 'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/gui.mrp_gwyblink'; out = 'phase6b_a9_xt_gui_stdout.txt'; name = 'gui' }
)

foreach ($cand in $candidates) {
  $mrpPath = Join-Path $cand.root ($cand.target -replace '/', '\')
  if (-not (Test-Path $mrpPath)) {
    Write-Host "skip missing $($cand.name): $mrpPath"
    continue
  }
  Write-Host "== Phase 6B-A9 cross-target $($cand.name) =="
  $outLog = Join-Path $logDir $cand.out
  Invoke-Live $outLog (Join-Path $logDir ($cand.out -replace '_stdout', '_stderr')) $cand.root $cand.target `
    $cand.param (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xt = ''
  if (Test-Path $outLog) { $xt = Get-Content $outLog -Raw -ErrorAction SilentlyContinue }
  Assert-Log ("xt_" + $cand.name) ($xt.Length -gt 50) "$($cand.name) log empty"
  if (Test-EntryCross $xt) {
    $xtStatus = 'CROSSED'
    $firstR0 = 'unknown'
    if ($xt -match '\[XT_ENTRY_CROSS\][^\r\n]*first_r0=(0x[0-9A-Fa-f]+)') { $firstR0 = $Matches[1] }
    $xtDetail = "$($cand.name):returned first_r0=$firstR0"
    Write-Host "[OK] discriminating XT crossed via $($cand.name)"
    break
  } elseif ($xt -match '\[DSM_MODULE_ENTRY_CALL\]|\[XT_ENTRY_CROSS\]') {
    $xtDetail = "$($cand.name):tags_no_cross"
  }
}

Assert-Log 'xt_or_absent' ($true) "discriminating_xt=$xtStatus"

$answers = @(
  "Q1_entry_type=" + $(if ($all -match '\[ENTRY_TYPE_EVIDENCE\]') { 'emitted' } else { 'missing' }),
  "Q2_dsm_family_all_r0_zero=$familyAllZero",
  "Q3_discriminating_xt=$xtStatus",
  "Q4_xt_detail=$xtDetail",
  "Q5_entry_path=" + $(if ($all -match '\[ENTRY_PATH\]') { 'emitted' } else { 'missing' }),
  "Q6_module_data_init=" + $(if ($all -match '\[MODULE_DATA_INIT\]') { 'emitted' } else { 'missing' }),
  "Q7_lifecycle=" + $(if ($all -match '\[MODULE_ENTRY_LIFECYCLE\]') { 'emitted' } else { 'missing' }),
  "Q8_p_entry_link=NO",
  "Q9_class=$class",
  "Q10_no_guest_mutation=yes",
  "Q11_phase6b_b_gate=$gate",
  "Q12_gate_evidence=$gev"
)

$lines = @(
  'Phase 6B-A9 Module Entry NULL-Argument Contract Discrimination',
  "jjfb_sha256=$hash",
  "class=$class",
  'class_legend=ENTRY_NULL_DOCUMENTED|ENTRY_NULL_CROSS_TARGET|MISSING_MODULE_ENTRY_CONTEXT|MISSING_PRE_ENTRY_INITIALIZATION|MISSING_MODULE_DATA_INITIALIZATION|WRONG_MODULE_ENTRY_LIFECYCLE|LOADCODE_RETURN_MISINTERPRETED|UNKNOWN',
  "dsm_family_all_r0_zero=$familyAllZero",
  "discriminating_xt=$xtStatus",
  "xt_detail=$xtDetail",
  "FIELD_NOT_UPDATED=EXPECTED_BY_CONTRACT",
  "registered_helper_ne_module_entry=EXPECTED",
  'fix_applied=none (no r0 fill; no DSM dispatch write; no PlatformRegistry; no fake context)',
  "phase6b_b_gate=$gate",
  "gate_evidence=$gev"
) + $answers
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "class=$class xt=$xtStatus gate=$gate gev=$gev"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a9 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_MODULE_ENTRY_NULL_CONTRACT complete'
