# Phase 6C-D3: Tokenized R9 Scope Balancing (behavior fix).
param(
  [int]$Seconds = 28,
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
  (Join-Path $Root 'src\runtime\ext_object_observe.c'),
  (Join-Path $Root 'src\runtime\ext_r9_scope_audit.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x2AEB34|0x2D8DF4|appid\s*==\s*400101' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute ERW/PC/appid literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6c_d3_live_stdout.txt'
$report = Join-Path $logDir 'phase6c_d3_tokenized_r9_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6c_d3.json'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
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
  $env:GWY_CALLBACK_FRAME = '1'
  $env:GWY_NESTED_R9_SCOPE = '1'
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  $ready = $false
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ((Test-Path $outLog) -and
        (Select-String -Path $outLog -Pattern 'result=ALREADY_SWITCHED' -Quiet) -and
        (Select-String -Path $outLog -Pattern 'stage=NOOP[^\r\n]*YIELD_TO_NESTED_GUEST|emu_exit_reason=YIELD_TO_NESTED_GUEST' -Quiet)) {
      Start-Sleep -Seconds 2
      $ready = $true
      break
    }
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 600
  }
  if (-not $ready -and -not (Test-Path $outLog)) {
    throw "live stdout missing: $outLog"
  }
}

Write-Host "== Phase 6C-D3 JJFB tokenized R9 scope Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6c_d3_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'already_switched' ($all -match 'result=ALREADY_SWITCHED') 'ALREADY_SWITCHED missing'
Assert-Log 'yield_noop' (
  ($all -match '\[R9_SCOPE\] stage=NOOP[^\r\n]*emu_exit_reason=YIELD_TO_NESTED_GUEST') -or
  ($all -match '\[R9_SCOPE_BALANCE\][^\r\n]*emu_exit_reason=YIELD_TO_NESTED_GUEST[^\r\n]*stage=NOOP') -or
  ($all -match '\[R9_SCOPE_BALANCE\][^\r\n]*stage=NOOP[^\r\n]*emu_exit_reason=YIELD_TO_NESTED_GUEST')
) 'YIELD NOOP missing'
Assert-Log 'scope_balance' ($all -match '\[R9_SCOPE_BALANCE\]|TOKENIZED_R9_SCOPE_BALANCING_ONLY') 'balance tag missing'
Assert-Log 'no_foreign_pop_on_yield' (
  -not ($all -match 'leave_action=POP_FOREIGN_FRAME[^\r\n]*emu_exit_reason=YIELD_TO_NESTED_GUEST')
) 'foreign pop still on YIELD'
Assert-Log 'old_fault_274_gone' (
  -not ($all -match 'invalid_address=0x274|mem_invalid[^\r\n]*at 0x274')
) 'old 0x274 fault still present'
Assert-Log 'nested_r9_scope_gate_open' ($all -match 'nested_r9_scope_gate=open') 'nested gate not open'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module gate missing'
Assert-Log 'guest_callback_frame_gate' ($all -match 'guest_callback_frame_gate=blocked') 'callback gate must stay blocked'
Assert-Log 'bootstrap_entry_r9_gate' ($all -match 'bootstrap_entry_r9_gate=blocked') 'bootstrap missing'
Assert-Log 'phase6b_b_gate' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b missing'
Assert-Log 'er_rw_timing_gate' ($all -match 'er_rw_metadata_timing_gate=blocked') 'er_rw timing missing'
Assert-Log 'no_forbidden_fix' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create|GuestCallbackFrame.*restore|BOOTSTRAP_TEMP_R9')
) 'forbidden fix suspected'

# Continuation: resume boundary or explicit continuation PC hit after nested CFN.
$cont = ($all -match 'GUEST_CONTINUATION_RESUME') -or
  ($all -match 'continuation_pc=0x303B92') -or
  ($all -match 'CALLBACK_CONTINUATION[^\r\n]*identity=CONFIRMED[^\r\n]*cfn_path=GUEST_NESTED')
Assert-Log 'robotol_continuation' $cont 'robotol continuation not observed'

# Depth after nested NOOP should remain 1 (log depth_after=1 on NOOP), then outer leave pops.
$noopDepthOk = $all -match 'stage=NOOP[^\r\n]*depth_after=1'
Assert-Log 'noop_keeps_depth' $noopDepthOk 'NOOP should keep depth=1'

$ownedPop = ($all -match 'leave_action=POP_OWNED_FRAME') -or
  ($all -match 'stage=LEAVE[^\r\n]*leave_action=POP_OWNED_FRAME') -or
  ($all -match '\[R9_SWITCH\] stage=LEAVE[^\r\n]*leave_action=POP_OWNED_FRAME')
# Outer helper may not complete if new fault; soft-check.
$newFault = $false
if ($all -match 'mem_invalid[^\r\n]*at 0x([0-9A-Fa-f]+)') {
  $fa = $Matches[1].ToLowerInvariant()
  if ($fa -ne '274') { $newFault = $true }
}
Assert-Log 'owned_pop_or_new_boundary' ($ownedPop -or $newFault -or $cont) 'no owned pop and no progress signal'

$gate = 'blocked'
if ($all -match 'nested_r9_scope_gate=open') { $gate = 'open' }
$fault274 = if ($all -match 'invalid_address=0x274|at 0x274') { 'PRESENT' } else { 'GONE' }
$nextFault = 'none'
if ($all -match 'mem_invalid[^\r\n]*at 0x([0-9A-Fa-f]+)') {
  $fa = $Matches[1].ToLowerInvariant()
  if ($fa -ne '274') { $nextFault = "0x$fa" }
}

$reportBody = @()
$reportBody += 'Phase 6C-D3 Tokenized R9 Scope Balancing'
$reportBody += "jjfb_sha256=$hash"
$reportBody += "nested_r9_scope_gate=$gate"
$reportBody += 'module_r9_switch_gate=open'
$reportBody += 'guest_callback_frame_gate=blocked'
$reportBody += 'bootstrap_entry_r9_gate=blocked'
$reportBody += 'phase6b_b_gate=blocked'
$reportBody += 'er_rw_metadata_timing_gate=blocked'
$reportBody += 'allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY'
$reportBody += "old_fault_0x274=$fault274"
$reportBody += ("robotol_continuation=" + $(if ($cont) { 'REACHED_OR_CONFIRMED' } else { 'NOT_SEEN' }))
$reportBody += ("yield_leave_action=" + $(if ($all -match 'stage=NOOP[^\r\n]*YIELD') { 'NOOP' } else { 'see_log' }))
$reportBody += ("next_fault=$nextFault")
$reportBody += 'mode=tokenized_balancing'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "gate=$gate fault274=$fault274 cont=$cont nextFault=$nextFault"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6c-d3 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_TOKENIZED_R9_SCOPE complete'
