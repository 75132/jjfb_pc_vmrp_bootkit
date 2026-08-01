# P17 — post-Case9 lifecycle + first genuine natural lock.
# Keeps P16 defer-drain + nest guard; long-window product runs; shell compare.
param(
  [int]$Seconds = 180,
  [int]$ShellSeconds = 90,
  [switch]$SkipBuild,
  [switch]$SkipShell
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\p17'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$matrix = Join-Path $reportDir 'p17_genuine_call_matrix.csv'
$longMatrix = Join-Path $reportDir 'p17_long_run_matrix.csv'
$shellCmp = Join-Path $reportDir 'p17_shell_direct_lifecycle_compare.csv'
$identity = Join-Path $outDir 'p17_build_identity.txt'
$verdict = Join-Path $reportDir 'p17_post_case9_lifecycle_verdict.md'
$timeline = Join-Path $outDir 'p17_post_case9_timeline.csv'
$hotspots = Join-Path $outDir 'p17_hotspots.txt'

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

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  $guard = Join-Path $Root 'build-i686\test_nested_emu_guard.exe'
  if (Test-Path $guard) {
    & $guard
    if ($LASTEXITCODE -ne 0) { throw 'test_nested_emu_guard failed' }
  }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
}

$sourceCommit = (git rev-parse HEAD).Trim()
$mainSha = Get-Sha $exe
$jjfbSha = Get-Sha $JJFB
$gwySha = Get-Sha $Launcher
$compiler = & gcc -dumpversion 2>$null
@"
source_commit=$sourceCommit
source_tree_clean=$(-not [bool]@(git status --porcelain -- 'src' 'include' 'third_party/vmrp_upstream/bridge.c' 'third_party/vmrp_upstream/header/gwy_ext_obs_abi.h' 'third_party/vmrp_upstream/gwy_ext_obs_weak.c' 'CMakeLists.txt' 'tests'))
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
main_exe_sha256=$mainSha
JJFB_Launcher_exe_sha256=$jjfbSha
gwy_launcher_exe_sha256=$gwySha
compiler=gcc-$compiler
product_default_return_mode=direct_lr
JJFB_BRIDGE_ENTRY_PROV=1
gate=P17_post_case9_lifecycle
guard=nested_emu_in_code_hook_blocked
fix=p16_defer_drain+p17_depth_guard
seconds=$Seconds
"@ | Set-Content $identity -Encoding utf8
Write-Host '=== P17 identity ==='; Get-Content $identity

function Invoke-P17Run([string]$tag, [int]$seconds) {
  Clear-CaseEnv
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
  $runId = ('{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $tag, (Get-Date), (Get-Random -Maximum 99999))
  $stdout = Join-Path $logDir ("{0}_stdout.txt" -f $tag)
  $stderr = Join-Path $logDir ("{0}_stderr.txt" -f $tag)
  $vmLog = Join-Path $logDir ("{0}_vmrp.txt" -f $tag)
  @($stdout, $stderr, $vmLog) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }
  $overlay = Join-Path $RunDir ("overlay_$runId")
  New-Item -ItemType Directory -Force -Path $overlay | Out-Null
  $param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'

  $env:GWY_PROFILE = $Profile
  $env:GWY_OVERLAY_ROOT = $overlay
  $env:GWY_PRODUCT_REPORTS_DIR = $reportDir
  $env:GWY_PRODUCT_RUN_ID = $runId
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_RUNAPP_NATIVE_ONLY = '0'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:JJFB_MODULE_REGISTRY_TRACE = '1'
  $env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
  $env:JJFB_MRC_INIT_TRACE = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_PRODUCT_P3_MODE = '1'
  $env:JJFB_PRODUCT_P4_MODE = '1'
  $env:JJFB_PRODUCT_FFP_MODE = '1'
  $env:JJFB_PRODUCT_FFP_PHASE = 'event'
  $env:JJFB_HWND_UNTIL_DISPUP = '1'
  $env:JJFB_VISIBLE_WINDOW = '1'
  $env:JJFB_E9B_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = $param
  $env:JJFB_BRIDGE_ENTRY_PROV = '1'
  $env:JJFB_BRIDGE_ENTRY_PROV_DIR = $outDir
  Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_EVENT_CONTRACT -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAMILY_4F_FOR_E6C -ErrorAction SilentlyContinue

  @('bridge_entry_provenance.csv','bridge_predecessor_ring.csv','bridge_insn_ring.csv','bridge_nest_audit.csv') | ForEach-Object {
    Remove-Item -Force (Join-Path $outDir $_) -EA SilentlyContinue
  }

  $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @(
    '/c',
    ('cd /d "{0}" && "{1}" > "{2}" 2> "{3}"' -f $RunDir, $exe, $vmLog, $stderr)
  ) -PassThru
  $deadline = (Get-Date).AddSeconds($seconds)
  do {
    Start-Sleep -Seconds 3
    if (Test-Path $vmLog) {
      Get-Content $vmLog -Tail 800 -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8
    }
  } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
  if (-not $p.HasExited) {
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 400
  }
  if (Test-Path $vmLog) { Get-Content $vmLog -EA SilentlyContinue | Out-File $stdout -Encoding utf8 }
  return @{ runId = $runId; stdout = $stdout; vmLog = $vmLog; exit = $(if ($p.HasExited) { "$($p.ExitCode)" } else { 'killed' }) }
}

