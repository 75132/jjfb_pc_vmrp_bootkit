# Stage E product track: descriptor_launcher → jjfb/wxjwq → mrc_loader → primary EXT → mrc_init
# Does NOT claim native_shell. Clears shell/gamelist research envs.
param(
  [int]$Seconds = 60,
  [string]$Target = 'gwy/jjfb.mrp',
  [int]$CfgIndex = 36,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$TargetNorm = $Target -replace '\\', '/'
$isJjfb = ($TargetNorm -match 'jjfb\.mrp$')
$isWxjwq = ($TargetNorm -match 'wxjwq\.mrp$')
$ExpectedHash = if ($isJjfb) {
  '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
} elseif ($isWxjwq) {
  '6ec628419bc4c0ca1f8fba37b0c5179961220cd53591fc55eba26735defbd02d'
} else { $null }
$mrpHost = Join-Path $ResourceRoot ($TargetNorm -replace '/', '\')
$Profile = if ($isWxjwq -and (Test-Path (Join-Path $Root 'profiles\wxjwq.json'))) {
  Join-Path $Root 'profiles\wxjwq.json'
} else {
  Join-Path $Root 'profiles\jjfb.json'
}
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$PrimaryExt = if ($isJjfb) { 'robotol.ext' } elseif ($isWxjwq) { 'mmochat.ext' } else { 'mmochat.ext' }
$tag = if ($isJjfb) { 'stage_e' } else { 'stage_e_wxjwq' }

if (-not (Test-Path $mrpHost)) { throw "missing $mrpHost" }
$hashBefore = (Get-FileHash -Algorithm SHA256 -Path $mrpHost).Hash.ToLowerInvariant()
if ($ExpectedHash -and $hashBefore -ne $ExpectedHash) {
  throw "$TargetNorm sha256 mismatch: $hashBefore"
}

# Clear shell / research envs that would pollute product-track judgment.
@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
  'JJFB_GWY_UPDATE_STUB', 'JJFB_SHELL_NATIVE_EXEC_TRACE', 'JJFB_RUNAPP_NATIVE_ONLY'
) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

if (-not (Test-Path $Launcher)) { throw "missing $Launcher — run RUN_BUILD.ps1" }
if (-not (Test-Path $exe)) { throw "missing $exe" }

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $logDir, $reportDir | Out-Null
$stdout = Join-Path $logDir "${tag}_product_robotol_stdout.txt"
$stderr = Join-Path $logDir "${tag}_product_robotol_stderr.txt"
$vmLog = Join-Path $logDir "${tag}_vmrp_stdout.txt"
$report = Join-Path $reportDir "${tag}_product_robotol_boot.md"
$identityReport = Join-Path $reportDir 'stage_e_robotol_identity_audit.md'
$handoffReport = Join-Path $reportDir 'stage_e_mrc_loader_to_robotol_handoff.md'
if ($isJjfb) {
  if (Test-Path $stdout) { Remove-Item -Force $stdout }
  if (Test-Path $stderr) { Remove-Item -Force $stderr }
  if (Test-Path $vmLog) { Remove-Item -Force $vmLog }
} else {
  $stdout = Join-Path $logDir 'stage_e_wxjwq_control_stdout.txt'
  $stderr = Join-Path $logDir 'stage_e_wxjwq_control_stderr.txt'
  $vmLog = Join-Path $logDir 'stage_e_wxjwq_vmrp_stdout.txt'
  $report = Join-Path $reportDir 'stage_e_wxjwq_control.md'
  if (Test-Path $stdout) { Remove-Item -Force $stdout }
  if (Test-Path $stderr) { Remove-Item -Force $stderr }
  if (Test-Path $vmLog) { Remove-Item -Force $vmLog }
}

$env:GWY_PROFILE = $Profile
$env:GWY_OVERLAY_ROOT = Join-Path $RunDir 'overlay'
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

# Product-track envs (Stage E §4)
$env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
$env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
$env:JJFB_PRIMARY_TARGET = $TargetNorm
$env:JJFB_LAUNCH_PATH = 'descriptor_direct'
$env:JJFB_RUNAPP_NATIVE_ONLY = '0'
$env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
$env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
$env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
$env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
$env:JJFB_MODULE_REGISTRY_TRACE = '1'
$env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
$env:JJFB_MRC_INIT_TRACE = '1'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_GAME_SELF_PATCH = '0'
# Stage E4: DOCUMENTED mr_doExt appInfo id/ver (profile/runner only — not fixed guest addrs).
# Do not overwrite if caller (E8B wxjwq) already set package identity.
if (-not $env:GWY_PACKAGE_APPID) {
  if ($isWxjwq) { $env:GWY_PACKAGE_APPID = '403095' } else { $env:GWY_PACKAGE_APPID = '400101' }
}
if (-not $env:GWY_PACKAGE_APPVER) {
  if ($isWxjwq) { $env:GWY_PACKAGE_APPVER = '1118' } else { $env:GWY_PACKAGE_APPVER = '12' }
}
# DOCUMENTED nested EXT R9 scope: DSM ER_RW guard + enter/leave (required for loader/robotol).
# Must be explicit — do not rely on ambient shell leftover from prior live scripts.
$env:GWY_MODULE_R9_SWITCH = '1'
# Callback-frame arms post_cont (auto via R9_SWITCH/CALLBACK). Do NOT set
# GWY_POST_CONT_AUDIT=1 here — that also enables GWY_P_EXTCHUNK_AUDIT flood and stalls boot.
$env:GWY_CALLBACK_FRAME = '1'
Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue

$param = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=${TargetNorm}_gwyblink"
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = $TargetNorm
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

Write-Host "== Stage E product robotol/mrc_init target=$TargetNorm Seconds=$Seconds =="
Write-Host "primary=$PrimaryExt source=descriptor_launcher appid=$($env:GWY_PACKAGE_APPID) appver=$($env:GWY_PACKAGE_APPVER)"

if ($isJjfb) {
  & $Launcher validate --root $ResourceRoot
  if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }
} else {
  Write-Host "[validate] skipped for non-jjfb target=$TargetNorm (hash gate still applied)"
}

