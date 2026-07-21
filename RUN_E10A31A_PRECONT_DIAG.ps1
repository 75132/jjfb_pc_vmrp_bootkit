# Stage E10A-3.1a: gbrwcore pre-continuation determinism and exit provenance
# Modes: manifest | exit_trace | tail_trace | ab_compare | validate
param(
  [int]$Seconds = 60,
  [int]$HoldSec = 5,
  [int]$Zoom = 2,
  [ValidateSet('manifest','exit_trace','tail_trace','ab_compare','validate')]
  [string]$Mode = 'exit_trace',
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
$outDir = Join-Path $Root 'out\e10a31a'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $outDir | Out-Null

$OUTER_KILL_SEC = [Math]::Max(30, [Math]::Min(90, $Seconds)) + $HoldSec + 15
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$verdictMd = Join-Path $reportDir 'stage_e10a31a_precont_diag_verdict.md'
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\e10a31a\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir ("e10a31a_{0}_stdout.txt" -f $Mode)
$stderrLog = Join-Path $logDir ("e10a31a_{0}_stderr.txt" -f $Mode)

function Stop-E10A31AChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'E10A31A_|E10A31_|E10A3_')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-AllInheritedCaseEnv {
  # Enumerate and remove every inherited JJFB_/GWY_/VMRP_ variable (no partial list).
  $names = @()
  Get-ChildItem Env: | ForEach-Object {
    if ($_.Name -match '^(JJFB_|GWY_|VMRP_)') { $names += $_.Name }
  }
  foreach ($n in $names) {
    Remove-Item -Path ("Env:{0}" -f $n) -ErrorAction SilentlyContinue
  }
  return $names
}