function Analyze-P17([string]$vmPath) {
  $vm = if (Test-Path $vmPath) { Get-Content $vmPath -Raw -EA SilentlyContinue } else { '' }
  if (-not $vm) { $vm = '' }

  $case9Deliver = ([regex]::Matches($vm, 'op=DELIVER[^\n]*handler=0x30D311')).Count
  $case9Done = ([regex]::Matches($vm, 'op=DELIVER_DONE[^\n]*handler=0x30D311')).Count
  $reachedStop = ([regex]::Matches($vm, 'handler=0x30D311[^\n]*reached_stop=1|reached_stop=1[^\n]*handler=0x30D311|end_class=REACHED_STOP[^\n]*handler=0x30D311|handler=0x30D311[^\n]*end_class=REACHED_STOP')).Count
  $case9Leave = ([regex]::Matches($vm, 'CASE9_LEAVE[^\n]*ok=1|CASE9_LEAVE[^\n]*reached_stop=1')).Count
  $defer = ([regex]::Matches($vm, 'SCHEDULE_DRAIN_OUTSIDE_HOOK')).Count
  $drainOut = ([regex]::Matches($vm, 'DRAIN_OUTSIDE_HOOK')).Count
  $nestedBlock = ([regex]::Matches($vm, 'NESTED_EMU_IN_CODE_HOOK_BLOCKED')).Count
  $dataTrap = ([regex]::Matches($vm, 'BRIDGE_DATA_EXEC_TRAP')).Count
  $fallthrough = ([regex]::Matches($vm, 'LINEAR_SLOT_FALLTHROUGH')).Count
  $mrPlat = ([regex]::Matches($vm, 'api=mr_plat\s')).Count
  $call10133 = ([regex]::Matches($vm, 'r0=0x10133|code=0x10133|event=0x10133')).Count
  $evt1e209 = ([regex]::Matches($vm, 'event=0x1E209')).Count
  $timerEnq = ([regex]::Matches($vm, 'timerStart|TIMER_ARM|op=TIMER_ENQUEUE|mr_timerStart')).Count
  $timerFire = ([regex]::Matches($vm, 'TIMER_FIRE|timer.?fire|op=FIRE')).Count
  $familyEnq = ([regex]::Matches($vm, 'PLATFORM_FAMILY_EVENT\] op=ENQUEUE')).Count
  $familyDrain = ([regex]::Matches($vm, 'DRAIN_OUTSIDE_HOOK|FAMILY_DRAIN_ENTER')).Count
  $lifeHelper = ([regex]::Matches($vm, '0x306762|lifecycle.?helper|LIFECYCLE_DELIVER|op=LIFECYCLE')).Count
  $modSwitch = ([regex]::Matches($vm, 'MODULE_R9_SWITCH|module.?switch|owner_module=')).Count
  $dispUp = ([regex]::Matches($vm, 'DispUp|mr_dispUp|REFRESH|note_product_refresh')).Count
  $draw = ([regex]::Matches($vm, 'mr_drawBitmap|note_product_draw|DRAW ')).Count
  $net = ([regex]::Matches($vm, 'mr_socket|mr_connect|network|TCP|HTTP')).Count
  $dsm = ([regex]::Matches($vm, 'DSM.?reinit|dsm_reinit|START_DSM')).Count
  $fault = ([regex]::Matches($vm, 'UC_FAULT|FETCH_UNMAPPED|mem_fault|GUEST_FAULT')).Count
  $bmp = ([regex]::Matches($vm, '\.bmp|BMP_|splash', 'IgnoreCase')).Count
  $layer1 = ([regex]::Matches($vm, 'Layer-1|layer1|LAYER1', 'IgnoreCase')).Count
  $frameSha = ([regex]::Matches($vm, 'sha256=|frame_sha|framebuffer')).Count
  $naturalExit = ([regex]::Matches($vm, 'mr_exit|NATURAL_EXIT|process.?exit')).Count

  $genuine = @()
  $genuineMatches = [regex]::Matches($vm, '\[JJFB_BRIDGE_ENTRY_PROV\][^\n]*api=(\S+)[^\n]*class=(GENUINE_\w+)')
  foreach ($m in $genuineMatches) { $genuine += "$($m.Groups[1].Value)/$($m.Groups[2].Value)" }
  $genuine = $genuine | Select-Object -Unique

  $firstGenuine = if ($genuine.Count) { $genuine[0] } else { '' }

  # Post-case9 snippet: from first CASE9 leave / DELIVER_DONE
  $postIdx = $vm.IndexOf('handler=0x30D311')
  $post = if ($postIdx -ge 0) { $vm.Substring($postIdx, [Math]::Min(200000, $vm.Length - $postIdx)) } else { '' }
  $postGenuine = ([regex]::Matches($post, 'class=GENUINE_')).Count
  $post1e209 = ([regex]::Matches($post, 'event=0x1E209')).Count
  $postLife = ([regex]::Matches($post, '0x306762|LIFECYCLE|lifecycle')).Count

  return [pscustomobject]@{
    case9_deliver = $case9Deliver
    case9_done = $case9Done
    reached_stop = $reachedStop
    case9_leave = $case9Leave
    defer_drain = $defer
    drain_outside = $drainOut
    nested_block = $nestedBlock
    data_trap = $dataTrap
    fallthrough = $fallthrough
    mr_plat = $mrPlat
    call_10133 = $call10133
    evt_1e209 = $evt1e209
    timer_enq = $timerEnq
    timer_fire = $timerFire
    family_enq = $familyEnq
    family_drain = $familyDrain
    life_helper = $lifeHelper
    mod_switch = $modSwitch
    disp_up = $dispUp
    draw = $draw
    net = $net
    dsm = $dsm
    fault = $fault
    bmp = $bmp
    layer1 = $layer1
    frame_sha = $frameSha
    natural_exit = $naturalExit
    first_genuine = $firstGenuine
    genuine_list = ($genuine -join ';')
    genuine_count = $genuine.Count
    post_genuine = $postGenuine
    post_1e209 = $post1e209
    post_life = $postLife
  }
}

