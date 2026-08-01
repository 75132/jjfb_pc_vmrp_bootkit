# P20-CLEAN: recover historical E10A-3.1 shell chain under P16-P19 clean guards.
# Evidence only. NATURAL_ONLY. No 0x10140 activator. No static capsule invent.
param(
  [int]$Seconds = 120,
  [int]$HoldSec = 8,
  [switch]$SkipBuild,
  [switch]$NoHistoricalFastAssist
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\p20'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$identity = Join-Path $outDir 'p20_build_identity.txt'
$verdict = Join-Path $reportDir 'p20_clean_shell_recovery_verdict.md'
$gateCsv = Join-Path $reportDir 'p20_clean_gate_matrix.csv'
$sgCsv = Join-Path $reportDir 'p20_gamelist_startgame_trace.csv'
$frameCsv = Join-Path $reportDir 'p20_parent_child_live_frame.csv'
$envDiff = Join-Path $reportDir 'p20_clean_env_diff.csv'
$vmLog = Join-Path $logDir 'p20_clean_vmrp.txt'
$stderr = Join-Path $logDir 'p20_clean_stderr.txt'

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

# --- Env diff: P19 shell vs historical E10A31 (maps to user "old P20/P21") ---
$diffRows = @(
  'variable,p19_shell,historical_e10a31,role,forges_events,migrate_to_p20'
  'JJFB_LAUNCH_PATH,gwy_shell_core_continue,gwy_shell_core_continue,shell_chain,no,yes'
  'JJFB_SHELL_CHAIN_MODE,continue_after_gbrwcore_init,continue_after_gbrwcore_init,continue_policy,no,yes'
  'GWY_LAUNCH_TARGET,gwy/gbrwcore.mrp,gwy/gbrwcore.mrp,entry_package,no,yes'
  'JJFB_EXTCHUNK_PROVIDER,shell_core,shell_core,chunk_owner,no,yes'
  'JJFB_ER_RW_BIND_RESTORE,shell_core,shell_core,erw_bind,no,yes'
  'JJFB_E10A31B_MODE,unset,1,P_ERW_isolation,no,yes'
  'JJFB_E10A31_TIMER_CONTEXT,unset,1,gamelist_timer_observe,no,yes'
  'JJFB_E10A31_WAIT_FOR_TIMER,unset,1,observe_stop,no,yes'
  'JJFB_E10A_MODE,unset,1,shell_trace_arm,no,yes'
  'JJFB_E10A_SHELL_TRACE,unset,1,phase_csv,no,yes'
  'JJFB_TIMER_DELIVER_TRACE,unset,1,natural_timer_trace,no,yes'
  'JJFB_TIMER_ARM_TRACE,unset,1,natural_timer_trace,no,yes'
  'JJFB_FAST_BD0_INIT_CALL,unset,1,host_calls_real_guest_0x2FC418,host_assist_not_state_forge,optional_historical'
  'JJFB_FAST_PROGRESS_TICK_CALL,unset,1,host_calls_real_guest_0x3124D8,host_assist_not_state_forge,optional_historical'
  'JJFB_E5_SCHEDULER_MODE,1,unset,product_scheduler,no,no_exclude'
  'JJFB_FORCE_10140_LIFECYCLE,0/unset,0/unset,NATURAL_ONLY,no,must_stay_off'
  'JJFB_P19_HANDOFF,1,unset,parent_child_observe,no,yes'
  'JJFB_P20_CLEAN,unset,1_this_round,gate_capsule_observe,no,yes'
  'note_first_fork,P19_no_br_exit_so_no_CONTINUE,E10A31_br_exit_CONTINUE_to_gamelist,scheduler_exit_path,n/a,restore_br_exit_conditions'
)
$diffRows | Set-Content $envDiff -Encoding utf8

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'GwyResearch build failed' }
}