function Get-FileSha256([string]$path) {
  if (-not (Test-Path $path)) { return 'MISSING' }
  return (Get-FileHash -Path $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Write-EnvManifest([string[]]$cleared, $whitelist) {
  $csv = Join-Path $reportDir 'e10a31a_environment_manifest.csv'
  $lines = @('run_id,variable,value,source,whitelisted')
  foreach ($n in ($cleared | Sort-Object -Unique)) {
    $lines += ('{0},"{1}","","cleared_inherited",0' -f $RunId, $n)
  }
  foreach ($k in @($whitelist.Keys | Sort-Object)) {
    $v = [string]$whitelist[$k]
    $v = $v -replace '"', '""'
    $lines += ('{0},"{1}","{2}","whitelist_set",1' -f $RunId, $k, $v)
  }
  Set-Content -Path $csv -Value $lines -Encoding UTF8
}

function Write-RuntimeManifest {
  $csv = Join-Path $reportDir 'e10a31a_runtime_manifest.csv'
  $profile = Join-Path $Root 'profiles\jjfb.json'
  $cfn = Join-Path $ResourceRoot 'cfunction.ext'
  if (-not (Test-Path $cfn)) { $cfn = Join-Path $ResourceRoot 'gwy\cfunction.ext' }
  $buildTs = if (Test-Path $exe) { (Get-Item $exe).LastWriteTimeUtc.ToString('o') } else { 'MISSING' }
  $rows = @(
    'run_id,key,value',
    ("{0},main_exe_sha256,{1}" -f $RunId, (Get-FileSha256 $exe)),
    ("{0},gbrwcore_mrp_sha256,{1}" -f $RunId, (Get-FileSha256 (Join-Path $ResourceRoot 'gwy\gbrwcore.mrp'))),
    ("{0},gamelist_mrp_sha256,{1}" -f $RunId, (Get-FileSha256 (Join-Path $ResourceRoot 'gwy\gamelist.mrp'))),
    ("{0},gwy_mrp_sha256,{1}" -f $RunId, (Get-FileSha256 (Join-Path $ResourceRoot 'gwy\gwy.mrp'))),
    ("{0},profile_sha256,{1}" -f $RunId, (Get-FileSha256 $profile)),
    ("{0},cfunction_member_view_sha256,{1}" -f $RunId, (Get-FileSha256 $cfn)),
    ("{0},working_directory,{1}" -f $RunId, $RunDir),
    ("{0},resource_root,{1}" -f $RunId, $ResourceRoot),
    ("{0},overlay_root,{1}" -f $RunId, $OverlayRoot),
    ("{0},build_timestamp_utc,{1}" -f $RunId, $buildTs)
  )
  Set-Content -Path $csv -Value $rows -Encoding UTF8
}

function Reset-E10A31AArtifacts {
  @(
    'reports\e10a31a_continue_gate_trace.csv',
    'reports\e10a31a_runtime_stop_trace.csv',
    'reports\e10a31a_gbrwcore_tail_trace.csv',
    'reports\e10a31a_ab_environment_diff.csv',
    'reports\e10a31a_ab_milestone_compare.csv'
  ) | ForEach-Object {
    $p = Join-Path $Root $_
    if (Test-Path $p) { Clear-Content -Path $p -ErrorAction SilentlyContinue }
  }
}

function Set-E10A31AWhitelist([string]$caseTag) {
  New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
  $wl = [ordered]@{
    JJFB_E10A_RUN_ID = "$RunId"
    JJFB_E10A31A_RUN_ID = "$RunId"
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
    JJFB_E10A31A_CONTINUE_CSV = (Join-Path $reportDir 'e10a31a_continue_gate_trace.csv')
    JJFB_E10A31A_STOP_CSV = (Join-Path $reportDir 'e10a31a_runtime_stop_trace.csv')
    JJFB_E10A31A_TAIL_CSV = (Join-Path $reportDir 'e10a31a_gbrwcore_tail_trace.csv')
  }

  switch ($Mode) {
    'manifest' { $wl['JJFB_E10A31A_EXIT_TRACE'] = '1' }
    'exit_trace' { $wl['JJFB_E10A31A_EXIT_TRACE'] = '1' }
    'tail_trace' {
      $wl['JJFB_E10A31A_EXIT_TRACE'] = '1'
      $wl['JJFB_E10A31A_TAIL_TRACE'] = '1'
    }
    'validate' { $wl['JJFB_E10A31A_MODE'] = '1' }
    'ab_compare' {
      if ($caseTag -eq 'B') {
        $wl['JJFB_E10A31_TIMER_CONTEXT'] = '1'
        $wl['JJFB_E10A31_MODE'] = '1'
      }
      $wl['JJFB_E10A31A_EXIT_TRACE'] = '1'
      $wl['JJFB_E10A31A_AB'] = '1'
    }
  }

  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  return $wl
}

function Invoke-E10A31ARun([string]$caseTag) {
  Stop-E10A31AChildren
  $cleared = Clear-AllInheritedCaseEnv
  $wl = Set-E10A31AWhitelist $caseTag
  Write-EnvManifest $cleared $wl
  Write-RuntimeManifest

  $tag = if ($caseTag) { $caseTag } else { $Mode }
  $outLog = Join-Path $logDir ("e10a31a_{0}_stdout.txt" -f $tag.ToLowerInvariant())
  $errLog = Join-Path $logDir ("e10a31a_{0}_stderr.txt" -f $tag.ToLowerInvariant())
  if ($Mode -eq 'exit_trace' -or $Mode -eq 'validate' -or $Mode -eq 'tail_trace') {
    $outLog = $stdoutLog
    $errLog = $stderrLog
  }

  Write-Host "=== E10A-3.1a mode=$Mode case=$caseTag run_id=$RunId outer_kill=${OUTER_KILL_SEC}s overlay=$OverlayRoot ==="
  $t0 = Get-Date
  $killedByRunner = $false
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog
  if (-not $p.WaitForExit($OUTER_KILL_SEC * 1000)) {
    $killedByRunner = $true
    try { Stop-Process -Id $p.Id -Force -EA SilentlyContinue } catch {}
    Stop-E10A31AChildren
    Start-Sleep -Milliseconds 400
  }
  try { $p.Refresh() } catch {}
  $elapsed = [Math]::Round(((Get-Date) - $t0).TotalSeconds, 1)
  $hasExited = [bool]$p.HasExited
  $exitCode = if ($hasExited) { [int]$p.ExitCode } else { -1 }

  return [pscustomobject]@{
    caseTag = $caseTag
    stdout = $outLog
    stderr = $errLog
    elapsed = $elapsed
    hasExited = $hasExited
    exitCode = $exitCode
    killedByRunner = $killedByRunner
    cleared = $cleared
  }
}

function Analyze-E10A31A([string]$log) {
  $t = if (Test-Path $log) { Get-Content $log -Raw -EA SilentlyContinue } else { '' }
  $flags = @{
    br_exit_enter = $t -match 'GWY_BR_EXIT_ENTER'
    continue_decision = $t -match 'GWY_CONTINUE_DECISION'
    continue_apply = $t -match 'GWY_CONTINUE_APPLY|JJFB_SHELL_CORE_CONTINUE'
    br_exit_fallback = $t -match 'GWY_BR_EXIT_FALLBACK'
    process_exit = $t -match 'GWY_BR_EXIT_PROCESS_EXIT'
    font_suc = $t -match 'font load suc|GBRWCORE_FONT_RETURN_REACHED'
    post_cont = $t -match 'POST_CONT_PUMP'
    gamelist_pc = $t -match 'GAMELIST_EXT_FIRST_PC|mark=GAMELIST_EXT_FIRST_PC'
    gbrwcore_start = $t -match 'SHELL_PHASE_GBRWCORE_START|gbrwcore'
    stop_line = $t -match 'JJFB_E10A31A_STOP'
  }
  $decision = '?'
  if ($t -match 'decision=(GWY_CONTINUE_[A-Z0-9_]+)') {
    $ms = [regex]::Matches($t, 'decision=(GWY_CONTINUE_[A-Z0-9_]+)')
    if ($ms.Count -gt 0) { $decision = $ms[$ms.Count - 1].Groups[1].Value }
  }
  $firstStop = '?'
  $finalStop = '?'
  if ($t -match 'first=([A-Za-z0-9_]+)') {
    $ms = [regex]::Matches($t, 'first=([A-Za-z0-9_]+)')
    if ($ms.Count -gt 0) { $firstStop = $ms[$ms.Count - 1].Groups[1].Value }
  }
  if ($t -match 'final=([A-Za-z0-9_]+)') {
    $ms = [regex]::Matches($t, 'final=([A-Za-z0-9_]+)')
    if ($ms.Count -gt 0) { $finalStop = $ms[$ms.Count - 1].Groups[1].Value }
  }

  $primary = 'E10A31A_INSUFFICIENT_PRECONT_EVIDENCE'
  if ($flags.continue_apply -and $flags.post_cont) { $primary = 'SHELL_CORE_CONTINUE_REACHED' }
  elseif ($flags.continue_apply) { $primary = 'SHELL_CONTINUE_APPLY_NO_POST_CONT' }
  elseif ($flags.br_exit_enter -and $decision -eq 'GWY_CONTINUE_READY') { $primary = 'SHELL_CONTINUE_GATE_READY' }
  elseif ($flags.br_exit_enter -and $decision -match 'NOT_ARMED|MODE_DISABLED') { $primary = 'SHELL_CONTINUE_GATE_NOT_ARMED' }
  elseif ($flags.br_exit_enter -and $decision -match 'TARGET_NOT_OBSERVED') { $primary = 'SHELL_CONTINUE_TARGET_NOT_OBSERVED' }
  elseif ($flags.br_exit_enter -and $flags.br_exit_fallback) { $primary = 'GBRWCORE_EXIT_WITHOUT_CONTINUATION' }
  elseif ($flags.br_exit_enter) { $primary = 'GBRWCORE_BR_EXIT_REACHED' }
  elseif ($flags.font_suc -and -not $flags.br_exit_enter) { $primary = 'GBRWCORE_RETURNED_WITHOUT_MR_EXIT' }
  elseif ($firstStop -match 'UNICORN_FAULT') { $primary = 'GBRWCORE_TAIL_FAULT' }

  return [pscustomobject]@{
    primary = $primary
    decision = $decision
    firstStop = $firstStop
    finalStop = $finalStop
    flags = $flags
  }
}

function Write-Verdict([object]$run, [object]$an) {
  $envOk = (Test-Path (Join-Path $reportDir 'e10a31a_environment_manifest.csv')) -and
           (Test-Path (Join-Path $reportDir 'e10a31a_runtime_manifest.csv'))
  $md = @"
# E10A-3.1a pre-continuation diagnostic verdict

- mode: ``$Mode``
- run_id: ``$RunId``
- requested_seconds: $Seconds
- outer_kill_sec: $OUTER_KILL_SEC
- elapsed_sec: $($run.elapsed)
- killed_by_runner: $($run.killedByRunner)
- process_exited: $($run.hasExited)
- process_exit_code: $($run.exitCode)
- overlay_root: ``$OverlayRoot``
- primary: ``$($an.primary)``
- continue_decision: ``$($an.decision)``
- first_stop: ``$($an.firstStop)``
- final_stop: ``$($an.finalStop)``

## Milestones

| marker | observed |
|--------|----------|
| font load / FONT_RETURN | $($an.flags.font_suc) |
| GWY_BR_EXIT_ENTER | $($an.flags.br_exit_enter) |
| GWY_CONTINUE_DECISION | $($an.flags.continue_decision) |
| GWY_CONTINUE_APPLY / SHELL_CORE_CONTINUE | $($an.flags.continue_apply) |
| GWY_BR_EXIT_FALLBACK | $($an.flags.br_exit_fallback) |
| GWY_BR_EXIT_PROCESS_EXIT | $($an.flags.process_exit) |
| POST_CONT_PUMP | $($an.flags.post_cont) |
| GAMELIST_EXT_FIRST_PC | $($an.flags.gamelist_pc) |
| E10A31A_STOP | $($an.flags.stop_line) |

## Environment

- deterministic_env_manifest: $envOk
- verdict_env: $(if ($envOk) { 'E10A31A_RUN_ENVIRONMENT_DETERMINISTIC' } else { 'E10A31A_ENV_MANIFEST_MISSING' })

## Artifacts

- logs: ``$($run.stdout)``, ``$($run.stderr)``
- continue CSV: ``reports/e10a31a_continue_gate_trace.csv``
- stop CSV: ``reports/e10a31a_runtime_stop_trace.csv``
- tail CSV: ``reports/e10a31a_gbrwcore_tail_trace.csv``

## Next

Only resume E10A-3.1 ``timer_context`` after strong success
(SHELL_CORE_CONTINUE_REACHED + POST_CONT_PUMP + GAMELIST_EXT_FIRST_PC).
"@
  Set-Content -Path $verdictMd -Value $md -Encoding UTF8
}

function Write-AbCompare([object]$a, [object]$b, [object]$anA, [object]$anB) {
  $diff = Join-Path $reportDir 'e10a31a_ab_environment_diff.csv'
  $cmp = Join-Path $reportDir 'e10a31a_ab_milestone_compare.csv'
  Set-Content -Path $diff -Value @(
    'run_id,variable,case_a,case_b,differs',
    ("{0},JJFB_E10A31_TIMER_CONTEXT,unset,1,1" -f $RunId),
    ("{0},JJFB_E10A31_MODE,unset,1,1" -f $RunId)
  ) -Encoding UTF8

  $keys = @(
    @{n='gbrwcore_font'; fa={$anA.flags.font_suc}; fb={$anB.flags.font_suc}},
    @{n='br_exit'; fa={$anA.flags.br_exit_enter}; fb={$anB.flags.br_exit_enter}},
    @{n='continue_decision'; fa={$anA.decision}; fb={$anB.decision}},
    @{n='continue_apply'; fa={$anA.flags.continue_apply}; fb={$anB.flags.continue_apply}},
    @{n='post_cont_pump'; fa={$anA.flags.post_cont}; fb={$anB.flags.post_cont}},
    @{n='gamelist_first_pc'; fa={$anA.flags.gamelist_pc}; fb={$anB.flags.gamelist_pc}},
    @{n='exit_code'; fa={$a.exitCode}; fb={$b.exitCode}},
    @{n='elapsed_sec'; fa={$a.elapsed}; fb={$b.elapsed}}
  )
  $lines = @('run_id,milestone,case_a,case_b,same')
  foreach ($k in $keys) {
    $va = & $k.fa; $vb = & $k.fb
    $same = if ("$va" -eq "$vb") { 1 } else { 0 }
    $lines += ('{0},{1},"{2}","{3}",{4}' -f $RunId, $k.n, $va, $vb, $same)
  }
  Set-Content -Path $cmp -Value $lines -Encoding UTF8

  $aOk = $anA.flags.continue_apply -and $anA.flags.gamelist_pc
  $bOk = $anB.flags.continue_apply -and $anB.flags.gamelist_pc
  if ($aOk -and -not $bOk) { return 'E10A31_TRACE_SIDE_EFFECT' }
  if (-not $aOk -and -not $bOk) { return 'BUILD_OVERLAY_OR_ENVIRONMENT_DRIFT' }
  if ($aOk -and $bOk) { return 'PREVIOUS_EARLY_EXIT_WAS_NONDETERMINISTIC_OR_STALE_ENV' }
  return 'E10A31A_AB_INCONCLUSIVE'
}

# ---- main ----
if (-not $SkipBuild) {
  & (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
}
if (-not (Test-Path $exe)) { throw "missing $exe" }

Reset-E10A31AArtifacts

if ($Mode -eq 'manifest') {
  Stop-E10A31AChildren
  $cleared = Clear-AllInheritedCaseEnv
  $wl = Set-E10A31AWhitelist ''
  Write-EnvManifest $cleared $wl
  Write-RuntimeManifest
  $fake = [pscustomobject]@{ elapsed=0; killedByRunner=$false; hasExited=$true; exitCode=0; stdout=''; stderr='' }
  $an = [pscustomobject]@{ primary='E10A31A_RUN_ENVIRONMENT_DETERMINISTIC'; decision='n/a'; firstStop='n/a'; finalStop='n/a'; flags=@{font_suc=$false;br_exit_enter=$false;continue_decision=$false;continue_apply=$false;br_exit_fallback=$false;process_exit=$false;post_cont=$false;gamelist_pc=$false;stop_line=$false} }
  Write-Verdict $fake $an
  Write-Host "manifest-only done -> $verdictMd"
  exit 0
}

if ($Mode -eq 'ab_compare') {
  $script:OverlayRoot = Join-Path $RunDir ("overlay\e10a31a\{0}_A" -f $RunId)
  $runA = Invoke-E10A31ARun 'A'
  $anA = Analyze-E10A31A $runA.stdout
  $script:OverlayRoot = Join-Path $RunDir ("overlay\e10a31a\{0}_B" -f $RunId)
  $script:RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $runB = Invoke-E10A31ARun 'B'
  $anB = Analyze-E10A31A $runB.stdout
  $ab = Write-AbCompare $runA $runB $anA $anB
  $anA | Add-Member -NotePropertyName primary -NotePropertyValue $ab -Force
  Write-Verdict $runB $anB
  Add-Content -Path $verdictMd -Value ("`n## A/B interpretation`n`n- ab_verdict: ``{0}`` `n- A primary: ``{1}`` exit={2}`n- B primary: ``{3}`` exit={4}`n" -f $ab, $anA.primary, $runA.exitCode, $anB.primary, $runB.exitCode)
  Write-Host "ab_compare done ab=$ab A.exit=$($runA.exitCode) B.exit=$($runB.exitCode)"
  exit 0
}

$run = Invoke-E10A31ARun ''
$an = Analyze-E10A31A $run.stdout
Write-Verdict $run $an
Write-Host "primary=$($an.primary) decision=$($an.decision) exit=$($run.exitCode) elapsed=$($run.elapsed)s killed=$($run.killedByRunner)"
Write-Host "verdict -> $verdictMd"
