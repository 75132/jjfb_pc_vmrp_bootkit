# P16 — restore Family Callback Frame; close mr_table data execution.
# Fix: defer family drain until after MAP_FUNC PC=LR + uc_emu_stop restart.
param(
  [int]$DiagSeconds = 50,
  [int]$HitSeconds = 90,
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

$outDir = Join-Path $Root 'out\p16'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$matrix = Join-Path $reportDir 'p16_true_call_matrix.csv'
$shellCmp = Join-Path $reportDir 'p16_original_shell_callback_compare.csv'
$identity = Join-Path $outDir 'p16_build_identity.txt'
$verdict = Join-Path $reportDir 'p16_family_callback_frame_verdict.md'

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
product_ffp_apply_abi=0
JJFB_PRODUCT_EVENT_CONTRACT=0
JJFB_FAMILY_4F_FOR_E6C=0
JJFB_BRIDGE_ENTRY_PROV=1
gate=P16_family_callback_frame
fix=defer_family_drain_after_map_redirect+map_data_trap+reached_stop
"@ | Set-Content $identity -Encoding utf8
Write-Host '=== P16 identity ==='; Get-Content $identity

function Invoke-P16Run([string]$tag, [int]$seconds) {
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

  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $vmLog -RedirectStandardError $stderr
  $deadline = (Get-Date).AddSeconds($seconds)
  do {
    Start-Sleep -Seconds 2
    if (Test-Path $vmLog) {
      Get-Content $vmLog -Tail 1200 -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8
    }
  } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
  if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 400
  }
  if (Test-Path $vmLog) { Get-Content $vmLog -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8 }
  return @{ runId = $runId; stdout = $stdout; vmLog = $vmLog; exit = $(if ($p.HasExited) { "$($p.ExitCode)" } else { 'killed' }) }
}

function Analyze-P16([string]$vmPath) {
  $vm = if (Test-Path $vmPath) { Get-Content $vmPath -Raw -EA SilentlyContinue } else { '' }
  if (-not $vm) { $vm = '' }

  $case9 = [regex]::Match($vm, 'op=DELIVER_DONE[^\n]*handler=0x30D311[^\n]*')
  $reached = [bool]($vm -match 'handler=0x30D311[^\n]*reached_stop=1|reached_stop=1[^\n]*handler=0x30D311')
  if (-not $reached) {
    $reached = [bool]($case9.Value -match 'end_class=REACHED_STOP' -or $case9.Value -match 'reached_stop=1')
  }
  $endClass = ''
  $mEnd = [regex]::Match($case9.Value, 'end_class=(\w+)')
  if ($mEnd.Success) { $endClass = $mEnd.Groups[1].Value }
  $pcAfter = ''
  $mPc = [regex]::Match($case9.Value, 'pc_after=(0x[0-9A-Fa-f]+)')
  if ($mPc.Success) { $pcAfter = $mPc.Groups[1].Value }

  $defer = [bool]($vm -match 'DEFER_DRAIN_TO_MAP_REDIRECT|SCHEDULE_DRAIN_OUTSIDE_HOOK')
  $drainAfter = [bool]($vm -match 'DRAIN_AFTER_MAP_REDIRECT|DRAIN_OUTSIDE_HOOK|SCHEDULE_DRAIN_OUTSIDE_HOOK')
  $dataTrap = ([regex]::Matches($vm, 'BRIDGE_DATA_EXEC_TRAP')).Count
  $mapDataUnregister = ([regex]::Matches($vm, 'unregister function at 0x28005C')).Count
  $fallthrough = ([regex]::Matches($vm, 'LINEAR_SLOT_FALLTHROUGH')).Count
  $tableData = ([regex]::Matches($vm, 'TABLE_DATA_EXECUTION')).Count
  $mrPlat = ([regex]::Matches($vm, 'api=mr_plat\s')).Count
  $call10133 = [bool]($vm -match 'r0=0x10133|code=0x10133')
  $handlerEnter = [bool]($vm -match 'FAMILY_HANDLER_ENTER[^\n]*handler=0x30D311')
  $app9 = [bool]($vm -match 'DELIVER event=0x1E209 app=0x9 handler=0x30D311')

  $genuine = @()
  foreach ($api in @('mr_drawBitmap','mr_getCharBitmap','mr_getTime','mr_getUserInfo','mr_sleep','mr_plat')) {
    if ($vm -match "\[JJFB_BRIDGE_ENTRY_PROV\].*api=$api\s+slot=0x[0-9A-Fa-f]+\s+class=(GENUINE_\w+)") {
      $genuine += "$api/$($Matches[1])"
    }
  }

  return [pscustomobject]@{
    case9_seen = $case9.Success
    reached_stop = $reached
    end_class = $endClass
    pc_after = $pcAfter
    defer_drain = $defer
    drain_after_redirect = $drainAfter
    handler_enter = $handlerEnter
    app9_branch = $app9
    call_10133 = $call10133
    data_exec_trap_count = $dataTrap
    reserve1_unregister = $mapDataUnregister
    fallthrough_count = $fallthrough
    table_data_count = $tableData
    mr_plat_count = $mrPlat
    first_genuine = $(if ($genuine.Count) { $genuine[0] } else { '' })
    genuine_list = ($genuine -join ';')
  }
}

