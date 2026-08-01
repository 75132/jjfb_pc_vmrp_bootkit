# P22G-CLEAN: close gamelist callback publication + init lifecycle.
# Lane A freeze. NATURAL_ONLY. No headless / cfg forge / Host callback invoke / FAST init.
param(
  [int]$Seconds = 240,
  [int]$HoldSec = 8,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$staticExt = Join-Path $Root 'out\tmp_gamelist_disasm\gamelist.ext'

function Get-Sha([string]$p) {
  if (-not (Test-Path $p)) { return 'UNKNOWN_NOT_EXPOSED' }
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
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'GwyResearch build failed' }
}

$runId = ('p22g_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))
$outDir = Join-Path $Root "out\p22g\$runId"
$reportDir = Join-Path $Root "reports\p22g\$runId"
$logDir = Join-Path $Root "logs\p22g\$runId"
New-Item -ItemType Directory -Force -Path $outDir, $reportDir, $logDir | Out-Null
Get-ChildItem $outDir, $reportDir, $logDir -File -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue

$identity = Join-Path $outDir 'p22g_build_identity.txt'
$verdict = Join-Path $reportDir 'p22g_callback_publication_verdict.md'
$sourceCommit = (git rev-parse HEAD).Trim()
$mainSha = Get-Sha $exe
$rawExtSha = Get-Sha $staticExt

@"
run_id=$runId
source_commit=$sourceCommit
main_exe_sha256=$mainSha
raw_gamelist_ext_sha256=$rawExtSha
gate=P22G_CLEAN_callback_publication
NATURAL_ONLY=yes
Lane=A
research_assisted=0
product_valid=1
FAST_BD0_INIT_CALL=0
FAST_PROGRESS_TICK_CALL=0
JJFB_P22_MODE=0
JJFB_P22_HEADLESS_SELECT=0
JJFB_P25_MODE=0
JJFB_FORCE_10140_LIFECYCLE=0
JJFB_FORCE_10140_ONESHOT=0
JJFB_PRODUCT_DESCRIPTOR_DIRECT=0
JJFB_P22G_CLEAN=1
JJFB_P22F_CLEAN=0
JJFB_P22_CLEAN=0
no_cfg_forge=yes
no_host_call_callback=yes
no_host_call_10740=yes
no_fast_init_680=yes
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8

Clear-CaseEnv
Stop-Vmrp
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null

$overlay = Join-Path $RunDir ("overlay_$runId")
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
$vmLog = Join-Path $logDir 'p22g_vmrp.txt'
$stderr = Join-Path $logDir 'p22g_stderr.txt'
@($vmLog, $stderr) | ForEach-Object {
  if (Test-Path $_) { Clear-Content $_ -EA SilentlyContinue }
  else { New-Item -ItemType File -Path $_ -Force | Out-Null }
}

$wl = [ordered]@{
  JJFB_E10A_RUN_ID = $runId
  JJFB_E10A31_RUN_ID = $runId
  JJFB_P22G_RUN_ID = $runId
  JJFB_P22G_CLEAN = '1'
  JJFB_P22G_SOURCE_COMMIT = $sourceCommit
  JJFB_P22G_MAIN_SHA = $mainSha
  JJFB_P22G_RAW_EXT_SHA = $rawExtSha
  JJFB_P21_MODE = '1'
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
  JJFB_E10A31_TIMER_CONTEXT = '1'
  JJFB_E10A31_WAIT_FOR_TIMER = '1'
  JJFB_E10A31_WAIT_FIRE_N = '20'
  JJFB_E10A31_TIMER_CSV = (Join-Path $reportDir 'p22g_e10a31_timer.csv')
  JJFB_E10A31B_MODE = '1'
  JJFB_E10A31B_PUB_CSV = (Join-Path $reportDir 'p22g_e10a31b_pub.csv')
  JJFB_E10A31_CFG_GATE = '1'
  JJFB_E10A31_PARAM_TRACE = '1'
  JJFB_E10A31_PARAM_CSV = (Join-Path $reportDir 'p22g_e10a31_param.csv')
  JJFB_E10A31_CFG_GATE_CSV = (Join-Path $reportDir 'p22g_e10a31_cfg_gate.csv')
  JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
  JJFB_P19_HANDOFF = '1'
  JJFB_P19_OUT_DIR = $outDir
  GWY_P19_PARENT_CHILD_HANDOFF = '1'
  JJFB_P20_CLEAN = '1'
  JJFB_P22G_WRITES_CSV = (Join-Path $reportDir 'p22g_target_pointer_writes.csv')
  JJFB_P22G_COPIES_CSV = (Join-Path $reportDir 'p22g_target_pointer_copies.csv')
  JJFB_P22G_TABLES_CSV = (Join-Path $reportDir 'p22g_guest_function_tables.csv')
  JJFB_P22G_LIFE_CSV = (Join-Path $reportDir 'p22g_pointer_lifetime.csv')
  JJFB_P22G_TIMELINE_CSV = (Join-Path $reportDir 'p22g_module_init_timeline.csv')
  JJFB_P22G_HELPER_CSV = (Join-Path $reportDir 'p22g_gamelist_helper_invocations.csv')
  JJFB_P22G_MISSING_CSV = (Join-Path $reportDir 'p22g_missing_init_transition.csv')
  JJFB_P22G_INDIR_CSV = (Join-Path $reportDir 'p22g_indirect_calls.csv')
  JJFB_P22G_VERDICT = $verdict
  JJFB_P22G_SUMMARY = (Join-Path $outDir 'p22g_runtime_summary.txt')
}
foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }

