# Product First-Frame Push (P6–P8): Event → Resource → Validate
# Modes: Event | Resource | Validate
# Gwy+stubs only. No research lib / forced DRAW / E9 / SMSCFG / fabricated 10165.
# One-shot (JJFB_PRODUCT_P5_ONE_SHOT) is diagnostic-only — not default product behavior.
param(
  [ValidateSet('Event', 'Resource', 'Validate')]
  [string]$Mode = 'Event',
  [int]$Seconds = 50,
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild,
  [switch]$ApplyAbi
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

$modeKey = $Mode.ToLowerInvariant()
$runId = ('ffp_{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $modeKey, (Get-Date), (Get-Random -Maximum 99999))

@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_SHELL_NATIVE_EXEC_TRACE', 'JJFB_RUNAPP_NATIVE_ONLY',
  'JJFB_E10A31_MODE', 'JJFB_E10A31C_MODE', 'JJFB_E10A31N_MODE', 'JJFB_E10A31Q_APPLY',
  'GWY_SMSCFG_BOOTSTRAP', 'GWY_DIAG_SMSCFG_GPT_MINIMAL', 'JJFB_E10A31K_MODE',
  'JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE', 'JJFB_E9C_MODE', 'JJFB_E8Z_MODE',
  'JJFB_FAST_REAL_BMP_HANDLE', 'JJFB_E9B_MODE', 'JJFB_VISIBLE_WINDOW',
  'JJFB_E8E_DRAIN_ORDER', 'JJFB_E8D_10165_PROBE', 'JJFB_E8C_IDLE_WATCH',
  'JJFB_E8E_EVENT_PROBE', 'JJFB_HANDLER_FORENSIC', 'JJFB_PLAT_CENSUS',
  'JJFB_PRODUCT_P5_ONE_SHOT', 'JJFB_PRODUCT_FFP_APPLY_ABI', 'JJFB_PRODUCT_P5_MODE'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }
}
$doVmrp = -not $SkipVmrpBuild
if ($SkipBuild -and -not $PSBoundParameters.ContainsKey('SkipVmrpBuild')) { $doVmrp = $false }
if ($doVmrp) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy product vmrp build failed' }
}

if (-not (Test-Path $Launcher)) { throw "missing $Launcher" }
if (-not (Test-Path $exe)) { throw "missing $exe — product Gwy main.exe required" }

$runtimeKind = 'Gwy+stubs'
$tokenA = 'E10A31N' + '_ARMED'
$tokenB = 'SMSCFG_METHOD0' + '_ENTER_APPLY_Q'
$stringsHit = & strings $exe 2>$null | Select-String "$tokenA|$tokenB"
if ($stringsHit) { throw 'main.exe looks like GwyResearch (research probe strings present)' }

$exeHash = (Get-FileHash -Algorithm SHA256 -Path $exe).Hash.ToLowerInvariant()
$launcherHash = (Get-FileHash -Algorithm SHA256 -Path $Launcher).Hash.ToLowerInvariant()

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$evidenceDir = Join-Path $Root 'evidence'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $evidenceDir | Out-Null

$stdout = Join-Path $logDir 'product_ffp_stdout.txt'
$stderr = Join-Path $logDir 'product_ffp_stderr.txt'
$vmLog = Join-Path $logDir 'product_ffp_vmrp.txt'
$verdictPath = Join-Path $reportDir 'product_first_frame_push_verdict.md'
$manifest = Join-Path $reportDir "product_ffp_manifest_$runId.txt"
$bmpPath = Join-Path $evidenceDir 'product_ffp_first_natural_frame.bmp'
$pngPath = Join-Path $evidenceDir 'product_ffp_first_natural_frame.png'

@(
  $stdout, $stderr, $vmLog,
  (Join-Path $reportDir 'product_ffp_event_requests.csv'),
  (Join-Path $reportDir 'product_ffp_10165_objects.csv'),
  (Join-Path $reportDir 'product_ffp_guest_request_samples.csv'),
  (Join-Path $reportDir 'product_ffp_handler_mem.csv'),
  (Join-Path $reportDir 'product_ffp_family_abi_manifest.json'),
  $bmpPath, $pngPath
) | ForEach-Object { Remove-Item -Force $_ -ErrorAction SilentlyContinue }

