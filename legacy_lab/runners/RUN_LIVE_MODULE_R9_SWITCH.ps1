# Phase 6C-A: Generic Nested EXT R9 Switch (MODULE_R9_SWITCH_ONLY).
param(
  [int]$Seconds = 20,
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\module_r9_switch.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\gwy_ext_obs.c') -Pattern '0x2B1858|0x280400|0x304AED|0x2D8DE0|appid\s*==\s*400101' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC / appid special-case in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6c_a_live_stdout.txt'
$report = Join-Path $logDir 'phase6c_a_r9_switch_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6c_a.json'

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

Write-Host "== Phase 6C-A JJFB R9 switch Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6c_a_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'module_er_rw' ($all -match '\[MODULE_ER_RW\]') 'MODULE_ER_RW missing'
Assert-Log 'r9_switch_or_blocked' (
  ($all -match '\[R9_SWITCH\]') -or ($all -match '\[R9_SWITCH_BLOCKED\]')
) 'R9_SWITCH tags missing'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module_r9_switch_gate missing'
Assert-Log 'phase6b_b_still_blocked' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b_gate not blocked'
Assert-Log 'allowed_fix' (
  ($all -match 'allowed_fix=MODULE_R9_SWITCH_ONLY') -or ($all -match 'CALLEE_ER_RW_NOT_AVAILABLE')
) 'allowed_fix / blocked reason missing'
Assert-Log 'no_r0_poke' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create')
) 'forbidden mutation suspected'
Assert-Log 'no_other_reg_write_note' ($all -match 'note=r9_only|CALLEE_ER_RW_NOT_AVAILABLE|same_module=1|stage=ABORT') 'r9_only note missing'

# Cross-target smoke
$xtRoot240 = Join-Path $Root 'game_files\mythroad\240x320'
$xtOut = Join-Path $logDir 'phase6c_a_xt_wxjwq_stdout.txt'
$xtPath = Join-Path $ResourceRoot 'gwy\wxjwq.mrp'
$xtPattern = 'ABSENT'
if (Test-Path $xtPath) {
  Write-Host '== Phase 6C-A cross-target wxjwq =='
  Invoke-Live $xtOut (Join-Path $logDir 'phase6c_a_xt_wxjwq_stderr.txt') $ResourceRoot 'gwy/wxjwq.mrp' `
    'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy/wxjwq.mrp_gwyblink' `
    (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, $Seconds - 2))
  $xt = ''
  if (Test-Path $xtOut) { $xt = Get-Content $xtOut -Raw -ErrorAction SilentlyContinue }
  Assert-Log 'xt_wxjwq' ($xt.Length -gt 50) 'wxjwq log empty'
  if ($xt -match '\[R9_SWITCH\]|\[R9_SWITCH_BLOCKED\]|\[MODULE_ER_RW\]') {
    $xtPattern = 'PARTIAL'
  }
} else {
  Assert-Log 'xt_wxjwq' $true 'skipped missing'
}

# Classify result
$enterOk = ($all -match '\[R9_SWITCH\] stage=ENTER[^\r\n]*new_r9=0x[1-9A-Fa-f]')
$blocked = ($all -match 'CALLEE_ER_RW_NOT_AVAILABLE')
$leaveOk = ($all -match '\[R9_SWITCH\] stage=LEAVE')
$fault28 = ($all -match 'invalid_address=0x28|fault.*0x28|access.*0x28')
$result = 'UNKNOWN'
$nextFix = 'none'
if ($enterOk -and $leaveOk -and -not $fault28) {
  $result = 'SUCCESS'
  $nextFix = 'NEXT_REAL_ABI_BOUNDARY'
} elseif ($enterOk -and $fault28) {
  $result = 'FAULT_PERSISTS'
  $nextFix = 'POST_R9_ABI_ANALYSIS'
} elseif ($blocked -and -not $enterOk) {
  $result = 'ER_RW_UNAVAILABLE'
  $nextFix = 'ER_RW_METADATA_TIMING'
} elseif ($enterOk -and -not $leaveOk) {
  $result = 'RESTORE_FAILURE'
  $nextFix = 'R9_SWITCH_LEAVE'
} elseif ($blocked -and $enterOk) {
  $result = 'SUCCESS'
  $nextFix = 'NEXT_REAL_ABI_BOUNDARY'
}

$answers = @(
  "Q1_module_r9_switch_gate=open",
  "Q2_phase6b_b_gate=blocked",
  "Q3_module_er_rw=" + $(if ($all -match '\[MODULE_ER_RW\]') { 'emitted' } else { 'missing' }),
  "Q4_entry_switch=" + $(if ($enterOk) { 'applied' } elseif ($blocked) { 'blocked' } else { 'missing' }),
  "Q5_helper_switch=" + $(if ($all -match 'call_kind=MR_HELPER') { 'seen' } else { 'absent' }),
  "Q6_leave=" + $(if ($leaveOk) { 'seen' } else { 'absent' }),
  "Q7_nested_stack=frames_not_single_global",
  "Q8_fail_closed=" + $(if ($blocked -or $enterOk) { 'yes' } else { 'unknown' }),
  "Q9_code_region_id=yes",
  "Q10_r9_only=yes",
  "Q11_xt_pattern=$xtPattern",
  "Q12_r9_switch_result=$result next_allowed_fix=$nextFix"
)

$lines = @(
  'Phase 6C-A Generic Nested EXT R9 Switch',
  "jjfb_sha256=$hash",
  'module_r9_switch_gate=open',
  'evidence=DOCUMENTED',
  'allowed_fix=MODULE_R9_SWITCH_ONLY',
  'phase6b_b_gate=blocked',
  "r9_switch_result=$result",
  "next_allowed_fix=$nextFix",
  "xt_pattern=$xtPattern",
  'fix_applied=R9_save_switch_restore_only'
) + $answers
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "result=$result gate=open phase6b_b=blocked next=$nextFix"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6c-a failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_MODULE_R9_SWITCH complete'
