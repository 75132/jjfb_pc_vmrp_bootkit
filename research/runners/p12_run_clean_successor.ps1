# P12 — Clean golden ×3 + Case-9 successor window (observe only; no ABI changes)
param(
  [int]$Seconds = 120,
  [int]$Runs = 3,
  [switch]$SkipBuild,
  [switch]$SuccessorOnly
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = Split-Path -Parent $MyInvocation.MyCommand.Path
  while ($Root -and -not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
    $Root = Split-Path -Parent $Root
  }
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\p12'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
$matrix = Join-Path $reportDir 'p12_clean_run_matrix.csv'
$identityOut = Join-Path $outDir 'p12_clean_build_identity.txt'

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

function Write-Identity {
  $sourceCommit = (git rev-parse HEAD).Trim()
  $dirty = @(git status --porcelain)
  $dirtySrc = @(git status --porcelain -- 'src' 'include' 'third_party/vmrp_upstream' 'CMakeLists.txt')
  $compiler = & gcc -dumpversion 2>$null
  $buildTs = if (Test-Path $exe) { (Get-Item $exe).LastWriteTimeUtc.ToString('o') } else { '?' }
  @"
source_commit=$sourceCommit
source_tree_clean=$(-not [bool]$dirty)
source_tree_clean_product_src=$(-not [bool]$dirtySrc)
build_time_utc=$buildTs
main_exe_sha256=$(Get-Sha $exe)
JJFB_Launcher_exe_sha256=$(Get-Sha $JJFB)
gwy_launcher_exe_sha256=$(Get-Sha $Launcher)
compiler=gcc-$compiler
build_commands=RUN_BUILD.ps1 -BuildDir build-i686; RUN_BUILD_VMRP.ps1 -Mode Gwy; RUN_TESTS.ps1 -SkipBuild
product_default_return_mode=direct_lr
product_ffp_apply_abi=0
JJFB_PRODUCT_EVENT_CONTRACT=0
JJFB_FAMILY_4F_FOR_E6C=0
git_status_porcelain=$(if ($dirty) { ($dirty -join '; ') } else { '(empty)' })
"@ | Set-Content -Path $identityOut -Encoding utf8
  Write-Host "=== P12 identity (out/) ==="
  Get-Content $identityOut
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
}

Write-Identity
$mainSha = Get-Sha $exe
$launcherSha = Get-Sha $Launcher
$sourceCommit = (git rev-parse HEAD).Trim()

# Freeze hashes for triple run — no rebuild between runs.
$freeze = Join-Path $outDir 'p12_binary_freeze.txt'
@"
source_commit=$sourceCommit
main_exe_sha256=$mainSha
gwy_launcher_sha256=$launcherSha
JJFB_Launcher_exe_sha256=$(Get-Sha $JJFB)
frozen_at_utc=$((Get-Date).ToUniversalTime().ToString('o'))
"@ | Set-Content $freeze -Encoding utf8

if (-not $SuccessorOnly) {
  'run_id,source_commit,main_exe_sha256,launcher_sha256,real_window,real_first_frame,first_frame_sha,natural_bmp_count,natural_bmp_order,CASE9_ENTER,CASE9_LEAVE_ok1,P3_FAULT,exited,final_state,strong_success,post_callback' |
    Set-Content -Path $matrix -Encoding utf8

  for ($i = 1; $i -le $Runs; $i++) {
    Write-Host "==== P12 GOLDEN RUN $i / $Runs (SkipBuild) ===="
    # Product script rebuilds by default — force skip.
    & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_PRODUCT_DIRECT_JJFB.ps1') `
      -Seconds $Seconds -SkipBuild -SkipVmrpBuild
    $prodExit = $LASTEXITCODE
    $stdout = Join-Path $logDir 'product_direct_jjfb_stdout.txt'
    $all = if (Test-Path $stdout) { Get-Content $stdout -Raw -EA SilentlyContinue } else { '' }
    $manifest = Get-ChildItem (Join-Path $reportDir 'product_direct_jjfb_manifest_*.txt') |
      Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $runId = if ($manifest) { ($manifest.BaseName -replace '^product_direct_jjfb_manifest_', '') } else { "p12_run$i" }
    $curMain = Get-Sha $exe
    $curLaunch = Get-Sha $Launcher
    if ($curMain -ne $mainSha -or $curLaunch -ne $launcherSha) {
      throw "BINARY DRIFT mid-triple: main=$curMain launcher=$curLaunch"
    }

    $bmps = [regex]::Matches($all, '\[JJFB_BMP_REQ\]\s+name=([^\s]+)') | ForEach-Object { $_.Groups[1].Value }
    $bmpOrder = ($bmps -join '>')
    $frameSha = ''
    $mSha = [regex]::Match($all, 'layer.?1.?sha[=:]?\s*([0-9a-fA-F]{16,})|FIRST_FRAME_SHA[=:]?\s*([0-9a-fA-F]{16,})|framebuffer.?sha[=:]?\s*([0-9a-fA-F]{16,})')
    if ($mSha.Success) { $frameSha = @($mSha.Groups[1].Value, $mSha.Groups[2].Value, $mSha.Groups[3].Value | Where-Object { $_ })[0] }

    $realWindow = [bool]($all -match '\[JJFB_WINDOW\].*kind=|ShowWindow|SDL_')
    $realFrame = [bool]($all -match 'FIRST_NATURAL_REFRESH|FIRST_NATURAL_DRAW|DispUpEx|LAYER.?1')
    $case9e = [bool]($all -match 'CASE9_ENTER|op=DELIVER event=0x1E209 app=0x9|event=0x1E209.*app=0x9')
    $case9l = [bool]($all -match 'CASE9_LEAVE ok=1|op=DELIVER_DONE ok=1.*handler=0x30D311')
    $p3 = [bool]($all -match '\[P3_FAULT\]|FIRE_DONE ok=0')
    $post = ''
    if (Test-Path (Join-Path $reportDir 'product_post_callback_visual.md')) {
      $post = ([regex]::Match((Get-Content (Join-Path $reportDir 'product_post_callback_visual.md') -Raw), 'verdict:\*\*\s*(\S+)')).Groups[1].Value
    }
    $strong = ($prodExit -eq 0)
    $final = if ($prodExit -eq 0) { 'STRONG_OK' } elseif ($prodExit -eq 2) { 'INCOMPLETE_GATES' } else { "EXIT_$prodExit" }

    # Archive per-run logs under out/p12
    $runOut = Join-Path $outDir $runId
    New-Item -ItemType Directory -Force -Path $runOut | Out-Null
    Copy-Item $stdout (Join-Path $runOut 'stdout.txt') -EA SilentlyContinue
    Copy-Item (Join-Path $logDir 'product_direct_jjfb_vmrp.txt') (Join-Path $runOut 'vmrp.txt') -EA SilentlyContinue
    Copy-Item (Join-Path $reportDir 'product_direct_jjfb_verdict.md') (Join-Path $runOut 'verdict.md') -EA SilentlyContinue

    $row = @(
      $runId, $sourceCommit, $curMain, $curLaunch,
      $(if ($realWindow) { 'yes' } else { 'no' }),
      $(if ($realFrame) { 'yes' } else { 'no' }),
      $(if ($frameSha) { $frameSha } else { 'NOT_SEEN' }),
      $bmps.Count, $bmpOrder,
      $(if ($case9e) { 'yes' } else { 'no' }),
      $(if ($case9l) { 'yes' } else { 'no' }),
      $(if ($p3) { 'yes' } else { 'no' }),
      'killed_or_exit', $final,
      $(if ($strong) { 'yes' } else { 'no' }),
      $(if ($post) { $post } else { 'n/a' })
    ) -join ','
    Add-Content -Path $matrix -Value $row -Encoding utf8
    Write-Host "run=$runId strong=$strong case9_leave=$case9l bmp=$($bmps.Count) post=$post"
  }
}

# Successor observation run: same binaries + P11 markers for CASE9_LEAVE T0
Write-Host '==== P12 SUCCESSOR OBSERVE (P11 markers, apply_abi=0) ===='
Clear-CaseEnv
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null

$succId = ('p12_succ_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))
$stdout = Join-Path $logDir 'p12_successor_stdout.txt'
$stderr = Join-Path $logDir 'p12_successor_stderr.txt'
$vmLog = Join-Path $logDir 'p12_successor_vmrp.txt'
@($stdout, $stderr, $vmLog) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }

$overlay = Join-Path $RunDir ("overlay_$succId")
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'

$env:GWY_PROFILE = $Profile
$env:GWY_OVERLAY_ROOT = $overlay
$env:GWY_PRODUCT_REPORTS_DIR = $reportDir
$env:GWY_PRODUCT_RUN_ID = $succId
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
$env:JJFB_P11_MODE = '1'
$env:JJFB_P11_REPORTS_DIR = (Join-Path $outDir 'p11_obs')
New-Item -ItemType Directory -Force -Path $env:JJFB_P11_REPORTS_DIR | Out-Null
$env:JJFB_VISIBLE_WINDOW = '1'
$env:JJFB_E9B_MODE = '1'
$env:JJFB_DISPLAY_FIRST = '1'
Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_EVENT_CONTRACT -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_FAMILY_4F_FOR_E6C -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_P5_ONE_SHOT -ErrorAction SilentlyContinue

$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

# Verify freeze
if ((Get-Sha $exe) -ne $mainSha) { throw 'main.exe drifted before successor run' }

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $vmLog -RedirectStandardError $stderr
$deadline = (Get-Date).AddSeconds($Seconds)
$sawLeave = $false
$leaveAt = $null
do {
  Start-Sleep -Seconds 2
  if (Test-Path $vmLog) {
    Get-Content $vmLog -Tail 400 -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8
  }
  $all = if (Test-Path $stdout) { Get-Content $stdout -Raw -EA SilentlyContinue } else { '' }
  if (-not $sawLeave -and $all -match 'CASE9_LEAVE ok=1') {
    $sawLeave = $true
    $leaveAt = Get-Date
    Write-Host "[P12] CASE9_LEAVE seen — holding +5s wall window"
  }
  if ($sawLeave -and $leaveAt -and ((Get-Date) -ge $leaveAt.AddSeconds(5))) {
    # keep running a bit more for module/unimpl evidence, then stop at original deadline preference
    if ((Get-Date) -ge $leaveAt.AddSeconds(12)) { break }
  }
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -EA SilentlyContinue
  Start-Sleep -Milliseconds 400
}
if (Test-Path $vmLog) { Get-Content $vmLog -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8 }

python (Join-Path $Root 'research\runners\p12_analyze_post_case9.py') `
  (Join-Path $logDir 'p12_successor_stdout.txt') `
  (Join-Path $logDir 'p12_successor_vmrp.txt')

Write-Host "P12 successor complete. matrix=$matrix identity=$identityOut"