Write-Host "=== P17 long runs x3 (${Seconds}s) same binary ==="
$results = @()
for ($i = 1; $i -le 3; $i++) {
  Write-Host ("=== P17 hit{0} ===" -f $i)
  $hit = Invoke-P17Run ("p17_hit{0}" -f $i) $Seconds
  $a = Analyze-P17 $hit.vmLog
  $row = [pscustomobject]@{
    run = "hit$i"
    runId = $hit.runId
    exit = $hit.exit
    case9_deliver = $a.case9_deliver
    case9_done = $a.case9_done
    reached_stop = $a.reached_stop
    nested_block = $a.nested_block
    data_trap = $a.data_trap
    fallthrough = $a.fallthrough
    mr_plat = $a.mr_plat
    call_10133 = $a.call_10133
    evt_1e209 = $a.evt_1e209
    timer_enq = $a.timer_enq
    timer_fire = $a.timer_fire
    family_enq = $a.family_enq
    family_drain = $a.family_drain
    life_helper = $a.life_helper
    disp_up = $a.disp_up
    draw = $a.draw
    net = $a.net
    fault = $a.fault
    bmp = $a.bmp
    layer1 = $a.layer1
    first_genuine = $a.first_genuine
    genuine_count = $a.genuine_count
    post_genuine = $a.post_genuine
    post_1e209 = $a.post_1e209
  }
  $results += $row
  $a | Format-List | Out-String | Write-Host
}

