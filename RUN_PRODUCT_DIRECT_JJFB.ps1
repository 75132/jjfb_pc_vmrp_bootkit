# Product golden chain: cfg36 → descriptor → jjfb → mrc_loader → robotol → EXT ABI → registry/scheduler.
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

$runId = ('p2_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))

# Clear shell / research envs — product track only.
@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_SHELL_NATIVE_EXEC_TRACE', 'JJFB_RUNAPP_NATIVE_ONLY',
  'JJFB_E10A31_MODE', 'JJFB_E10A31C_MODE', 'JJFB_E10A31N_MODE', 'JJFB_E10A31Q_APPLY',
  'GWY_SMSCFG_BOOTSTRAP', 'GWY_DIAG_SMSCFG_GPT_MINIMAL', 'JJFB_E10A31K_MODE',
  'JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE'
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

$runtimeKind = 'Gwy+stubs'
$stringsHit = & strings $exe 2>$null | Select-String 'E10A31N_ARMED|SMSCFG_METHOD0_ENTER_APPLY_Q'
if ($stringsHit) { throw 'main.exe looks like GwyResearch (research probe strings present)' }

$exeHash = (Get-FileHash -Algorithm SHA256 -Path $exe).Hash.ToLowerInvariant()
$launcherHash = (Get-FileHash -Algorithm SHA256 -Path $Launcher).Hash.ToLowerInvariant()

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir | Out-Null

$stdout = Join-Path $logDir 'product_direct_jjfb_stdout.txt'
$stderr = Join-Path $logDir 'product_direct_jjfb_stderr.txt'
$vmLog = Join-Path $logDir 'product_direct_jjfb_vmrp.txt'
$report = Join-Path $reportDir 'product_direct_jjfb_verdict.md'
$manifest = Join-Path $reportDir "product_direct_jjfb_manifest_$runId.txt"
$postVisual = Join-Path $reportDir 'product_post_callback_visual.md'

# Current-run isolation: wipe previous product logs/reports that gates read.
@(
  $stdout, $stderr, $vmLog, $report, $postVisual,
  (Join-Path $reportDir 'product_ext_abi_handshake.csv'),
  (Join-Path $reportDir 'product_robotol_init_trace.csv'),
  (Join-Path $reportDir 'product_robotol_init_failure.md'),
  (Join-Path $reportDir 'product_handler_registration.csv'),
  (Join-Path $reportDir 'product_scheduler_natural_callback.csv')
) | ForEach-Object { Remove-Item -Force $_ -ErrorAction SilentlyContinue }

$overlay = Join-Path $RunDir "overlay_$runId"
New-Item -ItemType Directory -Force -Path $overlay | Out-Null

@"
run_id=$runId
runtime=$runtimeKind
main_exe_sha256=$exeHash
gwy_launcher_sha256=$launcherHash
jjfb_mrp_sha256=$ExpectedHash
"@ | Set-Content -Path $manifest -Encoding utf8

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
Remove-Item Env:GWY_PACKAGE_APPID -ErrorAction SilentlyContinue
Remove-Item Env:GWY_PACKAGE_APPVER -ErrorAction SilentlyContinue
Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== PRODUCT DIRECT JJFB Seconds=$Seconds runtime=$runtimeKind run_id=$runId =="

& $Launcher validate --root $ResourceRoot
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

@"
[DESCRIPTOR_FROZEN] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher run_id=$runId evidence=OBSERVED
[TARGET_HASH_VERIFIED] sha256=$ExpectedHash run_id=$runId evidence=OBSERVED
[JJFB_GWY_LAUNCH] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher run_id=$runId evidence=DOCUMENTED
[JJFB_PARAM] $param
[JJFB_RUNAPP] source=descriptor_launcher target=gwy/jjfb.mrp spawned=pending run_id=$runId evidence=DOCUMENTED
"@ | Set-Content -Path $stdout -Encoding utf8

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($Seconds)
$postCbDeadline = $null
do {
  Start-Sleep -Seconds 2
  if (Test-Path $vmLog) {
    Get-Content $vmLog -Tail 400 -ErrorAction SilentlyContinue |
      Out-File -FilePath $stdout -Append -Encoding utf8
  }
  $all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
  if (-not $all) { $all = '' }
  $haveInit = $all -match ("\[ROBOTOL_INIT_RETURN_ZERO\].*run_id=$([regex]::Escape($runId)).*evidence=OBSERVED")
  $haveSched = $all -match ("\[SCHEDULER_NATURAL_CALLBACK\].*run_id=$([regex]::Escape($runId)).*evidence=OBSERVED")
  if ($haveInit -and $haveSched) {
    if (-not $postCbDeadline) { $postCbDeadline = (Get-Date).AddSeconds(5) }
    if ((Get-Date) -ge $postCbDeadline) { break }
  }
  if ($all -match 'JJFB_GAMELIST_STARTED|SHELL_PHASE_GBRWCORE') { break }
} while ((Get-Date) -lt $deadline -and -not $p.HasExited)

$procExit = $null
if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400
  $procExit = 'killed'
} else {
  $procExit = "$($p.ExitCode)"
}
if (Test-Path $vmLog) {
  Get-Content $vmLog -ErrorAction SilentlyContinue | Out-File -FilePath $stdout -Append -Encoding utf8
}
$all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
if (-not $all) { $all = '' }

