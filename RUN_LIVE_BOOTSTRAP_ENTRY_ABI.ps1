# Phase 6C-C: Bootstrap Entry R9 ABI Contract Recovery (observe-only).
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_bootstrap_abi.c'),
  (Join-Path $Root 'src\runtime\module_r9_switch.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x2D8DE0|0x274|appid\s*==\s*400101' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute ERW/PC/appid literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6c_c_live_stdout.txt'
$report = Join-Path $logDir 'phase6c_c_bootstrap_abi_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6c_c.json'

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
  $env:GWY_BOOTSTRAP_ABI = '1'
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

Write-Host "== Phase 6C-C JJFB bootstrap entry ABI Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6c_c_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'bootstrap_r9_abi' ($all -match '\[BOOTSTRAP_R9_ABI\]') 'BOOTSTRAP_R9_ABI missing'
Assert-Log 'call_kind_bootstrap' ($all -match 'call_kind=BOOTSTRAP_ENTRY') 'BOOTSTRAP_ENTRY kind missing'
Assert-Log 'bootstrap_fault' ($all -match '\[BOOTSTRAP_FAULT\]') 'BOOTSTRAP_FAULT missing'
Assert-Log 'bootstrap_r9_use' (
  ($all -match '\[BOOTSTRAP_R9_USE\]') -or
  ($all -match '\[BOOTSTRAP_FAULT\][^\r\n]*r9_in_dataflow=(yes|no)') -or
  ($all -match '\[BOOTSTRAP_FAULT\][^\r\n]*causality=R9_') -or
  ($all -match '\[BOOTSTRAP_ABI_CLASS\][^\r\n]*r9_causality=')
) 'BOOTSTRAP_R9_USE/causality missing'
Assert-Log 'bootstrap_path' ($all -match '\[BOOTSTRAP_PATH\]') 'BOOTSTRAP_PATH missing'
Assert-Log 'bootstrap_proto' ($all -match '\[BOOTSTRAP_PROTO\]') 'BOOTSTRAP_PROTO missing'
Assert-Log 'bootstrap_entry_identity' ($all -match '\[BOOTSTRAP_ENTRY_IDENTITY\]') 'BOOTSTRAP_ENTRY_IDENTITY missing'
Assert-Log 'fixr9_stage' ($all -match '\[FIXR9_STAGE_CONTRACT\]') 'FIXR9_STAGE_CONTRACT missing'
Assert-Log 'bootstrap_abi_class' ($all -match '\[BOOTSTRAP_ABI_CLASS\]') 'BOOTSTRAP_ABI_CLASS missing'
Assert-Log 'bootstrap_entry_r9_gate' ($all -match 'bootstrap_entry_r9_gate=blocked') 'bootstrap_entry_r9_gate missing'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module_r9_switch_gate missing'
Assert-Log 'phase6b_b_gate' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b_gate missing'
Assert-Log 'er_rw_timing_gate' ($all -match 'er_rw_metadata_timing_gate=blocked') 'er_rw_metadata_timing_gate missing'
Assert-Log 'no_guest_mutation' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create')
) 'guest mutation suspected'