# Long-run matrix CSV
$results | Export-Csv -Path $longMatrix -NoTypeInformation -Encoding utf8

# Genuine-call matrix from last hit
$lastVm = Join-Path $logDir 'p17_hit3_vmrp.txt'
if (-not (Test-Path $lastVm)) { $lastVm = Join-Path $logDir 'p17_hit1_vmrp.txt' }
$vmText = if (Test-Path $lastVm) { Get-Content $lastVm -Raw } else { '' }
@"
seq,api,slot,class,r0,r1,lr,branch,branch_pc,args_valid,note,post_case9
"@ | Set-Content $matrix -Encoding utf8
$case9Pos = $vmText.IndexOf('handler=0x30D311')
$seq = 0
[regex]::Matches($vmText, '\[JJFB_BRIDGE_ENTRY_PROV\] seq=(\d+) api=(\S+) slot=(0x[0-9A-Fa-f]+) class=(\w+)[^\n]*\blr=(0x[0-9A-Fa-f]+)[^\n]*\br0=(0x[0-9A-Fa-f]+)[^\n]*\br1=(0x[0-9A-Fa-f]+)[^\n]*branch=(\S+)[^\n]*branch_pc=(0x[0-9A-Fa-f]+)[^\n]*args_valid=(\d+)') | ForEach-Object {
  $seq++
  $cls = $_.Groups[4].Value
  $note = if ($cls -match '^GENUINE_') { 'TRUE_GUEST_CALL' } elseif ($cls -match 'FALLTHROUGH|TABLE_DATA|STALE') { 'FALSE_OR_TRAP' } else { 'OTHER' }
  $pos = $_.Index
  $post = if ($case9Pos -ge 0 -and $pos -gt $case9Pos) { 1 } else { 0 }
  '{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11}' -f `
    $_.Groups[1].Value, $_.Groups[2].Value, $_.Groups[3].Value, $cls, `
    $_.Groups[6].Value, $_.Groups[7].Value, $_.Groups[5].Value, $_.Groups[8].Value, `
    $_.Groups[9].Value, $_.Groups[10].Value, $note, $post | Add-Content $matrix -Encoding utf8
}

# Timeline from hit1 around Case-9
@"
sequence,time_from_case9,operation,detail
"@ | Set-Content $timeline -Encoding utf8
$tlSeq = 0
$hit1 = Join-Path $logDir 'p17_hit1_vmrp.txt'
if (Test-Path $hit1) {
  $lines = Get-Content $hit1
  $t0 = -1
  for ($li = 0; $li -lt $lines.Count; $li++) {
    $line = $lines[$li]
    if ($t0 -lt 0 -and $line -match 'CASE9_LEAVE|DELIVER_DONE[^\n]*handler=0x30D311|reached_stop=1[^\n]*0x30D311') {
      $t0 = $li
    }
    if ($t0 -ge 0 -and $li -ge $t0 -and $li -lt ($t0 + 4000)) {
      if ($line -match 'PLATFORM_FAMILY_EVENT|CALLBACK_FRAME|EMU_NEST|JJFB_BRIDGE_ENTRY_PROV|TIMER|LIFECYCLE|0x306762|0x1E209|0x10133|CASE9|DispUp|drawBitmap|NESTED_EMU') {
        $tlSeq++
        $op = ($line -replace '^\[', '' -replace '\].*', '')
        $det = ($line -replace '"', '''')
        if ($det.Length -gt 220) { $det = $det.Substring(0, 220) }
        '{0},{1},{2},"{3}"' -f $tlSeq, ($li - $t0), $op, $det | Add-Content $timeline -Encoding utf8
      }
    }
  }
}

# Hotspots (heuristic from log PC mentions if no genuine post-case9)
$aLast = Analyze-P17 $lastVm
$hot = New-Object System.Collections.Generic.List[string]
$hot.Add("genuine_count=$($aLast.genuine_count) post_genuine=$($aLast.post_genuine) post_1e209=$($aLast.post_1e209)")
if ($aLast.post_genuine -eq 0) {
  $pcHits = @{}
  [regex]::Matches($vmText, 'pc(_after)?=(0x[0-9A-Fa-f]+)') | ForEach-Object {
    $pc = $_.Groups[2].Value.ToUpper()
    if (-not $pcHits.ContainsKey($pc)) { $pcHits[$pc] = 0 }
    $pcHits[$pc]++
  }
  $hot.Add('--- top PC mentions (proxy hotspot) ---')
  $pcHits.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 64 | ForEach-Object {
    $hot.Add(('{0} count={1}' -f $_.Key, $_.Value))
  }
  $loopClass = 'UNKNOWN'
  if ($aLast.timer_enq -gt 0 -and $aLast.post_1e209 -gt 2) { $loopClass = 'A_NORMAL_WAIT_OR_TIMER_CYCLE' }
  elseif ($aLast.evt_1e209 -gt 10 -and $aLast.genuine_count -eq 0) { $loopClass = 'B_BUSY_OR_STATELESS_1E209' }
  elseif ($aLast.timer_enq -gt 0 -and $aLast.timer_fire -eq 0) { $loopClass = 'C_HOST_TIMER_PUMP_GAP' }
  elseif ($aLast.natural_exit -gt 0 -and $aLast.post_1e209 -eq 0) { $loopClass = 'D_MODULE_IDLE_EXIT' }
  $hot.Add("loop_class=$loopClass")
}
$hot | Set-Content $hotspots -Encoding utf8

# Shell compare: research suite + product hit1 (rebuild research separately so product binary stays fixed for 3 hits)
$shellNote = 'skipped'
$da = $results[0]
if (-not $SkipShell) {
  Write-Host "=== P17 research shell suite (short) ==="
  try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_RESEARCH_GWY_SHELL.ps1') `
      -SkipBuild -ShortSeconds ([Math]::Min(20, $ShellSeconds))
    $shellNote = "RUN_RESEARCH_GWY_SHELL_exit=$LASTEXITCODE"
  } catch {
    $shellNote = "shell_suite_failed:$_"
  }
  # Restore product Gwy main.exe after research rebuild
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
}