$overlay = Join-Path $RunDir "overlay_$runId"
New-Item -ItemType Directory -Force -Path $overlay | Out-Null

@"
run_id=$runId
mode=$Mode
runtime=$runtimeKind
main_exe_sha256=$exeHash
gwy_launcher_sha256=$launcherHash
jjfb_mrp_sha256=$ExpectedHash
apply_abi=$(if ($ApplyAbi) { '1' } else { '0' })
"@ | Set-Content -Path $manifest -Encoding utf8

$env:GWY_PROFILE = $Profile
$env:GWY_OVERLAY_ROOT = $overlay
$env:GWY_PRODUCT_REPORTS_DIR = $reportDir
$env:GWY_PRODUCT_RUN_ID = $runId
$env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
$env:JJFB_PRODUCT_P3_MODE = '1'
$env:JJFB_PRODUCT_P4_MODE = '1'
$env:JJFB_PRODUCT_FFP_MODE = '1'
$env:JJFB_PRODUCT_FFP_PHASE = $modeKey
$env:JJFB_HWND_UNTIL_DISPUP = '1'
$env:JJFB_PRODUCT_P3_SCREENSHOT = $bmpPath
$env:JJFB_PRODUCT_P4_SCREENSHOT = $bmpPath
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

# Round B opt-in: apply recovered context/stack into family delivery ABI.
if ($ApplyAbi -or $Mode -eq 'Validate') {
  $env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
} else {
  Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
}

# One-shot remains diagnostic-only — never default for FFP Event Round A.
Remove-Item Env:JJFB_PRODUCT_P5_ONE_SHOT -ErrorAction SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_P5_MODE -ErrorAction SilentlyContinue

Remove-Item Env:GWY_PACKAGE_APPID -ErrorAction SilentlyContinue
Remove-Item Env:GWY_PACKAGE_APPVER -ErrorAction SilentlyContinue

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== PRODUCT FFP Mode=$Mode Seconds=$Seconds apply_abi=$($env:JJFB_PRODUCT_FFP_APPLY_ABI) runtime=$runtimeKind run_id=$runId =="

& $Launcher validate --root $ResourceRoot
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

