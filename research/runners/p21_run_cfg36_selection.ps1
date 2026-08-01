# P21: cfg36 real list load / selection contract (observe-only).
# Freezes P20-CLEAN shell chain. NATURAL_ONLY. No startGame call / capsule / 10140.
param(
  [ValidateSet('A', 'B', 'Both')]
  [string]$Mode = 'Both',
  [int]$Seconds = 180,
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

$outDir = Join-Path $Root 'out\p21'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$identity = Join-Path $outDir 'p21_build_identity.txt'
$verdict = Join-Path $reportDir 'p21_cfg36_selection_verdict.md'

New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

function Get-Sha([string]$p) {
  if (-not (Test-Path $p)) { return 'MISSING' }
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

@"
source_commit=$((git rev-parse HEAD).Trim())
main_exe_sha256=$(Get-Sha $exe)
gate=P21_cfg36_selection_contract
NATURAL_ONLY=yes
JJFB_FORCE_10140_LIFECYCLE=0
no_headless_select=yes
no_startGame_call=yes
no_static_capsule=yes
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8

function Set-P21Env([string]$runId, [string]$overlay, [bool]$fastAssist, [string]$tag) {
  $wl = [ordered]@{
    JJFB_E10A_RUN_ID = $runId
    JJFB_E10A31_RUN_ID = $runId
    JJFB_P21_RUN_ID = $runId
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
    JJFB_E10A31_WAIT_FIRE_N = '3'
    JJFB_E10A31_TIMER_CSV = (Join-Path $reportDir ("p21_{0}_e10a31_timer.csv" -f $tag))
    JJFB_E10A31B_MODE = '1'
    JJFB_E10A31B_PUB_CSV = (Join-Path $reportDir ("p21_{0}_e10a31b_pub.csv" -f $tag))
    JJFB_E10A31_CFG_GATE = '1'
    JJFB_E10A31_PARAM_TRACE = '1'
    JJFB_E10A31_PARAM_CSV = (Join-Path $reportDir ("p21_{0}_e10a31_param.csv" -f $tag))
    JJFB_E10A31_CFG_GATE_CSV = (Join-Path $reportDir ("p21_{0}_e10a31_cfg_gate.csv" -f $tag))
    JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
    JJFB_P19_HANDOFF = '1'
    JJFB_P19_OUT_DIR = $outDir
    GWY_P19_PARENT_CHILD_HANDOFF = '1'
    JJFB_P20_CLEAN = '1'
    JJFB_P21_CFG_IO_CSV = (Join-Path $reportDir ("p21_{0}_cfg_file_io.csv" -f $tag))
    JJFB_P21_CFG_REC_CSV = (Join-Path $reportDir ("p21_{0}_cfg_record_inventory.csv" -f $tag))
    JJFB_P21_PARAM_CSV = (Join-Path $reportDir ("p21_{0}_launch_param_provenance.csv" -f $tag))
    JJFB_P21_SEL_CSV = (Join-Path $reportDir ("p21_{0}_cfg_selection_branches.csv" -f $tag))
    JJFB_P21_TIMER_CSV = (Join-Path $reportDir ("p21_{0}_timer_state_diff.csv" -f $tag))
  }
  if ($fastAssist) {
    $wl['JJFB_FAST_BD0_INIT_CALL'] = '1'
    $wl['JJFB_FAST_PROGRESS_TICK_CALL'] = '1'
    $wl['JJFB_E9U_TICK_N'] = '12'
  }
  foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }
  Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -EA SilentlyContinue
  Remove-Item Env:JJFB_FORCE_10140_ONESHOT -EA SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_DESCRIPTOR_DIRECT -EA SilentlyContinue
  Remove-Item Env:JJFB_E5_SCHEDULER_MODE -EA SilentlyContinue
  # Never enable P22 headless forge in P21.
  Remove-Item Env:JJFB_P22_MODE -EA SilentlyContinue
  Remove-Item Env:JJFB_P22_HEADLESS_SELECT -EA SilentlyContinue
  Remove-Item Env:JJFB_P25_MODE -EA SilentlyContinue
  return $wl
}

function Invoke-P21Case([string]$tag, [bool]$fastAssist) {
  Clear-CaseEnv
  Stop-Vmrp
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
  $runId = ('p21{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $tag, (Get-Date), (Get-Random -Maximum 99999))
  $overlay = Join-Path $RunDir ("overlay_$runId")
  New-Item -ItemType Directory -Force -Path $overlay | Out-Null
  $vmLog = Join-Path $logDir ("p21_{0}_vmrp.txt" -f $tag)
  $stderr = Join-Path $logDir ("p21_{0}_stderr.txt" -f $tag)
  @($vmLog, $stderr) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }
  $null = Set-P21Env $runId $overlay $fastAssist $tag

  Write-Host "=== P21-$tag run_id=$runId seconds=$Seconds fast_assist=$fastAssist ==="
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

  $r = [ordered]@{
    tag = $tag
    run_id = $runId
    research_assisted = [int]$fastAssist
    product_valid = [int](-not $fastAssist)
    FAST_BD0_INIT_CALL = if ($fastAssist) { 1 } else { 0 }
    FAST_PROGRESS_TICK_CALL = if ($fastAssist) { 1 } else { 0 }
    gbrwcore = [int]((Hit 'P19_START_DSM[^\n]*gbrwcore|SHELL_PHASE_GBRWCORE|gbrwcore\.mrp[^\n]*start') -gt 0)
    br_exit_continue = [int]((Hit 'SHELL_CORE_CONTINUE|GWY_CONTINUE_APPLY|phase=br_exit_enter') -gt 0)
    gamelist = [int]((Hit 'GAMELIST_STARTED|SHELL_PHASE_GAMELIST_LOAD|JJFB_SHELL_CORE_CONTINUE[^\n]*gamelist|JJFB_P21\] gate=GAMELIST_STARTED') -gt 0)
    erw_iso = [int]((Hit 'GAMELIST_ERW_HOST_ISOLATED|TIMER_CONTEXT_COHERENT') -gt 0)
    fire_ext = [int]((Hit 'PLATFORM_TIMER\] op=FIRE_EXT|FIRE_EXT code=') -gt 0)
    fire_ext_n = Hit 'PLATFORM_TIMER\] op=FIRE_EXT'
    fault30 = Hit '0x30D5D2'
    force10140 = Hit 'lifecycle_10140_forced|forced=yes'
    entry_ok = [int]((Hit 'MRPGCMAP_ENTRY[^\n]*result=EMU_OK') -gt 0)
    fmt_mapped = [int]((Hit 'gate=CFG_FMT_MAPPED|SHELL_PHASE_CFG_FMT_MAPPED') -gt 0)
    cfg_opened = [int]((Hit 'gate=CFG_FILE_OPENED|SHELL_PHASE_CFG_FILE_OPENED|CFG_FILE_OPENED') -gt 0)
    cfg_record_read = [int]((Hit 'gate=CFG_RECORD_READ') -gt 0)
    cfg36_present = [int]((Hit 'gate=CFG36_RECORD_PRESENT') -gt 0)
    cfg36_selected = [int]((Hit 'gate=CFG36_SELECTED') -gt 0)
    startgame_call = [int]((Hit 'PARENT_LAUNCH_ENTER[^\n]*call_kind=(BL|BLX|BX)') -gt 0)
    p21_final = FirstLine '\[JJFB_P21_FINAL\][^\n]+'
    cfg36_va_line = FirstLine 'CFG36_RECORD_PRESENT[^\n]+'
    param_write = Hit 'JJFB_P21_PARAM_WRITE'
    timer_idle = FirstLine 'JJFB_P21_TIMER_DIFF[^\n]*idle_no_io'
    vm_log = $vmLog
  }

  # Promote per-tag CSVs to canonical report names when this is primary evidence.
  $r['_io'] = Join-Path $reportDir ("p21_{0}_cfg_file_io.csv" -f $tag)
  $r['_rec'] = Join-Path $reportDir ("p21_{0}_cfg_record_inventory.csv" -f $tag)
  $r['_param'] = Join-Path $reportDir ("p21_{0}_launch_param_provenance.csv" -f $tag)
  $r['_sel'] = Join-Path $reportDir ("p21_{0}_cfg_selection_branches.csv" -f $tag)
  $r['_timer'] = Join-Path $reportDir ("p21_{0}_timer_state_diff.csv" -f $tag)
  Copy-Item $vmLog (Join-Path $outDir ("p21_{0}_vmrp.txt" -f $tag)) -Force -EA SilentlyContinue
  return $r
}

$cases = @()
if ($Mode -eq 'A' -or $Mode -eq 'Both') {
  $cases += ,(Invoke-P21Case 'A' $false)
}
if ($Mode -eq 'B' -or $Mode -eq 'Both') {
  $cases += ,(Invoke-P21Case 'B' $true)
}

$primary = $cases | Where-Object { $_.tag -eq 'A' } | Select-Object -First 1
if (-not $primary) { $primary = $cases[0] }
$b = $cases | Where-Object { $_.tag -eq 'B' } | Select-Object -First 1

# Canonical CSV names from primary evidence lane
foreach ($pair in @(
  @('_io', 'p21_cfg_file_io.csv'),
  @('_rec', 'p21_cfg_record_inventory.csv'),
  @('_param', 'p21_launch_param_provenance.csv'),
  @('_sel', 'p21_cfg_selection_branches.csv'),
  @('_timer', 'p21_timer_state_diff.csv')
)) {
  $src = $primary[$pair[0]]
  $dst = Join-Path $reportDir $pair[1]
  if ($src -and (Test-Path $src)) {
    Copy-Item $src $dst -Force
    Copy-Item $src (Join-Path $outDir $pair[1]) -Force -EA SilentlyContinue
  } else {
    "note,empty" | Set-Content $dst -Encoding utf8
  }
}

function Read-CsvHint([string]$path, [string]$colHint) {
  if (-not (Test-Path $path)) { return 'MISSING' }
  $lines = Get-Content $path -EA SilentlyContinue
  if (-not $lines -or $lines.Count -lt 2) { return 'empty' }
  return ("rows={0}" -f ($lines.Count - 1))
}

$aGl = if ($primary) { [bool]$primary.gamelist } else { $false }
$aCfgStop = if ($primary) {
  $primary.fmt_mapped -and -not $primary.cfg36_selected
} else { $false }
$sameStop = $true
if ($primary -and $b) {
  $sameStop = ($primary.gamelist -eq $b.gamelist) -and ($primary.cfg_opened -eq $b.cfg_opened) -and
              ($primary.cfg36_present -eq $b.cfg36_present) -and ($primary.cfg36_selected -eq $b.cfg36_selected)
}

$class = 'A'
if ($primary.cfg36_selected) { $class = 'F' }
elseif ($primary.cfg36_present -and -not $primary.cfg36_selected) {
  # D or E — distinguish later from selection CSV / UI
  $class = 'D_or_E'
}
elseif ($primary.cfg_record_read -and -not $primary.cfg36_present) { $class = 'B' }
elseif ($primary.cfg_opened -and -not $primary.cfg_record_read) { $class = 'B_partial' }
elseif (-not $primary.cfg_opened) { $class = 'A' }
else { $class = 'C_or_D' }

# Param handoff heuristic from provenance CSV
$paramCsv = Join-Path $reportDir 'p21_launch_param_provenance.csv'
$paramGlRead = 0
$paramParsed = 0
if (Test-Path $paramCsv) {
  $pl = Get-Content $paramCsv
  $paramParsed = @($pl | Select-Object -Skip 1 | Where-Object { $_ -and $_ -notmatch 'never_observed' }).Count
  $paramGlRead = @($pl | Select-Object -Skip 1 | Where-Object { $_ -match ',1,"' -or $_ -match ',1$' }).Count
}
if ($class -eq 'C_or_D' -or ($primary.fmt_mapped -and $paramParsed -gt 0 -and $paramGlRead -eq 0 -and -not $primary.cfg36_present)) {
  if (-not $primary.cfg_opened) { $class = 'A' }
  elseif ($paramParsed -gt 0 -and $paramGlRead -eq 0) { $class = 'C' }
}

$p20ok = ($primary.gbrwcore -and $primary.br_exit_continue -and $primary.gamelist -and
          $primary.erw_iso -and ($primary.fault30 -eq 0) -and ($primary.force10140 -eq 0))

@"
# P21 cfg36 Selection Verdict

## Bottom line

**Class: $class**
P20-CLEAN freeze held: $p20ok
Primary evidence lane: $($primary.tag) (research_assisted=$($primary.research_assisted) product_valid=$($primary.product_valid))

## Evidence tiers

| Lane | fast_assist | gamelist | ERW iso | FIRE_EXT | cfg_open | cfg36_present | cfg36_selected |
|------|-------------|----------|---------|----------|----------|---------------|----------------|
| A natural | $($primary.FAST_BD0_INIT_CALL)/$($primary.FAST_PROGRESS_TICK_CALL) | $($primary.gamelist) | $($primary.erw_iso) | $($primary.fire_ext_n) | $($primary.cfg_opened) | $($primary.cfg36_present) | $($primary.cfg36_selected) |
$(if ($b) { "| B assisted | $($b.FAST_BD0_INIT_CALL)/$($b.FAST_PROGRESS_TICK_CALL) | $($b.gamelist) | $($b.erw_iso) | $($b.fire_ext_n) | $($b.cfg_opened) | $($b.cfg36_present) | $($b.cfg36_selected) |" } else { '' })

Same cfg stop A/B: $sameStop

## Five gates (must not collapse)

| Gate | Pass |
|------|------|
| CFG_FMT_MAPPED | $($primary.fmt_mapped) |
| CFG_FILE_OPENED | $($primary.cfg_opened) |
| CFG_RECORD_READ | $($primary.cfg_record_read) |
| CFG36_RECORD_PRESENT | $($primary.cfg36_present) |
| CFG36_SELECTED | $($primary.cfg36_selected) |

## P20 freeze checks

| Check | Pass |
|-------|------|
| gbrwcore entry | $($primary.gbrwcore) |
| br_exit CONTINUE | $($primary.br_exit_continue) |
| gamelist entered | $($primary.gamelist) |
| gamelist ERW isolated | $($primary.erw_iso) |
| 0x30D5D2 fault = 0 | $($primary.fault30 -eq 0) |
| forced 10140 = 0 | $($primary.force10140 -eq 0) |
| MRPGCMAP EMU_OK | $($primary.entry_ok) |

## Acceptance answers

``````text
无 fast assist 是否仍进入 gamelist：$aGl
无 fast assist 是否仍到达相同 cfg 停点：$aCfgStop

真实 cfg 数据源：$(if ($primary.cfg_opened) { 'see p21_cfg_file_io.csv / 10112' } else { 'NONE_OPENED' })
真实打开路径：$(Read-CsvHint (Join-Path $reportDir 'p21_cfg_file_io.csv') 'path')
cfg 列表记录数：$(if ($primary.p21_final -match 'records=(\d+)') { $Matches[1] } else { '0_or_unknown' })
是否存在完整 cfg36 record：$([bool]$primary.cfg36_present)
cfg36 Guest 地址：$(if ($primary.cfg36_va_line -match 'guest=(0x[0-9A-Fa-f]+)') { $Matches[1] } else { 'N/A' })
cfg36 source offset：$(if ($primary.cfg36_va_line -match 'src_off=(\d+)') { $Matches[1] } else { 'N/A' })

启动参数是否被 cfunction 解析：$(if ($paramParsed -gt 0) { 'PARTIAL_OR_YES' } else { 'NO_OR_UNOBSERVED' })
解析结果写入地址：see reports/p21_launch_param_provenance.csv
gamelist 是否读取解析结果：$(if ($paramGlRead -gt 0) { 'YES' } else { 'NO_OR_UNOBSERVED' })
gwyblink 的真实语义：observe_only_see_param_csv

cfg36 选择谓词：$(if ($primary.cfg36_present) { 'see selection_branches.csv' } else { 'N/A_no_record' })
当前失败的第一项谓词：$(if (-not $primary.cfg_opened) { 'CFG_FILE_OPENED' } elseif (-not $primary.cfg_record_read) { 'CFG_RECORD_READ' } elseif (-not $primary.cfg36_present) { 'CFG36_RECORD_PRESENT' } elseif (-not $primary.cfg36_selected) { 'CFG36_SELECTED' } else { 'none' })
负责满足该谓词的自然生产者：TBD_from_csv
是否等待真实用户输入：$(if ($class -match 'E') { 'LIKELY' } else { 'UNKNOWN' })

cfg36 是否由 Guest 自然选中：$([bool]$primary.cfg36_selected)
selected state 写入 PC：$(if ($primary.cfg36_selected) { 'see selection_branches.csv' } else { 'N/A' })
post-select 第一个真实行为：$(if ($primary.cfg36_selected) { 'see log' } else { 'N/A' })
是否出现 Guest startGame 调用：$([bool]$primary.startgame_call)
当前唯一门锁：$class
``````

## Notes

- ``SHELL_PHASE_CFG_FMT_MAPPED`` is **not** evidence of cfg36 load/select.
- G6b (dynamic startGame) remains deferred until CFG36_SELECTED.
- Lane B is ``research_assisted=yes product_valid=no`` if fast assist was on.

## Artifacts

- reports/p21_cfg36_selection_verdict.md
- reports/p21_cfg_file_io.csv
- reports/p21_cfg_record_inventory.csv
- reports/p21_launch_param_provenance.csv
- reports/p21_cfg_selection_branches.csv
- reports/p21_timer_state_diff.csv
- out/p21/p21_build_identity.txt
- research/runners/p21_run_cfg36_selection.ps1
"@ | Set-Content $verdict -Encoding utf8

Copy-Item $verdict (Join-Path $outDir 'p21_cfg36_selection_verdict.md') -Force -EA SilentlyContinue
Add-Content $identity -Encoding utf8 -Value @"
verdict_class=$class
primary_lane=$($primary.tag)
p20_freeze_ok=$p20ok
gamelist=$($primary.gamelist)
cfg_opened=$($primary.cfg_opened)
cfg36_present=$($primary.cfg36_present)
cfg36_selected=$($primary.cfg36_selected)
"@

Write-Host "=== P21 done class=$class primary=$($primary.tag) ==="
Get-Content $verdict | Select-Object -First 60