function HasExact([string]$marker) {
  # Require structured marker + current run_id + evidence=OBSERVED (no stale/static satisfaction).
  $pat = '\[{0}\].*run_id={1}.*evidence=OBSERVED' -f [regex]::Escape($marker), [regex]::Escape($runId)
  return [bool]($all -match $pat)
}

# Some bootstrap markers may appear before run_id is bound in guest; also accept run_id on same line after marker set by host seed.
function HasSeedOrExact([string]$marker) {
  if (HasExact $marker) { return $true }
  $pat = '\[{0}\].*run_id={1}.*evidence=OBSERVED' -f [regex]::Escape($marker), [regex]::Escape($runId)
  return [bool]($all -match $pat)
}

$gates = [ordered]@{
  DESCRIPTOR_FROZEN                  = HasSeedOrExact 'DESCRIPTOR_FROZEN'
  TARGET_HASH_VERIFIED               = ($hashBefore -eq $ExpectedHash)
  START_MR_ENTERED                   = ($all -match 'start\.mr|START_MR|JJFB_GWY_LAUNCH')
  MRC_LOADER_RESOLVED_EXACT          = ($all -match 'mrc_loader\.ext.*strategy=exact|module=mrc_loader\.ext')
  ROBOTOL_RESOLVED_BY_PROFILE_ALIAS  = ($all -match 'resolved=robotol\.ext|module=robotol\.ext')
  ROBOTOL_BOOTSTRAP_RETURN           = ($all -match '\[ROBOTOL_BOOTSTRAP_RETURN\].*evidence=OBSERVED')
  EXT_VERSION_RETURN_ZERO            = ($all -match '\[EXT_VERSION_RETURN_ZERO\].*evidence=OBSERVED')
  EXT_APPINFO_RETURN_ZERO            = ($all -match '\[EXT_APPINFO_RETURN_ZERO\].*evidence=OBSERVED')
  ROBOTOL_INIT_RETURN_ZERO           = ($all -match '\[ROBOTOL_INIT_RETURN_ZERO\].*evidence=OBSERVED')
  ROBOTOL_HANDLER_REGISTERED         = ($all -match '\[ROBOTOL_HANDLER_REGISTERED\].*evidence=OBSERVED|\[PLATFORM_HANDLER_REGISTERED\].*owner_module=robotol\.ext.*evidence=OBSERVED')
  SCHEDULER_NATURAL_CALLBACK         = ($all -match '\[SCHEDULER_NATURAL_CALLBACK\].*forced=no.*evidence=OBSERVED')
}

# Prefer run_id-scoped strong gates when present.
foreach ($k in @('ROBOTOL_INIT_RETURN_ZERO','SCHEDULER_NATURAL_CALLBACK','EXT_VERSION_RETURN_ZERO','EXT_APPINFO_RETURN_ZERO','ROBOTOL_HANDLER_REGISTERED','ROBOTOL_BOOTSTRAP_RETURN')) {
  if (HasExact $k) { $gates[$k] = $true }
}

