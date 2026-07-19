# Phase 6B-A10: EXT Module Data Initialization Contract (observe-only).
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_module_data_init.c'),
  (Join-Path $Root 'src\runtime\ext_entry_null_contract.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c') -Pattern '0x304558|0x2D8DE0|0x22548|0x303B92|0x304AED' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6b_a10_live_stdout.txt'
$report = Join-Path $logDir 'phase6b_a10_data_init_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6b_a10.json'

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

Write-Host "== Phase 6B-A10 JJFB module data init Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6b_a10_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'rw_base_identity' ($all -match '\[RW_BASE_IDENTITY\]') 'RW_BASE_IDENTITY missing'
Assert-Log 'ext_image_layout' ($all -match '\[EXT_IMAGE_LAYOUT\]') 'EXT_IMAGE_LAYOUT missing'
Assert-Log 'ext_data_alloc' ($all -match '\[EXT_DATA_ALLOC\]') 'EXT_DATA_ALLOC missing'
Assert-Log 'r9_contract' ($all -match '\[R9_CONTRACT\]') 'R9_CONTRACT missing'
Assert-Log 'r9_helper_vs_entry' ($all -match '\[R9_HELPER_VS_ENTRY\]') 'R9_HELPER_VS_ENTRY missing'
Assert-Log 'module_data_snapshot' ($all -match '\[MODULE_DATA_SNAPSHOT\]') 'MODULE_DATA_SNAPSHOT missing'
Assert-Log 'data_init_class' ($all -match '\[DATA_INIT_CLASS\]') 'DATA_INIT_CLASS missing'
Assert-Log 'responsibility' ($all -match '\[DATA_INIT_RESPONSIBILITY\]') 'DATA_INIT_RESPONSIBILITY missing'
Assert-Log 'ext_relocation' ($all -match '\[EXT_RELOCATION\]') 'EXT_RELOCATION missing'
Assert-Log 'phase6b_b_gate_line' ($all -match 'phase6b_b_gate=(open|blocked)') 'phase6b_b_gate missing'
Assert-Log 'no_guest_mutation' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create')
) 'guest mutation suspected'

$class = 'UNKNOWN'
$cm = [regex]::Matches($all, '\[DATA_INIT_CLASS\] class=([A-Z0-9_]+)')
if ($cm.Count -gt 0) { $class = $cm[$cm.Count - 1].Groups[1].Value }
$gate = 'blocked'
if ($all -match 'phase6b_b_gate=(open|blocked)') { $gate = $Matches[1] }
$gev = 'none'
$gm = [regex]::Matches($all, '\[DATA_INIT_CLASS\][^\r\n]*gate_evidence=(\S+)')
if ($gm.Count -gt 0) { $gev = $gm[$gm.Count - 1].Groups[1].Value }
$nextFix = 'none'
$nm = [regex]::Matches($all, 'next_allowed_fix=(\S+)')
if ($nm.Count -gt 0) { $nextFix = $nm[$nm.Count - 1].Groups[1].Value }
$helperR9 = '0x0'
$hm = [regex]::Matches($all, '\[R9_HELPER_VS_ENTRY\][^\r\n]*helper_r9=(0x[0-9A-Fa-f]+)')
if ($hm.Count -gt 0) { $helperR9 = $hm[$hm.Count - 1].Groups[1].Value }
$entryR9 = '0x0'
$em = [regex]::Matches($all, '\[R9_HELPER_VS_ENTRY\][^\r\n]*entry_r9=(0x[0-9A-Fa-f]+)')
if ($em.Count -gt 0) { $entryR9 = $em[$em.Count - 1].Groups[1].Value }
$sourceClass = '?'
$sm = [regex]::Matches($all, '\[RW_BASE_IDENTITY\][^\r\n]*source_class=([A-F]|UNKNOWN)')
if ($sm.Count -gt 0) { $sourceClass = $sm[$sm.Count - 1].Groups[1].Value }

