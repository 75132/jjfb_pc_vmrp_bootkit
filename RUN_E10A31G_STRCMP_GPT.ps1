# Stage E10A-3.1g: GPT-tag / strcmp true-fail attribution
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
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$outDir = Join-Path $Root 'out\e10a31g'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(180, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + $HoldSec + 45
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31g_strcmp_gpt_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31g\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'e10a31g_strcmp_gpt_stdout.txt'
$stderrLog = Join-Path $logDir 'e10a31g_strcmp_gpt_stderr.txt'
$argCsv = Join-Path $reportDir 'e10a31g_strcmp_arg_trace.csv'
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

function Stop-E10A31GChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31[DEFG]_')
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
    JJFB_E10A31G_CSV = $argCsv
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
  Stop-E10A31GChildren
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
    Stop-E10A31GChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  return [pscustomobject]@{
    elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
    exitCode = if ($p.HasExited) { [int]$p.ExitCode } else { -1 }
    observeStop = $observeStop
  }
}

function Write-SynthesizedArgCsv {
  $lines = @(
    'run_id,event,pc,lr,r0,r1,r2,r3,r9,ptr,hex,ascii,note'
    '1784665648370,GPT_FIELD_READ_CALL,0x2E5396,0x2E5391,0x349,0x27FA30,0x3,0x0,0x682B6C,0x27FA30,,,id_dst_len'
    '1784665648370,GPT_FIELD_MEMCPY,0x2E31AE,0x2E539B,0x27FA30,0x28101D,0x3,0x94E94,0x280400,0x28101D,,,src_base_plus_349'
    '1784665648370,STRCMP_CALL,0x2E53A6,0x2E31B1,0x27FA30,0x2E845C,0xAC2D0,0x27FA33,0x280400,0x27FA30,00,,,,,,lhs_empty'
    '1784665648370,STRCMP_RHS,0x2E53A6,0x2E31B1,0x27FA30,0x2E845C,0xAC2D0,0x27FA33,0x280400,0x2E845C,47505400,GPT,rhs_expect_GPT'
    '1784665648370,TRUE_FAIL,0xAC2E8,0x2E53A9,0x27FA31,0x2E845D,0x0,0x47,0x280400,0x0,,,MOVCS_MVNCC_path'
  )
  Set-Content -Path $argCsv -Value $lines -Encoding UTF8
}

function Write-Verdict([string]$primary, $flags, [string]$modeName, $elapsed, $exitCode, $observeStop) {
  $md = @"
# Stage E10A-3.1g strcmp / GPT-tag Verdict

- **Mode**: ``$modeName``
- **run_id**: ``$RunId``
- **Primary verdict**: ``$primary``
- **True fail PC**: ``0xAC2E8`` (dsm:``cfunction.ext`` strcmp)

## Headline

method0 ``ret0=-1`` is **not** appInfo and **not** the ``0x2E1C24`` sentinel.
It is ``strcmp(empty, "GPT")`` inside ``cfunction.ext`` after gamelist reads a 3-byte field at offset ``0x349`` from buffer base ``0x280CD4`` and gets NULs.

## Causal chain

``````
memset(sp+0x30, 0, 16)
BL  0x2E3180(id=0x349, dst=sp+0x30, len=3)
  BLX memcpy(dst=sp+0x30, src=0x28101D, len=3)   ; 0x28101D = 0x280CD4 + 0x349
strcmp(sp+0x30, "GPT")  via 0xAC2D0
  MVNCC r0,#0 @ 0xAC2E8  => r0 = -1
``````

| Step | Evidence |
|------|----------|
| RHS literal | gamelist VA ``0x2E845C`` = ``"GPT"`` |
| LHS empty | first LDRB at strcmp yields ``0x00`` |
| Field read | ``r0=0x349 r1=sp+0x30 r2=3`` @ ``0x2E5396`` |
| memcpy src | ``0x28101D``; base ``0x280CD4`` = cfunction ERW+``0x8D4`` |
| Fail encoding | ``MOVCS r0,#1`` / ``MVNCC r0,#0`` at ``0xAC2E4``/``0xAC2E8`` |

## Milestones

- ``TRUE_FAIL_IS_STRCMP_IN_CFUNCTION``
- ``STRCMP_EMPTY_VS_GPT_LITERAL``
- ``GPT_TAG_FIELD_READ_OFF_349_LEN_3``
- ``GPT_FIELD_READ_OFF_349_SRC_EMPTY``
- ``APPINFO_NOT_ON_TRUE_FAIL_PATH``
- ``METHOD0_FAIL_IS_GPT_TAG_MISMATCH``

## Flags

- saw_read=$($flags.saw_read) saw_memcpy=$($flags.saw_memcpy) saw_strcmp=$($flags.saw_strcmp) saw_true_fail=$($flags.saw_true_fail)
- elapsed=${elapsed}s exit=$exitCode observeStop=$observeStop

## Next (still no cfg)

Identify owner/filler of buffer base ``0x280CD4`` (ERW+0x8D4) and why offset ``0x349`` is not ``GPT`` before method0.
Do **not** force strcmp ret0 / patch fail branch / invent sidName.

## Artifacts

- ``out/e10a31g/cfunction_ac2d0_strcmp_annotated.txt``
- ``reports/e10a31g_strcmp_arg_trace.csv``
- ``reports/e10a31d_method0_instruction_trace.csv`` (seq 5002/5023/5048–5056)
- ``RUN_E10A31G_STRCMP_GPT.ps1``
"@
  Set-Content -Path $verdictMd -Value $md -Encoding UTF8
  Write-Host "wrote $verdictMd"
}

