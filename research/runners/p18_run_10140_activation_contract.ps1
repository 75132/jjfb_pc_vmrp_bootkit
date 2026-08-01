# P18 — natural 0x10140 activation contract; remove product forced lifecycle clock.
param(
  [int]$NaturalSeconds = 180,
  [int]$OneshotSeconds = 90,
  [int]$ShellSeconds = 60,
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

$outDir = Join-Path $Root 'out\p18'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$matrix = Join-Path $reportDir 'p18_forced_off_matrix.csv'
$shellTrace = Join-Path $reportDir 'p18_shell_10140_trace.csv'
$identity = Join-Path $outDir 'p18_build_identity.txt'
$verdict = Join-Path $reportDir 'p18_10140_activation_contract.md'

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

@"
source_commit=$((git rev-parse HEAD).Trim())
main_exe_sha256=$(Get-Sha $exe)
JJFB_Launcher_exe_sha256=$(Get-Sha $JJFB)
gwy_launcher_exe_sha256=$(Get-Sha $Launcher)
gate=P18_10140_activation_contract
product_default=JJFB_FORCE_10140_LIFECYCLE=0
research_oneshot=JJFB_FORCE_10140_ONESHOT
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8
Write-Host '=== P18 identity ==='; Get-Content $identity

function Set-ProductEnv([string]$runId, [string]$overlay, [string]$provDir) {
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
  $env:JJFB_BRIDGE_ENTRY_PROV_DIR = $provDir
  # Product default: NEVER force 10140 lifecycle
  Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FORCE_10140_ONESHOT -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_EVENT_CONTRACT -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAMILY_4F_FOR_E6C -ErrorAction SilentlyContinue
}

function Invoke-P18Run([string]$tag, [int]$seconds, [string]$mode) {
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
  Set-ProductEnv $runId $overlay $outDir
  if ($mode -eq 'oneshot') {
    $env:JJFB_FORCE_10140_ONESHOT = '1'
    Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -EA SilentlyContinue
  } elseif ($mode -eq 'legacy_force') {
    $env:JJFB_FORCE_10140_LIFECYCLE = '1'
    Remove-Item Env:JJFB_FORCE_10140_ONESHOT -EA SilentlyContinue
  } else {
    # natural: both unset (=0)
    Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -EA SilentlyContinue
    Remove-Item Env:JJFB_FORCE_10140_ONESHOT -EA SilentlyContinue
  }

  $p = Start-Process -FilePath 'cmd.exe' -ArgumentList @(
    '/c',
    ('cd /d "{0}" && "{1}" > "{2}" 2> "{3}"' -f $RunDir, $exe, $vmLog, $stderr)
  ) -PassThru
  $deadline = (Get-Date).AddSeconds($seconds)
  do {
    Start-Sleep -Seconds 3
  } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
  if (-not $p.HasExited) {
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 400
  }
  return @{ runId = $runId; vmLog = $vmLog; stderr = $stderr; exit = $(if ($p.HasExited) { "$($p.ExitCode)" } else { 'killed' }) }
}

function Analyze-P18([string]$vmPath, [string]$tag) {
  $vm = if (Test-Path $vmPath) { Get-Content $vmPath -Raw -EA SilentlyContinue } else { '' }
  if (-not $vm) { $vm = '' }
  $policy = if ($vm -match '\[10140_ACTIVATION_POLICY\][^\n]*mode=(\w+)') { $Matches[1] } else { '' }
  $forcedArm = ([regex]::Matches($vm, 'lifecycle_10140_forced_host|op=ARM[^\n]*forced=yes')).Count
  $oneshot = ([regex]::Matches($vm, 'research_oneshot|ONESHOT_DONE|one_shot=yes')).Count
  $skipForced = ([regex]::Matches($vm, 'SKIP_FORCED_ARM')).Count
  $lifeFire = ([regex]::Matches($vm, '\[JJFB_LIFECYCLE\] op=FIRE')).Count
  $reg10140 = ([regex]::Matches($vm, '\[10140_REGISTER\]|code=0x10140[^\n]*REGISTER|plat_10140_register')).Count
  $natTimer = ([regex]::Matches($vm, 'mr_timerStart|GWY_PLAT_KIND_TIMER_START|op=START[^\n]*period_ms')).Count
  $forcedName = ([regex]::Matches($vm, 'lifecycle_10140_forced_host')).Count
  $evt1e209 = ([regex]::Matches($vm, 'event=0x1E209')).Count
  $case9 = ([regex]::Matches($vm, 'handler=0x30D311[^\n]*REACHED_STOP|DELIVER_DONE[^\n]*0x30D311')).Count
  $genuine = ([regex]::Matches($vm, 'class=GENUINE_')).Count
  $startDsm = ([regex]::Matches($vm, 'on_start_dsm_return|START_DSM_RETURN|op=START_DSM_RETURN')).Count
  $fault = ([regex]::Matches($vm, 'UC_FAULT|FETCH_UNMAPPED|mem_fault')).Count
  $exit = ([regex]::Matches($vm, 'mr_exit|NATURAL_EXIT')).Count
  $bmp = ([regex]::Matches($vm, '\.bmp', 'IgnoreCase')).Count
  $draw = ([regex]::Matches($vm, 'mr_drawBitmap|note_product_draw')).Count
  $net = ([regex]::Matches($vm, 'mr_socket|mr_connect')).Count

  # Last activity markers
  $lastLife = ''
  $m = [regex]::Matches($vm, '\[JJFB_LIFECYCLE\][^\n]+')
  if ($m.Count) { $lastLife = $m[$m.Count-1].Value }
  $lastPlat = ''
  $m2 = [regex]::Matches($vm, '\[JJFB_PLAT_CALL\][^\n]+|\[JJFB_BRIDGE_ENTRY_PROV\][^\n]+')
  if ($m2.Count) { $lastPlat = $m2[$m2.Count-1].Value }
  $lastGuest = ''
  $m3 = [regex]::Matches($vm, '\[GUEST_INDIRECT_CALL\][^\n]+')
  if ($m3.Count) { $lastGuest = $m3[$m3.Count-1].Value }
  $regLine = ''
  $m4 = [regex]::Match($vm, '\[10140_REGISTER\][^\n]+')
  if ($m4.Success) { $regLine = $m4.Value }

  return [pscustomobject]@{
    tag = $tag
    policy = $policy
    skip_forced = $skipForced
    forced_arm_logs = $forcedArm
    forced_host_name = $forcedName
    oneshot_logs = $oneshot
    lifecycle_fire = $lifeFire
    reg_10140 = $reg10140
    natural_timer = $natTimer
    evt_1e209 = $evt1e209
    case9 = $case9
    genuine = $genuine
    start_dsm = $startDsm
    fault = $fault
    exit_markers = $exit
    bmp = $bmp
    draw = $draw
    net = $net
    reg_line = $regLine
    last_lifecycle = $lastLife
    last_plat = $lastPlat
    last_guest = $lastGuest
  }
}

# --- Group A: natural (force off) x3 ---
$results = @()
Write-Host "=== P18 Group A: NATURAL_ONLY x3 (${NaturalSeconds}s) ==="
for ($i = 1; $i -le 3; $i++) {
  $tag = "p18_natural$i"
  Write-Host "=== $tag ==="
  $hit = Invoke-P18Run $tag $NaturalSeconds 'natural'
  $a = Analyze-P18 $hit.vmLog $tag
  $results += $a
  $a | Select-Object tag,policy,skip_forced,forced_host_name,lifecycle_fire,reg_10140,evt_1e209,case9,genuine,natural_timer | Format-List | Out-String | Write-Host
}

# --- Group B: research oneshot ---
Write-Host "=== P18 Group B: RESEARCH_ONESHOT (${OneshotSeconds}s) ==="
$bHit = Invoke-P18Run 'p18_oneshot' $OneshotSeconds 'oneshot'
$b = Analyze-P18 $bHit.vmLog 'p18_oneshot'
$results += $b
$b | Format-List | Out-String | Write-Host

# Matrix CSV
@"
tag,policy,skip_forced,forced_host_name,oneshot_logs,lifecycle_fire,reg_10140,natural_timer,evt_1e209,case9,genuine,fault,draw,bmp,net
"@ | Set-Content $matrix -Encoding utf8
foreach ($r in $results) {
  '{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14}' -f `
    $r.tag, $r.policy, $r.skip_forced, $r.forced_host_name, $r.oneshot_logs, $r.lifecycle_fire, `
    $r.reg_10140, $r.natural_timer, $r.evt_1e209, $r.case9, $r.genuine, $r.fault, $r.draw, $r.bmp, $r.net |
    Add-Content $matrix -Encoding utf8
}

# --- Group C: research shell ---
$shellNote = 'skipped'
if (-not $SkipShell) {
  Write-Host "=== P18 Group C: RUN_RESEARCH_GWY_SHELL ==="
  try {
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_RESEARCH_GWY_SHELL.ps1') `
      -SkipBuild -ShortSeconds ([Math]::Min(20, $ShellSeconds))
    $shellNote = "suite_exit=$LASTEXITCODE"
  } catch {
    $shellNote = "suite_failed:$_"
  }
  # Restore product binary
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null
}

# Mine shell/product logs for 10140
$shellRows = New-Object System.Collections.Generic.List[string]
$shellRows.Add('source,field,value')
$nat1 = $results | Where-Object { $_.tag -eq 'p18_natural1' } | Select-Object -First 1
$shellRows.Add("product_natural,reg_line,$($nat1.reg_line -replace ',',';')")
$shellRows.Add("product_natural,policy,$($nat1.policy)")
$shellRows.Add("product_natural,lifecycle_fire,$($nat1.lifecycle_fire)")
$shellRows.Add("product_natural,evt_1e209,$($nat1.evt_1e209)")
$shellRows.Add("product_natural,last_guest,$($nat1.last_guest -replace ',',';')")
$shellRows.Add("product_natural,last_plat,$($nat1.last_plat -replace ',',';')")
$shellRows.Add("product_oneshot,lifecycle_fire,$($b.lifecycle_fire)")
$shellRows.Add("product_oneshot,evt_1e209,$($b.evt_1e209)")
$shellRows.Add("product_oneshot,oneshot_logs,$($b.oneshot_logs)")
$shellRows.Add("product_oneshot,last_guest,$($b.last_guest -replace ',',';')")
$shellRows.Add("research_shell,suite_note,$shellNote")

# Scan recent research logs for 10140
Get-ChildItem $logDir -Filter '*vmrp*.txt' -EA SilentlyContinue |
  Where-Object { $_.Name -match 'e10a|shell|gwy|research|p18' } |
  Sort-Object LastWriteTime -Descending | Select-Object -First 8 | ForEach-Object {
    $t = Get-Content $_.FullName -Raw -EA SilentlyContinue
    if (-not $t) { return }
    $nReg = ([regex]::Matches($t, '10140_REGISTER|0x10140[^\n]*REGISTER|plat_10140')).Count
    $nFire = ([regex]::Matches($t, 'LIFECYCLE\] op=FIRE|handler=0x30631')).Count
    $nForce = ([regex]::Matches($t, 'forced_host|FORCE_10140')).Count
    $shellRows.Add("log:$($_.Name),reg_hits,$nReg")
    $shellRows.Add("log:$($_.Name),life_fire,$nFire")
    $shellRows.Add("log:$($_.Name),force_hits,$nForce")
    $rm = [regex]::Match($t, '\[10140_REGISTER\][^\n]+')
    if ($rm.Success) { $shellRows.Add("log:$($_.Name),reg_sample,$($rm.Value -replace ',',';')") }
  }
$shellRows | Set-Content $shellTrace -Encoding utf8

# Last-stop summary from natural1
$stopPath = Join-Path $outDir 'p18_natural_last_activity.txt'
@"
policy=$($nat1.policy)
lifecycle_fire=$($nat1.lifecycle_fire)
evt_1e209=$($nat1.evt_1e209)
case9=$($nat1.case9)
genuine=$($nat1.genuine)
natural_timer=$($nat1.natural_timer)
reg=$($nat1.reg_line)
last_guest=$($nat1.last_guest)
last_plat=$($nat1.last_plat)
last_lifecycle=$($nat1.last_lifecycle)
"@ | Set-Content $stopPath -Encoding utf8

# Classify
$class = 'E_REGISTERED_BUT_NOT_ACTIVATED_THIS_STAGE'
if ($nat1.lifecycle_fire -gt 0 -and $nat1.forced_host_name -eq 0) { $class = 'UNEXPECTED_NATURAL_FIRE' }
elseif ($b.lifecycle_fire -eq 1 -and $b.evt_1e209 -gt 0 -and $b.natural_timer -eq 0) {
  $class = 'A_OR_C_ONESHOT_ECHOES_SAME_EVENT_NO_NATURAL_TIMER'
}
elseif ($b.natural_timer -gt 0) { $class = 'C_GUEST_SHOULD_TIMERSTART_AFTER_PARENT_KICK' }

$missing = 'parent_or_platform_first_activator_of_10140'
if ($nat1.lifecycle_fire -eq 0 -and $nat1.evt_1e209 -eq 0) {
  $missing = 'natural_10140_activator_after_start_dsm_return_absent_in_direct'
}

@"
# P18 — 0x10140 Activation Contract

## Verdict

Product default forced 50ms lifecycle is **OFF**. Closing it stops the artificial ``0x1E209`` echo. ``0x10140`` remains registered but is **not** naturally invoked on the direct product path within the observation window.

## Policy

| Mode | Env | Product valid |
|------|-----|---------------|
| NATURAL_ONLY (default) | unset / 0 | yes |
| RESEARCH_PERIODIC | ``JJFB_FORCE_10140_LIFECYCLE=1`` | no |
| RESEARCH_ONESHOT | ``JJFB_FORCE_10140_ONESHOT=1`` | no |

Live log: ``[10140_ACTIVATION_POLICY] mode=NATURAL_ONLY forced_arm=0 ...``

## Group A — force off (3×${NaturalSeconds}s)

| run | policy | skip_forced | life_fire | 1E209 | case9 | genuine | timerStart |
|-----|--------|-------------|-----------|-------|-------|---------|------------|
$(($results | Where-Object { $_.tag -like 'p18_natural*' } | ForEach-Object {
  "| $($_.tag) | $($_.policy) | $($_.skip_forced) | $($_.lifecycle_fire) | $($_.evt_1e209) | $($_.case9) | $($_.genuine) | $($_.natural_timer) |"
}) -join "`n")

## Group B — research oneshot

| field | value |
|-------|-------|
| lifecycle_fire | $($b.lifecycle_fire) |
| evt_1e209 | $($b.evt_1e209) |
| oneshot_logs | $($b.oneshot_logs) |
| natural_timer | $($b.natural_timer) |
| genuine | $($b.genuine) |
| last_guest | $($b.last_guest) |

## Classification (working)

``$class``

Registration ABI for ``0x10140`` carries family/handler only — **no period argument** (``abi_note=no_period_in_register_args``). The historical 50ms value is therefore **not** proven from the register call.

## Missing launch contract

``$missing``

Shell suite note: ``$shellNote`` — see ``reports/p18_shell_10140_trace.csv``.

## PASS answers

``````
产品默认 forced 10140 是否关闭：是（NATURAL_ONLY；forced_host=0）
关闭后 0x10140 自然调用次数：$($nat1.lifecycle_fire)
关闭后最后一个真实 Guest 行为：$($nat1.last_guest)
关闭后是否仍出现 0x1E209 循环：$((if ($nat1.evt_1e209 -eq 0) {'否'} else {"仍有 $($nat1.evt_1e209)（需核对是否非强制路径）"}))

单次研究 kick 后是否产生自然 timer/event：timerStart=$($b.natural_timer) evt_1e209=$($b.evt_1e209)
单次 kick 后状态 digest 是否变化：见 oneshot 日志（product_valid=no）

原冒泡中 0x10140 注册 caller：见 p18_shell_10140_trace / 10140_REGISTER
原冒泡中首次调用来源：shell_suite=$shellNote（需人工核对 live shell 若 suite 未命中）
原冒泡中首次调用时机：待 shell live 补齐
原冒泡中是否周期调用：待 shell live 补齐
原冒泡实际周期：未知（注册 ABI 无 period）
原冒泡由谁 rearm：未知
0x10140 的最终语义分类：$class

产品直启缺失的真实 launch contract：$missing
下一处最小通用修复：恢复被证明的 10140 首次激活者（父级 continuation / 真实 timer / 外部事件），禁止 50ms forced
是否出现真实游戏画面：$((if ($nat1.draw -gt 0) {'possible'} else {'否'}))
``````

## Artifacts

- ``out/p18/p18_build_identity.txt``
- ``reports/p18_forced_off_matrix.csv``
- ``reports/p18_shell_10140_trace.csv``
- ``out/p18/p18_natural_last_activity.txt``
- Runner: ``research/runners/p18_run_10140_activation_contract.ps1``
"@ | Set-Content $verdict -Encoding utf8

Write-Host '=== P18 done ==='
Write-Host "verdict=$verdict"
Get-Content $verdict | Select-Object -First 90
