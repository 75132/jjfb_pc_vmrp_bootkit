# P19 — Parent→Child launch handoff evidence (NATURAL_ONLY; no 0x10140 product activator).
# Focused runner: GwyResearch shell chain → startGame/runapp → JJFB start_dsm return → first successor.
param(
  [int]$ShellSeconds = 90,
  [int]$ProductContrastSeconds = 45,
  [switch]$SkipBuild,
  [switch]$SkipProductContrast
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\p19'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$identity = Join-Path $outDir 'p19_build_identity.txt'
$verdict = Join-Path $reportDir 'p19_parent_child_handoff_verdict.md'
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'

New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

function Get-Sha([string]$p) {
  if (-not (Test-Path $p)) { return 'MISSING' }
  return (Get-FileHash -Algorithm SHA256 -Path $p).Hash.ToLowerInvariant()
}
function Clear-CaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } | ForEach-Object {
    Remove-Item -Path ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
  }
}
function Stop-Vmrp {
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  Write-Host '== build GwyResearch for shell handoff =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP GwyResearch failed' }
}

@"
source_commit=$((git rev-parse HEAD).Trim())
main_exe_sha256=$(Get-Sha $exe)
JJFB_Launcher_exe_sha256=$(Get-Sha $JJFB)
gwy_launcher_exe_sha256=$(Get-Sha $Launcher)
gate=P19_parent_child_handoff
product_default=JJFB_FORCE_10140_LIFECYCLE=0
JJFB_FORCE_10140_ONESHOT=0
NATURAL_ONLY=yes
JJFB_P19_HANDOFF=1
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8
Write-Host '=== P19 identity ==='; Get-Content $identity

function Set-ShellHandoffEnv([string]$runId, [string]$overlay) {
  $env:GWY_PROFILE = $Profile
  $env:GWY_OVERLAY_ROOT = $overlay
  $env:GWY_PRODUCT_REPORTS_DIR = $reportDir
  $env:GWY_PRODUCT_RUN_ID = $runId
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_PARAM = $param
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_LAUNCH_PATH = 'gwy_shell_core_continue'
  $env:JJFB_LAUNCH_SOURCE = 'gwy_shell'
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
  $env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update_native_branch'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
  $env:JJFB_EXTCHUNK_PROVIDER = 'shell_core'
  $env:JJFB_ER_RW_BIND_RESTORE = 'shell_core'
  $env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'shell'
  $env:JJFB_PUBLICATION_AUDIT = '1'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MODULE_REGISTRY_TRACE = '1'
  $env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
  $env:JJFB_MRC_INIT_TRACE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_P19_HANDOFF = '1'
  $env:JJFB_P19_OUT_DIR = $outDir
  $env:GWY_P19_PARENT_CHILD_HANDOFF = '1'
  # NATURAL_ONLY — never force 10140
  Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -EA SilentlyContinue
  Remove-Item Env:JJFB_FORCE_10140_ONESHOT -EA SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_DESCRIPTOR_DIRECT -EA SilentlyContinue
}

function Set-ProductContrastEnv([string]$runId, [string]$overlay) {
  $env:GWY_PROFILE = $Profile
  $env:GWY_OVERLAY_ROOT = $overlay
  $env:GWY_PRODUCT_REPORTS_DIR = $reportDir
  $env:GWY_PRODUCT_RUN_ID = $runId
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_RUNAPP_NATIVE_ONLY = '0'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:JJFB_MODULE_REGISTRY_TRACE = '1'
  $env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
  $env:JJFB_MRC_INIT_TRACE = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = $param
  $env:JJFB_P19_HANDOFF = '1'
  $env:JJFB_P19_OUT_DIR = $outDir
  $env:GWY_P19_PARENT_CHILD_HANDOFF = '1'
  Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -EA SilentlyContinue
  Remove-Item Env:JJFB_FORCE_10140_ONESHOT -EA SilentlyContinue
}