@"
[DESCRIPTOR_FROZEN] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher run_id=$runId evidence=OBSERVED
[TARGET_HASH_VERIFIED] sha256=$ExpectedHash run_id=$runId evidence=OBSERVED
[JJFB_GWY_LAUNCH] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher run_id=$runId evidence=OBSERVED
"@ | Set-Content -Path $stdout -Encoding utf8

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds($Seconds)
$holdAfterFrame = $null
$stableDeadline = $null
do {
  Start-Sleep -Seconds 2
  $src = if (Test-Path $vmLog) { $vmLog } else { $stdout }
  $all = Get-Content $src -Raw -ErrorAction SilentlyContinue
  if (-not $all) { $all = '' }

  $haveSched = ($all -match ("\[SCHEDULER_NATURAL_CALLBACK\].*run_id=$([regex]::Escape($runId)).*forced=no.*evidence=OBSERVED")) -or
               ($all -match '\[SCHEDULER_NATURAL_CALLBACK\].*forced=no.*evidence=OBSERVED')
  $okReturns = ([regex]::Matches($all, '\[P3_CALLBACK_LEAVE\].*class=CALLBACK_RETURN_SENTINEL_OK')).Count
  if ($okReturns -eq 0) {
    $okReturns = ([regex]::Matches($all, 'FIRE_DONE.*ok=1.*uc_err=0')).Count
  }
  $ffpDone = [bool]($all -match '\[FFP_FINALIZE\]|\[PES_FINALIZE\]')
  $stateAdv = [bool]($all -match '\[ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION\]|ROBOTOL_STATE_DIGEST_CHANGED')
  $idConfirmed = [bool]($all -match '\[EVENT_TRANSACTION_IDENTITY_CONFIRMED\]')
  $samples8 = ([regex]::Matches($all, '\[FFP_GUEST_REQUEST_SAMPLE\]')).Count -ge 8
  $resReq = [bool]($all -match '\[FIRST_NATURAL_RESOURCE_REQUEST\]|\[FIRST_NATURAL_VFS_OPEN\]')
  $frameHit = $all -match '\[FIRST_NATURAL_FRAME_CAPTURED\]|\[FIRST_NATURAL_REFRESH\]'

  if ($Mode -eq 'Event') {
    # Need ≥8 guest samples (or identity + finalize) before stopping Round A.
    if ($haveSched -and $samples8 -and $idConfirmed) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(3) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
    if ($haveSched -and $stateAdv) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(4) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
  }

  if ($Mode -eq 'Resource') {
    if ($resReq -or $stateAdv) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(5) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
    if ($haveSched -and $okReturns -ge 40 -and $ffpDone) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(3) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
  }

  if ($Mode -eq 'Validate') {
    if ($frameHit -and -not $holdAfterFrame) {
      $holdAfterFrame = (Get-Date).AddSeconds(10)
      Write-Host "first natural frame observed — holding HWND >=10s"
    }
    if ($holdAfterFrame -and (Get-Date) -ge $holdAfterFrame) { break }
    if ($stateAdv -and $okReturns -ge 20) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(4) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
    if (-not $frameHit -and -not $stateAdv -and $haveSched -and $okReturns -ge 40 -and $ffpDone) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(3) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
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
  Copy-Item -Force $vmLog $stdout
} elseif (-not (Test-Path $stdout)) {
  Set-Content -Path $stdout -Value '' -Encoding utf8
}
$all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
if (-not $all) { $all = '' }

function HasExact([string]$marker) {
  $pat = '\[{0}\].*run_id={1}.*evidence=OBSERVED' -f [regex]::Escape($marker), [regex]::Escape($runId)
  return [bool]($all -match $pat)
}
function HasMarker([string]$marker) {
  if (HasExact $marker) { return $true }
  return [bool]($all -match ("\[{0}\].*evidence=OBSERVED" -f [regex]::Escape($marker)))
}

if ((Test-Path $bmpPath) -and -not (Test-Path $pngPath)) {
  try {
    Add-Type -AssemblyName System.Drawing
    $img = [System.Drawing.Image]::FromFile($bmpPath)
    $img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
  } catch {
    Write-Host "BMP→PNG convert skipped: $($_.Exception.Message)"
  }
}

$drawHit = (HasMarker 'FIRST_NATURAL_DRAW') -or ($all -match '\[JJFB_DRAW\]')
$refreshHit = (HasMarker 'FIRST_NATURAL_REFRESH') -or ($all -match '\[JJFB_REFRESH\] api=_DispUpEx')
$faultHit = [bool]($all -match 'FIRE_DONE ok=0|\[EXT_FAULT\]|\[P3_FAULT\]|uc_err=[1-9]')
if ($all -match '\[PLATFORM_FAMILY_EVENT\] op=DELIVER_DONE ok=0') {
  if (-not ($all -match '\[JJFB_LIFECYCLE\] op=FIRE_DONE.*ok=0')) {
    $faultHit = [bool]($all -match 'FIRE_DONE ok=0|\[EXT_FAULT\]|\[P3_FAULT\]')
  }
}
$fbHit = HasMarker 'FRAMEBUFFER_NONEMPTY'
$hwndHit = HasMarker 'HWND_VISIBLE'
$captureHit = (HasMarker 'FIRST_NATURAL_FRAME_CAPTURED') -or (Test-Path $pngPath) -or (Test-Path $bmpPath)
$schedHit = (HasExact 'SCHEDULER_NATURAL_CALLBACK') -or ($all -match '\[SCHEDULER_NATURAL_CALLBACK\].*forced=no.*evidence=OBSERVED')
$initHit = (HasExact 'ROBOTOL_INIT_RETURN_ZERO') -or ($all -match '\[ROBOTOL_INIT_RETURN_ZERO\].*evidence=OBSERVED')
$okLeave = ([regex]::Matches($all, '\[P3_CALLBACK_LEAVE\].*ok=1')).Count
if ($okLeave -eq 0) { $okLeave = ([regex]::Matches($all, 'FIRE_DONE.*ok=1.*uc_err=0')).Count }

$acceptN = ([regex]::Matches($all, '\[EVENT_TXN\] op=ACCEPT')).Count
$suppressN = ([regex]::Matches($all, '\[EVENT_TXN\] op=SUPPRESS|\[PLATFORM_FAMILY_EVENT\] op=SUPPRESS')).Count
$deliverN = ([regex]::Matches($all, '\[PLATFORM_FAMILY_EVENT\] op=DELIVER ')).Count
$sampleN = ([regex]::Matches($all, '\[FFP_GUEST_REQUEST_SAMPLE\]')).Count
$ctxIdent = HasMarker 'EVENT_CONTEXT_OBJECT_IDENTIFIED'
$ctxOwner = HasMarker 'EVENT_CONTEXT_OWNER_CONFIRMED'
$ctxLife = HasMarker 'EVENT_CONTEXT_LIFETIME_CONFIRMED'
$idConfirmed = HasMarker 'EVENT_TRANSACTION_IDENTITY_CONFIRMED'
$abiOk = HasMarker 'FAMILY_EVENT_ABI_CONFIRMED'
$outWrites = HasMarker 'FAMILY_HANDLER_OUTPUT_WRITES_OBSERVED'
$stateAdv = [bool]($all -match '\[ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION\]|ROBOTOL_STATE_DIGEST_CHANGED')
$sigChanged = [bool]($all -match 'ROBOTOL_CALLBACK_SIGNATURE_CHANGED')
$resReq = (HasMarker 'FIRST_NATURAL_RESOURCE_REQUEST') -or ($all -match 'FIRST_NATURAL_RESOURCE_REQUEST')
$resRead = HasMarker 'FIRST_NATURAL_RESOURCE_READ'
$dispPred = [bool]($all -match 'DISPLAY_PREDECESSOR_REACHED')
$dispUp = HasMarker 'FIRST_NATURAL_REFRESH'
$idClass = 'UNKNOWN'
if ($all -match '\[EVENT_TRANSACTION_IDENTITY_CONFIRMED\] class=(\S+)') { $idClass = $Matches[1] }
elseif ($all -match '\[PES_FINALIZE\].*identity=(\S+)') { $idClass = $Matches[1] }

$forbidden = [ordered]@{
  gamelist_fast        = [bool]($all -match 'GAMELIST_STARTED|FAST_REAL_GAMELIST|JJFB_GAMELIST')
  method0_smscfg_write = [bool]($all -match 'SMSCFG_METHOD0_ENTER_APPLY|E10A31N_ARMED|SMSCFG_Q_BKEYS')
  fixed_pc_jump        = [bool]($all -match 'FORCE_PC|PATCH_PC')
  forced_callback      = [bool]($all -match 'FORCE_CALLBACK|V64_ENQUEUE|FAMILY_C0_AFTER')
  e9_inject            = [bool]($all -match 'JJFB_E9C_MODE|e9c_present_meaningful')
  e8e_drain            = [bool]($all -match 'JJFB_E8E_DRAIN_ORDER')
  fabricated_10165     = [bool]($all -match 'FABRICATE_10165|FORCE_10165')
}
$forbidHit = ($forbidden.GetEnumerator() | Where-Object { $_.Value }).Name

$farthest = 'timer_stable_poll'
if ($captureHit -and $fbHit -and $hwndHit) { $farthest = 'first_natural_frame' }
elseif ($dispUp -or $refreshHit) { $farthest = 'disp_up_ex' }
elseif ($fbHit -or $dispPred) { $farthest = 'framebuffer_mutation' }
elseif ($resRead) { $farthest = 'resource_bytes_consumed' }
elseif ($resReq) { $farthest = 'resource_request' }
elseif ($stateAdv -or $sigChanged) { $farthest = 'robotol_state_changed' }
elseif ($abiOk) { $farthest = 'family_abi_confirmed' }
elseif ($idConfirmed) { $farthest = 'event_identity_confirmed' }
elseif ($deliverN -ge 1) { $farthest = 'family_handler_delivered' }
elseif ($acceptN -ge 1) { $farthest = 'event_accepted' }
elseif ($ctxIdent) { $farthest = '10165_object_identified' }

$lastOk = 'none'
if ($stateAdv) { $lastOk = 'ROBOTOL_STATE_DIGEST_CHANGED' }
elseif ($outWrites) { $lastOk = 'FAMILY_HANDLER_OUTPUT_WRITES_OBSERVED' }
elseif ($idConfirmed) { $lastOk = 'EVENT_TRANSACTION_IDENTITY_CONFIRMED' }
elseif ($ctxOwner) { $lastOk = 'EVENT_CONTEXT_OWNER_CONFIRMED' }
elseif ($ctxIdent) { $lastOk = 'EVENT_CONTEXT_OBJECT_IDENTIFIED' }
elseif ($deliverN -ge 1) { $lastOk = 'PLATFORM_FAMILY_EVENT_DELIVER' }
elseif ($acceptN -ge 1) { $lastOk = 'EVENT_TXN_ACCEPT' }

$firstGap = 'unknown'
if (-not $ctxIdent) { $firstGap = '10165 context object not identified' }
elseif (-not $idConfirmed) { $firstGap = 'request identity not classified across samples' }
elseif (-not $abiOk) { $firstGap = 'family handler ABI / context not proven in delivery' }
elseif (-not $stateAdv) { $firstGap = 'Robotol state unchanged after delivery' }
elseif (-not $resReq) { $firstGap = 'no natural resource request after state change' }
elseif (-not $refreshHit) { $firstGap = 'no guest _DispUpEx / framebuffer present' }
else { $firstGap = 'none' }

$verdict = 'INCOMPLETE'
if ($forbidHit -or $faultHit) {
  $verdict = 'FORBIDDEN_OR_FAULT'
} elseif ($Mode -eq 'Event') {
  if ($stateAdv -or $sigChanged) {
    $verdict = 'P6_ROBOTOL_STATE_CHANGED'
  } elseif ($abiOk -and $idConfirmed) {
    $verdict = 'P6_ABI_AND_IDENTITY_MAPPED_NO_ADVANCE'
  } elseif ($idConfirmed -and $ctxIdent -and $sampleN -ge 2 -and $deliverN -ge 1) {
    $verdict = 'P6_EVENT_PROVENANCE_MAPPED'
  } elseif ($acceptN -ge 1 -and $deliverN -ge 1) {
    $verdict = 'P6_EVENT_PARTIAL'
  } else {
    $verdict = 'INCOMPLETE'
  }
} elseif ($Mode -eq 'Resource') {
  if ($resRead -and $stateAdv) { $verdict = 'P7_RESOURCE_TRANSACTION_COMPLETE' }
  elseif ($resReq) { $verdict = 'P7_RESOURCE_REQUEST_SEEN' }
  elseif ($stateAdv) { $verdict = 'P7_WAITING_RESOURCE_AFTER_STATE' }
  else { $verdict = 'INCOMPLETE' }
} else {
  if ($schedHit -and $initHit -and $drawHit -and $refreshHit -and $fbHit -and $hwndHit -and $captureHit) {
    $verdict = 'PRODUCT_FIRST_NATURAL_FRAME_STABLE'
  } elseif ($stateAdv -and ($resReq -or $dispPred -or $refreshHit)) {
    $verdict = 'ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION'
  } elseif ($stateAdv) {
    $verdict = 'ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION'
  } elseif ($abiOk) {
    $verdict = 'FAMILY_EVENT_ABI_CONFIRMED_NO_ADVANCE_YET'
  } else {
    $verdict = 'INCOMPLETE'
  }
}

$csvReq = Join-Path $reportDir 'product_ffp_event_requests.csv'
$csv65 = Join-Path $reportDir 'product_ffp_10165_objects.csv'
$csvSamp = Join-Path $reportDir 'product_ffp_guest_request_samples.csv'
$csvMem = Join-Path $reportDir 'product_ffp_handler_mem.csv'
$abiJson = Join-Path $reportDir 'product_ffp_family_abi_manifest.json'

@"
# Product First-Frame Push Verdict

- **run_id:** $runId
- **mode:** $Mode
- **verdict:** $verdict
- **runtime:** $runtimeKind
- **seconds:** $Seconds
- **process_exit:** $procExit
- **apply_abi:** $(if ($env:JJFB_PRODUCT_FFP_APPLY_ABI -eq '1') { 'yes' } else { 'no' })
- **ok_callback_returns:** $okLeave

## Farthest natural milestone

- **farthest:** $farthest
- **last_successful_transaction:** $lastOk
- **first_unmet_platform_contract:** $firstGap

## Event / ABI

- **guest request samples:** $sampleN
- **EVENT_TXN ACCEPT:** $acceptN
- **FAMILY DELIVER:** $deliverN
- **SUPPRESS:** $suppressN
- **identity_class:** $idClass
- **EVENT_TRANSACTION_IDENTITY_CONFIRMED:** $(if ($idConfirmed) { 'yes' } else { 'no' })
- **EVENT_CONTEXT_OBJECT_IDENTIFIED:** $(if ($ctxIdent) { 'yes' } else { 'no' })
- **EVENT_CONTEXT_OWNER_CONFIRMED:** $(if ($ctxOwner) { 'yes' } else { 'no' })
- **EVENT_CONTEXT_LIFETIME_CONFIRMED:** $(if ($ctxLife) { 'yes' } else { 'no' })
- **FAMILY_EVENT_ABI_CONFIRMED:** $(if ($abiOk) { 'yes' } else { 'no' })
- **FAMILY_HANDLER_OUTPUT_WRITES_OBSERVED:** $(if ($outWrites) { 'yes' } else { 'no' })
- **real state change:** $(if ($stateAdv) { 'yes' } else { 'no' })
- **callback signature change:** $(if ($sigChanged) { 'yes' } else { 'no' })

## Resource / display

- **resource request:** $(if ($resReq) { 'yes' } else { 'no' })
- **resource read:** $(if ($resRead) { 'yes' } else { 'no' })
- **framebuffer modified:** $(if ($fbHit) { 'yes' } else { 'no' })
- **_DispUpEx called:** $(if ($dispUp -or $refreshHit) { 'yes' } else { 'no' })
- **first frame:** $(if ($captureHit) { 'yes' } else { 'no' })
- **hwnd_visible:** $(if ($hwndHit) { 'yes' } else { 'no' })

## Gates

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | $(if ($schedHit) { 'yes' } else { 'no' }) |
| ROBOTOL_INIT_RETURN_ZERO | $(if ($initHit) { 'yes' } else { 'no' }) |
| EVENT samples / identity | $(if ($sampleN -ge 2 -or $idConfirmed) { 'yes' } else { 'no' }) |
| FAMILY DELIVER | $(if ($deliverN -ge 1) { 'yes' } else { 'no' }) |
| ROBOTOL_STATE_ADVANCED | $(if ($stateAdv) { 'yes' } else { 'no' }) |
| FIRST_NATURAL_REFRESH | $(if ($refreshHit) { 'yes' } else { 'no' }) |
| FRAMEBUFFER_NONEMPTY | $(if ($fbHit) { 'yes' } else { 'no' }) |
| HWND_VISIBLE | $(if ($hwndHit) { 'yes' } else { 'no' }) |

## Artifacts

- **manifest:** $manifest
- **csv_requests:** $(if (Test-Path $csvReq) { $csvReq } else { 'missing' })
- **csv_10165:** $(if (Test-Path $csv65) { $csv65 } else { 'missing' })
- **csv_samples:** $(if (Test-Path $csvSamp) { $csvSamp } else { 'missing' })
- **csv_mem:** $(if (Test-Path $csvMem) { $csvMem } else { 'missing' })
- **abi_manifest:** $(if (Test-Path $abiJson) { $abiJson } else { 'missing' })
- **forbidden_hits:** $(if ($forbidHit) { ($forbidHit -join ', ') } else { 'none' })

## Discipline

- Event Round A: collect identity + 10165 object + handler ABI (no one-shot default)
- Event Round B: ``-ApplyAbi`` once after provenance
- Resource/Validate auto-continue when state advances / display predecessor reached
- Forbidden: fixed PC, Robotol flag writes, fabricated 10165, forced DispUpEx, E9/E10A
"@ | Set-Content -Path $verdictPath -Encoding utf8

Write-Host "verdict=$verdict farthest=$farthest identity=$idClass state_adv=$stateAdv"
Write-Host "report=$verdictPath"

$okModes = @(
  'P6_EVENT_PROVENANCE_MAPPED',
  'P6_EVENT_PARTIAL',
  'P6_ABI_AND_IDENTITY_MAPPED_NO_ADVANCE',
  'P6_ROBOTOL_STATE_CHANGED',
  'P7_RESOURCE_REQUEST_SEEN',
  'P7_RESOURCE_TRANSACTION_COMPLETE',
  'P7_WAITING_RESOURCE_AFTER_STATE',
  'ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION',
  'PRODUCT_FIRST_NATURAL_FRAME_STABLE',
  'FAMILY_EVENT_ABI_CONFIRMED_NO_ADVANCE_YET'
)
if ($okModes -contains $verdict -and -not $forbidHit) { exit 0 }
exit 1