@(
  'JJFB_P22_MODE','JJFB_P22_HEADLESS_SELECT','JJFB_FORCE_10140_LIFECYCLE',
  'JJFB_FORCE_10140_ONESHOT','JJFB_PRODUCT_DESCRIPTOR_DIRECT','JJFB_P25_MODE',
  'JJFB_FAST_BD0_INIT_CALL','JJFB_FAST_PROGRESS_TICK_CALL','JJFB_P22_CLEAN',
  'JJFB_P22F_CLEAN','JJFB_FAST_REAL_GAMELIST_INIT_SEQUENCE'
) | ForEach-Object { Remove-Item -Path ("Env:{0}" -f $_) -EA SilentlyContinue }

Write-Host "=== P22G-CLEAN Lane A run_id=$runId seconds=$Seconds ==="
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

$summaryPath = Join-Path $outDir 'p22g_runtime_summary.txt'
$summary = @{}
if (Test-Path $summaryPath) {
  Get-Content $summaryPath | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { $summary[$Matches[1]] = $Matches[2] }
  }
}

if (-not (Test-Path $summaryPath)) {
  @"
run_id=$runId
source_commit=$sourceCommit
main_exe_sha256=$mainSha
raw_gamelist_ext_sha256=$rawExtSha
runtime_image_sha256=UNKNOWN_NOT_EXPOSED
class=G
farthest=UNKNOWN
missing_transition=finalize_missing
missing_contract=process killed before p22g_finalize; see logs
stop_reason=runner_timeout
guest_state_written=0
events_injected=0
headless=0
fast_init=0
p22g_final=$(FirstLine '\[JJFB_P22G_FINAL\][^\n]+')
fire_ext_n=$(Hit 'PLATFORM_TIMER\] op=FIRE_EXT')
"@ | Set-Content $summaryPath -Encoding utf8
  Get-Content $summaryPath | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { $summary[$Matches[1]] = $Matches[2] }
  }
}

