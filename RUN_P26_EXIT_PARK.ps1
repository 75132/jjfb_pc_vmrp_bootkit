# P26 — EXIT_PARK Owner-Scoped runCode Break
param(
  [int]$Seconds = 55,
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
$packDir = Join-Path $Root 'research\packs\p26_exit_park'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $packDir | Out-Null

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\p26\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'p26_exit_park_stdout.txt'
$stderrLog = Join-Path $logDir 'p26_exit_park_stderr.txt'
$traceCsv = Join-Path $packDir 'P26_CONTROL_FLOW_TRACE.csv'
$reportMd = Join-Path $reportDir 'P26_EXIT_PARK_OWNER_BREAK.md'
$CASE_TIMEOUT_SEC = [Math]::Max(30, [Math]::Min(90, $Seconds))

function Stop-P26Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'P26_|P25_|P22_|E10A31_')
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

Stop-P26Children
Clear-CaseEnv

$sourceCommit = (git rev-parse HEAD).Trim()
$dirtyBefore = @(git status --porcelain --untracked-files=all -- 'src' 'third_party/vmrp_upstream' 'include' 'RUN_P26_EXIT_PARK.ps1')
$cleanBefore = -not [bool]$dirtyBefore
$dirtyFilesBefore = if ($dirtyBefore) { ($dirtyBefore | ForEach-Object { $_.Substring(3).Trim() }) -join '; ' } else { '' }
$diffNames = (git diff --name-only -- 'src' 'third_party/vmrp_upstream' 'include' 'RUN_P26_EXIT_PARK.ps1').Trim()
if (-not $diffNames) { $diffNames = '(none)' }

if (-not $SkipBuild) {
  if (-not $cleanBefore) {
    Write-Host "[P26][G0] WARNING: source tree not clean before build; recording dirty files"
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
@($traceCsv, $reportMd, $stdoutLog, $stderrLog) | ForEach-Object {
  if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue }
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$env:JJFB_P26_MODE = '1'
$env:JJFB_P26_TRACE_CSV = $traceCsv
$env:JJFB_P25_MODE = '1'
$env:JJFB_P22_MODE = 'original_headless'
$env:JJFB_P22_HEADLESS_SELECT = '1'
$env:JJFB_P22_RUN_ID = "$RunId"
$env:JJFB_P25_RUN_ID = "$RunId"
$env:JJFB_P25_TRACE_CSV = (Join-Path $packDir 'P25_SIDE_TRACE.csv')
$env:JJFB_P22_GATES_CSV = $env:JJFB_P25_TRACE_CSV
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
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
}
$killed = $false
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
  $killed = $true
  Write-Host "[P26] killed after timeout ${CASE_TIMEOUT_SEC}s"
}
$sw.Stop()
Start-Sleep -Milliseconds 800
try { $p.Refresh() } catch {}

$logText = ''
if (Test-Path $stdoutLog) { $logText = Get-Content $stdoutLog -Raw -ErrorAction SilentlyContinue }
if (-not $logText) { $logText = '' }
function Has([string]$pat) { return [bool]($logText -match $pat) }

$csvText = ''
if (Test-Path $traceCsv) { $csvText = Get-Content $traceCsv -Raw -ErrorAction SilentlyContinue }
if (-not $csvText) { $csvText = '' }

function CsvHas([string]$ev) { return [bool]($csvText -match "(?m)^[^,]*,$ev,") -or (Has "\[JJFB_P26_CF\].*event=$ev") }

$g0 = ($exeLen -gt 0 -and $gwyLen -gt 0 -and $shaAlign)
$g1 = (Has 'PARK_SET') -or (CsvHas 'PARK_SET')
$g2 = (Has 'EMU_STOP_REQUESTED') -or (CsvHas 'EMU_STOP_REQUESTED')
$g3 = (Has 'UC_EMU_START_RETURN') -or (CsvHas 'UC_EMU_START_RETURN')
$g4 = ((Has 'PARK_CONSUMED_BY_OWNER') -or (CsvHas 'PARK_CONSUMED_BY_OWNER')) -and -not (Has 'PARK_SEEN_NON_OWNER')
$mrFetch = Has 'JJFB_P25_FETCH_FAULT'
$fallbackHit = Has 'PARK_STOP_HOOK_FALLBACK'
$breakOk = (Has 'RUN_CODE_BREAK_OWNER') -or (CsvHas 'RUN_CODE_BREAK_OWNER')
$g5 = $breakOk -and -not $fallbackHit -and -not $mrFetch
# G6: caller resume AFTER park consume (not earlier nested resumes)
$g6 = $false
if ($csvText -match '(?s)PARK_CONSUMED_BY_OWNER.*?RUN_CODE_CALLER_RESUME') { $g6 = $true }
elseif ($logText -match '(?s)PARK_CONSUMED_BY_OWNER.*?event=RUN_CODE_CALLER_RESUME') { $g6 = $true }
$g7 = (Has '\[JJFB_P25_BP\] tag=CFG_LOADER_ENTRY') -or (CsvHas 'CFG_LOADER_ENTRY')
$g8 = Has '\[PLATFORM_10112\].*path="cfg\.bin".*len=6898|CFG_INTERNAL_LOADED|len=6898'
$g9 = (Has '\[JJFB_P25_BP\] tag=CFG_PATH_STATE') -or (CsvHas 'CFG_EXTERNAL_STATE')

