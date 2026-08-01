# P22O-CLEAN: early command-buffer producer provenance. NATURAL_ONLY.
# Arms MEM_WRITE at cfunction map; captures first writers of 0x2AF8F8 / 0x2AF904.
# No [object+0x30]|=0x0C, no Host helper, no FAST, no +0x10740 call. P22N off.
param(
  [int]$Seconds = 360,
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

$runId = ('p22o_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))
$outDir = Join-Path $Root "out\p22o\$runId"
$reportDir = Join-Path $Root "reports\p22o\$runId"
$logDir = Join-Path $Root "logs\p22o\$runId"
New-Item -ItemType Directory -Force -Path $outDir, $reportDir, $logDir | Out-Null

$identity = Join-Path $outDir 'p22o_build_identity.txt'
$verdict = Join-Path $reportDir 'p22o_early_producer_verdict.md'
$summaryPath = Join-Path $outDir 'p22o_runtime_summary.txt'
$sourceCommit = (git rev-parse HEAD).Trim()
$mainSha = Get-Sha $exe
$rawExtSha = Get-Sha $staticExt
$cfBin = Join-Path $outDir 'cfunction_runtime.bin'
$cfSha = Join-Path $outDir 'cfunction_runtime.sha256'

@"
run_id=$runId
source_commit=$sourceCommit
main_exe_sha256=$mainSha
raw_gamelist_ext_sha256=$rawExtSha
gate=P22O_CMD_BUFFER_EARLY_PRODUCER
NATURAL_ONLY=yes
JJFB_P22O_CLEAN=1
JJFB_P22I_CLEAN=1
JJFB_P22J_CLEAN=1
JJFB_P22N_CLEAN=0
no_object_plus30_0x0C_force=yes
no_host_call_helper=yes
no_fast_init=yes
cfunction_runtime_sha256=PENDING_RUNTIME_DUMP
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8

Clear-CaseEnv
Stop-Vmrp
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null

$overlay = Join-Path $RunDir ("overlay_$runId")
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
$vmLog = Join-Path $logDir 'p22o_vmrp.txt'
$stderr = Join-Path $logDir 'p22o_stderr.txt'
@($vmLog, $stderr) | ForEach-Object {
  if (Test-Path $_) { Clear-Content $_ -EA SilentlyContinue }
  else { New-Item -ItemType File -Path $_ -Force | Out-Null }
}

$wl = [ordered]@{
  JJFB_E10A_RUN_ID = $runId
  JJFB_E10A31_RUN_ID = $runId
  JJFB_P22I_RUN_ID = $runId
  JJFB_P22O_RUN_ID = $runId
  JJFB_P22I_CLEAN = '1'
  JJFB_P22J_CLEAN = '1'
  JJFB_P22O_CLEAN = '1'
  JJFB_P22I_SOURCE_COMMIT = $sourceCommit
  JJFB_P22I_MAIN_SHA = $mainSha
  JJFB_P22I_RAW_EXT_SHA = $rawExtSha
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
  JJFB_E10A31_WAIT_FIRE_N = '30'
  JJFB_E10A31_TIMER_CSV = (Join-Path $reportDir 'p22o_e10a31_timer.csv')
  JJFB_E10A31B_MODE = '1'
  JJFB_E10A31B_PUB_CSV = (Join-Path $reportDir 'p22o_e10a31b_pub.csv')
  JJFB_E10A31_CFG_GATE = '1'
  JJFB_E10A31_PARAM_TRACE = '1'
  JJFB_E10A31_PARAM_CSV = (Join-Path $reportDir 'p22o_e10a31_param.csv')
  JJFB_P19_HANDOFF = '1'
  JJFB_P19_OUT_DIR = $outDir
  GWY_P19_PARENT_CHILD_HANDOFF = '1'
  JJFB_P20_CLEAN = '1'
  JJFB_P22I_STACK_CSV = (Join-Path $reportDir 'p22o_helper_call_stack.csv')
  JJFB_P22I_RETURNS_CSV = (Join-Path $reportDir 'p22o_p22i_helper_returns.csv')
  JJFB_P22I_PROV_CSV = (Join-Path $reportDir 'p22o_method_value_provenance.csv')
  JJFB_P22I_R9_CSV = (Join-Path $reportDir 'p22o_r9_owner_timeline.csv')
  JJFB_P22I_POST_CSV = (Join-Path $reportDir 'p22o_p22i_post_init_timeline.csv')
  JJFB_P22I_XREF_CSV = (Join-Path $reportDir 'p22o_p22i_xrefs.csv')
  JJFB_P22I_MATRIX_CSV = (Join-Path $reportDir 'p22o_method_dispatch_matrix.csv')
  JJFB_P22I_CALLSITE = (Join-Path $reportDir 'p22o_cfunction_helper_callsite.txt')
  JJFB_P22I_BRANCH_MD = (Join-Path $reportDir 'p22o_p22i_return_branch_chain.md')
  JJFB_P22I_VERDICT = (Join-Path $reportDir 'p22o_p22i_dispatcher_verdict.md')
  JJFB_P22I_SUMMARY = (Join-Path $outDir 'p22o_p22i_runtime_summary.txt')
  JJFB_P22I_IDENTITY = $identity
  JJFB_P22O_IDENTITY = $identity
  JJFB_P22O_CF_BIN = $cfBin
  JJFB_P22O_CF_SHA = $cfSha
  JJFB_P22O_WRITES_CSV = (Join-Path $reportDir 'p22o_buffer_writes.csv')
  JJFB_P22O_META_CSV = (Join-Path $reportDir 'p22o_buffer_meta_timeline.csv')
  JJFB_P22O_PROV_CSV = (Join-Path $reportDir 'p22o_record_provenance.csv')
  JJFB_P22O_SLICE_CSV = (Join-Path $reportDir 'p22o_producer_slice.csv')
  JJFB_P22O_SKIP_CSV = (Join-Path $reportDir 'p22o_skip_predicate.csv')
  JJFB_P22O_DIV_MD = (Join-Path $reportDir 'p22o_first_divergence.md')
  JJFB_P22O_VERDICT = $verdict
  JJFB_P22O_SUMMARY = $summaryPath
}
foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }

@(
  'JJFB_P22_MODE','JJFB_P22_HEADLESS_SELECT','JJFB_FORCE_10140_LIFECYCLE',
  'JJFB_FORCE_10140_ONESHOT','JJFB_PRODUCT_DESCRIPTOR_DIRECT','JJFB_P25_MODE',
  'JJFB_FAST_BD0_INIT_CALL','JJFB_FAST_PROGRESS_TICK_CALL','JJFB_P22_CLEAN',
  'JJFB_P22F_CLEAN','JJFB_P22G_CLEAN','JJFB_P22H_CLEAN','JJFB_P22K_CLEAN',
  'JJFB_P22L_CLEAN','JJFB_P22M_CLEAN','JJFB_P22N_CLEAN',
  'JJFB_FAST_REAL_GAMELIST_INIT_SEQUENCE','JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE'
) | ForEach-Object { Remove-Item -Path ("Env:{0}" -f $_) -EA SilentlyContinue }

Write-Host "=== P22O early cmd-buffer producer run_id=$runId seconds=$Seconds ==="
$p = Start-Process -FilePath 'cmd.exe' -ArgumentList @(
  '/c',
  ('cd /d "{0}" && "{1}" > "{2}" 2> "{3}"' -f $RunDir, $exe, $vmLog, $stderr)
) -PassThru
$deadline = (Get-Date).AddSeconds($Seconds)
$obsDone = $false
$holdUntil = $null
do {
  Start-Sleep -Seconds 3
  if (-not $obsDone -and (Test-Path $summaryPath)) {
    $stopLine = Select-String -Path $summaryPath -Pattern '^stop_reason=' -ErrorAction SilentlyContinue
    if ($stopLine) {
      $obsDone = $true
      $holdUntil = (Get-Date).AddSeconds($HoldSec)
      Write-Host "[P22O] summary stop_reason observed; holding ${HoldSec}s then stop"
    }
  }
  if ($obsDone -and $holdUntil -and (Get-Date) -ge $holdUntil) { break }
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

if (-not $p.HasExited) {
  Stop-Vmrp
  Stop-Process -Id $p.Id -Force -EA SilentlyContinue
  Start-Sleep -Milliseconds 500
}

$summary = @{}
if (Test-Path $summaryPath) {
  Get-Content $summaryPath | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { $summary[$Matches[1]] = $Matches[2] }
  }
}

Copy-Item $vmLog (Join-Path $outDir 'p22o_vmrp.txt') -Force -EA SilentlyContinue
Copy-Item $stderr (Join-Path $outDir 'p22o_stderr.txt') -Force -EA SilentlyContinue
if (Test-Path $identity) { Copy-Item $identity (Join-Path $reportDir 'p22o_build_identity.txt') -Force -EA SilentlyContinue }

$finalSha = if ($summary['cfunction_runtime_sha256']) { $summary['cfunction_runtime_sha256'] }
            elseif (Test-Path $cfSha) { (Get-Content $cfSha -TotalCount 1).Trim() }
            else { 'UNKNOWN' }
if (Test-Path $identity) {
  $lines = Get-Content $identity | Where-Object { $_ -notmatch '^cfunction_runtime_sha256=' }
  $lines += "cfunction_runtime_sha256=$finalSha"
  $lines | Set-Content $identity -Encoding utf8
  Copy-Item $identity (Join-Path $reportDir 'p22o_build_identity.txt') -Force
}

$r = [ordered]@{
  run_id = $runId
  sha = $finalSha
  opcodes = if ($summary['opcodes']) { $summary['opcodes'] } else { '?' }
  stream_n = if ($summary['stream_n']) { $summary['stream_n'] } else { '?' }
  fire2 = if ($summary['fire2_n']) { $summary['fire2_n'] } else { '?' }
  write14 = if ($summary['first_write14']) { $summary['first_write14'] } else { '?' }
  write1b = if ($summary['first_write1b']) { $summary['first_write1b'] } else { '?' }
  class = if ($summary['writer_class']) { $summary['writer_class'] } else { '?' }
  lock = if ($summary['sole_lock']) { $summary['sole_lock'] } else { '?' }
  next = if ($summary['next_fix']) { $summary['next_fix'] } else { '?' }
  stop = if ($summary['stop_reason']) { $summary['stop_reason'] } else { '?' }
}

if (-not (Test-Path $verdict)) {
  @"
# P22O verdict (runner fallback)

$($r.lock)

- sha=$($r.sha) opcodes=$($r.opcodes) stream=$($r.stream_n) fire2=$($r.fire2)
- write14=$($r.write14) write1b=$($r.write1b) class=$($r.class)
- stop=$($r.stop)
"@ | Set-Content $verdict -Encoding utf8
}

Write-Host "=== P22O done sha=$($r.sha) write14=$($r.write14) write1b=$($r.write1b) class=$($r.class) stream=$($r.stream_n) fire2=$($r.fire2) stop=$($r.stop) ==="
Write-Host "lock: $($r.lock)"
Write-Host "next: $($r.next)"
Write-Host "run_id=$runId"
Write-Host "verdict: $verdict"
Write-Host "reports: $reportDir"
