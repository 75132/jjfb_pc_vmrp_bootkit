# Phase 6C-D2: Nested CFN R9 Scope Ownership Audit (observe-only).
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_r9_scope_audit.c'),
  (Join-Path $Root 'src\runtime\module_r9_switch.c'),
  (Join-Path $Root 'src\runtime\ext_callback_frame.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x2AEB34|0x2D8DF4|appid\s*==\s*400101' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute ERW/PC/appid literals in core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6c_d2_live_stdout.txt'
$report = Join-Path $logDir 'phase6c_d2_r9_scope_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6c_d2.json'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
# Clear residual main.exe lock / stale process before live.
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
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if ((Test-Path $outLog) -and
        ((Select-String -Path $outLog -Pattern 'result=ALREADY_SWITCHED' -Quiet) -or
         (Select-String -Path $outLog -Pattern '\[R9_SCOPE_FIRST_ZERO\]' -Quiet))) {
      Start-Sleep -Seconds 2
      break
    }
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 600
  }
}

Write-Host "== Phase 6C-D2 JJFB nested R9 scope Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6c_d2_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'r9_scope_enter' ($all -match '\[R9_SCOPE\] stage=ENTER') 'R9_SCOPE ENTER missing'
Assert-Log 'r9_scope_leave_or_noop' (
  ($all -match '\[R9_SCOPE\] stage=(LEAVE_REQUEST|NOOP|LEAVE|REJECT)') -or
  ($all -match '\[R9_SCOPE_BALANCE\]')
) 'R9_SCOPE leave/NOOP missing'
Assert-Log 'r9_write' ($all -match '\[R9_WRITE\]') 'R9_WRITE missing'
Assert-Log 'already_switched' ($all -match 'result=ALREADY_SWITCHED|already_switched=1') 'ALREADY_SWITCHED missing'
# D2 historically required POP_FOREIGN; D3 fixes that path to NOOP. Accept either evidence.
$d2BugSeen = ($all -match 'leave_action=POP_FOREIGN_FRAME|POP_FOREIGN_FRAME') -and
  ($all -match '\[R9_SCOPE_FIRST_ZERO\]')
$d3Fixed = ($all -match 'stage=NOOP[^\r\n]*YIELD_TO_NESTED_GUEST') -or
  ($all -match '\[R9_SCOPE_BALANCE\][^\r\n]*NOOP')
Assert-Log 'foreign_pop_or_d3_noop' ($d2BugSeen -or $d3Fixed) 'neither D2 foreign-pop nor D3 NOOP'
Assert-Log 'first_zero_or_balanced' (
  ($all -match '\[R9_SCOPE_FIRST_ZERO\]') -or $d3Fixed
) 'FIRST_ZERO missing (and not D3-balanced)'
Assert-Log 'scope_class_or_balance' (
  ($all -match '\[R9_SCOPE_CLASS\]') -or ($all -match '\[R9_SCOPE_BALANCE\]')
) 'R9_SCOPE_CLASS/BALANCE missing'
Assert-Log 'scope_interference_or_balance' (
  ($all -match '\[R9_SCOPE_INTERFERENCE\]|\[CALLBACK_R9_SCOPE_INTERFERENCE\]') -or $d3Fixed
) 'scope interference missing'
Assert-Log 'nested_r9_scope_gate' ($all -match 'nested_r9_scope_gate=(blocked|open)') 'nested gate missing'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module_r9_switch_gate missing'
Assert-Log 'guest_callback_frame_gate' ($all -match 'guest_callback_frame_gate=blocked') 'guest_callback_frame must stay blocked'
Assert-Log 'bootstrap_entry_r9_gate' ($all -match 'bootstrap_entry_r9_gate=blocked') 'bootstrap gate missing'
Assert-Log 'phase6b_b_gate' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b missing'
Assert-Log 'er_rw_timing_gate' ($all -match 'er_rw_metadata_timing_gate=blocked') 'er_rw timing missing'
Assert-Log 'observe_or_balanced' (
  ($all -match 'observe_only_no_scope_balancing|mode=observe_only') -or
  ($all -match 'TOKENIZED_R9_SCOPE_BALANCING_ONLY|tokenized_leave_keeps_outer_frame')
) 'observe_only/balance note missing'
Assert-Log 'no_guest_mutation' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create|GuestCallbackFrame.*restore')
) 'guest mutation suspected'
Assert-Log 'emu_yield' ($all -match 'emu_exit_reason=YIELD_TO_NESTED_GUEST') 'YIELD_TO_NESTED_GUEST missing'
Assert-Log 'fault_or_progress' (
  ($all -match 'stage=FAULT[^\r\n]*function_entry_pc=') -or
  ($all -match 'FAULT_IN_CFN_CALLEE[^\r\n]*function_entry_pc=') -or
  ($all -match '\[R9_SCOPE_CLASS\][^\r\n]*function_entry_pc=') -or
  ($all -match 'GUEST_CONTINUATION_RESUME') -or
  $d3Fixed
) 'fault PC split / progress missing'

$class = 'UNKNOWN'
$cm = [regex]::Matches($all, '\[R9_SCOPE_CLASS\] class=([A-Z0-9_]+)')
if ($cm.Count -gt 0) { $class = $cm[$cm.Count - 1].Groups[1].Value }
elseif ($d3Fixed) { $class = 'EMU_YIELD_MISCLASSIFIED_AS_RETURN_FIXED' }
$allowed = @(
  'UNBALANCED_ALREADY_SWITCHED_SCOPE',
  'PREMATURE_OUTER_MR_HELPER_POP',
  'EMU_YIELD_MISCLASSIFIED_AS_RETURN',
  'EMU_YIELD_MISCLASSIFIED_AS_RETURN_FIXED',
  'GUEST_INSTRUCTION_WRITES_R9',
  'HOST_CONTEXT_RESET_WRITES_R9',
  'R9_WRITE_SOURCE_UNKNOWN'
)
Assert-Log 'class_allowed' ($allowed -contains $class) "class=$class"

