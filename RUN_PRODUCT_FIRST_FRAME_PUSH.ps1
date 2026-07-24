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
  [switch]$ApplyAbi,
  [switch]$Trace305E09,
  [switch]$TraceQueueBootstrap,
  [switch]$TraceNodeAlloc,
  [switch]$TraceQueueConsumer,
  [switch]$TracePostDrainGate,
  [switch]$TraceB71Dispatch
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
  'JJFB_PRODUCT_P5_ONE_SHOT', 'JJFB_PRODUCT_FFP_APPLY_ABI', 'JJFB_PRODUCT_P5_MODE',
  'JJFB_PRODUCT_TRACE_305E09', 'JJFB_PRODUCT_EVENT_CONTRACT',
  'JJFB_PRODUCT_TRACE_QUEUE_BOOTSTRAP', 'JJFB_PRODUCT_TRACE_NODE_ALLOC',
  'JJFB_PRODUCT_TRACE_QUEUE_CONSUMER', 'JJFB_POST_DRAIN_GATE_TRACE',
  'JJFB_B71_DISPATCH_TRACE',
  'JJFB_101AB_EMPTY', 'JJFB_101AB_WITH_RECORD'
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
# TraceQueueConsumer also needs Path-A + B54 ready to observe drain.
if ($ApplyAbi -or $Mode -eq 'Validate' -or $TraceQueueConsumer) {
  $env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
} else {
  Remove-Item Env:JJFB_PRODUCT_FFP_APPLY_ABI -ErrorAction SilentlyContinue
}

# Event Completion Contract Closure (not P6c): Trace 0x305E09 + Path-A publish.
# Internal phase=EventContract; user-facing mode remains Event/Resource/Validate.
if ($Trace305E09 -or $Mode -eq 'Validate' -or $Mode -eq 'Event') {
  $env:JJFB_PRODUCT_EVENT_CONTRACT = '1'
  $env:JJFB_PRODUCT_TRACE_305E09 = '1'
} else {
  Remove-Item Env:JJFB_PRODUCT_EVENT_CONTRACT -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_PRODUCT_TRACE_305E09 -ErrorAction SilentlyContinue
}

# Event Queue Bootstrap Closure: normalize B54/owner-store + write history.
if ($TraceQueueBootstrap -or $Mode -eq 'Validate' -or $TraceQueueConsumer) {
  $env:JJFB_PRODUCT_TRACE_QUEUE_BOOTSTRAP = '1'
} else {
  Remove-Item Env:JJFB_PRODUCT_TRACE_QUEUE_BOOTSTRAP -ErrorAction SilentlyContinue
}

# List-Node Allocation Contract Closure: classify 0x94E40 + first causal zero.
if ($TraceNodeAlloc -or $Mode -eq 'Validate') {
  $env:JJFB_PRODUCT_TRACE_NODE_ALLOC = '1'
} else {
  Remove-Item Env:JJFB_PRODUCT_TRACE_NODE_ALLOC -ErrorAction SilentlyContinue
}

# Event Queue Consumption Closure: enqueue timeline + natural drain trigger.
if ($TraceQueueConsumer -or $Mode -eq 'Validate' -or $Mode -eq 'Event') {
  $env:JJFB_PRODUCT_TRACE_QUEUE_CONSUMER = '1'
  $env:JJFB_PRODUCT_TRACE_NODE_ALLOC = '1'
} else {
  Remove-Item Env:JJFB_PRODUCT_TRACE_QUEUE_CONSUMER -ErrorAction SilentlyContinue
}

# Post-Drain Gate Contract Closure: candidate writers + ER_RW+15D/B71 watch (observe-only).
if ($TracePostDrainGate -or $Mode -eq 'Validate' -or $Mode -eq 'Event') {
  $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
} else {
  Remove-Item Env:JJFB_POST_DRAIN_GATE_TRACE -ErrorAction SilentlyContinue
}

