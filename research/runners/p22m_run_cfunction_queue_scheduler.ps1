# P22M-CLEAN: close post-+0x174C8 queue/scheduler gap. NATURAL_ONLY. No Guest mutation.
param(
  [int]$Seconds = 300,
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

$runId = ('p22m_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))
$outDir = Join-Path $Root "out\p22m\$runId"
$reportDir = Join-Path $Root "reports\p22m\$runId"
$logDir = Join-Path $Root "logs\p22m\$runId"
New-Item -ItemType Directory -Force -Path $outDir, $reportDir, $logDir | Out-Null
Get-ChildItem $outDir, $reportDir, $logDir -File -EA SilentlyContinue | Remove-Item -Force -EA SilentlyContinue

$identity = Join-Path $outDir 'p22m_build_identity.txt'
$verdict = Join-Path $reportDir 'p22m_scheduler_verdict.md'
$sourceCommit = (git rev-parse HEAD).Trim()
$mainSha = Get-Sha $exe
$rawExtSha = Get-Sha $staticExt

@"
run_id=$runId
source_commit=$sourceCommit
main_exe_sha256=$mainSha
raw_gamelist_ext_sha256=$rawExtSha
gate=P22M_CFUNCTION_QUEUE_SCHEDULER
NATURAL_ONLY=yes
Lane=A
JJFB_P22M_CLEAN=1
JJFB_P22I_CLEAN=1
JJFB_P22J_CLEAN=1
no_p22l_early_stop=yes
no_cfg_forge=yes
no_host_call_helper=yes
no_fast_init_680=yes
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8

Clear-CaseEnv
Stop-Vmrp
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null

$overlay = Join-Path $RunDir ("overlay_$runId")
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
$vmLog = Join-Path $logDir 'p22m_vmrp.txt'
$stderr = Join-Path $logDir 'p22m_stderr.txt'
@($vmLog, $stderr) | ForEach-Object {
  if (Test-Path $_) { Clear-Content $_ -EA SilentlyContinue }
  else { New-Item -ItemType File -Path $_ -Force | Out-Null }
}

$cfBin = Join-Path $outDir 'cfunction_runtime.bin'
$cfSha = Join-Path $outDir 'cfunction_runtime.sha256'

$wl = [ordered]@{
  JJFB_E10A_RUN_ID = $runId
  JJFB_E10A31_RUN_ID = $runId
  JJFB_P22I_RUN_ID = $runId
  JJFB_P22M_RUN_ID = $runId
  JJFB_P22I_CLEAN = '1'
  JJFB_P22J_CLEAN = '1'
  JJFB_P22M_CLEAN = '1'
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
  JJFB_E10A31_WAIT_FIRE_N = '20'
  JJFB_E10A31_TIMER_CSV = (Join-Path $reportDir 'p22m_e10a31_timer.csv')
  JJFB_E10A31B_MODE = '1'
  JJFB_E10A31B_PUB_CSV = (Join-Path $reportDir 'p22m_e10a31b_pub.csv')
  JJFB_E10A31_CFG_GATE = '1'
  JJFB_E10A31_PARAM_TRACE = '1'
  JJFB_E10A31_PARAM_CSV = (Join-Path $reportDir 'p22m_e10a31_param.csv')
  JJFB_P19_HANDOFF = '1'
  JJFB_P19_OUT_DIR = $outDir
  GWY_P19_PARENT_CHILD_HANDOFF = '1'
  JJFB_P20_CLEAN = '1'
  JJFB_P22I_STACK_CSV = (Join-Path $reportDir 'p22m_helper_call_stack.csv')
  JJFB_P22I_RETURNS_CSV = (Join-Path $reportDir 'p22m_p22i_helper_returns.csv')
  JJFB_P22I_PROV_CSV = (Join-Path $reportDir 'p22m_method_value_provenance.csv')
  JJFB_P22I_R9_CSV = (Join-Path $reportDir 'p22m_r9_owner_timeline.csv')
  JJFB_P22I_POST_CSV = (Join-Path $reportDir 'p22m_p22i_post_init_timeline.csv')
  JJFB_P22I_XREF_CSV = (Join-Path $reportDir 'p22m_p22i_cfunction_caller_xrefs.csv')
  JJFB_P22I_MATRIX_CSV = (Join-Path $reportDir 'p22m_method_dispatch_matrix.csv')
  JJFB_P22I_CALLSITE = (Join-Path $reportDir 'p22m_cfunction_helper_callsite.txt')
  JJFB_P22I_BRANCH_MD = (Join-Path $reportDir 'p22m_p22i_return_branch_chain.md')
  JJFB_P22I_VERDICT = (Join-Path $reportDir 'p22m_p22i_dispatcher_verdict.md')
  JJFB_P22I_SUMMARY = (Join-Path $outDir 'p22m_p22i_runtime_summary.txt')
  JJFB_P22I_IDENTITY = $identity
  JJFB_P22M_CF_BIN = $cfBin
  JJFB_P22M_CF_SHA = $cfSha
  JJFB_P22M_OBJ_CSV = (Join-Path $reportDir 'p22m_object_before_after.csv')
  JJFB_P22M_NODE_CSV = (Join-Path $reportDir 'p22m_node_memory_snapshot.csv')
  JJFB_P22M_WRITES_CSV = (Join-Path $reportDir 'p22m_object_field_writes.csv')
  JJFB_P22M_SLICE_CSV = (Join-Path $reportDir 'p22m_post_remove_slice.csv')
  JJFB_P22M_PARENT_CSV = (Join-Path $reportDir 'p22m_parent_state_timeline.csv')
  JJFB_P22M_CHAIN_MD = (Join-Path $reportDir 'p22m_parent_return_chain.md')
  JJFB_P22M_PROV_CSV = (Join-Path $reportDir 'p22m_node_provenance.csv')
  JJFB_P22M_LIFE_CSV = (Join-Path $reportDir 'p22m_queue_lifecycle.csv')
  JJFB_P22M_XREF_CSV = (Join-Path $reportDir 'p22m_function_xrefs.csv')
  JJFB_P22M_VERDICT = $verdict
  JJFB_P22M_SUMMARY = (Join-Path $outDir 'p22m_runtime_summary.txt')
}
foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }

@(
  'JJFB_P22_MODE','JJFB_P22_HEADLESS_SELECT','JJFB_FORCE_10140_LIFECYCLE',
  'JJFB_FORCE_10140_ONESHOT','JJFB_PRODUCT_DESCRIPTOR_DIRECT','JJFB_P25_MODE',
  'JJFB_FAST_BD0_INIT_CALL','JJFB_FAST_PROGRESS_TICK_CALL','JJFB_P22_CLEAN',
  'JJFB_P22F_CLEAN','JJFB_P22G_CLEAN','JJFB_P22H_CLEAN','JJFB_P22K_CLEAN',
  'JJFB_P22L_CLEAN',
  'JJFB_FAST_REAL_GAMELIST_INIT_SEQUENCE','JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE'
) | ForEach-Object { Remove-Item -Path ("Env:{0}" -f $_) -EA SilentlyContinue }

Write-Host "=== P22M cfunction queue scheduler run_id=$runId seconds=$Seconds ==="
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

# Post-process: disasm exported cfunction image
$disasm = Join-Path $reportDir 'cfunction_runtime_disasm.txt'
$xrefCsv = Join-Path $reportDir 'p22m_function_xrefs.csv'
if (Test-Path $cfBin) {
  $baseMatch = [regex]::Match((Get-Content $cfSha -Raw -EA SilentlyContinue), 'base=(0x[0-9A-Fa-f]+)')
  $cfBase = if ($baseMatch.Success) { $baseMatch.Groups[1].Value } else { '0x80000' }
  & python (Join-Path $Root 'tools\p22m_cfunction_runtime_disasm.py') `
    --bin $cfBin --base $cfBase --out $disasm --xref-out $xrefCsv `
    --start 0x17000 --end 0x1D300
  if ($LASTEXITCODE -ne 0) { Write-Warning 'disasm script failed' }
} else {
  Write-Warning "cfunction_runtime.bin missing at $cfBin"
}

$summaryPath = Join-Path $outDir 'p22m_runtime_summary.txt'
$summary = @{}
if (Test-Path $summaryPath) {
  Get-Content $summaryPath | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { $summary[$Matches[1]] = $Matches[2] }
  }
}

