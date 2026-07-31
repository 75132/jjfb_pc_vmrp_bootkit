# P27 — Reentrant START_DSM Parameter Ownership and Host Return
param(
  [int]$Seconds = 70,
  [int]$HoldSec = 5,
  [int]$Zoom = 2,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$packDir = Join-Path $Root 'research\packs\p27_start_dsm'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $packDir | Out-Null

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\p27\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'p27_start_dsm_stdout.txt'
$stderrLog = Join-Path $logDir 'p27_start_dsm_stderr.txt'
$traceCsv = Join-Path $packDir 'P27_START_DSM_TRACE.csv'
$allocCsv = Join-Path $packDir 'P27_ALLOCATION_OWNERSHIP.csv'
$reportMd = Join-Path $reportDir 'P27_REENTRANT_START_DSM_FRAME.md'
$CASE_TIMEOUT_SEC = [Math]::Max(40, [Math]::Min(120, $Seconds))

function Stop-P27Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'P27_|P26_|P25_|E10A31_')
      )
    } |
    ForEach-Object { try { Stop-Process -Id $_.ProcessId -Force -EA SilentlyContinue } catch {} }
}

function Clear-CaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } | ForEach-Object {
    Remove-Item -Path ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
  }
}

function Get-FileSha256([string]$path) {
  if (-not (Test-Path $path)) { return 'MISSING' }
  return (Get-FileHash -Path $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

Stop-P27Children
Clear-CaseEnv

$sourceCommit = (git rev-parse HEAD).Trim()
$dirtyBefore = @(git status --porcelain --untracked-files=all -- `
  'src' 'third_party/vmrp_upstream' 'include' 'RUN_P27_START_DSM.ps1')
$cleanBefore = -not [bool]$dirtyBefore
$dirtyFilesBefore = if ($dirtyBefore) { ($dirtyBefore | ForEach-Object { $_.Substring(3).Trim() }) -join '; ' } else { '' }
$diffNames = (git diff --name-only -- 'src' 'third_party/vmrp_upstream' 'include' 'RUN_P27_START_DSM.ps1').Trim()
if (-not $diffNames) { $diffNames = '(none)' }

if (-not $SkipBuild) {
  if (-not $cleanBefore) {
    Write-Host "[P27][G0] WARNING: source tree not clean before build; recording dirty files"
    Write-Host $dirtyFilesBefore
  }
  Remove-Item (Join-Path $RunDir 'main.exe') -ErrorAction SilentlyContinue
  Remove-Item (Join-Path $RunDir 'main_gwy.exe') -ErrorAction SilentlyContinue
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP GwyResearch failed' }
}

if (-not (Test-Path $exe)) { throw "missing $exe" }
$exeLen = (Get-Item $exe).Length
$exeSha = Get-FileSha256 $exe
$gwyExe = Join-Path $RunDir 'main_gwy.exe'
if (-not (Test-Path $gwyExe)) { $gwyExe = $exe }
$gwyLen = (Get-Item $gwyExe).Length
$gwySha = Get-FileSha256 $gwyExe
$buildTs = (Get-Item $exe).LastWriteTimeUtc.ToString('o')
$shaAlign = ($exeSha -eq $gwySha)

Write-Host "G0 source_commit=$sourceCommit clean_before=$cleanBefore main.exe=$exeLen sha=$exeSha align=$shaAlign"

New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
@($traceCsv, $allocCsv, $reportMd, $stdoutLog, $stderrLog) | ForEach-Object {
  if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue }
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$env:JJFB_P27_MODE = '1'
$env:JJFB_P27_TRACE_CSV = $traceCsv
$env:JJFB_P27_ALLOC_CSV = $allocCsv
$env:JJFB_P26_MODE = '1'
$env:JJFB_P26_TRACE_CSV = (Join-Path $packDir 'P26_SIDE_TRACE.csv')
$env:JJFB_P25_MODE = '1'
$env:JJFB_P22_MODE = 'original_headless'
$env:JJFB_P22_HEADLESS_SELECT = '1'
$env:JJFB_P22_RUN_ID = "$RunId"
$env:JJFB_P25_RUN_ID = "$RunId"
$env:JJFB_E10A_RUN_ID = "$RunId"
$env:JJFB_E10A31_RUN_ID = "$RunId"
$env:JJFB_E10A_MODE = '1'
$env:JJFB_E10A_SHELL_TRACE = '1'
$env:JJFB_E10A31_MODE = '1'
$env:JJFB_E10A31_PARAM_TRACE = '1'
$env:JJFB_E10A31_CFG_GATE = '1'
$env:JJFB_E10A31_START_DSM_ABI = '1'
$env:JJFB_E10A31_TIMER_CONTEXT = '1'
$env:JJFB_E10A3_MODE = '1'
$env:JJFB_E9Y_MODE = '1'
$env:JJFB_E9Y_NO_DEBUG_AC8 = '1'
$env:JJFB_E9Y_NO_WORKBUF_SEED = '1'
$env:JJFB_PLATFORM_WORKBUF_ALLOC = '1'
$env:JJFB_GWY_PACK_REGISTRY = '1'
$env:JJFB_E9W_MODE = '1'
$env:JJFB_E9W_ARCHIVE_EXACT = '1'
$env:JJFB_DISPLAY_FIRST = '1'
$env:JJFB_E9B_MODE = '1'
$env:JJFB_VISIBLE_WINDOW = '1'
$env:JJFB_WINDOW_ZOOM = "$Zoom"
$env:JJFB_E9B_HOLD_SEC = "$HoldSec"
$env:JJFB_REAL_MRP_PATH = $mrpPath
$env:JJFB_FAST_BD0_INIT_CALL = '1'
$env:JJFB_FAST_PROGRESS_TICK_CALL = '1'
$env:JJFB_E9U_TICK_N = '12'
$env:JJFB_TIMER_DELIVER_TRACE = '1'
$env:JJFB_TIMER_ARM_TRACE = '1'
$env:JJFB_E10A31_WAIT_MS = "$([Math]::Max(5000, $Seconds * 1000))"
$env:GWY_RESOURCE_ROOT = $ResourceRoot
$env:GWY_OVERLAY_ROOT = $OverlayRoot
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'
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
$env:JJFB_PLAT_RET0_TRACE = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'

$sw = [Diagnostics.Stopwatch]::StartNew()
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
$deadline = (Get-Date).AddSeconds($CASE_TIMEOUT_SEC + $HoldSec + 15)
$hostSeenAt = $null
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
  if (-not $hostSeenAt -and (Test-Path $stdoutLog)) {
    $tail = Get-Content $stdoutLog -Tail 80 -ErrorAction SilentlyContinue | Out-String
    if ($tail -match 'START_DSM_RETURN|HOST_LOOP_REENTER') {
      $hostSeenAt = Get-Date
      # Hold a bit after host return to observe natural CFG / scheduler.
      $deadline = $hostSeenAt.AddSeconds([Math]::Max(8, $HoldSec + 5))
    }
  }
}
$killed = $false
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
  $killed = $true
  Write-Host "[P27] killed after timeout"
}
$sw.Stop()
Start-Sleep -Milliseconds 800
try { $p.Refresh() } catch {}

$logText = ''
if (Test-Path $stdoutLog) { $logText = Get-Content $stdoutLog -Raw -ErrorAction SilentlyContinue }
if (-not $logText) { $logText = '' }
$csvText = ''
if (Test-Path $traceCsv) { $csvText = Get-Content $traceCsv -Raw -ErrorAction SilentlyContinue }
if (-not $csvText) { $csvText = '' }
$allocText = ''
if (Test-Path $allocCsv) { $allocText = Get-Content $allocCsv -Raw -ErrorAction SilentlyContinue }
if (-not $allocText) { $allocText = '' }

function Has([string]$pat) { return [bool]($logText -match $pat) }
function CsvHas([string]$ev) {
  return [bool]($csvText -match "(?m)^[^,]*,$ev,") -or (Has "\[JJFB_P27\].*event=$ev")
}

# Extract start_t guest VAs from FRAME_ENTER / PARAM_ALLOCATED extras
$startGuests = [regex]::Matches($logText, 'start_g=0x([0-9A-Fa-f]+)') | ForEach-Object { $_.Groups[1].Value.ToUpperInvariant() }
$startGuests = @($startGuests | Select-Object -Unique)
$outerFrame = Has 'file=gwy/gbrwcore\.mrp|file=gbrwcore'
$nestedFrame = Has 'file=gwy/gamelist\.mrp|file=gamelist|gamelist\.mrp'
$g0 = ($exeLen -gt 0 -and $gwyLen -gt 0 -and $shaAlign)
$g1 = (CsvHas 'START_DSM_FRAME_ENTER') -and $outerFrame
$g2 = (CsvHas 'START_DSM_FRAME_ENTER') -and $nestedFrame
$g3 = ($startGuests.Count -ge 2)
$g4 = -not (Has 'START_DSM_PARAM_CLOBBERED') -and (Has 'START_DSM_PARAM_OWNERSHIP_OK')
$badFree = (Has 'WOULD_FREE_INVALID') -or (Has 'DOUBLE_FREE_ATTEMPT') -or (Has 'FOREIGN_FREE_ATTEMPT')
$g5 = -not $badFree
$g6 = (CsvHas 'START_DSM_EVENT_RETURN') -and (Has 'BRIDGE_MR_EXTHELPER_RETURN|MR_EVENT_FRAME_LEAVE')
$g7 = CsvHas 'START_DSM_FRAME_LEAVE'
# Outer unlocked return: deepest leave after nested, plus outer FRAME_LEAVE with dsm_depth=1
$g7outer = [bool]($logText -match 'event=START_DSM_FRAME_LEAVE.*dsm_depth=1') -or `
           [bool]($csvText -match '(?m)^[^,]*,START_DSM_FRAME_LEAVE,.*,1,')
$g8 = (CsvHas 'MUTEX_UNLOCK_END') -or (Has 'MUTEX_UNLOCK_END')
$g9 = (CsvHas 'START_DSM_RETURN') -or (Has 'op=START_DSM_RETURN') -or (Has 'event=START_DSM_RETURN')
$g10 = (CsvHas 'HOST_LOOP_REENTER') -or (Has 'HOST_LOOP_REENTER')
$g11 = (Has '\[JJFB_P25_BP\] tag=CFG_LOADER_ENTRY') -or (Has 'event=CFG_LOADER_ENTRY') -or (Has 'CFG_LOADER_ENTRY')
$g12 = Has '\[PLATFORM_10112\].*path="cfg\.bin".*len=6898|CFG_INTERNAL_LOADED|len=6898'

$clobberEvent = Has 'MR_EVENT_FRAME_CLOBBERED'
$sameStartT = ($startGuests.Count -eq 1)
$resumeFixed = Has 'returned_serial=' -and -not (Has 'event=RUN_CODE_CALLER_RESUME.*returned_depth=0.*returned_serial=0.*resumed_parent')

$reportCommit = (git rev-parse HEAD).Trim()
$dirtyAfter = @(git status --porcelain)
$dirtyAfterNames = if ($dirtyAfter) { ($dirtyAfter | ForEach-Object { $_.Substring(3) }) -join '; ' } else { '' }

$md = @"
# P27 Reentrant START_DSM Parameter Ownership and Host Return

## Verdict
$(if ($g1 -and $g2 -and $g3 -and $g4 -and $g5 -and $g9 -and $g10) {
  'Host return path restored (START_DSM_RETURN + HOST_LOOP_REENTER).'
} elseif ($g1 -and $g2 -and $g3 -and $g4 -and $g5 -and -not $g9) {
  'Ownership fix active, but START_DSM_RETURN still missing — inspect helper-tail BEFORE marker.'
} else {
  'See gate matrix.'
})

## P26 freeze / rename
- EXIT_PARK frozen (no park/sentinel/stop-hook edits this round).
- Former P26 G6 renamed to ``RUN_CODE_RETURNED_TO_HELPER_TAIL`` (not START_DSM_RETURN / HOST_LOOP_REENTER).

## Build identity
- source_commit: ``$sourceCommit``
- source_tree_clean_before_build: $cleanBefore
- dirty_files_before_build: ``$dirtyFilesBefore``
- git_diff_name_only_before_build: ``$diffNames``
- binary_sha main.exe: ``$exeSha`` (len=$exeLen)
- binary_sha main_gwy.exe: ``$gwySha`` (len=$gwyLen)
- sha_aligned: $shaAlign
- build_time_utc: $buildTs
- report_commit: ``$reportCommit``
- dirty_after_run: ``$dirtyAfterNames``
- run_id: $RunId
- wall_ms: $($sw.ElapsedMilliseconds)
- killed_timeout: $killed

## Gate matrix
| Gate | Result |
|------|--------|
| G0 build/SHA | $(if ($g0) { 'PASS' } else { 'FAIL' }) |
| G1 outer gbrwcore frame | $(if ($g1) { 'PASS' } else { 'FAIL' }) |
| G2 nested gamelist frame | $(if ($g2) { 'PASS' } else { 'FAIL' }) |
| G3 distinct start_t guest VA | $(if ($g3) { 'PASS' } else { 'FAIL' }) (vas=$($startGuests -join ',')) |
| G4 child does not clobber parent | $(if ($g4) { 'PASS' } else { 'FAIL' }) |
| G5 no invalid/double/foreign free | $(if ($g5) { 'PASS' } else { 'FAIL' }) |
| G6 outer bridge_mr_event return | $(if ($g6) { 'PASS' } else { 'FAIL' }) |
| G7 outer _unlocked return | $(if ($g7 -and $g7outer) { 'PASS' } else { 'FAIL' }) |
| G8 mutex unlock | $(if ($g8) { 'PASS' } else { 'FAIL' }) |
| G9 START_DSM_RETURN | $(if ($g9) { 'PASS' } else { 'FAIL' }) |
| G10 HOST_LOOP_REENTER | $(if ($g10) { 'PASS' } else { 'FAIL' }) |
| G11 base+0x7B6C | $(if ($g11) { 'PASS' } else { 'FAIL' }) |
| G12 cfg.bin 6898 | $(if ($g12) { 'PASS' } else { 'NOT_SEEN' }) |

## Required answers
1. **outer/nested same start_t?** $(if ($sameStartT) { 'YES (FAIL — still shared)' } else { 'NO — distinct per-call frames' })
2. **child covered parent filename/ext/entry?** $(if (Has 'START_DSM_PARAM_CLOBBERED') { 'YES (FAIL)' } else { 'NO' })
3. **parent cleanup would free VA 0?** $(if (Has 'WOULD_FREE_INVALID') { 'WOULD have (blocked+logged)' } else { 'NO' })
4. **entry double free?** $(if (Has 'DOUBLE_FREE_ATTEMPT') { 'ATTEMPTED (blocked)' } else { 'NO' })
5. **mr_c_event nested clobber?** $(if ($clobberEvent) { 'YES (Case A — risk recorded, no per-call event_t yet)' } else { 'NOT_OBSERVED' })
6. **outer bridge_mr_event return?** $(if ($g6) { 'YES' } else { 'NO' })
7. **outer _unlocked return?** $(if ($g7outer) { 'YES' } else { 'NO' })
8. **mutex released?** $(if ($g8) { 'YES' } else { 'NO' })
9. **START_DSM_RETURN?** $(if ($g9) { 'YES' } else { 'NO' })
10. **HOST_LOOP_REENTER?** $(if ($g10) { 'YES' } else { 'NO' })
11. **base+0x7B6C natural?** $(if ($g11) { 'YES' } else { 'NO' })
12. **product Layer-1:** see product regression section (separate runners)

## Failure fork
$(if ($g9 -and $g10 -and -not $g11) {
  'C: host loop returned but +0x7B6C not hit — next = first scheduler/event dispatch; do not call CFG loader.'
} elseif (-not $g9) {
  'B: ownership OK or partial — stop at first missing HELPER_BEFORE_* / FRAME_LEAVE.'
} elseif ($g11) {
  'G11 hit — observe cfg.bin without forced calls.'
} else { 'See gates.' })

## Artifacts
- ``research/packs/p27_start_dsm/P27_START_DSM_TRACE.csv``
- ``research/packs/p27_start_dsm/P27_ALLOCATION_OWNERSHIP.csv``
- ``reports/P27_REENTRANT_START_DSM_FRAME.md``
- ``logs/p27_start_dsm_stdout.txt``
"@

Set-Content -Path $reportMd -Value $md -Encoding utf8
Write-Host $md
Write-Host "P27 report: $reportMd"
Write-Host "P27 trace: $traceCsv"
Write-Host "P27 alloc: $allocCsv"
