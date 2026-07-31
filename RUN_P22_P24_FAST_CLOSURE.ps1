# P22-P24 Gamelist Native Selection Fast Closure (original_headless research path)
param(
  [int]$Seconds = 45,
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
New-Item -ItemType Directory -Force -Path $logDir, $reportDir | Out-Null

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\p22\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'p22_p24_fast_closure_stdout.txt'
$stderrLog = Join-Path $logDir 'p22_p24_fast_closure_stderr.txt'
$CASE_TIMEOUT_SEC = [Math]::Max(30, [Math]::Min(90, $Seconds))

function Stop-P22Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'P22_|E10A31_|E10A3_')
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

Stop-P22Children
Clear-CaseEnv

if (-not $SkipBuild) {
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
$commit = (git rev-parse HEAD).Trim()
$dirty = if ((git status --porcelain) ) { 'dirty' } else { 'clean' }

Write-Host "G0 commit=$commit tree=$dirty main.exe=$exeLen sha=$exeSha main_gwy=$gwyLen sha=$gwySha ts=$buildTs"
if ($exeLen -le 0 -or $gwyLen -le 0) { throw 'G0 FAIL: zero-byte EXE' }

New-Item -ItemType Directory -Force -Path $OverlayRoot | Out-Null
@(
  (Join-Path $reportDir 'P22_SELECTION_GATES.csv'),
  (Join-Path $reportDir 'P23_STARTGAME_CONTRACT.json'),
  (Join-Path $reportDir 'P24_NESTED_JJFB_MATRIX.csv'),
  $stdoutLog, $stderrLog
) | ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue } }

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

# Env must be set AFTER visual staging (it may clear JJFB_/GWY_ vars).
$env:JJFB_P22_MODE = 'original_headless'
$env:JJFB_P22_HEADLESS_SELECT = '1'
$env:JJFB_P22_RUN_ID = "$RunId"
$env:JJFB_P22_GATES_CSV = (Join-Path $reportDir 'P22_SELECTION_GATES.csv')
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
$env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
$deadline = (Get-Date).AddSeconds($CASE_TIMEOUT_SEC + $HoldSec + 15)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
}
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
  Write-Host "[P22] killed after timeout ${CASE_TIMEOUT_SEC}s"
}
Start-Sleep -Milliseconds 800
try { $p.Refresh() } catch {}

# Build identity + gate summary into reports
$logText = ''
if (Test-Path $stdoutLog) { $logText = Get-Content $stdoutLog -Raw -ErrorAction SilentlyContinue }
if (-not $logText) { $logText = '' }
function Has([string]$pat) { return [bool]($logText -match $pat) }

$gates = [ordered]@{
  G0_BUILD = ($exeLen -gt 0)
  G1_STACK_DEPTH2 = (Has 'P22_GATE\] gate=G1_STACK_DEPTH2' )
  G2_LAUNCH_PARAM = (Has 'START_DSM_PARAM_ABI_CONFIRMED|G2_LAUNCH_PARAM|SHELL_PARAM_GWYBLINK_READ')
  G3_CFG_OPENED = (Has 'CFG_FILE_OPENED|G3_CFG_OPENED')
  G4_CFG36_PARSED = (Has 'CFG_RECORD_36_PARSED|G4_CFG36_PARSED')
  G5_ITEM_CREATED = (Has 'CFG36_ITEM_OBJECT_CREATED|G5_ITEM_CREATED')
  G6_SELECT_CALLBACK = (Has 'CFG36_ITEM_SELECTED|G6_SELECT_CALLBACK')
  G7_DESC_BUILDER = (Has 'DESCRIPTOR_BUILDER_ENTER|G7_DESC_BUILDER')
  G8_DESC_MATCH = (Has 'DESCRIPTOR_BUILT exact|G8_DESC_MATCH')
  G9_API_HANDOFF = (Has 'API_HANDOFF_ENTER|G9_API_HANDOFF')
  G10_STARTGAME_LOOKUP = (Has 'STARTGAME_LOOKUP|G10_STARTGAME_LOOKUP')
  G11_STARTGAME_ENTER = (Has 'STARTGAME_ENTER|G11_STARTGAME_ENTER')
  G12_OPCODE300 = (Has 'G12_OPCODE300|opcode.?300')
  G13_NESTED_JJFB = (Has 'G13_NESTED_JJFB|jjfb\.mrp.*start_dsm|SHELL_PHASE_JJFB')
  G14_ROBOTOL_EXT = (Has 'G14_ROBOTOL_EXT|robotol\.ext')
}

