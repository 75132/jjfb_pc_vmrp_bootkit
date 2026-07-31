# P25-P27 Gamelist Config State Machine + Stray Control-Flow Closure
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
$packDir = Join-Path $Root 'research\packs\p25_cfg_state'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $packDir | Out-Null

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$mrpPath = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink"
$RunId = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$OverlayRoot = Join-Path $RunDir ("overlay\p25\{0}" -f $RunId)
$stdoutLog = Join-Path $logDir 'p25_cfg_state_stdout.txt'
$stderrLog = Join-Path $logDir 'p25_cfg_state_stderr.txt'
$traceCsv = Join-Path $packDir 'P25_TRACE.csv'
$reportMd = Join-Path $reportDir 'P25_GAMELIST_CONFIG_AND_CONTROL_FLOW.md'
$CASE_TIMEOUT_SEC = [Math]::Max(30, [Math]::Min(90, $Seconds))

function Stop-P25Children {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.ProcessId -ne $PID -and (
        $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
        ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'P25_|P22_|E10A31_')
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

Stop-P25Children
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
  $traceCsv,
  $reportMd,
  $stdoutLog, $stderrLog
) | ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -EA SilentlyContinue } }

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$env:JJFB_P25_MODE = '1'
$env:JJFB_P22_MODE = 'original_headless'
$env:JJFB_P22_HEADLESS_SELECT = '1'
$env:JJFB_P22_RUN_ID = "$RunId"
$env:JJFB_P25_RUN_ID = "$RunId"
$env:JJFB_P25_TRACE_CSV = $traceCsv
$env:JJFB_P22_GATES_CSV = $traceCsv
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

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog
$deadline = (Get-Date).AddSeconds($CASE_TIMEOUT_SEC + $HoldSec + 15)
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
}
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
  Write-Host "[P25] killed after timeout ${CASE_TIMEOUT_SEC}s"
}
Start-Sleep -Milliseconds 800
try { $p.Refresh() } catch {}

$logText = ''
if (Test-Path $stdoutLog) { $logText = Get-Content $stdoutLog -Raw -ErrorAction SilentlyContinue }
if (-not $logText) { $logText = '' }
function Has([string]$pat) { return [bool]($logText -match $pat) }

# Prefer last [JJFB_P25_FINAL] gate bitmap over loose substring matches (false PASS risk).
$finalGates = @{}
$finalLine = [regex]::Matches($logText, '\[JJFB_P25_FINAL\][^\r\n]*') | Select-Object -Last 1
if ($finalLine) {
  foreach ($m in [regex]::Matches($finalLine.Value, '(G\d+_[A-Z0-9_]+)=([01])')) {
    $finalGates[$m.Groups[1].Value] = ($m.Groups[2].Value -eq '1')
  }
}
function GatePass([string]$name, [string]$loosePat) {
  if ($finalGates.ContainsKey($name)) { return [bool]$finalGates[$name] }
  return (Has $loosePat)
}

$gates = [ordered]@{
  G1_CFG_LOADER = (GatePass 'G1_CFG_LOADER' '\[JJFB_P25_BP\] tag=CFG_LOADER_ENTRY')
  G2_INTERNAL_REQUESTED = (GatePass 'G2_INTERNAL_REQUESTED' '\[PLATFORM_10112\].*path="cfg\.bin"')
  G3_INTERNAL_LOADED = (GatePass 'G3_INTERNAL_LOADED' '\[PLATFORM_10112\].*ns=MRP_MEMBER.*len=6898')
  G4_INTERNAL_PARSED = (GatePass 'G4_INTERNAL_PARSED' 'G4_INTERNAL_PARSED=1')
  G5_PATH_STATE = (GatePass 'G5_PATH_STATE' '\[JJFB_P25_BP\] tag=CFG_PATH_STATE')
  G6_EXTERNAL_LOADED = (GatePass 'G6_EXTERNAL_LOADED' '\[PLATFORM_10112\].*gwy/cfg\.bin.*len=20728')
  G7_CFG36_PARSED = (GatePass 'G7_CFG36_PARSED' 'G7_CFG36_PARSED=1')
  G8_ITEM_CREATED = (GatePass 'G8_ITEM_CREATED' 'G8_ITEM_CREATED=1')
  G9_SELECT_CALLBACK = (GatePass 'G9_SELECT_CALLBACK' 'G9_SELECT_CALLBACK=1')
  G10_DESC_BUILDER_LEGAL = (GatePass 'G10_DESC_BUILDER_LEGAL' 'DESCRIPTOR_BUILDER_LEGAL_LR|G10_DESC_BUILDER_LEGAL=1')
  G11_STATE_NONEMPTY = (GatePass 'G11_STATE_NONEMPTY' 'G11_STATE_NONEMPTY=1')
  G12_DESC_MATCH = (GatePass 'G12_DESC_MATCH' 'G12_DESC_MATCH=1')
  G13_STARTGAME_LOOKUP = (GatePass 'G13_STARTGAME_LOOKUP' 'G13_STARTGAME_LOOKUP=1')
  G14_STARTGAME_ENTER = (GatePass 'G14_STARTGAME_ENTER' 'G14_STARTGAME_ENTER=1')
  G15_OPCODE300 = (GatePass 'G15_OPCODE300' 'G15_OPCODE300=1')
  G16_NESTED_JJFB = (GatePass 'G16_NESTED_JJFB' 'G16_NESTED_JJFB=1')
  G17_ROBOTOL_EXT = (GatePass 'G17_ROBOTOL_EXT' 'G17_ROBOTOL_EXT=1')
}

