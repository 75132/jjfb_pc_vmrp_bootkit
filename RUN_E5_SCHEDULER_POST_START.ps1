# Stage E5: SCHEDULER-first post-start_dsm timer/event delivery audit
param(
  [int]$Seconds = 90,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
$env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
$env:JJFB_LAUNCH_PATH = 'descriptor_direct'
$env:JJFB_PRIMARY_TARGET = ($Target -replace '\\', '/')
$env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
$env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
$env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
$env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
$env:JJFB_GAME_PACKAGE_ER_RW_SOURCE = 'module_map_or_mrpgcmap'
$env:JJFB_GAME_PACKAGE_CONTEXT_PROVIDER = '1'
$env:JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
$env:JJFB_GAME_P_TIMELINE_TRACE = '1'
$env:JJFB_MODULE_REGISTRY_TRACE = '1'
$env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
$env:JJFB_MRC_INIT_TRACE = '1'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_GAME_SELF_PATCH = '0'
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'
# Same as product runner: DSM R9 guard + callback frame (post_cont auto-on).
$env:GWY_MODULE_R9_SWITCH = '1'
$env:GWY_CALLBACK_FRAME = '1'
Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue

# E5 scheduler traces
$env:JJFB_POST_START_SCHEDULER_TRACE = '1'
$env:JJFB_TIMER_ARM_TRACE = '1'
$env:JJFB_TIMER_POLL_TRACE = '1'
$env:JJFB_TIMER_DELIVER_TRACE = '1'
$env:JJFB_LIFECYCLE_EVENT_TRACE = '1'
$env:JJFB_RET0_SCHEDULER_CORRELATE = '1'
# Observe-only first; no automatic lifecycle inject this slice.
Remove-Item Env:JJFB_LIFECYCLE_EVENT_PROBE -ErrorAction SilentlyContinue

@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_RUNAPP_NATIVE_ONLY'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

$isJjfb = ($env:JJFB_PRIMARY_TARGET -match 'jjfb\.mrp$')
$tag = if ($isJjfb) { 'stage_e5_jjfb' } else { 'stage_e5_wxjwq' }

# Reuse product runner core (build/validate/launch) with E5 stop pattern via env.
$env:JJFB_E5_SCHEDULER_MODE = '1'

$eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
  (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
  '-Target', $Target, '-Seconds', "$Seconds")
if ($SkipBuild) { $eArgs += '-SkipBuild' }
& powershell @eArgs
$rc = $LASTEXITCODE

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$srcLog = if ($isJjfb) {
  Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
} else {
  Join-Path $logDir 'stage_e_wxjwq_control_stdout.txt'
}
$dstLog = Join-Path $logDir "${tag}_scheduler_stdout.txt"
if (Test-Path $srcLog) { Copy-Item -Force $srcLog $dstLog }

$all = ''
if (Test-Path $dstLog) { $all = [System.IO.File]::ReadAllText($dstLog) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$arm = Hit '\[JJFB_TIMER_ARM\]'
$armAbsent = Hit 'JJFB_TIMER_ARM_ABSENT'
$armAttempt = Hit 'JJFB_TIMER_ARM_ATTEMPT'
$postLoop = Hit '\[JJFB_POST_START_LOOP\]'
$poll = Hit '\[JJFB_TIMER_POLL\]'
$fire = Hit '\[JJFB_TIMER_FIRE\]|PLATFORM_TIMER.*FIRE'
$delivered = Hit '\[JJFB_TIMER_DELIVERED\]'
$mrc = Hit '\[JJFB_MRC_INIT\]'
$ret0ok = Hit 'JJFB_INIT_SEQ\] delivered.*ret0=0\b'
$draw = Hit '\[JJFB_DRAW\]|\[JJFB_REFRESH\]'
$startRet = Hit 'START_DSM_RETURN'
$notPolled = Hit 'JJFB_SCHEDULER_NOT_POLLED'

$decision = 'UNKNOWN'
if ($postLoop -and $arm -and -not $fire) { $decision = 'SCHEDULER_FIX' }
elseif ($arm -and $fire -and -not $delivered) { $decision = 'TIMER_CALLBACK_CONTEXT' }
elseif ($arm -and $delivered -and -not $draw) { $decision = 'RESOURCE_DRAW_CHAIN' }
elseif (-not $arm -and $armAbsent -and -not $ret0ok) { $decision = 'PLATFORM_RET0_CAUSE' }
elseif ($postLoop -and -not $arm) { $decision = 'LIFECYCLE_EVENT_REQUIRED' }
elseif (-not $postLoop) { $decision = 'SCHEDULER_FIX' }
elseif ($draw) { $decision = 'RESOURCE_DRAW_CHAIN' }

$verdict = Join-Path $reportDir 'stage_e5_verdict.md'
$armReport = Join-Path $reportDir 'stage_e5_timer_arm_audit.md'
$loopReport = Join-Path $reportDir 'stage_e5_host_loop_scheduler.md'
$delivReport = Join-Path $reportDir 'stage_e5_timer_delivery_chain.md'
$ret0Report = Join-Path $reportDir 'stage_e5_ret0_vs_scheduler_correlation.md'
$decisionReport = Join-Path $reportDir 'stage_e5_next_decision.md'
$ctrlReport = Join-Path $reportDir 'stage_e5_wxjwq_scheduler_control.md'

$samples = @()
if (Test-Path $dstLog) {
  $samples = Select-String -Path $dstLog -Pattern 'JJFB_TIMER_|JJFB_POST_START|JJFB_MRC_INIT|JJFB_INIT_SEQ|START_DSM_RETURN|JJFB_DRAW|JJFB_REFRESH|PLATFORM_TIMER' -ErrorAction SilentlyContinue |
    ForEach-Object { $_.Line } | Select-Object -First 80
}

@"
# Stage E5 — timer arm audit ($($env:JJFB_PRIMARY_TARGET))

| Gate | Result |
|------|--------|
| TIMER_ARM_ATTEMPT | $(if ($armAttempt) { 'yes' } else { 'no' }) |
| TIMER_ARM | $(if ($arm) { 'yes' } else { 'no' }) |
| TIMER_ARM_ABSENT | $(if ($armAbsent) { 'yes' } else { 'no' }) |
| START_DSM_RETURN | $(if ($startRet) { 'yes' } else { 'no' }) |

## Samples
``````
$($samples -join "`n")
``````
"@ | Set-Content -Path $(if ($isJjfb) { $armReport } else { $ctrlReport }) -Encoding utf8

if ($isJjfb) {
@"
# Stage E5 — host loop scheduler

| Gate | Result |
|------|--------|
| POST_START_LOOP | $(if ($postLoop) { 'yes' } else { 'no' }) |
| TIMER_POLL | $(if ($poll) { 'yes' } else { 'no' }) |
| SCHEDULER_NOT_POLLED | $(if ($notPolled) { 'yes' } else { 'no' }) |

Log: ``$dstLog``
"@ | Set-Content -Path $loopReport -Encoding utf8

@"
# Stage E5 — timer delivery chain

| Gate | Result |
|------|--------|
| TIMER_FIRE | $(if ($fire) { 'yes' } else { 'no' }) |
| TIMER_DELIVERED | $(if ($delivered) { 'yes' } else { 'no' }) |
| DRAW/REFRESH | $(if ($draw) { 'yes' } else { 'no' }) |
"@ | Set-Content -Path $delivReport -Encoding utf8

@"
# Stage E5 — ret0 vs scheduler correlation

| Item | Result |
|------|--------|
| JJFB_MRC_INIT | $(if ($mrc) { 'yes' } else { 'no' }) |
| ret0=0 | $(if ($ret0ok) { 'yes' } else { 'no' }) |
| TIMER_ARM before/after init | $(if ($arm) { 'yes' } else { 'no (ABSENT)' }) |
| POST_START_LOOP after return | $(if ($postLoop) { 'yes' } else { 'no' }) |

Interpretation: mr_doExt ignores mrc_init ret (DOCUMENTED). Scheduler arm independent of ret0 success.
"@ | Set-Content -Path $ret0Report -Encoding utf8

@"
# Stage E5 — next decision

**branch:** ``$decision``

| Signal | Value |
|--------|-------|
| arm | $arm |
| arm_absent | $armAbsent |
| post_loop | $postLoop |
| fire | $fire |
| delivered | $delivered |
| draw | $draw |
| ret0ok | $ret0ok |
"@ | Set-Content -Path $decisionReport -Encoding utf8

@"
# Stage E5 — verdict

- **target:** ``$($env:JJFB_PRIMARY_TARGET)``
- **seconds:** $Seconds
- **runner_exit:** $rc
- **decision:** ``$decision``

| Gate | Result |
|------|--------|
| E5-min POST_START_LOOP | $(if ($postLoop) { 'yes' } else { 'no' }) |
| E5-mid TIMER_ARM/FIRE/DELIVER | arm=$arm fire=$fire delivered=$delivered |
| E5-high DRAW/REFRESH | $(if ($draw) { 'yes' } else { 'no' }) |
| MRC_INIT | $(if ($mrc) { 'yes' } else { 'no' }) |
| ret0=0 | $(if ($ret0ok) { 'yes' } else { 'no' }) |

Log: ``$dstLog``
"@ | Set-Content -Path $verdict -Encoding utf8
}

Write-Host "e5_log=$dstLog arm=$arm absent=$armAbsent postLoop=$postLoop fire=$fire draw=$draw decision=$decision"
if (-not $postLoop) { Write-Host '[PARTIAL] E5-min: no POST_START_LOOP'; exit 2 }
if (-not $arm -and -not $armAbsent) { Write-Host '[PARTIAL] E5: neither ARM nor ABSENT'; exit 3 }
Write-Host "[OK] E5 audit complete decision=$decision"
exit 0
