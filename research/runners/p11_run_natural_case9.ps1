# P11 — Natural Case-9 + Late P3 Fault provenance (apply_abi=0)
param(
  [int]$Seconds = 70,
  [switch]$SkipBuild
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

$reportDir = Join-Path $Root 'reports'
$logDir = Join-Path $Root 'logs'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
$runId = ('p11_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))
$stdout = Join-Path $logDir 'p11_natural_case9_stdout.txt'
$stderr = Join-Path $logDir 'p11_natural_case9_stderr.txt'
$vmLog = Join-Path $logDir 'p11_natural_case9_vmrp.txt'
$identity = Join-Path $reportDir 'p11_clean_build_identity.txt'

New-Item -ItemType Directory -Force -Path $reportDir, $logDir | Out-Null

function Clear-CaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } | ForEach-Object {
    Remove-Item -Path ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
  }
}

function Get-Sha([string]$p) {
  if (-not (Test-Path $p)) { return 'MISSING' }
  return (Get-FileHash -Algorithm SHA256 -Path $p).Hash.ToLowerInvariant()
}

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Clear-CaseEnv

$sourceCommit = (git rev-parse HEAD).Trim()
$dirty = @(git status --porcelain)
if ($dirty.Count -gt 0) {
  Write-Host "[P11] WARNING: working tree not empty — G0 wants clean commit"
  $dirty | Select-Object -First 30 | ForEach-Object { Write-Host $_ }
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP Gwy failed' }
}

$compiler = & gcc -dumpversion 2>$null
$mainSha = Get-Sha $exe
$jjfbSha = Get-Sha $JJFB
$gwySha = Get-Sha $Launcher
$buildTs = if (Test-Path $exe) { (Get-Item $exe).LastWriteTimeUtc.ToString('o') } else { '?' }
$dirtyAfterBuild = @(git status --porcelain -- 'src' 'include' 'third_party/vmrp_upstream' 'CMakeLists.txt')
$cleanSrc = -not [bool]$dirtyAfterBuild

@"
clean_commit=$sourceCommit
source_commit=$sourceCommit
source_tree_clean=$cleanSrc
build_time_utc=$buildTs
main_exe_sha256=$mainSha
JJFB_Launcher_exe_sha256=$jjfbSha
gwy_launcher_exe_sha256=$gwySha
compiler=gcc-$compiler
build_commands=RUN_BUILD.ps1 -BuildDir build-i686; RUN_BUILD_VMRP.ps1 -Mode Gwy
git_status_porcelain_source=$(if ($dirtyAfterBuild) { ($dirtyAfterBuild -join '; ') } else { '(empty)' })
product_default_return_mode=direct_lr
product_ffp_apply_abi=0
JJFB_PRODUCT_EVENT_CONTRACT=0
JJFB_FAMILY_4F_FOR_E6C=0
run_id=$runId
"@ | Set-Content -Path $identity -Encoding utf8
Write-Host "=== P11 identity ==="
Get-Content $identity

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL failed' }

$overlay = Join-Path $RunDir ("overlay_$runId")
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'

# Strict P11: natural chain only — no speculative ABI / contract / 4F.
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
$env:JJFB_P11_MODE = '1'
$env:JJFB_P11_REPORTS_DIR = $reportDir
$env:JJFB_VISIBLE_WINDOW = '1'
$env:JJFB_E9B_MODE = '1'
$env:JJFB_DISPLAY_FIRST = '1'
# Explicitly OFF
Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_EVENT_CONTRACT -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_TRACE_305E09 -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_FAMILY_4F_FOR_E6C -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_P5_ONE_SHOT -ErrorAction SilentlyContinue

$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

@($stdout, $stderr, $vmLog) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }
@(
  'p11_10102_registration_trace.csv',
  'p11_case9_dynamic_slice.csv',
  'p11_late_fault_ring.csv',
  'p11_late_fault_context.md',
  'p11_runtime_case9_30d280_30d480.bin',
  'p11_runtime_fault_2d95c0_2d9660.bin'
) | ForEach-Object { Remove-Item -Force (Join-Path $reportDir $_) -EA SilentlyContinue }

Write-Host "== P11 NATURAL CASE9 Seconds=$Seconds run_id=$runId apply_abi=0 =="
& $Launcher validate --root $ResourceRoot | Out-Null

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
  -RedirectStandardOutput $vmLog -RedirectStandardError $stderr
$deadline = (Get-Date).AddSeconds($Seconds)
$sawCase9 = $false
$sawFault = $false
do {
  Start-Sleep -Seconds 2
  if (Test-Path $vmLog) {
    Get-Content $vmLog -Tail 300 -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8
  }
  $all = if (Test-Path $stdout) { Get-Content $stdout -Raw -EA SilentlyContinue } else { '' }
  if ($all -match 'CASE9_ENTER|event=0x1E209.*app=0x9|app=9') { $sawCase9 = $true }
  if ($all -match 'P3_FAULT|FAULT_CONTEXT|addr=0x1E205|pc=0x2D960E') { $sawFault = $true }
  if ($sawCase9 -and $sawFault) { Start-Sleep -Seconds 3; break }
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -EA SilentlyContinue
  Start-Sleep -Milliseconds 400
}
if (Test-Path $vmLog) { Get-Content $vmLog -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8 }

Write-Host "sawCase9=$sawCase9 sawFault=$sawFault"
python (Join-Path $Root 'research\runners\p11_disasm_runtime_case9.py')
Write-Host "P11 run complete. identity=$identity stdout=$stdout"
