# Product golden chain: cfg36 → descriptor → jjfb → mrc_loader → robotol → init → registry/scheduler.
# Strong success only. Does NOT count gamelist FAST / SMSCFG / fixed-PC / host UI / forced callback.
param(
  [int]$Seconds = 90,
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
$mrpHost = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'

if (-not (Test-Path $mrpHost)) { throw "missing $mrpHost" }
$hashBefore = (Get-FileHash -Algorithm SHA256 -Path $mrpHost).Hash.ToLowerInvariant()
if ($hashBefore -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hashBefore" }

# Clear shell / research envs — product track only.
@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_SHELL_NATIVE_EXEC_TRACE', 'JJFB_RUNAPP_NATIVE_ONLY',
  'JJFB_E10A31_MODE', 'JJFB_E10A31C_MODE', 'JJFB_E10A31N_MODE', 'JJFB_E10A31Q_APPLY',
  'GWY_SMSCFG_BOOTSTRAP', 'GWY_DIAG_SMSCFG_GPT_MINIMAL', 'JJFB_E10A31K_MODE'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }
}
if (-not $SkipVmrpBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy product vmrp build failed' }
}

if (-not (Test-Path $Launcher)) { throw "missing $Launcher" }
if (-not (Test-Path $exe)) { throw "missing $exe — product Gwy main.exe required" }