function Invoke-P19Run([string]$tag, [int]$seconds, [string]$mode) {
  Clear-CaseEnv
  Stop-Vmrp
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
  $runId = ('{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $tag, (Get-Date), (Get-Random -Maximum 99999))
  $stderr = Join-Path $logDir ("{0}_stderr.txt" -f $tag)
  $vmLog = Join-Path $logDir ("{0}_vmrp.txt" -f $tag)
  @($stderr, $vmLog) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }
  $overlay = Join-Path $RunDir ("overlay_$runId")
  New-Item -ItemType Directory -Force -Path $overlay | Out-Null
  if ($mode -eq 'shell') { Set-ShellHandoffEnv $runId $overlay }
  else { Set-ProductContrastEnv $runId $overlay }

  Write-Host "=== P19 $tag mode=$mode seconds=$seconds ==="
  $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @(
    '/c',
    ('cd /d "{0}" && "{1}" > "{2}" 2> "{3}"' -f $RunDir, $exe, $vmLog, $stderr)
  ) -PassThru
  $deadline = (Get-Date).AddSeconds($seconds)
  do { Start-Sleep -Seconds 3 } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
  if (-not $p.HasExited) {
    Stop-Vmrp
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 500
  }
  # Snapshot CSVs per run (flush overwrites reports/)
  foreach ($name in @('p19_startgame_call_trace.csv','p19_post_child_first_action.csv')) {
    $src = Join-Path $reportDir $name
    if (Test-Path $src) {
      Copy-Item $src (Join-Path $outDir ("{0}_{1}" -f $tag, $name)) -Force
    }
  }
  Copy-Item $vmLog (Join-Path $outDir ("{0}_vmrp.txt" -f $tag)) -Force -EA SilentlyContinue
  return @{ tag = $tag; mode = $mode; vmLog = $vmLog; stderr = $stderr; runId = $runId }
}

function Analyze-P19([string]$vmPath, [string]$tag) {
  $vm = if (Test-Path $vmPath) { Get-Content $vmPath -Raw -EA SilentlyContinue } else { '' }
  if (-not $vm) { $vm = '' }
  $jjfbChild = ($vm -match '\[CHILD_INIT_RETURN\][^\n]*jjfb') -or ($vm -match '\[P19_START_DSM\][^\n]*phase=jjfb_child_enter')
  $parentEnter = ([regex]::Matches($vm, '\[PARENT_LAUNCH_ENTER\]')).Count
  $startGameCall = ([regex]::Matches($vm, '\[JJFB_STARTGAME\]|\[JJFB_SHELL_EXPORT_CALL\][^\n]*startGame|SHELL_PHASE_STARTGAME')).Count
  $runapp = ([regex]::Matches($vm, '\[JJFB_RUNAPP\]|\[JJFB_SHELL_EXPORT_CALL\][^\n]*runapp|SHELL_PHASE_RUNAPP')).Count
  $jjfbDsm = ([regex]::Matches($vm, '\[P19_START_DSM\][^\n]*phase=jjfb_child_enter')).Count
  $shellParentDsm = ([regex]::Matches($vm, '\[P19_START_DSM\][^\n]*phase=shell_parent')).Count
  $childRet = ([regex]::Matches($vm, '\[CHILD_INIT_RETURN\][^\n]*jjfb')).Count
  $falseChild = ([regex]::Matches($vm, '\[CHILD_INIT_RETURN\][^\n]*gbrwcore|\[CHILD_INIT_RETURN\][^\n]*gamelist')).Count
  $parentRet = ([regex]::Matches($vm, '\[P19_SHELL_PARENT_RETURN\]')).Count
  $firstAct = ([regex]::Matches($vm, '\[POST_CHILD_FIRST_ACTION\]')).Count
  $reg10140 = ([regex]::Matches($vm, '\[10140_REGISTER\]')).Count
  $lifeFire = ([regex]::Matches($vm, '\[JJFB_LIFECYCLE\] op=FIRE')).Count
  $skipForced = ([regex]::Matches($vm, 'SKIP_FORCED_ARM')).Count
  $idle = ([regex]::Matches($vm, 'NO_GUEST_SUCCESSOR_HOST_IDLE')).Count
  $parentResume = ([regex]::Matches($vm, 'PARENT_CONTINUATION_RESUME|PARENT_MODULE_RESUME')).Count
  $direct10140 = ([regex]::Matches($vm, 'DIRECT_10140_ENTER')).Count
  $platAct = ([regex]::Matches($vm, 'PLATFORM_FAMILY_OR_ACTIVATE_EVENT|PLATFORM_HANDLER_REGISTER_POST_CHILD')).Count
  $flush = ([regex]::Matches($vm, '\[P19_HANDOFF_FLUSH\]')).Count
  $gbrw = $vm -match 'gbrwcore'
  $gl = $vm -match 'gamelist'
  $policy = if ($vm -match '\[10140_ACTIVATION_POLICY\][^\n]*mode=(\w+)') { $Matches[1] } else { '' }

  $parentLine = ''; $m = [regex]::Match($vm, '\[PARENT_LAUNCH_ENTER\][^\n]+'); if ($m.Success) { $parentLine = $m.Value }
  $childLine = ''; $m = [regex]::Match($vm, '\[CHILD_INIT_RETURN\][^\n]*jjfb[^\n]*'); if ($m.Success) { $childLine = $m.Value }
  $firstLine = ''; $m = [regex]::Match($vm, '\[POST_CHILD_FIRST_ACTION\][^\n]+'); if ($m.Success) { $firstLine = $m.Value }
  $regLine = ''; $m = [regex]::Match($vm, '\[10140_REGISTER\][^\n]+'); if ($m.Success) { $regLine = $m.Value }

  return [pscustomobject]@{
    tag = $tag
    policy = $policy
    gbrw = [int]$gbrw
    gamelist = [int]$gl
    parent_enter = $parentEnter
    startgame = $startGameCall
    runapp = $runapp
    jjfb_dsm = $jjfbDsm
    shell_parent_dsm = $shellParentDsm
    child_ret = $childRet
    false_child = $falseChild
    parent_ret = $parentRet
    first_action = $firstAct
    reg_10140 = $reg10140
    life_fire = $lifeFire
    skip_forced = $skipForced
    idle_gap = $idle
    parent_resume = $parentResume
    direct_10140 = $direct10140
    plat_activate = $platAct
    flush = $flush
    jjfb_child = [int]$jjfbChild
    parent_line = $parentLine
    child_line = $childLine
    first_line = $firstLine
    reg_line = $regLine
  }
}