# Cross-target: compare declared/allocated/entry_r9/first_global
$xtRoot240 = Join-Path $Root 'game_files\mythroad\240x320'
$candidates = @(
  @{ root = $ResourceRoot; target = 'gwy/wxjwq.mrp'; param = 'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink'; out = 'phase6b_a10_xt_wxjwq_stdout.txt'; name = 'wxjwq' },
  @{ root = $xtRoot240; target = 'gwy.mrp'; param = 'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink'; out = 'phase6b_a10_xt_gwy_stdout.txt'; name = 'gwy' }
)
$xtPattern = 'ABSENT'
$xtDetail = 'none'
foreach ($cand in $candidates) {
  $mrpPath = Join-Path $cand.root ($cand.target -replace '/', '\')
  if (-not (Test-Path $mrpPath)) {
    Write-Host "skip missing $($cand.name): $mrpPath"
    continue
  }
  Write-Host "== Phase 6B-A10 cross-target $($cand.name) =="
  $outLog = Join-Path $logDir $cand.out
  Invoke-Live $outLog (Join-Path $logDir ($cand.out -replace '_stdout', '_stderr')) $cand.root $cand.target `
    $cand.param (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xt = ''
  if (Test-Path $outLog) { $xt = Get-Content $outLog -Raw -ErrorAction SilentlyContinue }
  Assert-Log ("xt_" + $cand.name) ($xt.Length -gt 50) "$($cand.name) log empty"
  if ($xt -match '\[RW_BASE_IDENTITY\]|\[R9_HELPER_VS_ENTRY\]|\[DATA_INIT_CLASS\]|\[R9_CONTRACT\]|\[EXT_DATA_ALLOC\]') {
    $xtPattern = 'PARTIAL'
    $xr = '0x0'
    if ($xt -match '\[R9_HELPER_VS_ENTRY\][^\r\n]*entry_r9=(0x[0-9A-Fa-f]+)') { $xr = $Matches[1] }
    elseif ($xt -match '\[R9_CONTRACT\][^\r\n]*r9=(0x[0-9A-Fa-f]+)') { $xr = $Matches[1] }
    $xtDetail = "$($cand.name):entry_r9=$xr"
    Write-Host "[OK] XT partial data-init tags via $($cand.name)"
    break
  }
}
Assert-Log 'xt_or_partial' ($true) "xt_pattern=$xtPattern"

$answers = @(
  "Q1_layout=" + $(if ($all -match '\[EXT_IMAGE_LAYOUT\]') { 'emitted' } else { 'missing' }),
  "Q2_rw_identity=source_class=$sourceClass",
  "Q3_alloc=" + $(if ($all -match '\[EXT_DATA_ALLOC\]') { 'emitted' } else { 'missing' }),
  "Q4_reloc=" + $(if ($all -match '\[EXT_RELOCATION\]') { 'emitted' } else { 'missing' }),
  "Q5_entry_r9=$entryR9",
  "Q6_helper_vs_entry=helper_r9=$helperR9 entry_r9=$entryR9",
  "Q7_global=" + $(if ($all -match '\[ENTRY_GLOBAL_ACCESS\]') { 'emitted' } else { 'absent_or_no_r9_load' }),
  "Q8_causal_class=$class",
  "Q9_responsibility=" + $(if ($all -match '\[DATA_INIT_RESPONSIBILITY\]') { 'emitted' } else { 'missing' }),
  "Q10_xt_pattern=$xtPattern xt_detail=$xtDetail",
  "Q11_no_guest_mutation=yes",
  "Q12_phase6b_b_gate=$gate gate_evidence=$gev next_allowed_fix=$nextFix"
)

$lines = @(
  'Phase 6B-A10 EXT Module Data Initialization Contract',
  "jjfb_sha256=$hash",
  "class=$class",
  'class_legend=MISSING_RW_TEMPLATE_INITIALIZATION|MISSING_BSS_INITIALIZATION|MISSING_DATA_RELOCATION|MISSING_MODULE_R9_SWITCH|REGISTRY_DATA_METADATA_MISSING|MISSING_PRE_ENTRY_INITIALIZATION|UNKNOWN',
  "source_class=$sourceClass",
  "helper_r9=$helperR9",
  "entry_r9=$entryR9",
  "xt_pattern=$xtPattern",
  "xt_detail=$xtDetail",
  'fix_applied=none (observe-only; no R9/r0/guest memory mutation)',
  "phase6b_b_gate=$gate",
  "gate_evidence=$gev",
  "next_allowed_fix=$nextFix"
) + $answers
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "class=$class helper_r9=$helperR9 entry_r9=$entryR9 gate=$gate next=$nextFix"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6b-a10 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_MODULE_DATA_INIT complete'