$matrix = @('gate,pass,note')
foreach ($k in $gates.Keys) {
  $matrix += ('{0},{1},' -f $k, ($(if ($gates[$k]) { '1' } else { '0' })))
}
Set-Content -Path (Join-Path $reportDir 'P24_NESTED_JJFB_MATRIX.csv') -Value $matrix -Encoding UTF8

if (-not (Test-Path (Join-Path $reportDir 'P22_SELECTION_GATES.csv'))) {
  Set-Content -Path (Join-Path $reportDir 'P22_SELECTION_GATES.csv') -Value "run_id,t_sec,gate,value,note`n$RunId,0,G0_BUILD,1,runner" -Encoding UTF8
}
if (-not (Test-Path (Join-Path $reportDir 'P23_STARTGAME_CONTRACT.json'))) {
  Set-Content -Path (Join-Path $reportDir 'P23_STARTGAME_CONTRACT.json') -Value "{`"run_id`":$RunId,`"note`":`"no_p22_finalize`"}" -Encoding UTF8
}

$md = @"
# P22–P24 Fast Closure

## Build identity (G0)
- commit: ``$commit``
- tree: $dirty
- main.exe size/sha256: $exeLen / ``$exeSha``
- main_gwy.exe size/sha256: $gwyLen / ``$gwySha``
- build_time_utc: $buildTs
- run_id: $RunId
- mode: original_headless (P22 research)
- seconds: $CASE_TIMEOUT_SEC

## Answers
1. launch param read: $(if ($gates.G2_LAUNCH_PARAM) { 'YES' } else { 'NO' })
2. cfg36 real parse: $(if ($gates.G4_CFG36_PARSED) { 'YES' } else { 'NO' })
3. cfg36 item object + callback: item=$(if ($gates.G5_ITEM_CREATED) { 'YES' } else { 'NO' }) selected=$(if ($gates.G6_SELECT_CALLBACK) { 'YES' } else { 'NO' })
4. natural callsite into 0x13A34: $(if ($gates.G7_DESC_BUILDER) { 'SEE LOG builder_via_callsite_off' } else { 'NOT REACHED' })
5. full descriptor: $(if ($gates.G8_DESC_MATCH) { 'EXACT MATCH' } else { 'NOT MATCHED / NOT BUILT' })
6. 0x13B7C object contract: $(if ($gates.G9_API_HANDOFF) { 'ENTERED — see P23 JSON / log' } else { 'NOT REACHED' })
7. startGame three args: $(if ($gates.G11_STARTGAME_ENTER) { 'SEE P23_STARTGAME_CONTRACT.json' } else { 'NOT REACHED' })
8. opcode300 contract: $(if ($gates.G12_OPCODE300) { 'YES' } else { 'NOT REACHED' })
9. nested JJFB: $(if ($gates.G13_NESTED_JJFB) { 'YES' } else { 'NO' })
10. first screen improved: PENDING (requires G14 + framebuffer compare)

## Gate matrix
$(($gates.GetEnumerator() | ForEach-Object { '- {0}: {1}' -f $_.Key, ($(if ($_.Value) { 'PASS' } else { 'FAIL' })) }) -join "`n")

## Artifacts
- ``reports/P22_SELECTION_GATES.csv``
- ``reports/P23_STARTGAME_CONTRACT.json``
- ``reports/P24_NESTED_JJFB_MATRIX.csv``
- ``logs/p22_p24_fast_closure_stdout.txt``
"@
Set-Content -Path (Join-Path $reportDir 'P22_P24_FAST_CLOSURE.md') -Value $md -Encoding UTF8
Write-Host $md
Stop-P22Children