# Prefer live research log if present; else p16 shell baseline notes
$p16Shell = Join-Path $reportDir 'p16_original_shell_callback_compare.csv'
@"
item,original_shell,product_direct
case9_period_proxy,see_research_or_p16,$($da.case9_done)
evt_1e209,shell_or_p16,$($da.evt_1e209)
call_10133,shell_or_p16,$($da.call_10133)
life_helper,shell_or_p16,$($da.life_helper)
timer_enq,shell_or_p16,$($da.timer_enq)
timer_fire,shell_or_p16,$($da.timer_fire)
first_genuine,shell_or_p16,$($da.first_genuine)
genuine_count,shell_or_p16,$($da.genuine_count)
nested_block,expected_0,$($da.nested_block)
fallthrough,expected_0,$($da.fallthrough)
parent_scheduler_gap,p16_notes_wrapper_vs_direct,deferred_drain_outside_hook
shell_note,$shellNote,hit1=$($da.runId)
p16_shell_csv,$(if (Test-Path $p16Shell) {'present'} else {'missing'}),n/a
"@ | Set-Content $shellCmp -Encoding utf8

# BMP / Layer-1 adjudication
$bmpState = 'NOT_REACHED'
$layerState = 'NOT_REACHED'
if (($results | Measure-Object -Property bmp -Sum).Sum -gt 0) { $bmpState = 'PASS_OR_SEEN' }
if (($results | Measure-Object -Property layer1 -Sum).Sum -gt 0) { $layerState = 'PASS_OR_SEEN' }
# Prefer NOT_REACHED unless explicit splash path — do not call REGRESSION without prior P17 baseline hit