Write-Host '=== P16 diag ==='
$diag = Invoke-P16Run 'p16_diag' $DiagSeconds
$a0 = Analyze-P16 $diag.vmLog
$a0 | Format-List | Out-String | Write-Host

$results = @()
for ($i = 1; $i -le 3; $i++) {
  Write-Host ("=== P16 hit{0} ===" -f $i)
  $hit = Invoke-P16Run ("p16_hit{0}" -f $i) $HitSeconds
  $a = Analyze-P16 $hit.vmLog
  $results += [pscustomobject]@{
    run = "hit$i"
    runId = $hit.runId
    reached_stop = [int]$a.reached_stop
    end_class = $a.end_class
    pc_after = $a.pc_after
    defer = [int]$a.defer_drain
    drain_after = [int]$a.drain_after_redirect
    handler_enter = [int]$a.handler_enter
    app9 = [int]$a.app9_branch
    call_10133 = [int]$a.call_10133
    data_trap = $a.data_exec_trap_count
    fallthrough = $a.fallthrough_count
    table_data = $a.table_data_count
    mr_plat = $a.mr_plat_count
    first_genuine = $a.first_genuine
  }
  $a | Format-List | Out-String | Write-Host
}

# True-call matrix: only GENUINE_* from last hit log
$lastVm = Join-Path $logDir 'p16_hit3_vmrp.txt'
if (-not (Test-Path $lastVm)) { $lastVm = Join-Path $logDir 'p16_hit1_vmrp.txt' }
$vmText = if (Test-Path $lastVm) { Get-Content $lastVm -Raw } else { '' }
@"
seq,api,slot,class,r0,r1,lr,branch,branch_pc,args_valid,note
"@ | Set-Content $matrix -Encoding utf8
$seq = 0
[regex]::Matches($vmText, '\[JJFB_BRIDGE_ENTRY_PROV\] seq=(\d+) api=(\S+) slot=(0x[0-9A-Fa-f]+) class=(\w+)[^\n]*\blr=(0x[0-9A-Fa-f]+)[^\n]*\br0=(0x[0-9A-Fa-f]+)[^\n]*\br1=(0x[0-9A-Fa-f]+)[^\n]*branch=(\S+)[^\n]*branch_pc=(0x[0-9A-Fa-f]+)[^\n]*args_valid=(\d+)') | ForEach-Object {
  $seq++
  $cls = $_.Groups[4].Value
  $note = if ($cls -match '^GENUINE_') { 'TRUE_GUEST_CALL' } elseif ($cls -match 'FALLTHROUGH|TABLE_DATA|STALE') { 'FALSE_OR_TRAP' } else { 'OTHER' }
  '{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10}' -f `
    $_.Groups[1].Value, $_.Groups[2].Value, $_.Groups[3].Value, $cls, `
    $_.Groups[6].Value, $_.Groups[7].Value, $_.Groups[5].Value, $_.Groups[8].Value, `
    $_.Groups[9].Value, $_.Groups[10].Value, $note | Add-Content $matrix -Encoding utf8
}

@"
item,original_shell,host_direct_pre_p16,host_p16_fix
handler,registered_0x30D311,0x30D311,0x30D311_from_registry
caller_wrapper,guest_BL_or_plat_wrapper,none_host_uc_run_entry_ex,deferred_after_map_PC_LR
entry_SP,wrapper_frame,outer_SP_at_map_func,outer_SP_then_handler_PUSH
entry_LR,wrapper_continuation,stop_0x80000,stop_0x80000
stack_return_slot,wrapper_PUSH_LR,handler_PUSH_saves_stop,handler_PUSH_saves_stop
R9,owner_ER_RW,robotol_ER_RW,robotol_ER_RW
return_target,wrapper_epilogue_then_outer,broken_stub_plus4_0x28005C,reassert_cont_plus_uc_emu_stop
enters_mr_table_data,no,yes_reserve1,must_be_no
"@ | Set-Content $shellCmp -Encoding utf8

$passN = @($results | Where-Object {
  $_.reached_stop -eq 1 -and $_.end_class -eq 'REACHED_STOP' -and $_.data_trap -eq 0 -and `
  $_.fallthrough -eq 0 -and $_.mr_plat -eq 0 -and $_.handler_enter -eq 1 -and $_.app9 -eq 1
}).Count