@"
source_commit=$((git rev-parse HEAD).Trim())
main_exe_sha256=$(Get-Sha $exe)
gate=P20_CLEAN_shell_recovery
NATURAL_ONLY=yes
JJFB_FORCE_10140_LIFECYCLE=0
JJFB_FORCE_10140_ONESHOT=0
historical_map=E10A31_timer_context_plus_E10A31B_isolation
no_static_capsule=yes
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8

function Set-P20CleanEnv([string]$runId, [string]$overlay) {
  $wl = [ordered]@{
    JJFB_E10A_RUN_ID = $runId
    JJFB_E10A31_RUN_ID = $runId
    JJFB_E10A_MODE = '1'
    JJFB_E10A_SHELL_TRACE = '1'
    JJFB_E9Y_MODE = '1'
    JJFB_E9Y_NO_DEBUG_AC8 = '1'
    JJFB_E9Y_NO_WORKBUF_SEED = '1'
    JJFB_PLATFORM_WORKBUF_ALLOC = '1'
    JJFB_GWY_PACK_REGISTRY = '1'
    JJFB_E9W_MODE = '1'
    JJFB_E9W_ARCHIVE_EXACT = '1'
    JJFB_DISPLAY_FIRST = '1'
    JJFB_E9B_MODE = '1'
    JJFB_VISIBLE_WINDOW = '1'
    JJFB_WINDOW_ZOOM = '2'
    JJFB_E9B_HOLD_SEC = "$HoldSec"
    JJFB_REAL_MRP_PATH = (Join-Path $ResourceRoot 'gwy\jjfb.mrp')
    JJFB_TIMER_DELIVER_TRACE = '1'
    JJFB_TIMER_ARM_TRACE = '1'
    JJFB_E10A31_WAIT_MS = "$([Math]::Max(5000, $Seconds * 1000))"
    GWY_RESOURCE_ROOT = $ResourceRoot
    GWY_OVERLAY_ROOT = $overlay
    GWY_PROFILE = $Profile
    GWY_LAUNCH = '1'
    GWY_LAUNCH_PARAM = $param
    GWY_PACKAGE_APPID = '400101'
    GWY_PACKAGE_APPVER = '12'
    GWY_MODULE_R9_SWITCH = '1'
    GWY_CALLBACK_FRAME = '1'
    JJFB_GAME_SELF_PATCH = '0'
    JJFB_LAUNCH_PATH = 'gwy_shell_core_continue'
    JJFB_LAUNCH_SOURCE = 'gwy_shell'
    JJFB_GWY_LAUNCHER_MODE = '1'
    JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
    JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
    JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
    JJFB_GWY_UPDATE_STUB = 'no_update_native_branch'
    JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
    JJFB_EXTCHUNK_PROVIDER = 'shell_core'
    JJFB_ER_RW_BIND_RESTORE = 'shell_core'
    JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'shell'
    JJFB_PUBLICATION_AUDIT = '1'
    JJFB_PACKAGE_SCOPED_CLOAD = '1'
    GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
    # Historical isolation + timer context (old P21)
    JJFB_E10A31_TIMER_CONTEXT = '1'
    JJFB_E10A31_WAIT_FOR_TIMER = '1'
    JJFB_E10A31_WAIT_FIRE_N = '3'
    JJFB_E10A31_TIMER_CSV = (Join-Path $reportDir 'p20_e10a31_timer_binding.csv')
    JJFB_E10A31B_MODE = '1'
    JJFB_E10A31B_PUB_CSV = (Join-Path $reportDir 'p20_e10a31b_publication.csv')
    JJFB_E10A31_CFG_GATE = '1'
    JJFB_E10A31_PARAM_TRACE = '1'
    JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
    # P19 handoff + P20 capsule observe
    JJFB_P19_HANDOFF = '1'
    JJFB_P19_OUT_DIR = $outDir
    GWY_P19_PARENT_CHILD_HANDOFF = '1'
    JJFB_P20_CLEAN = '1'
  }
  if (-not $NoHistoricalFastAssist) {
    $wl['JJFB_FAST_BD0_INIT_CALL'] = '1'
    $wl['JJFB_FAST_PROGRESS_TICK_CALL'] = '1'
    $wl['JJFB_E9U_TICK_N'] = '12'
  }
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  # Hard NATURAL_ONLY
  Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -EA SilentlyContinue
  Remove-Item Env:JJFB_FORCE_10140_ONESHOT -EA SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_DESCRIPTOR_DIRECT -EA SilentlyContinue
  Remove-Item Env:JJFB_E5_SCHEDULER_MODE -EA SilentlyContinue
  return $wl
}