# --- Primary: shell handoff ---
$shellHit = Invoke-P19Run 'p19_shell' $ShellSeconds 'shell'
$s = Analyze-P19 $shellHit.vmLog 'p19_shell'
$s | Format-List | Out-String | Write-Host

# --- Optional short product contrast (not 3x180) ---
$p = $null
if (-not $SkipProductContrast) {
  Write-Host '== restore product Gwy binary for contrast =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'product Gwy rebuild failed' }
  $prodHit = Invoke-P19Run 'p19_product' $ProductContrastSeconds 'product'
  $p = Analyze-P19 $prodHit.vmLog 'p19_product'
  $p | Format-List | Out-String | Write-Host
}

# Ensure CSV artifacts exist even if process died before flush
if (-not (Test-Path (Join-Path $reportDir 'p19_startgame_call_trace.csv'))) {
  "seq,api,call_kind,parent_module,call_pc,continuation,sp,lr,cpsr,r9,r0,r1,r2,r3`n" |
    Set-Content (Join-Path $reportDir 'p19_startgame_call_trace.csv') -Encoding utf8
}
if (-not (Test-Path (Join-Path $reportDir 'p19_post_child_first_action.csv'))) {
  "seen,kind,actor_module,pc,instruction,target,r0,r1,r2,r3,event_id,handler,continuation,detail,child_ret,handler_10140,parent_cont,host_ticks`n" |
    Set-Content (Join-Path $reportDir 'p19_post_child_first_action.csv') -Encoding utf8
}

# Copy CSVs into out/p19 if present
Copy-Item (Join-Path $reportDir 'p19_startgame_call_trace.csv') (Join-Path $outDir 'p19_startgame_call_trace.csv') -Force -EA SilentlyContinue
Copy-Item (Join-Path $reportDir 'p19_post_child_first_action.csv') (Join-Path $outDir 'p19_post_child_first_action.csv') -Force -EA SilentlyContinue