$baseRefine = if (Has 'gamelist_base_refine|RAW_BASE_REFINE.*module_id=4') { 'YES pad=0x14' } else { 'NO' }
$abi10112 = if (Has '\[PLATFORM_10112\]') { 'OBSERVED' } else { 'NOT_SEEN' }
$stray = if (Has 'DESCRIPTOR_BUILDER_STRAY_ENTRY') { 'YES' } else { 'NO' }
$fault = if (Has '\[JJFB_P25_FETCH_FAULT\]|UC_ERR_FETCH_UNMAPPED') { 'YES' } else { 'NO' }

$md = @"
# P25 Gamelist Config State Machine + Control Flow

## Build identity
- commit: ``$commit``
- tree: $dirty
- main.exe: $exeLen / ``$exeSha``
- main_gwy.exe: $gwyLen / ``$gwySha``
- build_time_utc: $buildTs
- run_id: $RunId
- seconds: $CASE_TIMEOUT_SEC

## Answers (required)

1. **0x10112 ABI**: $abi10112 — ``R0=0x10112 R1=ctx(R9+0x46C) R2=path R3=*out_buf SP[0]=*out_len``; bare ``cfg.bin`` → MRP_MEMBER; ``gwy/cfg.bin`` → SHARED_ROOT. See ``[PLATFORM_10112]`` lines.
2. **Internal cfg.bin loaded**: $(if ($gates.G3_INTERNAL_LOADED) { 'YES (6898)' } else { 'NO' })
3. **External cfg state machine**: $(if ($gates.G5_PATH_STATE) { 'ENTERED 0xD768' } else { 'NOT ENTERED' }) / external read=$(if ($gates.G6_EXTERNAL_LOADED) { 'YES 20728' } else { 'NO' })
4. **gwy/cfg.bin read**: $(if ($gates.G6_EXTERNAL_LOADED) { 'YES' } else { 'NO' })
5. **Gamelist base refine**: $baseRefine — file RVAs use refined raw base (not cacheSync align); prior B008/BL "patch" was base skew vs getter ``0x13A20``.
6. **Stray builder source**: $stray — see ``DESCRIPTOR_BUILDER_STRAY_ENTRY`` / ``JJFB_P25_CF_RING`` (legal LR set excludes B00D/AFF8 getter path)
7. **Fetch unmapped source**: $fault — see ``JJFB_P25_FETCH_FAULT``
8. **cfg36 item**: parsed=$(if ($gates.G7_CFG36_PARSED) { 'YES' } else { 'NO' }) item=$(if ($gates.G8_ITEM_CREATED) { 'YES' } else { 'NO' })
9. **startGame/opcode300/nested**: lookup=$(if ($gates.G13_STARTGAME_LOOKUP) { 'YES' } else { 'NO' }) enter=$(if ($gates.G14_STARTGAME_ENTER) { 'YES' } else { 'NO' }) op300=$(if ($gates.G15_OPCODE300) { 'YES' } else { 'NO' }) nested=$(if ($gates.G16_NESTED_JJFB) { 'YES' } else { 'NO' })

## Gate matrix
$(($gates.GetEnumerator() | ForEach-Object { '- {0}: {1}' -f $_.Key, ($(if ($_.Value) { 'PASS' } else { 'FAIL' })) }) -join "`n")

## Artifacts
- ``research/packs/p25_cfg_state/P25_TRACE.csv``
- ``reports/P25_GAMELIST_CONFIG_AND_CONTROL_FLOW.md``
- ``logs/p25_cfg_state_stdout.txt``
"@
Set-Content -Path $reportMd -Value $md -Encoding UTF8
if (-not (Test-Path $traceCsv)) {
  Set-Content -Path $traceCsv -Value "run_id,t_sec,event,gate,value,note`n$RunId,0,G0_BUILD,G0,1,runner" -Encoding UTF8
}
Write-Host $md
Stop-P25Children
