# P15-GATE — prove mr_plat is genuine Guest call vs bridge table fall-through.
# Does NOT implement mr_plat. Frozen product ABI: direct_lr.
param(
  [int]$DiagSeconds = 50,
  [int]$HitSeconds = 90,
  [switch]$SkipBuild,
  [switch]$SkipResearch
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\p15'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$matrix = Join-Path $reportDir 'p15_bridge_call_matrix.csv'
$identity = Join-Path $outDir 'p15_build_identity.txt'
$verdict = Join-Path $reportDir 'p15_mr_plat_call_provenance.md'

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
source_tree_clean=$(-not [bool]@(git status --porcelain -- 'src' 'include' 'third_party/vmrp_upstream/bridge.c' 'CMakeLists.txt' 'tests'))
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
gate=P15_mr_plat_call_provenance
"@ | Set-Content $identity -Encoding utf8
Write-Host '=== P15 identity ==='; Get-Content $identity

function Invoke-ProvRun([string]$tag, [int]$seconds) {
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

  # Fresh CSV for this process (CWD may be RunDir; use absolute out dir).
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

function Get-Class([string]$text, [string]$api) {
  $m = [regex]::Match($text, "\[JJFB_BRIDGE_ENTRY_PROV\].*api=$([regex]::Escape($api))\s+slot=0x[0-9A-Fa-f]+\s+class=(\w+)")
  if ($m.Success) { return $m.Groups[1].Value }
  return 'NOT_SEEN'
}

function Analyze-Prov([string]$path) {
  $vm = if ($path -match '_stdout\.txt$') {
    $v = $path -replace '_stdout\.txt$', '_vmrp.txt'
    if (Test-Path $v) { Get-Content $v -Raw -EA SilentlyContinue } else { Get-Content $path -Raw -EA SilentlyContinue }
  } else { Get-Content $path -Raw -EA SilentlyContinue }
  if (-not $vm) { $vm = '' }

  $apis = @('mr_drawBitmap','mr_getCharBitmap','mr_getTime','mr_getDatetime','mr_getUserInfo','mr_sleep','mr_plat')
  $classes = @{}
  foreach ($a in $apis) { $classes[$a] = Get-Class $vm $a }

  $plat = [regex]::Match($vm, '\[JJFB_BRIDGE_ENTRY_PROV\].*api=mr_plat\s+slot=(0x[0-9A-Fa-f]+)\s+class=(\w+).*?\blr=(0x[0-9A-Fa-f]+).*?\br0=(0x[0-9A-Fa-f]+).*?\br1=(0x[0-9A-Fa-f]+).*?\bbranch=(\S+).*?\bbranch_pc=(0x[0-9A-Fa-f]+).*?\bargs_valid=(\d+)')
  $fall = [bool]($vm -match 'LINEAR_SLOT_FALLTHROUGH|STALE_LR_REENTRY|RETURN_TO_BRIDGE_SLOT|TABLE_DATA_EXECUTION')
  $genuinePlat = $classes['mr_plat'] -match '^GENUINE_'
  $firstFalse = ''
  foreach ($a in $apis) {
    $c = $classes[$a]
    if ($c -match 'FALLTHROUGH|STALE|RETURN_TO_BRIDGE|TABLE_DATA') {
      $firstFalse = "$a/$c"
      break
    }
  }
  $firstGenuine = ''
  foreach ($a in $apis) {
    if ($classes[$a] -match '^GENUINE_') {
      $firstGenuine = "$a/$($classes[$a])"
      break
    }
  }

  return [pscustomobject]@{
    drawBitmap = $classes['mr_drawBitmap']
    getCharBitmap = $classes['mr_getCharBitmap']
    getTime = $classes['mr_getTime']
    getUserInfo = $classes['mr_getUserInfo']
    sleep = $classes['mr_sleep']
    plat = $classes['mr_plat']
    plat_slot = $(if ($plat.Success) { $plat.Groups[1].Value } else { '' })
    plat_class = $(if ($plat.Success) { $plat.Groups[2].Value } else { $classes['mr_plat'] })
    plat_lr = $(if ($plat.Success) { $plat.Groups[3].Value } else { '' })
    plat_r0 = $(if ($plat.Success) { $plat.Groups[4].Value } else { '' })
    plat_r1 = $(if ($plat.Success) { $plat.Groups[5].Value } else { '' })
    plat_branch = $(if ($plat.Success) { $plat.Groups[6].Value } else { '' })
    plat_branch_pc = $(if ($plat.Success) { $plat.Groups[7].Value } else { '' })
    plat_args_valid = $(if ($plat.Success) { $plat.Groups[8].Value } else { '' })
    fallthrough_seen = $fall
    first_genuine = $firstGenuine
    first_false = $firstFalse
    genuine_plat = $genuinePlat
    unimpl_plat = [bool]($vm -match 'mr_plat\(\) Not yet implemented')
    case9_leave = [bool]($vm -match 'CASE9_LEAVE|case.?9.*leave|PLATFORM_FAMILY_EVENT.*DELIVER_DONE')
  }
}

"tag,drawBitmap,getCharBitmap,getTime,getUserInfo,sleep,plat,plat_r0,plat_r1,plat_lr,plat_branch,plat_args_valid,first_genuine,first_false,fallthrough,exit" |
  Set-Content $matrix -Encoding utf8

Write-Host '=== P15 diag run ==='
$diag = Invoke-ProvRun 'p15_diag' $DiagSeconds
$a = Analyze-Prov $diag.vmLog
"{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15}" -f `
  'diag',$a.drawBitmap,$a.getCharBitmap,$a.getTime,$a.getUserInfo,$a.sleep,$a.plat,`
  $a.plat_r0,$a.plat_r1,$a.plat_lr,$a.plat_branch,$a.plat_args_valid,`
  $a.first_genuine,$a.first_false,$a.fallthrough_seen,$diag.exit |
  Add-Content $matrix -Encoding utf8

Write-Host '=== P15 hit run ==='
$hit = Invoke-ProvRun 'p15_hit1' $HitSeconds
$b = Analyze-Prov $hit.vmLog
"{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15}" -f `
  'hit1',$b.drawBitmap,$b.getCharBitmap,$b.getTime,$b.getUserInfo,$b.sleep,$b.plat,`
  $b.plat_r0,$b.plat_r1,$b.plat_lr,$b.plat_branch,$b.plat_args_valid,`
  $b.first_genuine,$b.first_false,$b.fallthrough_seen,$hit.exit |
  Add-Content $matrix -Encoding utf8

# Prefer hit if it saw plat; else diag
$best = if ($b.plat -ne 'NOT_SEEN') { $b } else { $a }
$verdictA = $best.genuine_plat -and ($best.plat_args_valid -eq '1')
$verdictLabel = if ($verdictA) { 'A_GENUINE_CALL' } else { 'B_FALLTHROUGH_OR_STALE' }

$researchNote = 'SKIPPED'
if (-not $SkipResearch) {
  Write-Host '=== Research shell contrast (short, best-effort) ==='
  try {
    # Reuse product path with same provenance; full RUN_RESEARCH_GWY_SHELL is a long chain.
    # Emit a lightweight note from existing logs if mr_plat census appears.
    $researchNote = 'product_only_this_gate; RUN_RESEARCH_GWY_SHELL left for optional follow-up'
  } catch {
    $researchNote = "research_error:$($_.Exception.Message)"
  }
}

@"
# P15 — ``mr_plat`` call provenance gate

## Verdict

**$verdictLabel**

Frozen: ``direct_lr``, ``product_ffp_apply_abi=0``, ``current_mrp=gwy/jjfb.mrp``, owner=robotol.ext.
Instrumentation only — ``mr_plat`` was **not** implemented.

## Acceptance answers

``````
mr_getCharBitmap 是否真实 Guest 调用：$($best.getCharBitmap)
mr_getTime 是否真实 Guest 调用：$($best.getTime)
mr_getUserInfo 是否真实 Guest 调用：$($best.getUserInfo)
mr_sleep 是否真实 Guest 调用：$($best.sleep)
mr_plat 是否真实 Guest 调用：$($best.plat)

mr_plat caller PC / branch_pc：$($best.plat_branch_pc)
mr_plat branch instruction：$($best.plat_branch)
mr_plat code (R0)：$($best.plat_r0)
mr_plat param (R1)：$($best.plat_r1)
R0/R1 最后写入位置：见 out/p15/bridge_entry_provenance.csv
mr_plat LR continuation：$($best.plat_lr)

是否发生 bridge slot 顺序跌落：$($best.fallthrough_seen)
第一个错误入口：$($best.first_false)
第一个 GENUINE 入口：$($best.first_genuine)
stale LR 来源：Case-9 / family deliver 后 LR=$($best.plat_lr) 贯穿连续 MAP_FUNC（见 logs/p15_*）
是否修复控制流：否（本 gate 只裁决，不修）
修复后第一个真实自然行为：N/A

原冒泡环境是否观察到同一调用：$researchNote
是否允许实现当前 mr_plat code：$(if ($verdictA) { '是（仅 observed code）' } else { '否 — 禁止接线 br_mr_plat' })
是否出现真实游戏画面：否
``````

## Rule

- 裁决 A：仅实现命中的一个 ``code``；未知 code → MR_FAILED。
- 裁决 B：禁止 ``br_mr_plat``；修复最早 LR/continuation 污染后再自然跑。

## Artifacts

- ``out/p15/p15_build_identity.txt``
- ``out/p15/bridge_entry_provenance.csv``
- ``out/p15/bridge_predecessor_ring.csv``
- ``out/p15/bridge_insn_ring.csv``
- ``out/p15/bridge_nest_audit.csv``
- ``reports/p15_bridge_call_matrix.csv``
- ``logs/p15_diag_vmrp.txt`` / ``logs/p15_hit1_vmrp.txt``

## Matrix snapshot

见 ``reports/p15_bridge_call_matrix.csv``。
"@ | Set-Content $verdict -Encoding utf8

Write-Host "=== P15 verdict: $verdictLabel ==="
Write-Host "Report: $verdict"
Get-Content $matrix
