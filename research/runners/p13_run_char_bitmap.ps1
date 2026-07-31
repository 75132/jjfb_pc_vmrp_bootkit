# P13 — diagnose + triple golden + 180s visual (same frozen binaries)
param(
  [int]$DiagSeconds = 30,
  [int]$GoldenSeconds = 120,
  [int]$VisualSeconds = 180,
  [int]$Runs = 3,
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

$outDir = Join-Path $Root 'out\p13'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JJFB = Join-Path $Root 'build-i686\JJFB_Launcher.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$matrix = Join-Path $reportDir 'p13_char_bitmap_run_matrix.csv'
$identity = Join-Path $outDir 'p13_build_identity.txt'

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
source_tree_clean=$(-not [bool]@(git status --porcelain -- 'src' 'include' 'third_party/vmrp_upstream/bridge.c' 'CMakeLists.txt'))
build_time_utc=$((Get-Item $exe).LastWriteTimeUtc.ToString('o'))
main_exe_sha256=$mainSha
JJFB_Launcher_exe_sha256=$jjfbSha
gwy_launcher_exe_sha256=$gwySha
compiler=gcc-$compiler
product_default_return_mode=direct_lr
product_ffp_apply_abi=0
JJFB_PRODUCT_EVENT_CONTRACT=0
JJFB_FAMILY_4F_FOR_E6C=0
"@ | Set-Content $identity -Encoding utf8
Write-Host '=== P13 identity ==='; Get-Content $identity

function Invoke-NaturalRun([string]$tag, [int]$seconds, [switch]$WithP11) {
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
  $env:JJFB_P13_GLYPH_DUMP = '1'
  $env:JJFB_P13_DUMP_DIR = $outDir
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = $param
  Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_EVENT_CONTRACT -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_FAMILY_4F_FOR_E6C -ErrorAction SilentlyContinue
  if ($WithP11) {
    $env:JJFB_P11_MODE = '1'
    $env:JJFB_P11_REPORTS_DIR = (Join-Path $outDir 'p11_obs')
    New-Item -ItemType Directory -Force -Path $env:JJFB_P11_REPORTS_DIR | Out-Null
  }

  # Force dump dir via working directory relative out/p13
  New-Item -ItemType Directory -Force -Path $outDir | Out-Null

  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -PassThru `
    -RedirectStandardOutput $vmLog -RedirectStandardError $stderr
  $deadline = (Get-Date).AddSeconds($seconds)
  do {
    Start-Sleep -Seconds 2
    if (Test-Path $vmLog) {
      Get-Content $vmLog -Tail 500 -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8
    }
  } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
  if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -EA SilentlyContinue
    Start-Sleep -Milliseconds 400
  }
  if (Test-Path $vmLog) { Get-Content $vmLog -EA SilentlyContinue | Out-File $stdout -Append -Encoding utf8 }
  return @{ runId = $runId; stdout = $stdout; vmLog = $vmLog; exit = $(if ($p.HasExited) { "$($p.ExitCode)" } else { 'killed' }) }
}

function Analyze-Log([string]$path) {
  $all = if (Test-Path $path) { Get-Content $path -Raw -EA SilentlyContinue } else { '' }
  $getN = ([regex]::Matches($all, '\[JJFB_P13_GETCHAR\]')).Count
  $retN = ([regex]::Matches($all, '\[JJFB_P13_GETCHAR\] ret guest=0x[1-9A-Fa-f]')).Count
  $firstCh = ''
  $m = [regex]::Match($all, '\[JJFB_P13_GETCHAR\] call_index=1 .* ch=(0x[0-9A-Fa-f]+)')
  if ($m.Success) { $firstCh = $m.Groups[1].Value }
  $firstCjk = ''
  foreach ($mm in [regex]::Matches($all, '\[JJFB_P13_GETCHAR\] call_index=\d+ .* ch=(0x[0-9A-Fa-f]+)')) {
    $v = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($v -ge 0x80) { $firstCjk = $mm.Groups[1].Value; break }
  }
  $unimpl = [regex]::Matches($all, '\[POST_CONT_UNIMPLEMENTED_API\] api=(\S+)') | ForEach-Object { $_.Groups[1].Value }
  $unimpl = $unimpl | Select-Object -Unique
  return [ordered]@{
    case9_leave = [bool]($all -match 'CASE9_LEAVE ok=1|DELIVER_DONE ok=1.*handler=0x30D311')
    mmochat = [bool]($all -match 'module=mmochat\.ext.*state=REGISTERED')
    getchar_n = $getN
    getchar_ret_n = $retN
    first_ch = $(if ($firstCh) { $firstCh } else { 'NONE' })
    first_cjk = $(if ($firstCjk) { $firstCjk } else { 'NONE' })
    unimpl_getchar = [bool]($all -match 'mr_getCharBitmap\(\) Not yet implemented')
    dsm_reinit = ([regex]::Matches($all, 'initMemoryManager:')).Count
    draw = [bool]($all -match '\[JJFB_DRAW\]|FIRST_NATURAL_DRAW|mr_drawBitmap')
    refresh = [bool]($all -match 'FIRST_NATURAL_REFRESH|DispUpEx')
    bmp_n = ([regex]::Matches($all, '\[JJFB_BMP_REQ\]')).Count
    net = [bool]($all -match 'initNetwork|mr_initNetwork|getHostByName')
    next_unimpl = $(if ($unimpl) { ($unimpl -join '|') } else { 'none' })
    p3_fault = [bool]($all -match '\[P3_FAULT\]|FIRE_DONE ok=0')
  }
}

Write-Host '==== P13 DIAG 30s ===='
$diag = Invoke-NaturalRun -tag 'p13_diag' -seconds $DiagSeconds -WithP11
$da = Analyze-Log $diag.stdout
$da.GetEnumerator() | ForEach-Object { Write-Host ("  {0}={1}" -f $_.Key, $_.Value) }
$da | ConvertTo-Json | Set-Content (Join-Path $outDir 'p13_diag_summary.json') -Encoding utf8

if ($da.unimpl_getchar -or $da.getchar_ret_n -lt 1) {
  Write-Host '[P13] diagnose did not clear getCharBitmap — still writing matrix from available evidence'
}

'run_kind,run_id,main_sha,case9_leave,mmochat,getchar_n,getchar_ret_n,first_ch,first_cjk,unimpl_getchar,dsm_reinit,draw,refresh,bmp_n,net,next_unimpl,p3_fault,proc' |
  Set-Content $matrix -Encoding utf8

function Add-MatrixRow($kind, $runId, $a, $proc) {
  $row = @(
    $kind, $runId, $mainSha,
    $(if ($a.case9_leave) { 'yes' } else { 'no' }),
    $(if ($a.mmochat) { 'yes' } else { 'no' }),
    $a.getchar_n, $a.getchar_ret_n, $a.first_ch, $a.first_cjk,
    $(if ($a.unimpl_getchar) { 'yes' } else { 'no' }),
    $a.dsm_reinit,
    $(if ($a.draw) { 'yes' } else { 'no' }),
    $(if ($a.refresh) { 'yes' } else { 'no' }),
    $a.bmp_n,
    $(if ($a.net) { 'yes' } else { 'no' }),
    $a.next_unimpl,
    $(if ($a.p3_fault) { 'yes' } else { 'no' }),
    $proc
  ) -join ','
  Add-Content $matrix $row -Encoding utf8
}
Add-MatrixRow 'diag30' $diag.runId $da $diag.exit

Write-Host "==== P13 GOLDEN x$Runs (SkipBuild product script) ===="
for ($i = 1; $i -le $Runs; $i++) {
  if ((Get-Sha $exe) -ne $mainSha) { throw 'binary drift' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_PRODUCT_DIRECT_JJFB.ps1') `
    -Seconds $GoldenSeconds -SkipBuild -SkipVmrpBuild
  $prodExit = $LASTEXITCODE
  $stdout = Join-Path $logDir 'product_direct_jjfb_stdout.txt'
  # Also enable glyph path markers: product script clears env — copy analysis from product log + optional re-run markers
  $a = Analyze-Log $stdout
  # Product run may lack P13 markers if env cleared — check unimplemented absence + mmochat progress
  $manifest = Get-ChildItem (Join-Path $reportDir 'product_direct_jjfb_manifest_*.txt') |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
  $runId = if ($manifest) { $manifest.BaseName -replace '^product_direct_jjfb_manifest_', '' } else { "golden_$i" }
  Add-MatrixRow "golden$i" $runId $a "prod_exit=$prodExit"
  Write-Host ("golden{0} strongish_exit={1} unimpl_getchar={2} mmochat={3} next={4}" -f $i, $prodExit, $a.unimpl_getchar, $a.mmochat, $a.next_unimpl)
}

Write-Host '==== P13 VISUAL 180s ===='
$vis = Invoke-NaturalRun -tag 'p13_visual' -seconds $VisualSeconds -WithP11
$va = Analyze-Log $vis.stdout
Add-MatrixRow 'visual180' $vis.runId $va $vis.exit
$va.GetEnumerator() | ForEach-Object { Write-Host ("  {0}={1}" -f $_.Key, $_.Value) }

Write-Host "matrix=$matrix identity=$identity"
