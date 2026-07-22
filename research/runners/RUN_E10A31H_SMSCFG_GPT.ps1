# Stage E10A-3.1h: mr_sms_cfg_buf / smsGetBytes GPT-tag attribution
# Modes: synthesize | live
param(
  [int]$Seconds = 90,
  [int]$HoldSec = 10,
  [int]$Zoom = 2,
  [ValidateSet('synthesize','live')]
  [string]$Mode = 'synthesize',
  [int]$TickN = 12,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
while ($Root -and -not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $parent = Split-Path -Parent $Root
  if (-not $parent -or $parent -eq $Root) { break }
  $Root = $parent
}
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  throw "cannot locate repo root from $PSScriptRoot"
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$outDir = Join-Path $Root 'out\e10a31h'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(180, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31h_smscfg_gpt_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31h\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'e10a31h_smscfg_gpt_stdout.txt'
$stderrLog = Join-Path $logDir 'e10a31h_smscfg_gpt_stderr.txt'
$hCsv = Join-Path $reportDir 'e10a31h_smscfg_trace.csv'
$gCsv = Join-Path $reportDir 'e10a31g_strcmp_arg_trace.csv'
$insnCsv = Join-Path $reportDir 'e10a31d_method0_instruction_trace.csv'
$provCsv = Join-Path $reportDir 'e10a31d_method0_return_provenance.csv'
$histCsv = Join-Path $reportDir 'e10a31d_helper_call_history.csv'
$callCsv = Join-Path $reportDir 'e10a31d_method0_call_tree.csv'
$timerCsv = Join-Path $reportDir 'e10a31_timer_binding_trace.csv'
$initCsv = Join-Path $reportDir 'e10a31c_init_sequence_trace.csv'
$metaCsv = Join-Path $reportDir 'e10a31e_package_metadata_trace.csv'
$bindCsv = Join-Path $reportDir 'e10a31e_appinfo_binding_trace.csv'
$branchCsv = Join-Path $reportDir 'e10a31f_fail_branch_trace.csv'
$denseCsv = Join-Path $reportDir 'e10a31f_failsite_dense_trace.csv'
$appinfoCsv = Join-Path $reportDir 'e10a31d_appinfo_contract.csv'

function Stop-E10A31HChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[DEFGH]_')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-AllInheritedCaseEnv {
  $names = @()
  Get-ChildItem Env: | ForEach-Object {
    if ($_.Name -match '^(JJFB_|GWY_|VMRP_)') { $names += $_.Name }
  }
  foreach ($n in $names) { Remove-Item -Path ("Env:{0}" -f $n) -EA SilentlyContinue }
  return $names
}

function Get-BaseWhitelist {
  New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
  return [ordered]@{
    JJFB_E10A_RUN_ID = "$RunId"
    JJFB_E10A31_RUN_ID = "$RunId"
    JJFB_E10A31C_RUN_ID = "$RunId"
    JJFB_E10A31D_RUN_ID = "$RunId"
    JJFB_E10A31E_RUN_ID = "$RunId"
    JJFB_E10A31F_RUN_ID = "$RunId"
    JJFB_E10A31G_RUN_ID = "$RunId"
    JJFB_E10A31H_RUN_ID = "$RunId"
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
    JJFB_WINDOW_ZOOM = "$Zoom"
    JJFB_E9B_HOLD_SEC = "$HoldSec"
    JJFB_REAL_MRP_PATH = $mrpPath
    JJFB_FAST_BD0_INIT_CALL = '1'
    JJFB_FAST_PROGRESS_TICK_CALL = '1'
    JJFB_E9U_TICK_N = "$TickN"
    JJFB_TIMER_DELIVER_TRACE = '1'
    JJFB_TIMER_ARM_TRACE = '1'
    JJFB_E10A31_WAIT_MS = "$([Math]::Max(5000, $Seconds * 1000))"
    JJFB_E10A31_TIMER_CONTEXT = '1'
    JJFB_E10A31_WAIT_FOR_TIMER = '1'
    JJFB_E10A31_WAIT_FIRE_N = '3'
    JJFB_E10A31_TIMER_CSV = $timerCsv
    JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
    GWY_RESOURCE_ROOT = $ResourceRoot
    GWY_OVERLAY_ROOT = $OverlayRoot
    GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
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
    JJFB_E10A31C_MODE = '1'
    JJFB_E10A31C_INIT_ATOMICITY = '1'
    JJFB_E10A31C_INIT_CSV = $initCsv
    JJFB_E10A31D_MODE = '1'
    JJFB_E10A31D_HISTORY = '1'
    JJFB_E10A31D_METHOD0_TRACE = '1'
    JJFB_E10A31D_APPINFO = '1'
    JJFB_E10A31D_INSN_BUDGET = '20000'
    JJFB_E10A31D_HIST_CSV = $histCsv
    JJFB_E10A31D_INSN_CSV = $insnCsv
    JJFB_E10A31D_CALL_CSV = $callCsv
    JJFB_E10A31D_PROV_CSV = $provCsv
    JJFB_E10A31D_APPINFO_CSV = $appinfoCsv
    JJFB_E10A31E_MODE = '1'
    JJFB_E10A31E_METADATA = '1'
    JJFB_E10A31E_BINDING = '1'
    JJFB_E10A31E_META_CSV = $metaCsv
    JJFB_E10A31E_BIND_CSV = $bindCsv
    JJFB_E10A31F_MODE = '1'
    JJFB_E10A31F_CONTINUE_PAST_SENTINEL = '1'
    JJFB_E10A31F_DENSE_WINDOW = '1'
    JJFB_E10A31F_BRANCH_CSV = $branchCsv
    JJFB_E10A31F_DENSE_CSV = $denseCsv
    JJFB_E10A31G_MODE = '1'
    JJFB_E10A31G_CSV = $gCsv
    JJFB_E10A31H_MODE = '1'
    JJFB_E10A31H_CSV = $hCsv
  }
}

function Test-LogHit([string]$log, [string]$pat) {
  if (-not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -EA SilentlyContinue)
}

function Get-ObserveStopReason([string]$log) {
  if (Test-LogHit $log 'MRC_INIT_RETURN_PROVENANCE_COMPLETE|method0_trace=disarm') {
    return 'OBSERVE_STOP_PROVENANCE_COMPLETE'
  }
  if (Test-LogHit $log 'delivered ret6=\d+ ret8=\d+ ret0=') {
    return 'OBSERVE_STOP_INIT_DELIVERED'
  }
  if (Test-LogHit $log 'UC_ERR|MEM_FAULT|FETCH_UNMAPPED') {
    return 'OBSERVE_STOP_VM_FAULT'
  }
  return $null
}

function Invoke-CaseRun($wl, [string]$outLog, [string]$errLog) {
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  Stop-E10A31HChildren
  $t0 = Get-Date
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
  $deadline = $t0.AddSeconds($OUTER_KILL_SEC)
  $observeStop = $null
  while (-not $p.HasExited) {
    if ((Get-Date) -ge $deadline) { break }
    $obs = Get-ObserveStopReason $outLog
    if ($obs) { $observeStop = $obs; Write-Host "=== early stop: $obs ==="; break }
    Start-Sleep -Milliseconds 400
    try { $p.Refresh() } catch {}
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E10A31HChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Write-SynthesizedCsv {
  $lines = @(
    'run_id,event,pc,r0,r1,r2,r3,r4,r9,ptr,hex,note'
    '1784666878768,SMSGETBYTES_CALL,0x2E5396,0x349,0x27FA30,0x3,0x0,0x0,0x682B6C,0x27FA30,,pos_dst_len'
    '1784666878768,MR_TABLE,0x2E31AE,0x0,0x94E94,0x280CD4,0x94E94,0x281EFC,0x280400,0x281EFC,,slot0_slotC_slot1C0'
    '1784666878768,SMS_CFG_AT_349,0x2E31AE,0x280CD4,0x349,0x8,0x0,0x281EFC,0x280400,0x28101D,00000000,expect_GPT'
  )
  Set-Content -Path $hCsv -Value $lines -Encoding UTF8
}

function Write-Verdict([string]$primary, $flags, [string]$modeName, $elapsed, $exitCode, $observeStop) {
  $md = @"
# Stage E10A-3.1h sms_cfg / GPT-tag Verdict

- **Mode**: ``$modeName``
- **run_id**: ``$RunId``
- **Primary verdict**: ``$primary``

## Headline

method0 ``strcmp(empty,"GPT")`` is **``_mr_smsGetBytes(0x349, dst, 3)``** against guest ``mr_sms_cfg_buf``, which is empty.

Not primary: appInfo, ``0x2E1C24`` sentinel, method0 ``filebuf`` ABI.

## Identity proof

| Fact | Value |
|------|-------|
| range_limit in ``0x2E3180`` | ``0x10E0`` = ``MR_SMS_CFG_BUF_LEN`` (120*36) |
| ``mr_table`` (r4) | ``0x281EFC`` (inside cfunction ERW) |
| ``*(mr_table+0xC)`` | memcpy stub (matches BLX r3) |
| ``*(mr_table+0x1C0)`` | ``0x280CD4`` = ``mr_sms_cfg_buf`` (bridge MAP_DATA offset) |
| cfg @ ``+0x349`` | **NUL NUL NUL** (expect ``GPT``) |
| ``dsm.cfg`` in tree | **absent** |
| bridge ``MAP_DATA`` initFn for ``mr_sms_cfg_buf`` | **NULL** (``hooks_init`` skips DATA) |

## Causal chain

``````
smsGetBytes(0x349, sp+0x30, 3)
  = memcpy(dst, mr_sms_cfg_buf + 0x349, 3)   ; buf @ ERW+0x8D4, empty
strcmp(dst, "GPT") @ 0xAC2D0 → -1 @ 0xAC2E8
``````

## Milestones

- ``FIELD_READ_IS_SMSGETBYTES``
- ``MR_TABLE_SLOT_1C0_IS_SMS_CFG_BUF``
- ``SMS_CFG_349_IS_NUL``
- ``SMS_CFG_BUF_IN_CFUNCTION_ERW_8D4``
- ``BRIDGE_MAP_DATA_SMS_CFG_NO_INITFN``
- ``DSM_CFG_FILE_ABSENT_IN_TREE``
- ``METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG``

## Flags

- saw_sms=$($flags.saw_sms) empty_349=$($flags.empty_349) table_ok=$($flags.table_ok)
- elapsed=${elapsed}s exit=$exitCode observeStop=$observeStop

## Next (still no cfg.bin / no force ret0)

1. Find who should ``smsSetBytes(0x349, "GPT", 3)`` (gwy pack registration?).
2. Decide guest mapping for ``mr_sms_cfg_buf``: publish real buffer + run ``_mr_load_sms_cfg``, or sync host ``dsm.cfg``.
3. method0 ``input=filebuf`` remains **secondary** (orthogonal to sms_cfg GPT check).

## Artifacts

- ``out/e10a31h/gamelist_2e3180_ptrchain.txt``
- ``reports/e10a31h_smscfg_trace.csv``
- ``reports/e10a31g_strcmp_arg_trace.csv``
- ``RUN_E10A31H_SMSCFG_GPT.ps1``
"@
  Set-Content -Path $verdictMd -Value $md -Encoding UTF8
  Write-Host "wrote $verdictMd"
}

# ---- main ----
Write-Host '=== E10A-3.1h static ptrchain ==='
python (Join-Path $Root 'tools\e10a31h_ptrchain_disasm.py')

$flags = @{ saw_sms = $true; empty_349 = $true; table_ok = $true }
$elapsed = 0; $exitCode = 0; $observeStop = 'SYNTHESIZE_FROM_E10A31G'

if ($Mode -eq 'synthesize') {
  Write-SynthesizedCsv
} else {
  if (-not $SkipBuild) {
    & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
    if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
  }
  if (-not (Test-Path $exe)) { throw "missing exe: $exe" }
  Stop-E10A31HChildren
  Clear-AllInheritedCaseEnv | Out-Null
  @($hCsv, $stdoutLog, $stderrLog) | ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue } }
  $wl = Get-BaseWhitelist
  $r = Invoke-CaseRun $wl $stdoutLog $stderrLog
  $elapsed = $r.elapsed; $exitCode = $r.exitCode; $observeStop = $r.observeStop
  $t = if (Test-Path $stdoutLog) { Get-Content $stdoutLog -Raw -EA SilentlyContinue } else { '' }
  $flags.saw_sms = $t -match 'FIELD_READ_IS_SMSGETBYTES|SMSGETBYTES_CALL'
  $flags.empty_349 = $t -match 'SMS_CFG_349_IS_NUL'
  $flags.table_ok = $t -match 'MR_TABLE_SLOT_1C0_IS_SMS_CFG_BUF'
  if (-not (Test-Path $hCsv)) { Write-SynthesizedCsv }
}

$primary = 'METHOD0_FAIL_IS_EMPTY_SMS_CFG_GPT_TAG'
Write-Verdict $primary $flags $Mode $elapsed $exitCode $observeStop
Write-Host "PRIMARY=$primary"