$nextFix = 'UNKNOWN'
$nm = [regex]::Matches($all, '\[R9_SCOPE_CLASS\][^\r\n]*next_allowed_fix=(\S+)')
if ($nm.Count -gt 0) { $nextFix = $nm[$nm.Count - 1].Groups[1].Value }
elseif ($all -match 'next_allowed_fix=TOKENIZED_R9_SCOPE_BALANCING_ONLY') {
  $nextFix = 'TOKENIZED_R9_SCOPE_BALANCING_ONLY'
}
$gate = 'blocked'
if ($all -match 'nested_r9_scope_gate=open') { $gate = 'open' }
elseif ($all -match '\[R9_SCOPE_CLASS\][^\r\n]*nested_r9_scope_gate=(open|blocked)') {
  $gate = $Matches[1]
}
$writer = 'unknown'
if ($all -match '\[R9_SCOPE_FIRST_ZERO\][^\r\n]*reason=(\S+)') { $writer = $Matches[1] }
elseif ($d3Fixed) { $writer = 'NONE_BALANCED' }
$callsite = 'unknown'
if ($all -match '\[R9_SCOPE_FIRST_ZERO\][^\r\n]*host_callsite=(\S+)') { $callsite = $Matches[1] }
elseif ($d3Fixed) { $callsite = 'CROSS_MODULE_MRP_TO_DSM_NOOP' }
$ev = 'unknown'
if ($all -match '\[R9_SCOPE_FIRST_ZERO\][^\r\n]*event_seq=(\d+)') { $ev = $Matches[1] }
$fid = 'unknown'
if ($all -match '\[R9_SCOPE_FIRST_ZERO\][^\r\n]*frame_id=(\d+)') { $fid = $Matches[1] }
$oldR9 = 'unknown'
if ($all -match '\[R9_SCOPE_FIRST_ZERO\][^\r\n]*old=(0x[0-9A-Fa-f]+)') { $oldR9 = $Matches[1] }
$newR9 = 'unknown'
if ($all -match '\[R9_SCOPE_FIRST_ZERO\][^\r\n]*new=(0x[0-9A-Fa-f]+)') { $newR9 = $Matches[1] }

$illegalFix = $false
if ($nextFix -match 'BOOTSTRAP_TEMP_R9|EARLY_ER_RW|DEFER_ENTRY|GUEST_CALLBACK_FRAME_RESTORE') { $illegalFix = $true }
Assert-Log 'next_fix_gated' (-not $illegalFix) "illegal next_allowed_fix=$nextFix"
if ($gate -eq 'open') {
  Assert-Log 'open_fix_tokenized' ($nextFix -eq 'TOKENIZED_R9_SCOPE_BALANCING_ONLY') "next=$nextFix"
}

$reportBody = @()
$reportBody += 'Phase 6C-D2 Nested CFN R9 Scope Ownership Audit'
$reportBody += "jjfb_sha256=$hash"
$reportBody += "nested_r9_scope_gate=$gate"
$reportBody += 'module_r9_switch_gate=open'
$reportBody += 'guest_callback_frame_gate=blocked'
$reportBody += 'bootstrap_entry_r9_gate=blocked'
$reportBody += 'phase6b_b_gate=blocked'
$reportBody += 'er_rw_metadata_timing_gate=blocked'
$reportBody += "abi_class=$class"
$reportBody += "next_allowed_fix=$nextFix"
$reportBody += ("mode=" + $(if ($d3Fixed) { 'd3_tokenized_balance_accepted' } else { 'observe_only' }))
$reportBody += "Q1_first_r9_writer=$writer callsite=$callsite"
$reportBody += "Q2_event_seq=$ev"
$reportBody += "Q3_frame_id=$fid"
$reportBody += ("Q4_enter_pushed=" + $(if ($all -match 'result=ALREADY_SWITCHED[^\r\n]*frame_pushed=no') { 'no' } else { 'see_log' }))
$reportBody += ("Q5_leave_same_frame=" + $(if ($d3Fixed) { 'noop_balanced' } elseif ($all -match 'leave_action=POP_FOREIGN_FRAME') { 'no_foreign' } else { 'unknown' }))
$reportBody += ("Q6_top_frame_owner=" + $(if ($all -match 'top_frame_kind=MR_HELPER') { 'MR_HELPER' } else { 'see_log' }))
$reportBody += ("Q7_emu_exit_reason=" + $(if ($all -match 'emu_exit_reason=YIELD_TO_NESTED_GUEST') { 'YIELD_TO_NESTED_GUEST' } else { 'see_log' }))
$reportBody += ("Q8_guest_prologue_r9_write=" + $(if ($all -match 'guest_prologue_wrote_r9=yes') { 'yes' } else { 'no' }))
$reportBody += ("Q9_scope_level_interference=" + $(if ($d3Fixed) { 'no_balanced' } elseif ($all -match 'scope_level|FOREIGN_FRAME_POP') { 'yes' } else { 'no' }))
$reportBody += "Q10_nested_r9_scope_gate=$gate next=$nextFix"
$reportBody += ("Q11_observe_only=" + $(if ($d3Fixed) { 'superseded_by_d3' } else { 'yes' }))
$reportBody += "Q12_old_new_r9=$oldR9->$newR9"
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class gate=$gate writer=$writer callsite=$callsite next=$nextFix"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6c-d2 failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_NESTED_R9_SCOPE complete'
