# Product P5: event transaction semantics → advance Robotol past stable poll.
# Modes: transaction | validate
# Gwy+stubs only. No research lib / forced DRAW / E9 / SMSCFG / fabricated 10165.
param(
  [ValidateSet('transaction', 'validate')]
  [string]$Mode = 'transaction',
  [int]$Seconds = 45,
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

$runId = ('p5_{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $Mode, (Get-Date), (Get-Random -Maximum 99999))

@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_SHELL_NATIVE_EXEC_TRACE', 'JJFB_RUNAPP_NATIVE_ONLY',
  'JJFB_E10A31_MODE', 'JJFB_E10A31C_MODE', 'JJFB_E10A31N_MODE', 'JJFB_E10A31Q_APPLY',
  'GWY_SMSCFG_BOOTSTRAP', 'GWY_DIAG_SMSCFG_GPT_MINIMAL', 'JJFB_E10A31K_MODE',
  'JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE', 'JJFB_E9C_MODE', 'JJFB_E8Z_MODE',
  'JJFB_FAST_REAL_BMP_HANDLE', 'JJFB_E9B_MODE', 'JJFB_VISIBLE_WINDOW',
  'JJFB_E8E_DRAIN_ORDER', 'JJFB_E8D_10165_PROBE', 'JJFB_E8C_IDLE_WATCH',
  'JJFB_E8E_EVENT_PROBE', 'JJFB_HANDLER_FORENSIC', 'JJFB_PLAT_CENSUS',
  'JJFB_PRODUCT_P5_ONE_SHOT'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }
}
# -SkipBuild also skips vmrp unless explicitly rebuilding with -SkipVmrpBuild:$false
$doVmrp = -not $SkipVmrpBuild
if ($SkipBuild -and -not $PSBoundParameters.ContainsKey('SkipVmrpBuild')) { $doVmrp = $false }
if ($doVmrp) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy product vmrp build failed' }
}

if (-not (Test-Path $Launcher)) { throw "missing $Launcher" }
if (-not (Test-Path $exe)) { throw "missing $exe — product Gwy main.exe required" }

$runtimeKind = 'Gwy+stubs'
# Split tokens so product binary gate does not false-positive on this script/string table.
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

$stdout = Join-Path $logDir 'product_p5_stdout.txt'
$stderr = Join-Path $logDir 'product_p5_stderr.txt'
$vmLog = Join-Path $logDir 'product_p5_vmrp.txt'
$stageReport = Join-Path $reportDir 'stage_product_p5_event_advance.md'
$manifest = Join-Path $reportDir "product_p5_manifest_$runId.txt"
$bmpPath = Join-Path $evidenceDir 'product_p5_first_natural_frame.bmp'
$pngPath = Join-Path $evidenceDir 'product_p5_first_natural_frame.png'