$passReached = ($results | Where-Object { $_.reached_stop -gt 0 }).Count
$anyNested = ($results | Measure-Object -Property nested_block -Sum).Sum
$anyFall = ($results | Measure-Object -Property fallthrough -Sum).Sum
$anyData = ($results | Measure-Object -Property data_trap -Sum).Sum
$firstG = ($results | ForEach-Object { $_.first_genuine } | Where-Object { $_ } | Select-Object -First 1)
if (-not $firstG) { $firstG = '(none)' }
$loopLine = (Get-Content $hotspots | Select-String 'loop_class=' | Select-Object -First 1)
$loopClass = if ($loopLine) { ($loopLine -replace '.*loop_class=', '') } else { 'UNKNOWN' }

$lock = if ($firstG -ne '(none)') { "first_GENUINE_API:$firstG" }
  elseif ($loopClass -match 'C_HOST') { 'missing_timer_event_pump' }
  elseif ($loopClass -match 'D_MODULE') { 'missing_parent_shell_lifecycle' }
  elseif ($loopClass -match 'A_NORMAL') { 'waiting_external_event_after_timer_cycle' }
  else { 'post_case9_no_genuine_api_yet' }

@"
# P17 — Post-Case9 Lifecycle Verdict

## Verdict inputs

- runs: 3 x ${Seconds}s same binary
- nested guard: permanent ``hook_depth`` / ``guest_run_depth`` / ``family_drain_depth``
- shell: $shellNote

## Long-run matrix

See ``reports/p17_long_run_matrix.csv``.

| run | reached_stop | nested_block | data_trap | fallthrough | 1E209 | 10133 | genuine |
|-----|--------------|--------------|-----------|-------------|-------|-------|---------|
$(($results | ForEach-Object { "| $($_.run) | $($_.reached_stop) | $($_.nested_block) | $($_.data_trap) | $($_.fallthrough) | $($_.evt_1e209) | $($_.call_10133) | $($_.first_genuine) |" }) -join "`n")

## PASS answers

``````
Case-9 是否持续 REACHED_STOP：$passReached / 3 runs with reached_stop>0
是否再次发生 Hook 内嵌套 emu：blocked_events=$anyNested (must stay 0 if defer path healthy)
MAP_DATA / fallthrough 是否保持为 0：data_trap=$anyData fallthrough=$anyFall
Case-9 后真实 lifecycle 路径：see out/p17/p17_post_case9_timeline.csv
重复 0x1E209 是否正常：loop_class=$loopClass post_1e209=$($aLast.post_1e209)
第一个真实 Guest 平台 API：$firstG
是否存在稳定等待/忙循环：$loopClass
是否缺 timer/event pump：$((if ($loopClass -match 'C_HOST') {'YES'} else {'NO_OR_UNCLEAR'}))
是否缺原冒泡父级 lifecycle：$((if ($loopClass -match 'D_MODULE') {'LIKELY'} else {'NO_OR_UNCLEAR'}))
五 BMP 状态：$bmpState
Layer-1 状态：$layerState
是否出现真实游戏画面：$((if (($results | Measure-Object -Property draw -Sum).Sum -gt 0) {'possible_draw_events'} else {'no'}))
当前唯一自然门锁：$lock
下一处最小通用平台缺口：$lock
``````

## Artifacts

- ``out/p17/p17_build_identity.txt``
- ``reports/p17_genuine_call_matrix.csv``
- ``reports/p17_shell_direct_lifecycle_compare.csv``
- ``reports/p17_long_run_matrix.csv``
- ``out/p17/p17_post_case9_timeline.csv``
- ``out/p17/p17_hotspots.txt``
"@ | Set-Content $verdict -Encoding utf8

Write-Host '=== P17 done ==='
Write-Host "verdict=$verdict"
Get-Content $verdict | Select-Object -First 80
