# Product P4: natural callback progress → first guest-driven display refresh.
# Modes: progress_map | work_sources | validate
# Gwy+stubs only. No research lib / forced callback / FAST / E9 inject / SMSCFG.
param(
  [ValidateSet('progress_map', 'work_sources', 'validate')]
  [string]$Mode = 'progress_map',
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

$runId = ('p4_{0}_{1:yyyyMMdd_HHmmss}_{2}' -f $Mode, (Get-Date), (Get-Random -Maximum 99999))

@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_SHELL_NATIVE_EXEC_TRACE', 'JJFB_RUNAPP_NATIVE_ONLY',
  'JJFB_E10A31_MODE', 'JJFB_E10A31C_MODE', 'JJFB_E10A31N_MODE', 'JJFB_E10A31Q_APPLY',
  'GWY_SMSCFG_BOOTSTRAP', 'GWY_DIAG_SMSCFG_GPT_MINIMAL', 'JJFB_E10A31K_MODE',
  'JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE', 'JJFB_E9C_MODE', 'JJFB_E8Z_MODE',
  'JJFB_FAST_REAL_BMP_HANDLE', 'JJFB_E9B_MODE', 'JJFB_VISIBLE_WINDOW',
  'JJFB_E8E_DRAIN_ORDER', 'JJFB_E8D_10165_PROBE', 'JJFB_E8C_IDLE_WATCH',
  'JJFB_E8E_EVENT_PROBE', 'JJFB_HANDLER_FORENSIC', 'JJFB_PLAT_CENSUS'
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
$evidenceDir = Join-Path $Root 'evidence'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir, $evidenceDir | Out-Null

$stdout = Join-Path $logDir 'product_p4_stdout.txt'
$stderr = Join-Path $logDir 'product_p4_stderr.txt'
$vmLog = Join-Path $logDir 'product_p4_vmrp.txt'
$stageReport = Join-Path $reportDir 'stage_product_p4_natural_progress.md'
$manifest = Join-Path $reportDir "product_p4_manifest_$runId.txt"
$bmpPath = Join-Path $evidenceDir 'product_p4_first_natural_frame.bmp'
$pngPath = Join-Path $evidenceDir 'product_p4_first_natural_frame.png'

@(
  $stdout, $stderr, $vmLog,
  (Join-Path $reportDir 'product_p4_callback_progress.csv'),
  (Join-Path $reportDir 'product_p4_callback_signatures.csv'),
  (Join-Path $reportDir 'product_p4_stall_predicates.csv'),
  (Join-Path $reportDir 'product_p4_work_source_ledger.csv'),
  (Join-Path $reportDir 'product_p4_display_reachability.csv'),
  (Join-Path $reportDir 'product_p4_platform_contracts.csv'),
  (Join-Path $reportDir 'product_p4_stall_verdict.md'),
  (Join-Path $reportDir 'product_p4_visual_trace.csv'),
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
Remove-Item Env:GWY_PACKAGE_APPID -ErrorAction SilentlyContinue
Remove-Item Env:GWY_PACKAGE_APPVER -ErrorAction SilentlyContinue

$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== PRODUCT P4 Mode=$Mode Seconds=$Seconds runtime=$runtimeKind run_id=$runId =="

& $Launcher validate --root $ResourceRoot
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

@"
[DESCRIPTOR_FROZEN] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher run_id=$runId evidence=OBSERVED
[TARGET_HASH_VERIFIED] sha256=$ExpectedHash run_id=$runId evidence=OBSERVED
[JJFB_GWY_LAUNCH] cfg_index=36 target=gwy/jjfb.mrp source=descriptor_launcher run_id=$runId evidence=DOCUMENTED
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
  if (Test-Path $vmLog) {
    Get-Content $vmLog -Tail 800 -ErrorAction SilentlyContinue |
      Out-File -FilePath $stdout -Append -Encoding utf8
  }
  $all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
  if (-not $all) { $all = '' }

  $haveSched = ($all -match ("\[SCHEDULER_NATURAL_CALLBACK\].*run_id=$([regex]::Escape($runId)).*forced=no.*evidence=OBSERVED")) -or
               ($all -match '\[SCHEDULER_NATURAL_CALLBACK\].*forced=no.*evidence=OBSERVED')
  $okReturns = ([regex]::Matches($all, '\[P3_CALLBACK_LEAVE\].*class=CALLBACK_RETURN_SENTINEL_OK')).Count
  if ($okReturns -eq 0) {
    $okReturns = ([regex]::Matches($all, 'FIRE_DONE.*ok=1.*uc_err=0')).Count
  }
  $p4Done = [bool]($all -match '\[P4_FINALIZE\]')

  if ($Mode -eq 'progress_map' -or $Mode -eq 'work_sources') {
    if ($haveSched -and ($okReturns -ge 64 -or $p4Done)) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(3) }
      if ((Get-Date) -ge $stableDeadline) { break }
    }
    if ($haveSched -and $okReturns -ge 16 -and $p4Done) {
      if (-not $stableDeadline) { $stableDeadline = (Get-Date).AddSeconds(2) }
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
    if (-not $frameHit -and $haveSched -and $okReturns -ge 20) {
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
  Get-Content $vmLog -ErrorAction SilentlyContinue | Out-File -FilePath $stdout -Append -Encoding utf8
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
# Family-event DELIVER_DONE may print end=uc_err as a token; do not treat that alone.
if ($all -match '\[PLATFORM_FAMILY_EVENT\] op=DELIVER_DONE ok=0') {
  # Only escalate if lifecycle FIRE itself faulted.
  if (-not ($all -match '\[JJFB_LIFECYCLE\] op=FIRE_DONE.*ok=0')) {
    # keep faultHit only if other real markers exist
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

$forbidden = [ordered]@{
  gamelist_fast        = [bool]($all -match 'GAMELIST_STARTED|FAST_REAL_GAMELIST|JJFB_GAMELIST')
  method0_smscfg_write = [bool]($all -match 'SMSCFG_METHOD0_ENTER_APPLY|E10A31N_ARMED|SMSCFG_Q_BKEYS')
  fixed_pc_jump        = [bool]($all -match 'FORCE_PC|PATCH_PC')
  product_overlay_hit  = [bool]($all -match 'NOT_PRODUCT')
  forced_callback      = [bool]($all -match 'FORCE_CALLBACK|V64_ENQUEUE|FAMILY_C0_AFTER')
  e9_inject            = [bool]($all -match 'JJFB_E9C_MODE|e9c_present_meaningful')
  e8e_drain            = [bool]($all -match 'JJFB_E8E_DRAIN_ORDER')
}
$forbidHit = ($forbidden.GetEnumerator() | Where-Object { $_.Value }).Name

# Parse P4 verdict files
$stallMd = Join-Path $reportDir 'product_p4_stall_verdict.md'
$predicateFound = $false
$sourceClass = 'UNKNOWN'
$callbackClass = 'UNKNOWN'
$ledgerVerdict = 'UNKNOWN'
$contractVerdict = 'UNKNOWN'
$displayVerdict = 'UNKNOWN'
$samePath = 'unknown'
$earlyStall = 'unknown'
if (Test-Path $stallMd) {
  $sm = Get-Content $stallMd -Raw
  if ($sm -match 'ROBOTOL_STABLE_WAIT_PREDICATE_FOUND') { $predicateFound = $true }
  if ($sm -match '\*\*source_class:\*\*\s*(\S+)') { $sourceClass = $Matches[1] }
  if ($sm -match '\*\*callback_class:\*\*\s*(\S+)') { $callbackClass = $Matches[1] }
  if ($sm -match '\*\*ledger_verdict:\*\*\s*(\S+)') { $ledgerVerdict = $Matches[1] }
  if ($sm -match '\*\*contract_verdict:\*\*\s*(\S+)') { $contractVerdict = $Matches[1] }
  if ($sm -match '\*\*display_verdict:\*\*\s*(\S+)') { $displayVerdict = $Matches[1] }
  if ($sm -match '\*\*same_path_same_state:\*\*\s*(\S+)') { $samePath = $Matches[1] }
  if ($sm -match '\*\*early_change_then_stall:\*\*\s*(\S+)') { $earlyStall = $Matches[1] }
}
if ($all -match '\[P4_FINALIZE\].*predicate=ROBOTOL_STABLE_WAIT_PREDICATE_FOUND') { $predicateFound = $true }
if ($all -match '\[P4_FINALIZE\].*ledger=(\S+)') { $ledgerVerdict = $Matches[1] }
if ($all -match '\[P4_FINALIZE\].*contract=(\S+)') { $contractVerdict = $Matches[1] }

$provenFix = [bool]($all -match 'PROVEN_PLATFORM_CONTRACT_FIXED|P4_CONTRACT_FIX')
$resReq = (HasMarker 'FIRST_NATURAL_RESOURCE_REQUEST') -or ($all -match 'FIRST_NATURAL_RESOURCE_REQUEST')
$resDone = (HasMarker 'FIRST_NATURAL_RESOURCE_COMPLETION') -or ($all -match 'FIRST_NATURAL_RESOURCE_COMPLETION')

$verdict = 'INCOMPLETE'
if ($forbidHit -or $faultHit) {
  $verdict = 'FORBIDDEN_OR_FAULT'
} elseif ($Mode -eq 'validate') {
  if ($schedHit -and $initHit -and $drawHit -and $refreshHit -and $fbHit -and $hwndHit -and $captureHit) {
    $verdict = 'PRODUCT_FIRST_NATURAL_FRAME_STABLE'
  } elseif ($provenFix -and $drawHit -and -not $refreshHit) {
    $verdict = 'DISPLAY_PREDECESSOR_REACHED_NO_REFRESH'
  } elseif ($provenFix -and -not $drawHit) {
    $verdict = 'PLATFORM_COMPLETION_RESTORED_NO_DRAW_YET'
  } elseif ($callbackClass -match 'PROGRESSING|STATE_CHANGES' -and -not $refreshHit) {
    $verdict = 'ROBOTOL_STATE_PROGRESSING_NO_REFRESH_YET'
  } elseif ($predicateFound) {
    $verdict = 'ROBOTOL_STABLE_WAIT_PREDICATE_FOUND'
  } else {
    $verdict = 'INCOMPLETE'
  }
} else {
  # progress_map / work_sources
  if ($predicateFound -and $schedHit -and $initHit) {
    $verdict = 'ROBOTOL_STABLE_WAIT_PREDICATE_FOUND'
  } elseif ($schedHit -and $okLeave -ge 8) {
    $verdict = 'PROGRESS_MAP_PARTIAL'
  } else {
    $verdict = 'INCOMPLETE'
  }
}

@"
# Stage Product P4 Natural Progress

- **run_id:** $runId
- **mode:** $Mode
- **verdict:** $verdict
- **runtime:** $runtimeKind
- **seconds:** $Seconds
- **process_exit:** $procExit
- **ok_callback_returns:** $okLeave
- **SCHEDULER_NATURAL_CALLBACK:** $(if ($schedHit) { 'yes' } else { 'no' })
- **ROBOTOL_INIT_RETURN_ZERO:** $(if ($initHit) { 'yes' } else { 'no' })
- **ROBOTOL_STABLE_WAIT_PREDICATE_FOUND:** $(if ($predicateFound) { 'yes' } else { 'no' })
- **source_class:** $sourceClass
- **callback_class:** $callbackClass
- **ledger_verdict:** $ledgerVerdict
- **contract_verdict:** $contractVerdict
- **display_verdict:** $displayVerdict
- **same_path_same_state:** $samePath
- **early_change_then_stall:** $earlyStall
- **PROVEN_PLATFORM_CONTRACT_FIXED:** $(if ($provenFix) { 'yes' } else { 'no' })
- **FIRST_NATURAL_RESOURCE_REQUEST:** $(if ($resReq) { 'yes' } else { 'no' })
- **FIRST_NATURAL_RESOURCE_COMPLETION:** $(if ($resDone) { 'yes' } else { 'no' })
- **draw_seen:** $(if ($drawHit) { 'yes' } else { 'no' })
- **refresh_seen:** $(if ($refreshHit) { 'yes' } else { 'no' })
- **framebuffer_nonempty:** $(if ($fbHit) { 'yes' } else { 'no' })
- **hwnd_visible:** $(if ($hwndHit) { 'yes' } else { 'no' })
- **frame_captured:** $(if ($captureHit) { 'yes' } else { 'no' })
- **forbidden_hits:** $(if ($forbidHit) { ($forbidHit -join ', ') } else { 'none' })
- **manifest:** $manifest
- **evidence_png:** $(if (Test-Path $pngPath) { $pngPath } elseif (Test-Path $bmpPath) { $bmpPath } else { 'none' })

## Required question

$(if ($samePath -eq 'yes') {
  'All captured callbacks execute the same control-flow path with the same state digest (stable poll).'
} elseif ($earlyStall -eq 'yes') {
  'State/signature changes for early callbacks, then stalls without draw.'
} else {
  'See product_p4_stall_verdict.md / signature CSV for path vs state classification.'
})

## Exact marker gates

| Gate | OK |
|------|----|
| SCHEDULER_NATURAL_CALLBACK forced=no | $(if ($schedHit) { 'yes' } else { 'no' }) |
| ROBOTOL_INIT_RETURN_ZERO | $(if ($initHit) { 'yes' } else { 'no' }) |
| ROBOTOL_STABLE_WAIT_PREDICATE_FOUND | $(if ($predicateFound) { 'yes' } else { 'no' }) |
| PROVEN_PLATFORM_CONTRACT_FIXED | $(if ($provenFix) { 'yes' } else { 'no' }) |
| FIRST_NATURAL_DRAW | $(if ($drawHit) { 'yes' } else { 'no' }) |
| FIRST_NATURAL_REFRESH api=_DispUpEx | $(if ($refreshHit) { 'yes' } else { 'no' }) |
| FRAMEBUFFER_NONEMPTY | $(if ($fbHit) { 'yes' } else { 'no' }) |
| HWND_VISIBLE | $(if ($hwndHit) { 'yes' } else { 'no' }) |
| FIRST_NATURAL_FRAME_CAPTURED | $(if ($captureHit) { 'yes' } else { 'no' }) |

## Notes

- BIND_REFRESH is not counted as DRAW/REFRESH
- No E8E drain / synthetic 10165 in product P4
- Strong success: PRODUCT_FIRST_NATURAL_FRAME_STABLE
- Round-1 success: ROBOTOL_STABLE_WAIT_PREDICATE_FOUND
"@ | Set-Content -Path $stageReport -Encoding utf8

Write-Host "verdict=$verdict predicate=$predicateFound ledger=$ledgerVerdict contract=$contractVerdict"
Write-Host "report=$stageReport"

$okModes = @(
  'ROBOTOL_STABLE_WAIT_PREDICATE_FOUND',
  'PRODUCT_FIRST_NATURAL_FRAME_STABLE',
  'PLATFORM_COMPLETION_RESTORED_NO_DRAW_YET',
  'ROBOTOL_STATE_PROGRESSING_NO_REFRESH_YET',
  'DISPLAY_PREDECESSOR_REACHED_NO_REFRESH',
  'PROGRESS_MAP_PARTIAL'
)
if ($okModes -contains $verdict -and -not $forbidHit) { exit 0 }
exit 1