@(
  $stdout, $stderr, $vmLog,
  (Join-Path $reportDir 'product_p5_event_transaction.csv'),
  (Join-Path $reportDir 'product_p5_family_handler_state.csv'),
  (Join-Path $reportDir 'product_p5_10165_provenance.csv'),
  (Join-Path $reportDir 'product_p5_post_completion_progress.csv'),
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
"@ | Set-Content -Path $manifest -Encoding utf8

$env:GWY_PROFILE = $Profile
$env:GWY_OVERLAY_ROOT = $overlay
$env:GWY_PRODUCT_REPORTS_DIR = $reportDir
$env:GWY_PRODUCT_RUN_ID = $runId
$env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
$env:JJFB_PRODUCT_P3_MODE = '1'
$env:JJFB_PRODUCT_P4_MODE = '1'
$env:JJFB_PRODUCT_P5_MODE = '1'
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

# Round A observes duplicates; Round B validate enables one-shot completion.
if ($Mode -eq 'validate') {
  $env:JJFB_PRODUCT_P5_ONE_SHOT = '1'
} else {
  Remove-Item Env:JJFB_PRODUCT_P5_ONE_SHOT -ErrorAction SilentlyContinue
}

Remove-Item Env:GWY_PACKAGE_APPID -ErrorAction SilentlyContinue
Remove-Item Env:GWY_PACKAGE_APPVER -ErrorAction SilentlyContinue

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== PRODUCT P5 Mode=$Mode Seconds=$Seconds runtime=$runtimeKind run_id=$runId =="

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
  # Prefer live vmlog (avoid UTF-16 append corruption from Out-File).
  $src = if (Test-Path $vmLog) { $vmLog } else { $stdout }
  $all = Get-Content $src -Raw -ErrorAction SilentlyContinue
  if (-not $all) { $all = '' }

  $haveSched = ($all -match ("\[SCHEDULER_NATURAL_CALLBACK\].*run_id=$([regex]::Escape($runId)).*forced=no.*evidence=OBSERVED")) -or
               ($all -match '\[SCHEDULER_NATURAL_CALLBACK\].*forced=no.*evidence=OBSERVED')
  $okReturns = ([regex]::Matches($all, '\[P3_CALLBACK_LEAVE\].*class=CALLBACK_RETURN_SENTINEL_OK')).Count
  if ($okReturns -eq 0) {
    $okReturns = ([regex]::Matches($all, 'FIRE_DONE.*ok=1.*uc_err=0')).Count
  }
  $p5Done = [bool]($all -match '\[P5_FINALIZE\]')
  $stateAdv = [bool]($all -match '\[ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION\]')

  if ($Mode -eq 'transaction') {
    if ($haveSched -and ($okReturns -ge 24 -or $p5Done)) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(3) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
  }

  if ($Mode -eq 'validate') {
    $frameHit = $all -match '\[FIRST_NATURAL_FRAME_CAPTURED\]|\[FIRST_NATURAL_REFRESH\]'
    if ($frameHit -and -not $holdAfterFrame) {
      $holdAfterFrame = (Get-Date).AddSeconds(10)
      Write-Host "first natural frame observed — holding HWND >=10s"
    }
    if ($holdAfterFrame -and (Get-Date) -ge $holdAfterFrame) { break }
    if ($stateAdv -and $okReturns -ge 20) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(4) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
    if (-not $frameHit -and -not $stateAdv -and $haveSched -and $okReturns -ge 40 -and $p5Done) {
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

$acceptN = ([regex]::Matches($all, '\[P5_TXN\] op=ACCEPT')).Count
$suppressN = ([regex]::Matches($all, '\[P5_TXN\] op=SUPPRESS_DUP|\[PLATFORM_FAMILY_EVENT\] op=SUPPRESS')).Count
$deliverN = ([regex]::Matches($all, '\[PLATFORM_FAMILY_EVENT\] op=DELIVER ')).Count
$guestReissue = [bool]($all -match 'GUEST_REISSUED_REQUEST|verdict=GUEST_REISSUED')
$hostReplay = [bool]($all -match 'HOST_REPLAYED_COMPLETION_FOUND')
$oneShot = [bool]($all -match 'EVENT_TRANSACTION_ONE_SHOT|EVENT_TRANSACTION_LIFETIME_FIXED')
$stateAdv = [bool]($all -match '\[ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION\]|ROBOTOL_STATE_DIGEST_CHANGED')
$falseAck = [bool]($all -match 'FAMILY_EVENT_FALSE_ACK')
$ackNoChange = [bool]($all -match 'FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE')
$abiInc = [bool]($all -match 'FAMILY_EVENT_ABI_INCOMPLETE')
$abiOk = [bool]($all -match 'FAMILY_EVENT_ABI_CONFIRMED')
$dupComp = [bool]($all -match 'FAMILY_EVENT_DUPLICATE_COMPLETION')
$role10165 = 'UNKNOWN'
if ($all -match 'role10165=(\S+)') { $role10165 = $Matches[1] }
elseif ($all -match 'SECOND_STAGE_COMPLETION_MISSING') { $role10165 = 'SECOND_STAGE_COMPLETION_MISSING' }
elseif ($all -match 'HANDLER_10165_NOT_REQUIRED') { $role10165 = 'HANDLER_10165_NOT_REQUIRED' }
$familyClass = 'UNKNOWN'
if ($all -match '\[P5_FINALIZE\].*family=(\S+)') { $familyClass = $Matches[1] }
$txnVerdict = 'UNKNOWN'
if ($all -match '\[P5_FINALIZE\].*txn=(\S+)') { $txnVerdict = $Matches[1] }
$resReq = (HasMarker 'FIRST_NATURAL_RESOURCE_REQUEST') -or ($all -match 'FIRST_NATURAL_RESOURCE_REQUEST')
$dispPred = [bool]($all -match 'DISPLAY_PREDECESSOR_REACHED')

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

$csvTxn = Join-Path $reportDir 'product_p5_event_transaction.csv'
$csvFam = Join-Path $reportDir 'product_p5_family_handler_state.csv'
$csv65 = Join-Path $reportDir 'product_p5_10165_provenance.csv'
$csvProg = Join-Path $reportDir 'product_p5_post_completion_progress.csv'

$verdict = 'INCOMPLETE'
if ($forbidHit -or $faultHit) {
  $verdict = 'FORBIDDEN_OR_FAULT'
} elseif ($Mode -eq 'transaction') {
  if ($acceptN -ge 1 -and $deliverN -ge 1 -and ($guestReissue -or $hostReplay -or $ackNoChange -or $falseAck -or $abiInc -or $dupComp)) {
    $verdict = 'P5_TRANSACTION_PROVENANCE_MAPPED'
  } elseif ($acceptN -ge 1 -and $deliverN -ge 1) {
    $verdict = 'P5_TRANSACTION_PARTIAL'
  } else {
    $verdict = 'INCOMPLETE'
  }
} else {
  # validate
  if ($schedHit -and $initHit -and $drawHit -and $refreshHit -and $fbHit -and $hwndHit -and $captureHit) {
    $verdict = 'PRODUCT_FIRST_NATURAL_FRAME_STABLE'
  } elseif ($stateAdv -and ($resReq -or $dispPred -or $refreshHit)) {
    $verdict = 'ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION'
  } elseif ($stateAdv) {
    $verdict = 'ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION'
  } elseif ($oneShot -and $suppressN -gt 0 -and -not $stateAdv) {
    $verdict = 'EVENT_TRANSACTION_LIFETIME_FIXED_NO_ADVANCE_YET'
  } elseif ($abiOk -and -not $stateAdv) {
    $verdict = 'FAMILY_EVENT_ABI_CONFIRMED_NO_ADVANCE_YET'
  } else {
    $verdict = 'INCOMPLETE'
  }
}

@"
# Stage Product P5 Event Advance

- **run_id:** $runId
- **mode:** $Mode
- **verdict:** $verdict
- **runtime:** $runtimeKind
- **seconds:** $Seconds
- **process_exit:** $procExit
- **ok_callback_returns:** $okLeave
- **P5_TXN ACCEPT count:** $acceptN
- **completion DELIVER count:** $deliverN
- **SUPPRESS count:** $suppressN
- **family_class:** $familyClass
- **txn_verdict:** $txnVerdict
- **role_10165:** $role10165
- **GUEST_REISSUED_REQUEST:** $(if ($guestReissue) { 'yes' } else { 'no' })
- **HOST_REPLAYED_COMPLETION:** $(if ($hostReplay) { 'yes' } else { 'no' })
- **ONE_SHOT / LIFETIME_FIXED:** $(if ($oneShot) { 'yes' } else { 'no' })
- **ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION:** $(if ($stateAdv) { 'yes' } else { 'no' })
- **FAMILY_EVENT_FALSE_ACK:** $(if ($falseAck) { 'yes' } else { 'no' })
- **FAMILY_EVENT_ACK_WITHOUT_STATE_CHANGE:** $(if ($ackNoChange) { 'yes' } else { 'no' })
- **FAMILY_EVENT_ABI_INCOMPLETE:** $(if ($abiInc) { 'yes' } else { 'no' })
- **FAMILY_EVENT_ABI_CONFIRMED:** $(if ($abiOk) { 'yes' } else { 'no' })
- **FIRST_NATURAL_RESOURCE_REQUEST:** $(if ($resReq) { 'yes' } else { 'no' })
- **DISPLAY_PREDECESSOR_REACHED:** $(if ($dispPred) { 'yes' } else { 'no' })
- **draw_seen:** $(if ($drawHit) { 'yes' } else { 'no' })
- **refresh_seen:** $(if ($refreshHit) { 'yes' } else { 'no' })
- **framebuffer_nonempty:** $(if ($fbHit) { 'yes' } else { 'no' })
- **hwnd_visible:** $(if ($hwndHit) { 'yes' } else { 'no' })
- **frame_captured:** $(if ($captureHit) { 'yes' } else { 'no' })
- **forbidden_hits:** $(if ($forbidHit) { ($forbidHit -join ', ') } else { 'none' })
- **manifest:** $manifest
- **csv_txn:** $(if (Test-Path $csvTxn) { $csvTxn } else { 'missing' })
- **csv_family:** $(if (Test-Path $csvFam) { $csvFam } else { 'missing' })
- **csv_10165:** $(if (Test-Path $csv65) { $csv65 } else { 'missing' })
- **csv_progress:** $(if (Test-Path $csvProg) { $csvProg } else { 'missing' })

## Exact marker gates

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | $(if ($schedHit) { 'yes' } else { 'no' }) |
| ROBOTOL_INIT_RETURN_ZERO | $(if ($initHit) { 'yes' } else { 'no' }) |
| P5_TXN ACCEPT | $(if ($acceptN -ge 1) { 'yes' } else { 'no' }) |
| FAMILY DELIVER | $(if ($deliverN -ge 1) { 'yes' } else { 'no' }) |
| ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION | $(if ($stateAdv) { 'yes' } else { 'no' }) |
| FIRST_NATURAL_REFRESH | $(if ($refreshHit) { 'yes' } else { 'no' }) |
| FRAMEBUFFER_NONEMPTY | $(if ($fbHit) { 'yes' } else { 'no' }) |
| HWND_VISIBLE | $(if ($hwndHit) { 'yes' } else { 'no' }) |

## Notes

- Round A (transaction): observe one-shot vs guest reissue vs ABI; do not enable ONE_SHOT
- Round B (validate): enable JJFB_PRODUCT_P5_ONE_SHOT=1 after provenance proves host/guest duplicate
- Do not fabricate 10165; HWND reveal is separate from event fix
"@ | Set-Content -Path $stageReport -Encoding utf8

Write-Host "verdict=$verdict family=$familyClass txn=$txnVerdict role10165=$role10165 state_adv=$stateAdv"
Write-Host "report=$stageReport"

$okModes = @(
  'P5_TRANSACTION_PROVENANCE_MAPPED',
  'P5_TRANSACTION_PARTIAL',
  'ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION',
  'PRODUCT_FIRST_NATURAL_FRAME_STABLE',
  'EVENT_TRANSACTION_LIFETIME_FIXED_NO_ADVANCE_YET',
  'FAMILY_EVENT_ABI_CONFIRMED_NO_ADVANCE_YET'
)
if ($okModes -contains $verdict -and -not $forbidHit) { exit 0 }
exit 1