# Classify — require REAL jjfb child boundary, not gbrwcore param containing "jjfb"
$jjfbReached = ($s.jjfb_dsm -gt 0) -or ($s.child_ret -gt 0) -or ($s.jjfb_child -gt 0)
$parentResumed = $s.parent_resume -gt 0
$firstKind = 'NOT_CAPTURED'
if ($s.first_line -match 'kind=([A-Z0-9_]+)') { $firstKind = $Matches[1] }
elseif ($s.idle_gap -gt 0 -and $jjfbReached) { $firstKind = 'NO_GUEST_SUCCESSOR_HOST_IDLE' }

$prodJjfb = $false
$prodIdle = $false
$prodReg = $false
if ($p) {
  $prodJjfb = ($p.child_ret -gt 0) -or ($p.jjfb_dsm -gt 0)
  $prodIdle = ($p.idle_gap -gt 0) -or ($p.first_line -match 'NO_GUEST_SUCCESSOR_HOST_IDLE')
  $prodReg = ($p.reg_10140 -gt 0)
}

$semantic = 'INCOMPLETE'
$evidenceClass = 'NOT_PROVEN'
if ($s.direct_10140 -gt 0) {
  $semantic = 'A_parent_oneshot_10140'
  $evidenceClass = 'A_CANDIDATE'
} elseif ($s.plat_activate -gt 0 -and $jjfbReached) {
  $semantic = 'B_platform_activate_event'
  $evidenceClass = 'B_CANDIDATE'
} elseif ($jjfbReached -and -not $parentResumed -and ($firstKind -match 'IDLE|NO_GUEST')) {
  $semantic = 'D_parent_not_resumed_child_should_own_lifecycle'
  $evidenceClass = 'D_LEADING'
} elseif ($jjfbReached -and -not $parentResumed) {
  $semantic = 'D_or_missing_activate'
  $evidenceClass = 'D_HYPOTHESIS'
} elseif (-not $jjfbReached -and $prodJjfb -and $prodIdle -and $prodReg -and ($p.life_fire -eq 0)) {
  $semantic = 'SHELL_BLOCKED_PRODUCT_SHOWS_D_GAP'
  $evidenceClass = 'FALLBACK_NEEDED_PLUS_PRODUCT_D_LEADING'
} elseif (-not $jjfbReached) {
  $semantic = 'SHELL_DID_NOT_REACH_JJFB'
  $evidenceClass = 'FALLBACK_NEEDED'
}

$startGameApi = if ($s.startgame -gt 0) { 'lib.startGame (string/body observe)' }
  elseif ($s.runapp -gt 0) { 'lib.runapp' }
  elseif ($s.parent_enter -gt 0) { 'see PARENT_LAUNCH_ENTER' }
  else { 'NOT_CAPTURED' }

$prodBlock = ''
if ($p) {
  $prodBlock = @"
## Product contrast (short)

| Field | Value |
|------|-------|
| CHILD_INIT_RETURN | $($p.child_ret) |
| first_action | $($p.first_action) |
| life_fire | $($p.life_fire) |
| skip_forced | $($p.skip_forced) |
| idle_gap | $($p.idle_gap) |
| first_line | $($p.first_line)
"@
}

@"
# P19 - Parent-Child Handoff Verdict

## Bottom line

**Evidence class: $evidenceClass**
**Semantic lean: $semantic**

NATURAL_ONLY held (force lifecycle/oneshot unset). No product `0x10140` activator was implemented.

## Shell run summary

| Field | Value |
|------|-------|
| gbrwcore seen | $($s.gbrw) |
| gamelist seen | $($s.gamelist) |
| PARENT_LAUNCH_ENTER | $($s.parent_enter) |
| startGame markers | $($s.startgame) |
| runapp markers | $($s.runapp) |
| JJFB start_dsm | $($s.jjfb_dsm) |
| CHILD_INIT_RETURN | $($s.child_ret) |
| POST_CHILD_FIRST_ACTION | $($s.first_action) |
| 10140 register | $($s.reg_10140) |
| lifecycle FIRE | $($s.life_fire) |
| SKIP_FORCED_ARM | $($s.skip_forced) |
| parent resume | $($s.parent_resume) |
| direct 10140 enter | $($s.direct_10140) |
| first kind | $firstKind |