"[JJFB_GWY_LAUNCH] cfg_index=$CfgIndex target=$TargetNorm source=descriptor_launcher evidence=DOCUMENTED" |
  Out-File -FilePath $stdout -Encoding utf8
"[JJFB_PARAM] $param" | Out-File -FilePath $stdout -Append -Encoding utf8
"[JJFB_RUNAPP] source=descriptor_launcher target=$TargetNorm spawned=pending evidence=DOCUMENTED" |
  Out-File -FilePath $stdout -Append -Encoding utf8

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $vmLog `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

# Stop milestones:
# - E1: first primary ENTRY_CALLED
# - E2/E3: do NOT stop on R9_SWITCH_OK / MRC_INIT_ATTEMPT.
# - E4: after MRC_INIT, continue until natural draw/refresh/timer or fatal/timeout
#   (mr_doExt ignores mrc_init ret; post-init is host event loop).
$e2Mode = ($env:JJFB_GAME_PACKAGE_CONTEXT_PROVIDER -eq '1') -or
          ($env:JJFB_ER_RW_BIND_RESTORE -eq 'game_package')
$e4Mode = ($env:JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE -eq '1')
$e5Mode = ($env:JJFB_E5_SCHEDULER_MODE -eq '1')
$e7Mode = ($env:JJFB_E7_LIFECYCLE_MODE -eq '1')
$e8bMode = ($env:JJFB_E8B_MODE -eq '1')
$e8cMode = ($env:JJFB_E8C_MODE -eq '1')
$e8dMode = ($env:JJFB_E8D_MODE -eq '1')
if ($e8dMode) {
  # After 10165 probe (tick1), continue to tick=40 for ER_RW diffs unless DRAW/transition.
  $stopPat = '\[JJFB_DRAW\]|JJFB_E8C_FLAG_TRANSITION|JJFB_LIFECYCLE\] op=FIRE_DONE tick=40\b|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
} elseif ($e8cMode) {
  # Flag transition, DRAW, tick=600, or fatal — boot needs ~35s wall.
  $stopPat = '\[JJFB_DRAW\]|JJFB_E8C_FLAG_TRANSITION|JJFB_LIFECYCLE\] op=FIRE_DONE tick=600\b|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
} elseif ($e8bMode) {
  # Long post-handler observe: DRAW, tick>=25 (census dump), or wall-clock Seconds.
  $stopPat = '\[JJFB_DRAW\]|JJFB_LIFECYCLE\] op=FIRE_DONE tick=25\b|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
} elseif ($e7Mode) {
  # Wait for natural DRAW or enough lifecycle fires; do not stop on PLATFORM_TIMER FIRE alone.
  $stopPat = '\[JJFB_DRAW\]|\[JJFB_REFRESH\]|JJFB_LIFECYCLE\] op=FIRE_DONE tick=8\b|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
} elseif ($e5Mode) {
  # Collect POST_START_LOOP samples; stop early only on fire/draw/fault.
  $stopPat = '\[JJFB_TIMER_FIRE\]|\[JJFB_TIMER_DELIVERED\]|\[JJFB_DRAW\]|\[JJFB_REFRESH\]|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
} elseif ($e4Mode) {
  $stopPat = '\[JJFB_DRAW\]|\[JJFB_REFRESH\]|PLATFORM_TIMER.*FIRE|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
} elseif ($e2Mode) {
  $stopPat = '\[JJFB_MRC_INIT\]|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
} else {
  $stopPat = 'JJFB_ROBOTOL_ENTRY_CALLED|\[JJFB_MRC_INIT\]|MODULE_REGISTRY.*' + [regex]::Escape($PrimaryExt) +
             '.*ENTRY_CALLED|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
}
$deadline = (Get-Date).AddSeconds([Math]::Max(10, $Seconds))
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 400
  if ((Test-Path $vmLog) -and (Select-String -Path $vmLog -Pattern $stopPat -Quiet -ErrorAction SilentlyContinue)) {
    break
  }
}
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force } catch {}
}
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
if (Test-Path $vmLog) {
  $bytes = [System.IO.File]::ReadAllBytes($vmLog)
  if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
    $all += [System.Text.Encoding]::Unicode.GetString($bytes)
  } else {
    $all += [System.Text.Encoding]::UTF8.GetString($bytes)
  }
}
[System.IO.File]::WriteAllText($stdout, $all)

$hashAfter = (Get-FileHash -Algorithm SHA256 -Path $mrpHost).Hash.ToLowerInvariant()

function Hit([string]$pat) { return [bool]($all -match $pat) }

$e1 = (Hit "MODULE_REGISTRY.*module=$([regex]::Escape($PrimaryExt)).*state=ENTRY_CALLED") -or
      (Hit 'JJFB_ROBOTOL_ENTRY_CALLED')
$e2 = Hit '\[JJFB_MRC_INIT\]'
$e4ret0 = Hit 'JJFB_INIT_SEQ\] delivered.*ret0=0\b'
$e4draw = Hit '\[JJFB_DRAW\]|\[JJFB_REFRESH\]'
$rejectN = ([regex]::Matches($all, 'JJFB_ROBOTOL_ENTER_REJECT')).Count
$trueEnter = (Hit 'JJFB_ROBOTOL_ENTRY_CALLED') -or (Hit 'HELPER_ABI\] stage=ROBOTOL_ENTER module=robotol\.ext')
$fakeEnter = (Hit 'BOOTSTRAP_SEQ event=ROBOTOL_ENTER') -and (-not $trueEnter)
$mrcLoaderEntry = Hit 'MODULE_REGISTRY.*mrc_loader\.ext.*state=ENTRY_CALLED'
$primaryExtracted = Hit "MODULE_REGISTRY.*module=$([regex]::Escape($PrimaryExt)).*state=EXTRACTED"
$primaryMapped = Hit "MODULE_REGISTRY.*module=$([regex]::Escape($PrimaryExt)).*state=MAPPED"
$primaryRegistered = Hit "MODULE_REGISTRY.*module=$([regex]::Escape($PrimaryExt)).*state=REGISTERED"
$r9Blocked = Hit 'R9_SWITCH_BLOCKED.*(mrc_loader|robotol|mmochat)'
$cload = Hit 'JJFB_CLOAD_SCOPE'
$hashOk = (-not $ExpectedHash) -or ($hashAfter -eq $ExpectedHash)

@"
# Stage E — Product Track Robotol / mrc_init

- **source:** ``descriptor_launcher``
- **target:** ``$TargetNorm``
- **primary:** ``$PrimaryExt``
- **hash before/after:** ``$hashBefore`` / ``$hashAfter`` match=$hashOk
- **seconds:** $Seconds

## Gates

| Gate | Result |
|------|--------|
| E1 primary ENTRY_CALLED | $(if ($e1) { 'yes' } else { 'no' }) |
| E2 JJFB_MRC_INIT | $(if ($e2) { 'yes' } else { 'no' }) |
| E4 ret0=0 (DOCUMENTED args) | $(if ($e4ret0) { 'yes' } else { 'no' }) |
| E4 DRAW/REFRESH | $(if ($e4draw) { 'yes' } else { 'no' }) |
| mrc_loader ENTRY_CALLED | $(if ($mrcLoaderEntry) { 'yes' } else { 'no' }) |
| primary EXTRACTED | $(if ($primaryExtracted) { 'yes' } else { 'no' }) |
| primary MAPPED | $(if ($primaryMapped) { 'yes' } else { 'no' }) |
| primary REGISTERED | $(if ($primaryRegistered) { 'yes' } else { 'no' }) |
| JJFB_CLOAD_SCOPE | $(if ($cload) { 'yes' } else { 'no' }) |
| true ROBOTOL_ENTER / ENTRY_CALLED | $(if ($trueEnter) { 'yes' } else { 'no' }) |
| ROBOTOL_ENTER_REJECT count | $rejectN |
| fake BOOTSTRAP ROBOTOL_ENTER without identity | $(if ($fakeEnter) { 'POLLUTED' } else { 'clean' }) |
| R9_SWITCH_BLOCKED (loader/primary) | $(if ($r9Blocked) { 'yes' } else { 'no' }) |
| no gamelist | $(if ($all -notmatch 'JJFB_GAMELIST_STARTED') { 'yes' } else { 'no' }) |

## Log

- ``$stdout``
"@ | Set-Content -Path $report -Encoding utf8

# Identity audit (jjfb runs)
if ($isJjfb) {
  $idLines = Select-String -Path $stdout -Pattern 'JJFB_MODULE_IDENTITY|JJFB_ROBOTOL_ENTER_REJECT|JJFB_ROBOTOL_ENTRY_CALLED|HELPER_ABI\] stage=(ROBOTOL_ENTER|MODULE_ENTER)|BOOTSTRAP_SEQ event=(ROBOTOL_ENTER|MODULE_ENTER)' -ErrorAction SilentlyContinue |
    ForEach-Object { $_.Line }
  @"
# Stage E — Robotol Identity Audit

- **purpose:** prove ROBOTOL_ENTER is not applied to mrc_loader/DSM
- **reject_count:** $rejectN
- **true_robotol_entry:** $(if ($trueEnter) { 'yes' } else { 'no' })
- **fake_bootstrap_robotol_enter:** $(if ($fakeEnter) { 'yes' } else { 'no' })

## Sample lines

``````
$(($idLines | Select-Object -First 40) -join "`n")
``````
"@ | Set-Content -Path $identityReport -Encoding utf8

  $handLines = Select-String -Path $stdout -Pattern 'mrc_loader|robotol|CLOAD_SCOPE|REG_PRIMARY|EXT_RESOLVE|CROSS_MODULE|R9_SWITCH|EXTCHUNK|MODULE_REGISTRY|ENTRY_CALLED|PACKAGE_SCOPE' -ErrorAction SilentlyContinue |
    ForEach-Object { $_.Line } | Select-Object -First 80
  @"