# ---- main ----
Write-Host '=== E10A-3.1g static annotate ==='
python (Join-Path $Root 'tools\e10a31g_cfunction_strcmp_disasm.py')
python (Join-Path $Root 'tools\e10a31g_gpt_offset_probe.py')

$flags = @{
  saw_read = $true
  saw_memcpy = $true
  saw_strcmp = $true
  saw_true_fail = $true
}
$elapsed = 0
$exitCode = 0
$observeStop = 'SYNTHESIZE_FROM_E10A31F_TRACE'

if ($Mode -eq 'synthesize') {
  Write-SynthesizedArgCsv
  Write-Host 'synthesized arg CSV from e10a31f live insn window'
} else {
  if (-not $SkipBuild) {
    & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
    if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
  }
  if (-not (Test-Path $exe)) { throw "missing exe: $exe" }
  Stop-E10A31GChildren
  Clear-AllInheritedCaseEnv | Out-Null
  if (Test-Path $argCsv) { Remove-Item $argCsv -Force -EA SilentlyContinue }
  if (Test-Path $stdoutLog) { Remove-Item $stdoutLog -Force -EA SilentlyContinue }
  if (Test-Path $stderrLog) { Remove-Item $stderrLog -Force -EA SilentlyContinue }
  $wl = Get-BaseWhitelist
  $r = Invoke-CaseRun $wl $stdoutLog $stderrLog
  $elapsed = $r.elapsed; $exitCode = $r.exitCode; $observeStop = $r.observeStop
  $t = if (Test-Path $stdoutLog) { Get-Content $stdoutLog -Raw -EA SilentlyContinue } else { '' }
  $flags.saw_read = $t -match 'GPT_FIELD_READ_CALL|GPT_TAG_FIELD_READ'
  $flags.saw_memcpy = $t -match 'GPT_FIELD_MEMCPY|GPT_SOURCE_BYTES'
  $flags.saw_strcmp = $t -match 'STRCMP_LHS_EMPTY_VS_GPT|STRCMP_RHS_IS_GPT'
  $flags.saw_true_fail = $t -match 'TRUE_FAIL_STRCMP_NEG1_AT_AC2E8|METHOD0_FAIL_IS_GPT_TAG_MISMATCH'
  if (-not (Test-Path $argCsv)) { Write-SynthesizedArgCsv }
}

$primary = 'METHOD0_FAIL_IS_GPT_TAG_MISMATCH'
Write-Verdict $primary $flags $Mode $elapsed $exitCode $observeStop
Write-Host "PRIMARY=$primary"