# B71 dispatch predicate: bounded CFG/read trace inside 0x2E2520 (observe-only).
# Requires POST_DRAIN_GATE_TRACE hooks; TraceB71Dispatch alone also enables PDGT.
if ($TraceB71Dispatch) {
  $env:JJFB_POST_DRAIN_GATE_TRACE = '1'
  $env:JJFB_B71_DISPATCH_TRACE = '1'
} else {
  Remove-Item Env:JJFB_B71_DISPATCH_TRACE -ErrorAction SilentlyContinue
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

$listHeadOk = [bool]($all -match '\[EVENT_LIST_HEAD_INITIALIZED\]')
$pathAOk = [bool]($all -match '\[EVENT_PATH_A_ENQUEUE_OK\]')
$pathARecovered = [bool]($all -match '\[EVENT_PATH_A_LIST_HEAD_RECOVERED\]')
$node94Id = [bool]($all -match '\[NODE_94E40_FUNCTION_IDENTIFIED\]')
$nodeFirstZero = [bool]($all -match '\[NODE_FIRST_CAUSAL_ZERO_FOUND\]')
$nodeLinked = [bool]($all -match '\[EVENT_LIST_NODE_LINKED\]')
$nodeAllocOk = [bool]($all -match '\[NODE_ALLOCATION_RETURN_VALID\]')
$pathAComplete = [bool]($all -match '\[EVENT_PATH_A_ENQUEUE_COMPLETE\]')
# Do not match NODE_94E40_FUNCTION_IDENTIFIED lines that merely name fault_pc=.
$fault94 = [bool]($all -match '\[EXT_FAULT\].*fault_pc=0x94E40|\[NA_FAULT_94E40\]|memory_access_pc=0x94E40')
$nodeCtorOk = [bool]($all -match '\[EVENT_NODE_CONSTRUCTION_COMPLETE\]')
$uiModeNz = [bool]($all -match 'ui_mode=0x[1-9A-Fa-f]|UI_MODE.*nonzero|ui_mode=[1-9]')
$consumerEnter = [bool]($all -match '\[EVENT_QUEUE_CONSUMER_ENTER\]')
$consumerTrig = [bool]($all -match '\[EVENT_QUEUE_CONSUMER_TRIGGER|DRAIN_TRIGGER')
$nodeConsumed = [bool]($all -match '\[EVENT_NODE_CONSUMED\]')
$queueAck = [bool]($all -match '\[EVENT_QUEUE_CONSUMED_AND_ACKNOWLEDGED\]|EVENT_COMPLETION_GUEST_VISIBLE|\[POST_DRAIN_SUCCESSOR_REACHED\]')
$countChanged = [bool]($all -match '\[EVENT_LIST_COUNT_CHANGED\]')
$nonemptyVis = [bool]($all -match '\[EVENT_QUEUE_NONEMPTY_VISIBLE\]')
$postDrainGateOk = [bool]($all -match '\[EVENT_POST_DRAIN_GATE_OK\]')
$uiWriterEnter = [bool]($all -match '\[EVENT_UI_MODE_WRITER_ENTER\]|\[EVENT_UI_WRITER_ENTER\]|pc=0x2FC418')
$gateSampled = [bool]($all -match '\[EVENT_POST_DRAIN_GATE\]')
$gate15d = $null; $gateB71 = $null; $gate134d = $null; $gateC76 = $null
if ($all -match '\[EVENT_POST_DRAIN_GATE\]\s+15D=(\d+)\s+B71=(\d+)\s+134D=(\d+)\s+C76=(\d+)') {
  $gate15d = [int]$Matches[1]; $gateB71 = [int]$Matches[2]; $gate134d = [int]$Matches[3]; $gateC76 = [int]$Matches[4]
} elseif ($all -match '\[EVENT_POST_DRAIN_GATE\]\s+15D=(\d+)\s+B71=(\d+)\s+134D=(\d+)') {
  $gate15d = [int]$Matches[1]; $gateB71 = [int]$Matches[2]; $gate134d = [int]$Matches[3]
}
$successorReached = $postDrainGateOk -or $uiWriterEnter -or $queueAck
$successorBlocker = 'none'
if ($nodeConsumed -and -not $successorReached) {
  if ($gateSampled -and $null -ne $gate15d -and -not ($gate15d -eq 1 -and $gateB71 -ne 0 -and $gate134d -eq 0)) {
    $successorBlocker = 'POST_DRAIN_GATE_15D_B71_134D'
  } elseif ($gateSampled -and $gateB71 -eq 0) {
    $successorBlocker = 'POST_DRAIN_GATE_B71'
  } else {
    $successorBlocker = 'POST_DRAIN_GATE_NOT_PASSED'
  }
}
$pdgtEnter30 = [bool]($all -match '\[PDGT_ENTER\].*0x30CBBC')
$pdgtEnter2e = [bool]($all -match '\[PDGT_ENTER\].*0x2E2520')
$pdgtEnter2dc = [bool]($all -match '\[PDGT_ENTER\].*0x2DC4D8')
$pdgtStore15d = [bool]($all -match '\[PDGT_ER_RW_WRITE\].*off=15D')
$pdgtStoreB71 = [bool]($all -match '\[PDGT_ER_RW_WRITE\].*off=B71')

$farthest = 'timer_stable_poll'
if ($captureHit -and $fbHit -and $hwndHit) { $farthest = 'first_natural_frame' }
elseif ($dispUp -or $refreshHit) { $farthest = 'disp_up_ex' }
elseif ($fbHit -or $dispPred) { $farthest = 'framebuffer_mutation' }
elseif ($resRead) { $farthest = 'resource_bytes_consumed' }
elseif ($resReq) { $farthest = 'resource_request' }
elseif ($stateAdv -or $sigChanged) { $farthest = 'robotol_state_changed' }
elseif ($uiWriterEnter) { $farthest = 'post_drain_ui_writer_enter' }
elseif ($postDrainGateOk) { $farthest = 'post_drain_successor_reached' }
elseif ($nodeConsumed) { $farthest = 'event_node_consumed' }
elseif ($consumerEnter) { $farthest = 'event_queue_consumer_enter' }
elseif ($consumerTrig) { $farthest = 'event_queue_consumer_trigger' }
elseif ($pathAComplete -or $nodeCtorOk) { $farthest = 'event_path_a_enqueue_complete' }
elseif ($nodeLinked) { $farthest = 'event_list_node_linked' }
elseif ($pathAOk) { $farthest = 'event_path_a_enqueue_ok' }
elseif ($listHeadOk) { $farthest = 'event_list_head_initialized' }
elseif ($abiOk) { $farthest = 'family_abi_confirmed' }
elseif ($idConfirmed) { $farthest = 'event_identity_confirmed' }
elseif ($deliverN -ge 1) { $farthest = 'family_handler_delivered' }
elseif ($acceptN -ge 1) { $farthest = 'event_accepted' }
elseif ($ctxIdent) { $farthest = '10165_object_identified' }

$lastOk = 'none'
if ($stateAdv) { $lastOk = 'ROBOTOL_STATE_DIGEST_CHANGED' }
elseif ($uiWriterEnter) { $lastOk = 'EVENT_UI_WRITER_ENTER' }
elseif ($postDrainGateOk) { $lastOk = 'EVENT_POST_DRAIN_GATE_OK' }
elseif ($nodeConsumed) { $lastOk = 'EVENT_NODE_CONSUMED' }
elseif ($consumerEnter) { $lastOk = 'EVENT_QUEUE_CONSUMER_ENTER' }
elseif ($pathAOk) { $lastOk = 'EVENT_PATH_A_ENQUEUE_OK' }
elseif ($listHeadOk) { $lastOk = 'EVENT_LIST_HEAD_INITIALIZED' }
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
elseif ($fault94 -and -not $stateAdv) {
  $firstGap = 'list-node/path-A DSM mem primitive fault @0x94E40 (UI_MODE still 0)'
}
elseif ($nodeConsumed -and -not $successorReached) {
  if ($successorBlocker -ne 'none') {
    $gateTxt = if ($null -ne $gate15d) { "15D=$gate15d B71=$gateB71 134D=$gate134d" } else { 'gate sampled without parse' }
    $firstGap = "post-drain gates $gateTxt block 2DADC4->2FC418 ($successorBlocker)"
  } else {
    $firstGap = 'node consumed; post-drain successor not reached'
  }
}
elseif ($nodeLinked -and -not $consumerEnter -and -not $stateAdv) {
  $firstGap = 'node linked but queue consumer/drain trigger not reached (UI_MODE still 0)'
}
elseif ($pathAComplete -and -not $uiModeNz -and -not $stateAdv) {
  $firstGap = 'Path A node construction complete; UI_MODE still 0 / state not advanced'
}
elseif ($pathARecovered -and $pathAOk -and -not $stateAdv -and -not $pathAComplete) {
  $firstGap = 'Path A enqueued after B54 recover; node construction incomplete'
}
elseif (-not $stateAdv) { $firstGap = 'Robotol state unchanged after delivery' }
elseif (-not $resReq) { $firstGap = 'no natural resource request after state change' }
elseif (-not $refreshHit) { $firstGap = 'no guest _DispUpEx / framebuffer present' }
else { $firstGap = 'none' }

$verdict = 'INCOMPLETE'
if ($forbidHit) {
  $verdict = 'FORBIDDEN_OR_FAULT'
} elseif ($fault94 -and -not ($nodeCtorOk -and $pathAComplete)) {
  if ($node94Id -and $nodeFirstZero) {
    $verdict = 'EVENT_NODE_ALLOC_PROVENANCE_MAPPED'
  } elseif ($pathARecovered -and $pathAOk) {
    $verdict = 'EVENT_QUEUE_BOOTSTRAP_FIXED_NEW_BLOCKER'
  } else {
    $verdict = 'FORBIDDEN_OR_FAULT'
  }
} elseif ($successorReached -and $stateAdv) {
  $verdict = 'POST_DRAIN_SUCCESSOR_REACHED'
} elseif ($queueAck -and $stateAdv) {
  $verdict = 'POST_DRAIN_SUCCESSOR_REACHED' # legacy alias EVENT_QUEUE_CONSUMED_AND_ACKNOWLEDGED
} elseif ($nodeConsumed -and $consumerEnter -and -not $successorReached) {
  $verdict = 'EVENT_QUEUE_CONSUMER_REACHED'
} elseif ($nodeConsumed -and $consumerEnter) {
  $verdict = 'EVENT_QUEUE_CONSUMER_REACHED'
} elseif ($consumerTrig -and $nonemptyVis -and -not $consumerEnter) {
  $verdict = 'EVENT_QUEUE_CONSUMER_NOT_SCHEDULED'
} elseif ($nodeCtorOk -and $pathAComplete -and $stateAdv) {
  $verdict = 'EVENT_NODE_CONSTRUCTION_COMPLETE'
} elseif ($pathAComplete -and -not $fault94 -and $stateAdv) {
  $verdict = 'EVENT_PATH_A_ENQUEUE_OK'
} elseif ($nodeCtorOk -and $pathAComplete -and -not $fault94 -and $resReq) {
  $verdict = 'EVENT_NODE_CONSTRUCTION_COMPLETE'
} elseif ($nodeCtorOk -and $pathAComplete -and -not $fault94) {
  if ($consumerEnter) { $verdict = 'EVENT_QUEUE_CONSUMER_REACHED' }
  elseif ($nonemptyVis) { $verdict = 'EVENT_QUEUE_NONEMPTY_VISIBLE' }
  else { $verdict = 'EVENT_PATH_A_ENQUEUE_OK' }
} elseif ($Mode -eq 'Event') {
  if ($stateAdv -or $sigChanged) {
    $verdict = 'P6_ROBOTOL_STATE_CHANGED'
  } elseif ($node94Id -and $nodeFirstZero -and $fault94) {
    $verdict = 'EVENT_NODE_ALLOC_PROVENANCE_MAPPED'
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
  } elseif ($nodeCtorOk -and $pathAComplete -and -not $fault94 -and $resReq) {
    $verdict = 'EVENT_NODE_CONSTRUCTION_COMPLETE'
  } elseif ($nodeLinked -and $nodeAllocOk -and -not $fault94) {
    $verdict = 'EVENT_PATH_A_ENQUEUE_OK'
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
$pdgtCsv = Join-Path $reportDir 'product_post_drain_gate_watch.csv'
$pdgtTl = Join-Path $reportDir 'product_post_drain_gate_timeline.md'
$dispTl = Join-Path $reportDir 'product_b71_dispatch_timeline.md'
$dispCalls = Join-Path $reportDir 'product_b71_dispatch_calls.csv'
$dispBr = Join-Path $reportDir 'product_b71_dispatch_branches.csv'
$dispRd = Join-Path $reportDir 'product_b71_dispatch_reads.csv'
$hashesPath = Join-Path $reportDir "product_ffp_hashes_$runId.txt"

function Get-Sha256OrMissing([string]$path) {
  if (-not (Test-Path $path)) { return 'missing' }
  return (Get-FileHash -Algorithm SHA256 -Path $path).Hash.ToLowerInvariant()
}

$runnerPath = $MyInvocation.MyCommand.Path
$runnerSha = Get-Sha256OrMissing $runnerPath
$gitCommit = 'unknown'
$gitTree = 'unknown'
$gitDirty = 'unknown'
try {
  Push-Location $Root
  $gitCommit = (git rev-parse HEAD 2>$null).Trim()
  $gitTree = (git rev-parse 'HEAD^{tree}' 2>$null).Trim()
  $dirtyOut = git status --porcelain 2>$null
  $gitDirty = if ($dirtyOut) { 'yes' } else { 'no' }
  Pop-Location
} catch {
  Pop-Location -ErrorAction SilentlyContinue
}

$stdoutSha = Get-Sha256OrMissing $stdout
$stderrSha = Get-Sha256OrMissing $stderr
$csvReqSha = Get-Sha256OrMissing $csvReq
$csv65Sha = Get-Sha256OrMissing $csv65
$csvSampSha = Get-Sha256OrMissing $csvSamp
$csvMemSha = Get-Sha256OrMissing $csvMem
$abiSha = Get-Sha256OrMissing $abiJson
$pdgtCsvSha = Get-Sha256OrMissing $pdgtCsv
$pdgtTlSha = Get-Sha256OrMissing $pdgtTl
$dispTlSha = Get-Sha256OrMissing $dispTl
$dispCallsSha = Get-Sha256OrMissing $dispCalls
$dispBrSha = Get-Sha256OrMissing $dispBr
$dispRdSha = Get-Sha256OrMissing $dispRd

$gateSampleLine = if ($null -ne $gate15d) {
  "15D=$gate15d B71=$gateB71 134D=$gate134d C76=$(if ($null -ne $gateC76) { $gateC76 } else { '?' })"
} else { 'not_sampled_in_log' }

$successorStatus = if ($successorReached) { 'POST_DRAIN_SUCCESSOR_REACHED' } else { 'POST_DRAIN_SUCCESSOR_BLOCKED' }

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

## Provenance

- **git_commit:** $gitCommit
- **git_tree:** $gitTree
- **git_dirty:** $gitDirty
- **runner_path:** $runnerPath
- **runner_sha256:** $runnerSha
- **main_exe:** $exe
- **main_exe_sha256:** $exeHash
- **gwy_launcher_sha256:** $launcherHash
- **stdout:** $stdout
- **stdout_sha256:** $stdoutSha
- **stderr:** $stderr
- **stderr_sha256:** $stderrSha
- **hashes_sidecar:** $hashesPath

## Farthest natural milestone

- **farthest:** $farthest
- **last_successful_transaction:** $lastOk
- **first_unmet_platform_contract:** $firstGap
- **note:** ``EVENT_PATH_A_ENQUEUE_COMPLETE`` is an independent marker; if node linked/consumed, prefer consumer milestones over that marker.

## Post-Drain Gate (successor, not protocol ACK)

- **successor_status:** $successorStatus
- **successor_blocker:** $successorBlocker
- **gate_sample:** $gateSampleLine
- **EVENT_POST_DRAIN_GATE_OK:** $(if ($postDrainGateOk) { 'yes' } else { 'no' })
- **UI_writer_2FC418:** $(if ($uiWriterEnter) { 'yes' } else { 'no' })
- **legacy_alias:** former ``Ack path`` / ``ack_done`` == post-drain successor reachability
- **PDGT enter 30CBBC:** $(if ($pdgtEnter30) { 'yes' } else { 'no' })
- **PDGT enter 2E2520:** $(if ($pdgtEnter2e) { 'yes' } else { 'no' })
- **PDGT enter 2DC4D8:** $(if ($pdgtEnter2dc) { 'yes' } else { 'no' })
- **PDGT store 15D:** $(if ($pdgtStore15d) { 'yes' } else { 'no' })
- **PDGT store B71:** $(if ($pdgtStoreB71) { 'yes' } else { 'no' })
- **B71_dispatch_trace:** $(if ($env:JJFB_B71_DISPATCH_TRACE -eq '1') { 'yes' } else { 'no' })
- **b71_dispatch_timeline:** $(if (Test-Path $dispTl) { "$dispTl sha256=$dispTlSha" } else { 'missing' })
- **b71_dispatch_calls:** $(if (Test-Path $dispCalls) { "$dispCalls sha256=$dispCallsSha" } else { 'missing' })
- **b71_dispatch_branches:** $(if (Test-Path $dispBr) { "$dispBr sha256=$dispBrSha" } else { 'missing' })
- **b71_dispatch_reads:** $(if (Test-Path $dispRd) { "$dispRd sha256=$dispRdSha" } else { 'missing' })

## Event / ABI

- **guest request samples:** $sampleN
- **EVENT_TXN ACCEPT:** $acceptN
- **FAMILY DELIVER:** $deliverN
- **SUPPRESS:** $suppressN
- **identity_class:** $idClass
- **EVENT_LIST_HEAD_INITIALIZED:** $(if ($listHeadOk) { 'yes' } else { 'no' })
- **EVENT_PATH_A_ENQUEUE_OK:** $(if ($pathAOk) { 'yes' } else { 'no' })
- **NODE_94E40_FUNCTION_IDENTIFIED:** $(if ($node94Id) { 'yes' } else { 'no' })
- **NODE_FIRST_CAUSAL_ZERO_FOUND:** $(if ($nodeFirstZero) { 'yes' } else { 'no' })
- **EVENT_LIST_NODE_LINKED:** $(if ($nodeLinked) { 'yes' } else { 'no' })
- **EVENT_LIST_COUNT_CHANGED:** $(if ($countChanged) { 'yes' } else { 'no' })
- **EVENT_QUEUE_NONEMPTY_VISIBLE:** $(if ($nonemptyVis) { 'yes' } else { 'no' })
- **EVENT_QUEUE_CONSUMER_TRIGGER:** $(if ($consumerTrig) { 'yes' } else { 'no' })
- **EVENT_QUEUE_CONSUMER_ENTER:** $(if ($consumerEnter) { 'yes' } else { 'no' })
- **EVENT_NODE_CONSUMED:** $(if ($nodeConsumed) { 'yes' } else { 'no' })
- **NODE_ALLOCATION_RETURN_VALID:** $(if ($nodeAllocOk) { 'yes' } else { 'no' })
- **EVENT_PATH_A_ENQUEUE_COMPLETE:** $(if ($pathAComplete) { 'yes' } else { 'no' })
- **fault_at_0x94E40:** $(if ($fault94) { 'yes' } else { 'no' })
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
- **csv_requests:** $(if (Test-Path $csvReq) { "$csvReq sha256=$csvReqSha" } else { 'missing' })
- **csv_10165:** $(if (Test-Path $csv65) { "$csv65 sha256=$csv65Sha" } else { 'missing' })
- **csv_samples:** $(if (Test-Path $csvSamp) { "$csvSamp sha256=$csvSampSha" } else { 'missing' })
- **csv_mem:** $(if (Test-Path $csvMem) { "$csvMem sha256=$csvMemSha" } else { 'missing' })
- **abi_manifest:** $(if (Test-Path $abiJson) { "$abiJson sha256=$abiSha" } else { 'missing' })
- **pdgt_watch_csv:** $(if (Test-Path $pdgtCsv) { "$pdgtCsv sha256=$pdgtCsvSha" } else { 'missing' })
- **pdgt_timeline:** $(if (Test-Path $pdgtTl) { "$pdgtTl sha256=$pdgtTlSha" } else { 'missing' })
- **b71_dispatch_timeline:** $(if (Test-Path $dispTl) { "$dispTl sha256=$dispTlSha" } else { 'missing' })
- **forbidden_hits:** $(if ($forbidHit) { ($forbidHit -join ', ') } else { 'none' })

## Discipline

- Event Round A: collect identity + 10165 object + handler ABI (no one-shot default)
- Event Round B: ``-ApplyAbi`` once after provenance
- Resource/Validate auto-continue when state advances / display predecessor reached
- Forbidden: fixed PC patches, Robotol flag writes, fabricated 10165, forced DispUpEx, E9/E10A
- Post-drain gate: observe-only writers/watchpoints; no forced 15D/B71/UI_MODE
"@ | Set-Content -Path $verdictPath -Encoding utf8

$verdictSha = Get-Sha256OrMissing $verdictPath
@"
run_id=$runId
git_commit=$gitCommit
git_tree=$gitTree
git_dirty=$gitDirty
runner_sha256=$runnerSha
main_exe_sha256=$exeHash
gwy_launcher_sha256=$launcherHash
stdout_sha256=$stdoutSha
stderr_sha256=$stderrSha
csv_requests_sha256=$csvReqSha
csv_10165_sha256=$csv65Sha
csv_samples_sha256=$csvSampSha
csv_mem_sha256=$csvMemSha
abi_manifest_sha256=$abiSha
pdgt_watch_sha256=$pdgtCsvSha
pdgt_timeline_sha256=$pdgtTlSha
b71_dispatch_timeline_sha256=$dispTlSha
b71_dispatch_calls_sha256=$dispCallsSha
b71_dispatch_branches_sha256=$dispBrSha
b71_dispatch_reads_sha256=$dispRdSha
verdict_sha256=$verdictSha
verdict_path=$verdictPath
"@ | Set-Content -Path $hashesPath -Encoding utf8

# Append verdict hash into verdict Provenance section for one-file binding.
$verdictBody = Get-Content $verdictPath -Raw
$verdictBody = $verdictBody -replace '(?m)^- \*\*hashes_sidecar:\*\*.*$', ("- **hashes_sidecar:** $hashesPath`r`n- **verdict_sha256:** $verdictSha")
Set-Content -Path $verdictPath -Value $verdictBody -Encoding utf8
$verdictSha = Get-Sha256OrMissing $verdictPath
(Get-Content $hashesPath) -replace '^verdict_sha256=.*', "verdict_sha256=$verdictSha" | Set-Content $hashesPath -Encoding utf8

@"
run_id=$runId
mode=$Mode
runtime=$runtimeKind
main_exe_sha256=$exeHash
gwy_launcher_sha256=$launcherHash
jjfb_mrp_sha256=$ExpectedHash
apply_abi=$(if ($ApplyAbi) { '1' } else { '0' })
git_commit=$gitCommit
git_tree=$gitTree
git_dirty=$gitDirty
runner_sha256=$runnerSha
stdout_sha256=$stdoutSha
verdict_sha256=$verdictSha
"@ | Set-Content -Path $manifest -Encoding utf8

Write-Host "verdict=$verdict farthest=$farthest identity=$idClass state_adv=$stateAdv successor=$successorStatus"
Write-Host "report=$verdictPath"
Write-Host "hashes=$hashesPath"

$okModes = @(
  'P6_EVENT_PROVENANCE_MAPPED',
  'P6_EVENT_PARTIAL',
  'P6_ABI_AND_IDENTITY_MAPPED_NO_ADVANCE',
  'P6_ROBOTOL_STATE_CHANGED',
  'EVENT_NODE_ALLOC_PROVENANCE_MAPPED',
  'EVENT_NODE_CONSTRUCTION_COMPLETE',
  'EVENT_PATH_A_ENQUEUE_OK',
  'EVENT_QUEUE_NONEMPTY_VISIBLE',
  'EVENT_QUEUE_CONSUMER_REACHED',
  'EVENT_QUEUE_CONSUMER_NOT_SCHEDULED',
  'EVENT_QUEUE_CONSUMED_AND_ACKNOWLEDGED',
  'POST_DRAIN_SUCCESSOR_REACHED',
  'P7_RESOURCE_REQUEST_SEEN',
  'P7_RESOURCE_TRANSACTION_COMPLETE',
  'P7_WAITING_RESOURCE_AFTER_STATE',
  'ROBOTOL_STATE_ADVANCED_AFTER_COMPLETION',
  'PRODUCT_FIRST_NATURAL_FRAME_STABLE',
  'FAMILY_EVENT_ABI_CONFIRMED_NO_ADVANCE_YET'
)
if ($okModes -contains $verdict -and -not $forbidHit) { exit 0 }
exit 1