# Stage E — mrc_loader → robotol handoff

- **mrc_loader ENTRY_CALLED:** $(if ($mrcLoaderEntry) { 'yes' } else { 'no' })
- **robotol EXTRACTED/MAPPED/REGISTERED/ENTRY:** $primaryExtracted / $primaryMapped / $primaryRegistered / $e1
- **CLOAD_SCOPE seen:** $(if ($cload) { 'yes' } else { 'no' })

## Gate separation

| Gate | Proven |
|------|--------|
| mrc_loader ENTRY_CALLED | $(if ($mrcLoaderEntry) { 'yes' } else { 'no' }) |
| cfunction reg_primary resolved | $(if ($all -match 'resolved=robotol\.ext') { 'yes' } else { 'no' }) |
| robotol mapped/register | $(if ($primaryMapped -or $primaryRegistered) { 'yes' } else { 'no' }) |
| robotol ENTRY_CALLED | $(if ($e1) { 'yes' } else { 'no' }) |

## Sample

``````
$($handLines -join "`n")
``````
"@ | Set-Content -Path $handoffReport -Encoding utf8
}

Write-Host "==== Stage E markers ===="
Select-String -Path $stdout -Pattern 'JJFB_MODULE_IDENTITY|JJFB_ROBOTOL|JJFB_CLOAD_SCOPE|MODULE_REGISTRY.*robotol|MODULE_REGISTRY.*mmochat|JJFB_MRC_INIT|R9_SWITCH_BLOCKED|PACKAGE_SCOPE|descriptor_launcher' -ErrorAction SilentlyContinue |
  Select-Object -First 50 | ForEach-Object { $_.Line }
Write-Host "report=$report"
if ($isJjfb) {
  Write-Host "identity=$identityReport"
  Write-Host "handoff=$handoffReport"
}

if (-not $hashOk) { Write-Host '[FAIL] hash changed'; exit 1 }
if ($all -match 'JJFB_GAMELIST_STARTED') { Write-Host '[FAIL] gamelist started'; exit 1 }
if ($fakeEnter) { Write-Host '[FAIL] identity pollution: BOOTSTRAP ROBOTOL_ENTER without true robotol'; exit 1 }

# E1 is the Stage E minimum; do not exit 0 claiming complete until ENTRY_CALLED.
if (-not $e1) {
  Write-Host '[PARTIAL] Stage E identity/env path ok; primary ENTRY_CALLED not yet proven'
  exit 2
}
Write-Host '[OK] Stage E E1 primary ENTRY_CALLED'
if ($e2) { Write-Host '[OK] Stage E E2 JJFB_MRC_INIT' }
if ($e4ret0) { Write-Host '[OK] Stage E E4 ret0=0' } elseif ($e2) { Write-Host '[PARTIAL] E4 ret0 still non-zero' }
if ($e4draw) { Write-Host '[OK] Stage E E4 DRAW/REFRESH' }
exit 0