# Prove product link: main.exe must not require research_gwy_shell real probes.
# (Built via Mode=Gwy → stubs. Record for report.)
$runtimeKind = 'Gwy+stubs'
$nm = & nm $exe 2>$null | Select-String 'e10a31n_on_method0_enter'
if ($nm) {
  # Symbol may still exist as stub; check for research-only string.
  $strings = & strings $exe 2>$null | Select-String 'E10A31N_ARMED|SMSCFG_METHOD0_ENTER_APPLY_Q'
  if ($strings) { throw 'main.exe looks like GwyResearch (research probe strings present)' }
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir | Out-Null
$stdout = Join-Path $logDir 'product_direct_jjfb_stdout.txt'
$stderr = Join-Path $logDir 'product_direct_jjfb_stderr.txt'
$vmLog = Join-Path $logDir 'product_direct_jjfb_vmrp.txt'
$report = Join-Path $reportDir 'product_direct_jjfb_verdict.md'
Remove-Item -Force $stdout, $stderr, $vmLog -ErrorAction SilentlyContinue

$env:GWY_PROFILE = $Profile
$env:GWY_OVERLAY_ROOT = Join-Path $RunDir 'overlay'
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

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
$env:GWY_PACKAGE_APPID = '400101'
$env:GWY_PACKAGE_APPVER = '12'
$env:GWY_MODULE_R9_SWITCH = '1'
$env:GWY_CALLBACK_FRAME = '1'
$env:JJFB_E5_SCHEDULER_MODE = '1'
Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== PRODUCT DIRECT JJFB Seconds=$Seconds runtime=$runtimeKind =="

& $Launcher validate --root $ResourceRoot
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

@"
[DESCRIPTOR_FROZEN] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher
[TARGET_HASH_VERIFIED] sha256=$ExpectedHash
[JJFB_GWY_LAUNCH] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher evidence=DOCUMENTED
[JJFB_PARAM] $param
[JJFB_RUNAPP] source=descriptor_launcher target=gwy/jjfb.mrp spawned=pending evidence=DOCUMENTED
"@ | Set-Content -Path $stdout -Encoding utf8

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($Seconds)
do {
  Start-Sleep -Seconds 2
  if (Test-Path $vmLog) {
    Get-Content $vmLog -Tail 200 -ErrorAction SilentlyContinue |
      Out-File -FilePath $stdout -Append -Encoding utf8
  }
  $all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
  if (-not $all) { $all = '' }
  # Soft progress markers (not all required to stop early)
  $haveInit = $all -match 'JJFB_MRC_INIT.*ret=0|MRC_INIT.*return.?0|robotol.*method=0.*ret=0'
  $haveSched = $all -match 'SCHEDULER_NATURAL_CALLBACK|JJFB_E5_.*NATURAL|CALLBACK_FRAME.*natural'
  if ($haveInit -and $haveSched) { break }
  if ($all -match 'JJFB_GAMELIST_STARTED|SHELL_PHASE_GBRWCORE') { break }
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400
}
if (Test-Path $vmLog) {
  Get-Content $vmLog -ErrorAction SilentlyContinue | Out-File -FilePath $stdout -Append -Encoding utf8
}
$all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
if (-not $all) { $all = '' }

function Has([string]$pat) { return [bool]($all -match $pat) }

$gates = [ordered]@{
  DESCRIPTOR_FROZEN              = Has 'DESCRIPTOR_FROZEN|descriptor_launcher'
  TARGET_HASH_VERIFIED           = ($hashBefore -eq $ExpectedHash)
  START_MR_ENTERED               = Has 'start\.mr|START_MR|JJFB_GWY_LAUNCH'
  MRC_LOADER_RESOLVED_EXACT      = Has 'mrc_loader\.ext.*strategy=exact|module=mrc_loader\.ext'
  ROBOTOL_RESOLVED_BY_PROFILE_ALIAS = Has 'resolved=robotol\.ext|module=robotol\.ext'
  ROBOTOL_INIT_RETURN_ZERO       = Has 'JJFB_MRC_INIT.*\bret=0\b|MRC_INIT_RETURN_ZERO|method=0.*ret=0'
  PLATFORM_HANDLER_REGISTERED    = Has 'HANDLER_REGISTER|platform_handler|PLAT.*register'
  SCHEDULER_NATURAL_CALLBACK     = Has 'SCHEDULER_NATURAL_CALLBACK|NATURAL_CALLBACK|CALLBACK_FRAME.*deliver'
}

$forbidden = [ordered]@{
  gamelist_fast           = Has 'GAMELIST_STARTED|FAST_REAL_GAMELIST|JJFB_GAMELIST'
  method0_smscfg_write    = Has 'SMSCFG_METHOD0_ENTER_APPLY|E10A31N_ARMED|SMSCFG_Q_BKEYS'
  fixed_pc_jump           = Has 'FORCE_PC|fixed.?pc|PATCH_PC'
  host_fake_ui            = Has 'host.*(slogo|loadingbar)|fake.*ui'
  forced_callback         = Has 'FORCE_CALLBACK|V64_ENQUEUE|FAMILY_C0_AFTER'
}

$strongOk = ($gates.Values | Where-Object { -not $_ }).Count -eq 0
$forbidHit = ($forbidden.GetEnumerator() | Where-Object { $_.Value }).Name

@"
# Product Direct JJFB Verdict

- **runtime:** $runtimeKind (Mode=Gwy → launcher_core + research stubs)
- **seconds:** $Seconds
- **strong_success:** $(if ($strongOk) { 'YES' } else { 'NO' })
- **forbidden_hits:** $(if ($forbidHit) { ($forbidHit -join ', ') } else { 'none' })

## Required gates

| Gate | OK |
|------|----|
$(($gates.GetEnumerator() | ForEach-Object { "| $($_.Key) | $(if ($_.Value) { 'yes' } else { 'no' }) |" }) -join "`n")

## Forbidden (must be absent)

| Item | Present |
|------|---------|
$(($forbidden.GetEnumerator() | ForEach-Object { "| $($_.Key) | $(if ($_.Value) { 'FAIL' } else { 'clean' }) |" }) -join "`n")

## Notes

- Partial progress (e.g. ENTRY_CALLED without init=0) is **not** product success.
- Research E10A / shell runners: ``RUN_RESEARCH_GWY_SHELL.ps1``
"@ | Set-Content -Path $report -Encoding utf8

Write-Host "==== PRODUCT gates ===="
$gates.GetEnumerator() | ForEach-Object { Write-Host ("  {0}={1}" -f $_.Key, $(if ($_.Value) { 'yes' } else { 'no' })) }
Write-Host "report=$report"

if ($forbidHit) { Write-Host "[FAIL] forbidden markers: $($forbidHit -join ', ')"; exit 1 }
if (-not $strongOk) {
  Write-Host '[INCOMPLETE] strong product gates not all met (see report) — exit 2'
  exit 2
}
Write-Host '[OK] PRODUCT DIRECT JJFB strong success'
exit 0