### Key lines

parent_line=$($s.parent_line)
child_line=$($s.child_line)
first_line=$($s.first_line)
reg_line=$($s.reg_line)

$prodBlock

## Acceptance answers

shell_reached_jjfb=$jjfbReached
startgame_or_runapp=$startGameApi
parent_caller_module=$(if ($s.parent_line -match 'module=(\S+)') { $Matches[1] } else { 'NOT_CAPTURED' })
parent_call_instruction=$(if ($s.parent_line -match 'call_kind=(\S+)') { $Matches[1] } else { 'NOT_CAPTURED' })
parent_continuation=$(if ($s.parent_line -match 'cont=(0x[0-9A-Fa-f]+)') { $Matches[1] } else { 'NOT_CAPTURED' })
parent_resumed_after_child=$(if ($parentResumed) { 'YES' } elseif ($jjfbReached) { 'NO_OR_NOT_OBSERVED' } else { 'N/A_NO_CHILD' })

first_10140_activator=$(if ($s.direct_10140 -gt 0) { 'guest_entered_registered_handler' } elseif ($s.life_fire -gt 0) { 'HOST_FORCED_unexpected' } else { 'NONE_OBSERVED' })
first_activation_mode=$(if ($s.direct_10140 -gt 0) { 'DIRECT_ENTER' } elseif ($s.plat_activate -gt 0) { 'PLATFORM_EVENT' } else { 'NONE' })
first_activation_timing=$(if ($jjfbReached) { 'post_CHILD_INIT_RETURN_window' } else { 'N/A' })
first_activation_args=NOT_CAPTURED_unless_direct
called_once_only=$(if ($s.direct_10140 -eq 1) { 'yes_once' } elseif ($s.direct_10140 -gt 1) { 'multiple' } else { 'n/a' })
periodic_call=no_period_observed_life_fire=$($s.life_fire)
platform_event_triggered=$(if ($s.plat_activate -gt 0) { 'candidate' } else { 'not_observed' })

start_dsm_return_semantics=$(if ($semantic -match '^D') { 'child_initialized_NOT_app_finished_LEADING' } elseif ($jjfbReached) { 'return_seen_no_parent_activate' } else { 'JJFB_return_boundary_not_captured' })
product_direct_misread=$(if ($semantic -match '^D') { 'treats_start_dsm_return_as_child_finished_then_host_idle' } else { 'missing_parent_child_handoff_contract_A_not_proven' })
missing_launch_contract=parent_startGame_runapp_to_child_init_to_first_activator_ABCD
next_generic_fix=$(if ($semantic -match '^D') { 'restore_child_app_lifecycle_INIT_ACTIVATE_EVENT_LOOP' } elseif ($evidenceClass -eq 'FALLBACK_NEEDED') { 'fallback1_sibling_shell_handoff_or_fallback2_research_launch_capsule' } else { 'evidence_only_no_activator_impl' })

## Classification precision

- **E (proven earlier in P18):** registered 10140 is not naturally activated on product direct.
- **A/B/C/D this round:** $evidenceClass / $semantic — do **not** implement activator yet.

## Artifacts

- ``reports/p19_parent_child_handoff_verdict.md``
- ``reports/p19_startgame_call_trace.csv``
- ``reports/p19_post_child_first_action.csv``
- ``out/p19/p19_build_identity.txt``
- ``logs/p19_shell_vmrp.txt``
- Runner: ``research/runners/p19_run_parent_child_handoff.ps1``
"@ | Set-Content $verdict -Encoding utf8

# Append classification to identity
Add-Content $identity -Encoding utf8 -Value @"
shell_jjfb_reached=$jjfbReached
semantic=$semantic
evidence_class=$evidenceClass
first_kind=$firstKind
parent_enter=$($s.parent_enter)
child_ret=$($s.child_ret)
life_fire=$($s.life_fire)
"@

Write-Host "=== P19 verdict written: $verdict ==="
Write-Host "evidence_class=$evidenceClass semantic=$semantic"
Get-Content $verdict | Select-Object -First 40