$xtOut = Join-Path $logDir 'phase6c_c_xt_wxjwq_stdout.txt'
$xtPath = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
$xtPattern = 'ABSENT'
if (Test-Path $xtPath) {
  Write-Host '== Phase 6C-C cross-target wxjwq =='
  Invoke-Live $xtOut (Join-Path $logDir 'phase6c_c_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xt = ''
  if (Test-Path $xtOut) { $xt = Get-Content $xtOut -Raw -ErrorAction SilentlyContinue }
  Assert-Log 'xt_wxjwq' ($xt.Length -gt 50) 'wxjwq log empty'
  if ($xt -match '\[XT_BOOTSTRAP_ORDER\]|\[BOOTSTRAP_R9_ABI\]|\[BOOTSTRAP_ABI_CLASS\]') {
    $xtPattern = 'PARTIAL'
  }
  if ($xt -match '\[XT_BOOTSTRAP_ORDER\][^\r\n]*pattern=([ABCD]|PARTIAL|UNKNOWN)') {
    $xtPattern = $Matches[1]
  }
} else {
  Assert-Log 'xt_wxjwq' $true 'skipped missing'
}

$class = 'UNKNOWN'
$cm = [regex]::Matches($all, '\[BOOTSTRAP_ABI_CLASS\] class=([A-Z0-9_]+)')
if ($cm.Count -gt 0) { $class = $cm[$cm.Count - 1].Groups[1].Value }
$nextFix = 'UNKNOWN'
$nm = [regex]::Matches($all, '\[BOOTSTRAP_ABI_CLASS\][^\r\n]*next_allowed_fix=(\S+)')
if ($nm.Count -gt 0) { $nextFix = $nm[$nm.Count - 1].Groups[1].Value }
$causality = 'UNKNOWN'
$ca = [regex]::Matches($all, '\[BOOTSTRAP_ABI_CLASS\][^\r\n]*r9_causality=(\S+)')
if ($ca.Count -eq 0) {
  $ca = [regex]::Matches($all, '\[BOOTSTRAP_FAULT\][^\r\n]*causality=(\S+)')
}
if ($ca.Count -gt 0) { $causality = $ca[$ca.Count - 1].Groups[1].Value }
$addrExpr = 'unknown'
$ae = [regex]::Matches($all, '\[BOOTSTRAP_FAULT\][^\r\n]*address_expr=(\S+)')
if ($ae.Count -gt 0) { $addrExpr = $ae[$ae.Count - 1].Groups[1].Value }
$r9In = 'unknown'
if ($all -match '\[BOOTSTRAP_FAULT\][^\r\n]*r9_in_dataflow=(yes|no)') { $r9In = $Matches[1] }
$identity = 'unknown'
$im = [regex]::Matches($all, '\[BOOTSTRAP_ENTRY_IDENTITY\][^\r\n]*relation=(\S+)')
if ($im.Count -gt 0) { $identity = $im[$im.Count - 1].Groups[1].Value }

$illegalFix = $false
if ($nextFix -match 'BOOTSTRAP_TEMP_R9_SWITCH|EARLY_ER_RW|DEFER_ENTRY') {
  $gev = ''
  $gm = [regex]::Matches($all, '\[BOOTSTRAP_ABI_CLASS\][^\r\n]*gate_evidence=(\S+)')
  if ($gm.Count -gt 0) { $gev = $gm[$gm.Count - 1].Groups[1].Value }
  if ($gev -notmatch 'DOCUMENTED|CROSS_TARGET') { $illegalFix = $true }
}
Assert-Log 'next_fix_gated' (-not $illegalFix) "illegal next_allowed_fix=$nextFix"

$pathLine = ''
if ($all -match '(\[BOOTSTRAP_PATH\][^\r\n]+)') { $pathLine = $Matches[1] }
$protoLine = ''
if ($all -match '(\[BOOTSTRAP_PROTO\][^\r\n]+)') { $protoLine = $Matches[1] }
$staticLine = ''
if ($all -match '(\[BOOTSTRAP_STATIC_BASE\][^\r\n]+)') { $staticLine = $Matches[1] }

$answers = @(
  "Q1_fault_address_expr=$addrExpr",
  "Q2_r9_in_dataflow=$r9In",
  "Q3_r9_causality=$causality",
  "Q4_bootstrap_path=" + $(if ($pathLine) { $pathLine } else { 'missing' }),
  "Q5_bootstrap_proto=" + $(if ($protoLine) { $protoLine } else { 'missing' }),
  "Q6_entry_identity=$identity",
  "Q7_static_base=" + $(if ($staticLine) { $staticLine } else { 'missing' }),
  "Q8_fixr9_stages=" + $(if ($all -match '\[FIXR9_STAGE_CONTRACT\]') { 'emitted_bootstrap_helper_callback' } else { 'missing' }),
  "Q9_xt_pattern=$xtPattern",
  "Q10_class=$class next_allowed_fix=$nextFix note=R9_causal_but_no_DOCUMENTED_CROSS_TARGET_fix",
  "Q11_observe_only=yes no_r9_invent no_early_er_rw no_defer_entry",
  "Q12_gates=bootstrap_entry_r9_gate=blocked;module_r9_switch_gate=open;phase6b_b_gate=blocked;er_rw_metadata_timing_gate=blocked"
)

$lines = @(
  'Phase 6C-C Bootstrap Entry R9 ABI Contract Recovery',
  "jjfb_sha256=$hash",
  'bootstrap_entry_r9_gate=blocked',
  'module_r9_switch_gate=open',
  'phase6b_b_gate=blocked',
  'er_rw_metadata_timing_gate=blocked',
  "abi_class=$class",
  "r9_causality=$causality",
  "fault_address_expr=$addrExpr",
  "entry_identity=$identity",
  "next_allowed_fix=$nextFix",
  "xt_pattern=$xtPattern",
  'mode=observe_only'
) + $answers
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "class=$class causality=$causality expr=$addrExpr next=$nextFix"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6c-c failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_BOOTSTRAP_ENTRY_ABI complete'
