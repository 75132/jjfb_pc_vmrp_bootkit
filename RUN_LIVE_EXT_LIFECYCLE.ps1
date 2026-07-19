# Phase 5C live: guest LOOKUP 3004 fixed; robotol MAPPED+; nested observer.
param(
  [int]$Seconds = 12,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase5c_live_ext_stdout.txt'
$stderr = Join-Path $logDir 'phase5c_live_ext_stderr.txt'
$report = Join-Path $logDir 'phase5c_live_ext_report.txt'
$snap = Join-Path $logDir 'module_registry_phase5c.json'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
if (-not (Test-Path $exe)) { throw "missing $exe" }
if (-not (Test-Path (Join-Path $RunDir 'cfunction.ext'))) {
  throw 'missing DSM host cfunction.ext in run dir'
}

$legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }

$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_RESOURCE_ROOT = $ResourceRoot
$env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
$env:GWY_MODULE_SNAPSHOT = $snap
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

Write-Host "== Phase 5C live ext lifecycle Seconds=$Seconds =="
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force } catch {}
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 400
}

$all = ''
if (Test-Path $stdout) { $all += (Get-Content $stdout -Raw -ErrorAction SilentlyContinue) }
if (Test-Path $stderr) { $all += "`n" + (Get-Content $stderr -Raw -ErrorAction SilentlyContinue) }

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if ($ok) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
}

Assert-Log 'cfunction_alias' (
  $all -match 'requested=cfunction\.ext.*profile_alias' -or
  $all -match '\[EXT_RESOLVE\].*cfunction\.ext.*profile_alias' -or
  $all -match '\[MRP_VIEW_LOOKUP\].*cfunction\.ext'
) 'alias/view missing'

Assert-Log 'robotol_extracted' (
  $all -match '\[MODULE_REGISTRY\].*robotol\.ext.*EXTRACTED' -or
  $all -match 'unpacked_size=253420'
) 'robotol EXTRACTED missing'

Assert-Log 'dsm_origin_separate' (
  $all -match 'dsm:cfunction\.ext' -or
  $all -match 'origin=DSM'
) 'DSM origin row missing'

Assert-Log 'no_ordinal_identity' (
  -not ($all -match 'helper.?ordinal|Nth.?helper|third.?helper')
) 'ordinal language found'

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
$origHash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
Assert-Log 'resource_hash_unchanged' ($origHash -eq $ExpectedHash) $origHash

Assert-Log 'view_provenance' (
  $all -match 'original_sha256=52c13182' -or
  (Test-Path $snap -and (Get-Content $snap -Raw) -match 'original_sha256')
) 'member-view provenance missing'

# 5C-A gate: guest LOOKUP must not 3004 on cfunction.ext
$err3004 = $all -match 'cfunction\.ext.*3004|read file "cfunction\.ext" err, code=3004'
$err3006 = $all -match 'cfunction\.ext.*3006|read file "cfunction\.ext" err, code=3006'
Assert-Log 'no_cfunction_3004' (-not $err3004) 'guest LOOKUP 3004 still present (torn index)'

# Classify robotol stop state (highest wins)
$robotolState = 'EXTRACTED'
if ($all -match '\[EXT_MAP\]' -or $all -match 'stage=MAPPED' -or $all -match 'reason=decompress_alloc') {
  if ($all -match 'robotol\.ext.*state=MAPPED' -or
      $all -match 'requested=cfunction\.ext.*state=MAPPED' -or
      $all -match 'resolved=robotol\.ext.*state=MAPPED' -or
      $all -match 'stage=MAPPED.*reason=decompress_alloc') {
    $robotolState = 'MAPPED'
  }
}
if ($all -match '\[EXT_REGISTER\]' -or $all -match '\[GUEST_CFUNCTION_NEW\]') {
  if ($all -match 'resolved=robotol\.ext.*state=REGISTERED' -or
      $all -match 'requested=cfunction\.ext.*state=REGISTERED' -or
      $all -match 'robotol\.ext.*REGISTERED') {
    $robotolState = 'REGISTERED'
  }
}
if ($all -match '\[EXT_ENTRY\]' -and (
    $all -match 'resolved=robotol\.ext.*state=ENTRY_CALLED' -or
    $all -match 'requested=cfunction\.ext.*state=ENTRY_CALLED' -or
    $all -match 'robotol\.ext.*ENTRY_CALLED'
  )) { $robotolState = 'ENTRY_CALLED' }

$case = switch ($robotolState) {
  'ENTRY_CALLED' { 'A' }
  'REGISTERED' { 'A' }
  'MAPPED' { 'B' }
  default { 'C' }
}
if ($all -match 'stage=UNASSOCIATED') { $case = 'D' }

$nestedGuestNew = $all -match '_mr_c_function_new\([0-9A-Fa-f]+'
$gcoAttach = $all -match '\[GUEST_CALL_OBSERVER\]'
$gcoIndirect = $all -match '\[GUEST_INDIRECT_CALL\]|\[GUEST_CFUNCTION_NEW\]'

Assert-Log 'robotol_stop_classified' (
  $robotolState -in @('EXTRACTED','MAPPED','REGISTERED','ENTRY_CALLED')
) "state=$robotolState case=$case"

# Soft expectation after 5C-A: MAPPED via decompress alloc (not hard-fail EXTRACTED-only yet if alloc missed)
Assert-Log 'robotol_at_least_mapped' (
  $robotolState -in @('MAPPED','REGISTERED','ENTRY_CALLED')
) "stop=$robotolState (want >=MAPPED after 3004 fix)"

$failed = @($checks | Where-Object { -not $_.ok })
$lines = @(
  'Phase 5C live ext lifecycle',
  "jjfb_sha256=$hash",
  "robotol_final_state=$robotolState",
  "classification_case=$case",
  "guest_nested_c_function_new=$nestedGuestNew",
  "cfunction_read_3004=$err3004",
  "cfunction_read_3006=$err3006",
  "guest_call_observer_attach=$gcoAttach",
  "guest_call_observer_indirect=$gcoIndirect",
  '3004_root_cause=LOOKUP invalid_name_len after torn index; fix=insert_at=first_data+12',
  'note=5C-A no 3004; 5C-B observe nested helper via code region (no ordinal)'
)
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "robotol_final_state=$robotolState case=$case"
Write-Host "report=$report"

if ($failed.Count -gt 0) {
  throw ("phase5c live failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_EXT_LIFECYCLE complete'