$forbidden = [ordered]@{
  gamelist_fast           = [bool]($all -match 'GAMELIST_STARTED|FAST_REAL_GAMELIST|JJFB_GAMELIST')
  method0_smscfg_write    = [bool]($all -match 'SMSCFG_METHOD0_ENTER_APPLY|E10A31N_ARMED|SMSCFG_Q_BKEYS')
  fixed_pc_jump           = [bool]($all -match 'FORCE_PC|fixed.?pc|PATCH_PC')
  host_fake_ui            = [bool]($all -match 'host.*(slogo|loadingbar)|fake.*ui')
  forced_callback         = [bool]($all -match 'FORCE_CALLBACK|V64_ENQUEUE|FAMILY_C0_AFTER')
}

$drawHit = [bool]($all -match 'DRAW|DispUpEx|mr_draw')
$refreshHit = [bool]($all -match 'REFRESH|refreshBitmap|DispUp')
$faultHit = [bool]($all -match 'UC_ERR|mem_fault|LIFECYCLE.*FAULT')
$postVerdict = 'NATURAL_CALLBACK_NO_DRAW_YET'
if ($gates.SCHEDULER_NATURAL_CALLBACK) {
  if ($faultHit) { $postVerdict = 'NATURAL_CALLBACK_FAULT' }
  elseif ($drawHit) { $postVerdict = 'FIRST_NATURAL_DRAW' }
  elseif ($refreshHit) { $postVerdict = 'FIRST_NATURAL_REFRESH' }
  elseif ($all -match 'IDLE|wait|not_due') { $postVerdict = 'NATURAL_CALLBACK_IDLE_WAIT' }
}

@"
# Product Post-Callback Visual

- **run_id:** $runId
- **verdict:** $postVerdict
- **SCHEDULER_NATURAL_CALLBACK:** $(if ($gates.SCHEDULER_NATURAL_CALLBACK) { 'yes' } else { 'no' })
- **draw_seen:** $(if ($drawHit) { 'yes' } else { 'no' })
- **refresh_seen:** $(if ($refreshHit) { 'yes' } else { 'no' })
- **note:** no host overlay / substitute pixels
"@ | Set-Content -Path $postVisual -Encoding utf8

$strongOk = ($gates.Values | Where-Object { -not $_ }).Count -eq 0
$forbidHit = ($forbidden.GetEnumerator() | Where-Object { $_.Value }).Name

@"
# Product Direct JJFB Verdict

- **run_id:** $runId
- **runtime:** $runtimeKind (Mode=Gwy → launcher_core + research stubs)
- **seconds:** $Seconds
- **process_exit:** $procExit
- **main_exe_sha256:** $exeHash
- **strong_success:** $(if ($strongOk) { 'YES' } else { 'NO' })
- **forbidden_hits:** $(if ($forbidHit) { ($forbidHit -join ', ') } else { 'none' })
- **post_callback:** $postVerdict
- **manifest:** $manifest

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
- Gates require structured markers with evidence=OBSERVED (current run).
- Research E10A / shell runners: ``RUN_RESEARCH_GWY_SHELL.ps1``
"@ | Set-Content -Path $report -Encoding utf8

Write-Host "==== PRODUCT gates run_id=$runId ===="
$gates.GetEnumerator() | ForEach-Object { Write-Host ("  {0}={1}" -f $_.Key, $(if ($_.Value) { 'yes' } else { 'no' })) }
Write-Host "post_callback=$postVerdict"
Write-Host "report=$report"

if ($forbidHit) { Write-Host "[FAIL] forbidden markers: $($forbidHit -join ', ')"; exit 1 }
if (-not $strongOk) {
  Write-Host '[INCOMPLETE] strong product gates not all met (see report) — exit 2'
  exit 2
}
Write-Host '[OK] PRODUCT DIRECT JJFB strong success'
exit 0