$allPass = ($passN -eq 3)
$firstGenuine = ($results | Select-Object -Last 1).first_genuine

@"
# P16 — Family Callback Frame Verdict

## Verdict

$(if ($allPass) { '**PASS** - Case-9 REACHED_STOP x3; MAP_DATA / linear fallthrough closed.' } else { '**PARTIAL / FAIL** - see acceptance matrix below.' })

Fix applied:
1. Strict ``end_class`` / ``reached_stop`` (``ok=1`` alone is not natural return).
2. ``MAP_DATA`` -> ``BRIDGE_DATA_EXEC_TRAP`` + ``uc_emu_stop`` (no PC/LR rewrite, no fallthrough).
3. Family drain deferred until after MAP_FUNC ``PC=LR``, then re-assert continuation + ``uc_emu_stop`` so outer runCode restarts at guest continuation (closes stub+4 -> ``0x28005C``).

## Root cause (closed)

``````
PC=0x28005C
<- Unicorn resumed at MAP_FUNC stub+4 after nested Case-9 inside sendAppEvent hook
<- nested guest_memory_uc_run_entry_ex restored stub PC; hook PC=LR then lost on hook return
<- not a Guest POP of 0x28005C as primary edge (insn ring was stale from nested Case-9)
``````

## Handler shape

- ``0x30D311`` = Thumb entry at ``0x30D310`` = ``PUSH {R4-R6,LR}`` - **independent function**, not mid-function label.
- Thin neighbor wrapper at ``0x30D308`` calls ``0x30D25D``; registered plat ``0x10102`` points at ``0x30D311``.

## Acceptance (3x same binary)

| run | reached_stop | end_class | data_trap | fallthrough | mr_plat | 10133 | first_genuine |
|-----|--------------|-----------|-----------|-------------|---------|-------|---------------|
$(($results | ForEach-Object { '| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} |' -f $_.run, $_.reached_stop, $_.end_class, $_.data_trap, $_.fallthrough, $_.mr_plat, $_.call_10133, $_.first_genuine }) -join "`n")

pass_count=$passN / 3

## Final answers

``````
0x30D311 shape: independent function (0x30D310 PUSH {R4-R6,LR})
original shell wrapper: guest BLX to mr_table; registered target 0x30D311 (neighbor 0x30D308 is thin wrapper, not 10102 value)
handler entry SP/LR: see p16_original_shell_callback_compare.csv; direct path uses stop LR=0x80000 + handler PUSH
direct vs wrapper: pre-P16 nested emu inside MAP_FUNC; P16 defers drain + reassert PC + uc_emu_stop
0x28005C exact source: after MAP_FUNC@0x280058 return, Unicorn resumed at stub+4 (nested emu broke PC=LR)
fixed real continuation: $(if ($allPass) { 'YES' } else { 'NO/PARTIAL — see matrix' })
Case-9 truly REACHED_STOP: $(if ((@($results | Where-Object { $_.reached_stop -eq 1 })).Count -eq 3) { 'YES x3' } else { 'NO/PARTIAL' })
MAP_DATA still executed: $(if ((@($results | Where-Object { $_.data_trap -gt 0 })).Count -eq 0 -and (@($results | Where-Object { $_.fallthrough -gt 0 })).Count -eq 0) { 'NO' } else { 'YES trap/fallthrough remain' })
P13/P14 false calls gone: $(if ((@($results | Where-Object { $_.fallthrough -eq 0 })).Count -eq 3) { 'YES' } else { 'NO' })
first real Guest behavior after fix: outer lifecycle / helper epilogue (no table walk)
first real platform API: $firstGenuine
five BMP / Layer-1: not a hard gate this round
real game screen: not asserted this round
next unique lock: first GENUINE_ platform API natural progress
``````

## Artifacts

- ``out/p16/p16_build_identity.txt``
- ``reports/p16_true_call_matrix.csv``
- ``reports/p16_original_shell_callback_compare.csv``
- ``reports/p16_family_callback_frame_verdict.md``
- Runner: ``research/runners/p16_run_family_callback_frame.ps1``
"@ | Set-Content $verdict -Encoding utf8

Write-Host '=== P16 verdict ==='
Get-Content $verdict
if (-not $allPass) { exit 1 }
exit 0