Clear-CaseEnv
Stop-Vmrp
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
$runId = ('p20clean_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))
$overlay = Join-Path $RunDir ("overlay_$runId")
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
@($vmLog, $stderr) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }
$wl = Set-P20CleanEnv $runId $overlay

Write-Host "=== P20-CLEAN run_id=$runId seconds=$Seconds fast_assist=$(-not $NoHistoricalFastAssist) ==="
$p = Start-Process -FilePath 'cmd.exe' -ArgumentList @(
  '/c',
  ('cd /d "{0}" && "{1}" > "{2}" 2> "{3}"' -f $RunDir, $exe, $vmLog, $stderr)
) -PassThru
$deadline = (Get-Date).AddSeconds($Seconds)
do { Start-Sleep -Seconds 3 } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
if (-not $p.HasExited) {
  Stop-Vmrp
  Stop-Process -Id $p.Id -Force -EA SilentlyContinue
  Start-Sleep -Milliseconds 500
}

$vm = if (Test-Path $vmLog) { Get-Content $vmLog -Raw -EA SilentlyContinue } else { '' }
if (-not $vm) { $vm = '' }

function Hit([string]$pat) { return ([regex]::Matches($vm, $pat)).Count }
function FirstLine([string]$pat) {
  $m = [regex]::Match($vm, $pat)
  if ($m.Success) { return $m.Value } else { return '' }
}

$g1 = (Hit 'P19_START_DSM[^\n]*gbrwcore|SHELL_PHASE_GBRWCORE|gbrwcore\.mrp[^\n]*start') -gt 0
$g2 = (Hit '0x10102|plat_10102_register|slot28_10102_register') -gt 0
$g2h = FirstLine 'handler=0x30B7[0-9A-Fa-f]+|r2=0x30B7[0-9A-Fa-f]+'
$g3nat = (Hit 'PLATFORM_TIMER\] op=FIRE_EXT|FIRE_EXT code=') -gt 0
$g3 = $g3nat
$g4 = (Hit 'INIT_SEQ|CFN_P_SLOT|mr_c_function_new|C_FUNCTION_NEW') -gt 0
$g5 = (Hit 'JJFB_SHELL_EXPORT|CFN_P_SLOT|named_service') -gt 0
$g6 = (Hit 'lib\.startGame|JJFB_SHELL_EXPORT[^\n]*startGame') -gt 0
$g6dyn = (Hit 'lib\.startGame[^\n]*kind=(?!string_va_not_entry)') -gt 0 -or (Hit 'startGame[^\n]*dispatcher=(?!unknown)[^\n]+') -gt 0
$g6str = (Hit 'lib\.startGame[^\n]*string_va_not_entry') -gt 0
$gl = (Hit 'GAMELIST_STARTED|SHELL_PHASE_GAMELIST_LOAD|JJFB_SHELL_CORE_CONTINUE[^\n]*gamelist') -gt 0
$cont = (Hit 'SHELL_CORE_CONTINUE|GWY_CONTINUE_APPLY|phase=br_exit_enter') -gt 0
$iso = (Hit 'GAMELIST_ERW_HOST_ISOLATED|GAMELIST_P_COLLISION_NEW_CHUNK|GAMELIST_CHUNK_REUSE_REFUSED|TIMER_CONTEXT_COHERENT') -gt 0
$fault30 = (Hit '0x30D5D2') -gt 0
$cfg36 = (Hit 'CFG_FILE_OPENED|CFG_RECORD_36|CFG36_ITEM|real_cfg_selected|JJFB_GAMELIST_CFG') -gt 0
$g7 = (Hit 'PARENT_LAUNCH_ENTER') -gt 0
$g7bl = (Hit 'PARENT_LAUNCH_ENTER[^\n]*call_kind=(BL|BLX|BX)') -gt 0
$jjfb = (Hit 'phase=jjfb_child_enter|CHILD_INIT_RETURN[^\n]*jjfb') -gt 0
$nestBlock = (Hit 'NESTED_EMU_IN_CODE_HOOK_BLOCKED') -gt 0
$entryDefer = (Hit 'MRPGCMAP_ENTRY[^\n]*DEFERRED|DRAIN_PENDING') -gt 0
$entryOk = (Hit 'MRPGCMAP_ENTRY[^\n]*result=EMU_OK') -gt 0
$force10140 = (Hit 'lifecycle_10140_forced|forced=yes') -gt 0
$lifeFire = Hit '\[JJFB_LIFECYCLE\] op=FIRE'
$parentLine = FirstLine '\[PARENT_LAUNCH_ENTER\][^\n]+'
$preSg = FirstLine '\[PARENT_PRE_STARTGAME\][^\n]+'
$childLine = FirstLine '\[CHILD_INIT_RETURN\][^\n]*jjfb[^\n]*'
$firstAct = FirstLine '\[POST_CHILD_FIRST_ACTION\][^\n]+'
$isoLine = FirstLine 'GAMELIST_ERW_HOST_ISOLATED[^\n]+|TIMER_CONTEXT_COHERENT[^\n]+'
$contLine = FirstLine '\[JJFB_SHELL_CORE_CONTINUE\][^\n]+|\[GWY_CONTINUE_APPLY\][^\n]+'
$brExit = FirstLine 'BR_EXIT|br_exit_enter|GWY_BR_EXIT[^\n]+'