Copy-Item $vmLog (Join-Path $outDir 'p22m_vmrp.txt') -Force -EA SilentlyContinue
Copy-Item $stderr (Join-Path $outDir 'p22m_stderr.txt') -Force -EA SilentlyContinue
if (Test-Path $identity) { Copy-Item $identity (Join-Path $reportDir 'p22m_build_identity.txt') -Force -EA SilentlyContinue }

# Append runtime identity into sha file / identity
if (Test-Path $cfSha) {
  $shaLine = (Get-Content $cfSha -TotalCount 1)
  Add-Content $identity "cfunction_runtime_sha256=$shaLine"
}

$r = [ordered]@{
  run_id = $runId
  cf_sha = if ($summary['cf_sha']) { $summary['cf_sha'] } else { '?' }
  node = if ($summary['node']) { $summary['node'] } else { '?' }
  object = if ($summary['object']) { $summary['object'] } else { '?' }
  ret_174c8 = if ($summary['ret_174c8']) { $summary['ret_174c8'] } else { '?' }
  fn_ret = if ($summary['fn_1d098_ret']) { $summary['fn_1d098_ret'] } else { '?' }
  slice_n = if ($summary['slice_n']) { $summary['slice_n'] } else { '?' }
  entered_10740 = if ($summary['entered_10740']) { $summary['entered_10740'] } else { Hit 'enter_\+0x10740' }
  lock = if ($summary['sole_lock']) { $summary['sole_lock'] } else { '?' }
  next = if ($summary['next_fix']) { $summary['next_fix'] } else { '?' }
  p22m_final = FirstLine '\[JJFB_P22M_FINAL\][^\n]+'
  dense = Hit 'JJFB_P22M\] dense_begin'
  ret174 = Hit 'JJFB_P22M\] ret_174c8'
}

if (-not (Test-Path $verdict)) {
  @"
# P22M scheduler verdict (runner fallback)

$($r.lock)

- cf_sha=$($r.cf_sha) node=$($r.node) object=$($r.object)
- ret_174c8=$($r.ret_174c8) fn_1d098_ret=$($r.fn_ret) slice_n=$($r.slice_n)
- 10740=$($r.entered_10740) dense_hits=$($r.dense) ret174_hits=$($r.ret174)

$($r.p22m_final)
"@ | Set-Content $verdict -Encoding utf8
}

Write-Host "=== P22M done sha=$($r.cf_sha) node=$($r.node) ret174c8=$($r.ret_174c8) fn_ret=$($r.fn_ret) slice=$($r.slice_n) 10740=$($r.entered_10740) ==="
Write-Host "lock: $($r.lock)"
Write-Host "next: $($r.next)"
Write-Host "run_id=$runId"
Write-Host "verdict: $verdict"
Write-Host "cf_bin: $cfBin"
Write-Host "disasm: $disasm"
Write-Host "reports: $reportDir"