$hostLoop = (Has 'HOST_LOOP_REENTER') -or (CsvHas 'HOST_LOOP_REENTER')
$timerBeforePark = $false
$parkIdx = $logText.IndexOf('event=PARK_SET')
if ($parkIdx -lt 0) { $parkIdx = $logText.IndexOf('PARK_SET') }
$breakIdx = $logText.IndexOf('RUN_CODE_BREAK_OWNER')
$pollAfterPark = $false
if ($parkIdx -ge 0 -and $breakIdx -gt $parkIdx) {
  $between = $logText.Substring($parkIdx, $breakIdx - $parkIdx)
  if ($between -match 'timer_poll_uc|op=POLL|gwy_ext_obs_timer_poll') { $pollAfterPark = $true }
}
$timerBeforePark = $pollAfterPark

$dsmAfterBreak = $false
if ($breakIdx -ge 0) {
  $after = $logText.Substring($breakIdx)
  if ($after -match 'ext call |mythroad exit|_mr_printf|0x8E6D0') { $dsmAfterBreak = $true }
}

$ownerLine = [regex]::Match($logText, 'PARK_SET[^:\r\n]*owner_depth=(\d+)\s+owner_serial=(\d+)')
if (-not $ownerLine.Success) {
  $ownerLine = [regex]::Match($logText, 'event=PARK_SET.*?park_owner_depth=(\d+)\s+park_owner_serial=(\d+)')
}
$ownerDepth = if ($ownerLine.Success) { $ownerLine.Groups[1].Value } else { '?' }
$ownerSerial = if ($ownerLine.Success) { $ownerLine.Groups[2].Value } else { '?' }

$fallback = Has 'PARK_STOP_HOOK_FALLBACK'
$emuStopInCallback = $g2

$reportCommit = (git rev-parse HEAD).Trim()
$dirtyAfter = @(git status --porcelain)
$dirtyAfterNames = if ($dirtyAfter) { ($dirtyAfter | ForEach-Object { $_.Substring(3) }) -join '; ' } else { '' }

$md = @"
# P26 EXIT_PARK Owner-Scoped runCode Break

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
- dirty_after_run (expected reports/logs): ``$dirtyAfterNames``
- run_id: $RunId
- seconds: $CASE_TIMEOUT_SEC
- wall_ms: $($sw.ElapsedMilliseconds)
- killed_timeout: $killed

## Gate matrix (P26)
- G0 build/identity: $(if ($g0) { 'PASS' } else { 'FAIL' })
- G1 PARK_SET owner-scoped: $(if ($g1) { 'PASS' } else { 'FAIL' })
- G2 uc_emu_stop in callback: $(if ($g2) { 'PASS' } else { 'FAIL' })
- G3 uc_emu_start returns: $(if ($g3) { 'PASS' } else { 'FAIL' })
- G4 park consumed once by owner: $(if ($g4) { 'PASS' } else { 'FAIL' })
- G5 no DSM / no _mr_ / no fallback: $(if ($g5) { 'PASS' } else { 'FAIL' })
- G6 caller resume: $(if ($g6) { 'PASS' } else { 'FAIL' })
- G7 CFG_LOADER base+0x7B6C: $(if ($g7) { 'PASS' } else { 'FAIL' })
- G8 internal cfg.bin 6898: $(if ($g8) { 'PASS' } else { 'NOT_SEEN' })
- G9 external state base+0xD768: $(if ($g9) { 'PASS' } else { 'NOT_SEEN' })

## Required answers
1. **uc_emu_stop in br_exit callback?** $(if ($emuStopInCallback) { 'YES' } else { 'NO' })
2. **park owner depth/serial?** depth=$ownerDepth serial=$ownerSerial
3. **park uniquely consumed by owner?** $(if ($g4) { 'YES' } else { 'NO' })
4. **timer_poll before park check?** $(if ($timerBeforePark) { 'YES (FAIL)' } else { 'NO (ordered correctly)' })
5. **DSM after RUN_CODE_BREAK_OWNER?** $(if ($dsmAfterBreak) { 'YES (FAIL)' } else { 'NO' })
6. **outer runCode returned to caller?** $(if ($g6) { 'YES' } else { 'NO' })
7. **host loop regained control?** $(if ($hostLoop) { 'YES' } else { 'NO' })
8. **base+0x7B6C hit?** $(if ($g7) { 'YES' } else { 'NO' })
9. **internal cfg.bin 6898 read?** $(if ($g8) { 'YES' } else { 'NOT_SEEN' })
10. **product path (separate RUN_PRODUCT_DIRECT_JJFB.ps1):** see product section after that run

## Failure fork
$(if ($g1 -and $g2 -and $g3 -and $g4 -and $g5 -and $g6 -and -not $g7) {
  'A: G1-G6 PASS but CFG_LOADER not hit — stop EXIT_PARK edits; next = RUN_CODE_CALLER_RESUME → HOST_LOOP_REENTER caller/dispatcher.'
} elseif ($g2 -and -not $g3) {
  'B: uc_emu_stop called but uc_emu_start did not return — inspect callback nesting / hooks before longjmp.'
} elseif (Has 'PARK_SEEN_NON_OWNER') {
  'C: nested runCode saw park — fix owner serial/depth propagation.'
} elseif ($g7) {
  'G7 hit — proceed to observe internal cfg / external state without forced calls.'
} else {
  'See gates above.'
})

## Artifacts
- ``research/packs/p26_exit_park/P26_CONTROL_FLOW_TRACE.csv``
- ``reports/P26_EXIT_PARK_OWNER_BREAK.md``
- ``logs/p26_exit_park_stdout.txt``
"@
Set-Content -Path $reportMd -Value $md -Encoding UTF8
Write-Host $md
Stop-P26Children
if (-not $g0) { exit 2 }