# Gate matrix CSV
@(
  'gate,name,pass,detail'
  ("G1,gbrwcore_command0_or_start,{0},gbrwcore_started" -f [int]$g1)
  ("G2,plat_10102_register,{0},""$($g2h -replace '"','''')""" -f [int]$g2)
  ("G3,natural_callback_or_timer_fire,{0},fire_ext={1}" -f [int]($g3 -or $g3nat), [int]$g3nat)
  ("G4,lazy_init_or_cfn,{0},markers" -f [int]$g4)
  ("G5,api_builder_or_export_table,{0},string_table_ok_dynamic_ptr={1}" -f [int]$g5, [int]$g6dyn)
  ("G6,lib_startGame_publish,{0},string_or_lookup" -f [int]$g6)
  ("G6b,dynamic_startGame_fnptr,{0},string_va_only={1}" -f [int]$g6dyn, [int]$g6str)
  ("C0,br_exit_continue,{0},""$($contLine -replace '"','''')""" -f [int]$cont)
  ("GL,enter_gamelist,{0},isolation={1}" -f [int]$gl, [int]$iso)
  ("ISO,runtime_frame_isolation,{0},""$($isoLine -replace '"','''')""" -f [int]$iso)
  ("F30,fault_0x30D5D2_absent,{0},hits={1}" -f [int](-not $fault30), $fault30)
  ("CFG,cfg36_path,{0},opened_or_selected" -f [int]$cfg36)
  ("G7,guest_startGame_call,{0},bl_family={1}" -f [int]$g7, [int]$g7bl)
  ("JJ,nested_jjfb_start_dsm,{0},child" -f [int]$jjfb)
  ("ENTRY,mrpgcmap_entry_ok,{0},defer={1}" -f [int]$entryOk, [int]$entryDefer)
  ("SAFE,nest_guard_clean,{0},raw_blocks={1}" -f [int](-not ($nestBlock -and -not $entryDefer)), [int]$nestBlock)
  ("SAFE2,no_forced_10140,{0},fires={1}" -f [int](-not $force10140 -and $lifeFire -eq 0), $lifeFire)
) | Set-Content $gateCsv -Encoding utf8

# startGame trace
@(
  'field,value'
  ("parent_launch_line,""$($parentLine -replace '"','''')""")
  ("pre_startgame_stack,""$($preSg -replace '"','''')""")
  ("continue_line,""$($contLine -replace '"','''')""")
  ("br_exit,""$($brExit -replace '"','''')""")
  ("export_startGame_count,$(Hit 'lib\.startGame')")
  ("parent_launch_count,$(Hit 'PARENT_LAUNCH_ENTER')")
  ("body_0x2AAD84,$(Hit '0x2AAD84')")
  ("entry_deferred,$([int]$entryDefer)")
  ("entry_emu_ok,$([int]$entryOk)")
) | Set-Content $sgCsv -Encoding utf8

# Classify fork — prefer CONTINUE/entry fork over G2 detector noise
$fork = 'UNKNOWN'
$verdictClass = 'INCOMPLETE'
$histGates = ($g1 -and $g2 -and $g3nat -and $g5 -and $g6)
if (-not $g1) { $fork = 'A_before_G1'; $verdictClass = 'A_gates_1_6_fail' }
elseif ($nestBlock -and -not $entryOk -and -not $cont) {
  $fork = 'A_mrpgcmap_entry_blocked_in_hook_no_br_exit'
  $verdictClass = 'A_entry_defer_or_block_before_continue'
}
elseif (-not $cont -and -not $gl) {
  $fork = 'A_no_br_exit_CONTINUE_first_fork_vs_history'
  $verdictClass = 'A_gates_1_6_partial_no_continue'
}
elseif ($gl -and -not $iso) { $fork = 'ISO_fail_after_gamelist'; $verdictClass = 'ISO_REGRESSION' }
elseif ($gl -and $iso -and -not $cfg36 -and -not $g7) { $fork = 'B_gamelist_no_cfg36_select'; $verdictClass = 'B_cfg36' }
elseif ($cfg36 -and -not $g7) { $fork = 'C_cfg36_no_startGame_call'; $verdictClass = 'C_no_call' }
elseif ($g7 -and -not $jjfb) { $fork = 'D_startGame_no_jjfb_child'; $verdictClass = 'D_call_hit' }
elseif ($jjfb) { $fork = 'E_jjfb_child_reached'; $verdictClass = 'E_child_lifecycle_window' }
elseif ($histGates) { $fork = 'A_G1_6_markers_but_no_continue'; $verdictClass = 'A_gates_1_6_partial_no_continue' }

$histFull = $histGates -and $cont -and $gl -and $iso

# live frame CSV
@(
  'field,value'
  ("frame_valid,$([int]($g7 -or $jjfb))")
  ("parent_line,""$($parentLine -replace '"','''')""")
  ("child_line,""$($childLine -replace '"','''')""")
  ("first_action,""$($firstAct -replace '"','''')""")
  ("isolation,""$($isoLine -replace '"','''')""")
  ("entry_ok,$([int]$entryOk)")
  ("capsule_written,0")
) | Set-Content $frameCsv -Encoding utf8

$capsule = $false
if ($g7 -and $g7bl) {
  $capPath = Join-Path $outDir 'parent_startgame_capsule.json'
  @"
{
  "research_replay": "yes",
  "product_valid": "no",
  "sha_gated": "yes",
  "captured_from_live_guest_call": "yes",
  "parent_launch": "$($parentLine -replace '"','\"')",
  "stack_window": "$($preSg -replace '"','\"')",
  "main_exe_sha256": "$(Get-Sha $exe)",
  "run_id": "$runId"
}
"@ | Set-Content $capPath -Encoding utf8
  $capsule = $true
}

@"
# P20-CLEAN Shell Recovery Verdict

## Bottom line

**Class: $verdictClass**
**First fork vs historical E10A-3.1: $fork**

NATURAL_ONLY held. Nest blocks=$nestBlock. Forced 10140 fires=$lifeFire. Capsule=$capsule.

Historical map: user "old P20/P21" = repo ``E10A-3 / E10A-3.1b`` (no files named P20_*).

## Gate matrix

| Gate | Pass |
|------|------|
| G1 gbrwcore start/command0 | $g1 |
| G2 0x10102 register | $g2 |
| G3 natural timer/callback | $($g3 -or $g3nat) |
| G4 lazy/cfn | $g4 |
| G5 export/api table | $g5 |
| G6 startGame publish | $g6 |
| G6b dynamic fnptr | $g6dyn |
| br_exit CONTINUE | $cont |
| enter gamelist | $gl |
| P/ERW isolation | $iso |
| 0x30D5D2 absent | $(-not $fault30) |
| cfg36 path | $cfg36 |
| G7 Guest startGame call | $g7 |
| nested jjfb | $jjfb |

## Key lines

continue=$contLine
isolation=$isoLine
parent=$parentLine
child=$childLine
first_action=$firstAct

## Acceptance answers

historical_gates_1_6_reproduced=$histGates
historical_full_to_gamelist_isolated=$histFull
first_fork_vs_P20_history=$fork
gbrwcore_callback_natural=$g3nat
api_builder_natural=$g5
startGame_ptr_dynamic=$g6dyn
entered_gamelist=$gl
P_isolated=$iso
fault_0x30D5D2_gone=$(-not $fault30)
cfg36_natural=$cfg36
guest_startGame_call=$g7
caller_pc=$(if ($parentLine -match 'call_pc=(0x[0-9A-Fa-f]+)') { $Matches[1] } else { 'NOT_CAPTURED' })
branch_instruction=$(if ($parentLine -match 'call_kind=(\S+)') { $Matches[1] } else { 'NOT_CAPTURED' })
dynamic_startGame_pointer=$(if ($g6dyn) { 'YES' } else { 'string_va_only_or_none' })
parent_continuation=$(if ($parentLine -match 'cont=(0x[0-9A-Fa-f]+)') { $Matches[1] } else { 'NOT_CAPTURED' })
nested_jjfb_start_dsm=$jjfb
live_research_capsule=$capsule
post_child_first_action=$(if ($firstAct) { $firstAct } else { 'N/A' })
10140_first_activator_decidable=$(if ($jjfb) { 'window_open_see_first_action' } else { 'NO_still_blocked_before_child' })
current_only_lock=$fork

## Next

$(if ($verdictClass -match '^A') { 'Fix only the CONTINUE/br_exit scheduling fork; do not jump to gamelist or invent capsule.' }
  elseif ($verdictClass -eq 'B_cfg36') { 'Trace cfg list/selection; do not call startGame.' }
  elseif ($verdictClass -eq 'C_no_call') { 'Trace post-select branch / API lookup / missing parent state.' }
  elseif ($verdictClass -match '^D|^E') { 'Use live capsule; study child lifecycle; still no 10140 activator.' }
  else { 'Re-read gate matrix.' })

## Artifacts

- reports/p20_clean_shell_recovery_verdict.md
- reports/p20_clean_gate_matrix.csv
- reports/p20_gamelist_startgame_trace.csv
- reports/p20_parent_child_live_frame.csv
- reports/p20_clean_env_diff.csv
- out/p20/p20_build_identity.txt
- logs/p20_clean_vmrp.txt
- research/runners/p20_run_clean_shell_recovery.ps1
"@ | Set-Content $verdict -Encoding utf8

Copy-Item $gateCsv, $sgCsv, $frameCsv, $envDiff, $verdict -Destination $outDir -Force -EA SilentlyContinue
Copy-Item $vmLog (Join-Path $outDir 'p20_clean_vmrp.txt') -Force -EA SilentlyContinue
Add-Content $identity -Encoding utf8 -Value @"
verdict_class=$verdictClass
first_fork=$fork
entered_gamelist=$gl
isolation=$iso
gate7=$g7
jjfb=$jjfb
capsule=$capsule
"@

Write-Host "=== P20-CLEAN done class=$verdictClass fork=$fork ==="
Get-Content $gateCsv
Write-Host '---'
Get-Content $verdict | Select-Object -First 45