$r = [ordered]@{
  run_id = $runId
  research_assisted = 0
  product_valid = 1
  gbrwcore = [int]((Hit 'P19_START_DSM[^\n]*gbrwcore|SHELL_PHASE_GBRWCORE|gbrwcore\.mrp[^\n]*start') -gt 0)
  br_exit_continue = [int]((Hit 'SHELL_CORE_CONTINUE|GWY_CONTINUE_APPLY|phase=br_exit_enter') -gt 0)
  gamelist = [int]((Hit 'GAMELIST_STARTED|SHELL_PHASE_GAMELIST_LOAD|JJFB_P22G\] gamelist_started') -gt 0)
  erw_iso = [int]((Hit 'GAMELIST_ERW_HOST_ISOLATED|TIMER_CONTEXT_COHERENT') -gt 0)
  fire_ext_n = Hit 'PLATFORM_TIMER\] op=FIRE_EXT '
  fault30 = Hit '0x30D5D2'
  force10140 = Hit 'lifecycle_10140_forced|forced=yes'
  p22_mode = Hit 'JJFB_P25\] armed|HEADLESS_SELECT|JJFB_P22_MODE=1|FAST_REAL_GAMELIST'
  p22g_final = FirstLine '\[JJFB_P22G_FINAL\][^\n]+'
  class = if ($summary['class']) { $summary['class'] } else { '?' }
  farthest = if ($summary['farthest']) { $summary['farthest'] } else { '?' }
  missing = if ($summary['missing_transition']) { $summary['missing_transition'] } else { '?' }
  contract = if ($summary['missing_contract']) { $summary['missing_contract'] } else { '?' }
  gl_base = if ($summary['gl_base']) { $summary['gl_base'] } else { '0x0' }
  wrote_f670 = if ($summary['wrote_f670']) { $summary['wrote_f670'] } else { '0' }
  wrote_8cdc = if ($summary['wrote_8cdc']) { $summary['wrote_8cdc'] } else { '0' }
  wrote_d978 = if ($summary['wrote_d978']) { $summary['wrote_d978'] } else { '0' }
  helpers = if ($summary['helper_invoke_n']) { $summary['helper_invoke_n'] } else { '0' }
}

Copy-Item $vmLog (Join-Path $outDir 'p22g_vmrp.txt') -Force -EA SilentlyContinue
Copy-Item $stderr (Join-Path $outDir 'p22g_stderr.txt') -Force -EA SilentlyContinue
Copy-Item $identity (Join-Path $reportDir 'p22g_build_identity.txt') -Force -EA SilentlyContinue

# Ensure empty CSV shells exist if native never opened them.
@(
  'p22g_target_pointer_writes.csv',
  'p22g_target_pointer_copies.csv',
  'p22g_guest_function_tables.csv',
  'p22g_pointer_lifetime.csv',
  'p22g_module_init_timeline.csv',
  'p22g_gamelist_helper_invocations.csv',
  'p22g_missing_init_transition.csv',
  'p22g_indirect_calls.csv'
) | ForEach-Object {
  $pCsv = Join-Path $reportDir $_
  if (-not (Test-Path $pCsv)) {
    "# run_id=$runId note=empty_placeholder`n" | Set-Content $pCsv -Encoding utf8
  }
}

if (-not (Test-Path $verdict)) {
  @"
# P22G-CLEAN callback publication verdict

## Bottom line

**Class: $($r.class)** (runner fallback — native verdict missing)

farthest=$($r.farthest)
missing=$($r.missing)
$($r.contract)

## Freeze checks

| Check | Result |
|------|--------|
| gbrwcore | $(if($r.gbrwcore){'PASS'}else{'FAIL'}) |
| br_exit CONTINUE | $(if($r.br_exit_continue){'PASS'}else{'FAIL'}) |
| gamelist | $(if($r.gamelist){'PASS'}else{'FAIL'}) |
| ERW isolated | $(if($r.erw_iso){'PASS'}else{'FAIL'}) |
| FIRE_EXT | $(if($r.fire_ext_n -gt 0){"PASS ($($r.fire_ext_n))"}else{'FAIL'}) |
| forced 10140 / headless / FAST | $(if(($r.force10140 -eq 0) -and ($r.p22_mode -eq 0)){'PASS'}else{'FAIL'}) |

## Log extract

``````
$($r.p22g_final)
gl_base=$($r.gl_base) helpers=$($r.helpers) wrote F670/8CDC/D978=$($r.wrote_f670)/$($r.wrote_8cdc)/$($r.wrote_d978)
``````

See out/p22g/$runId/ and reports/p22g/$runId/.
"@ | Set-Content $verdict -Encoding utf8
}

$freezeOk = ($r.gbrwcore -and $r.br_exit_continue -and $r.gamelist -and $r.erw_iso -and ($r.fire_ext_n -ge 1) -and ($r.fault30 -eq 0) -and ($r.force10140 -eq 0) -and ($r.p22_mode -eq 0))
Write-Host "=== P22G-CLEAN done class=$($r.class) farthest=$($r.farthest) missing=$($r.missing) freeze=$(if($freezeOk){'PASS'}else{'FAIL'}) fire=$($r.fire_ext_n) ==="
Write-Host "run_id=$runId"
Write-Host "verdict: $verdict"
Write-Host "summary: $summaryPath"
Write-Host "reports: $reportDir"
