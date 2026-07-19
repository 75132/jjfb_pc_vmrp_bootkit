# Phase 5A live: scoped ExtResolver + MRP member view on Gwy runtime.
param(
  [int]$Seconds = 8,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..')).Path
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
$stdout = Join-Path $logDir 'phase5a_live_ext_stdout.txt'
$stderr = Join-Path $logDir 'phase5a_live_ext_stderr.txt'
$report = Join-Path $logDir 'phase5a_live_ext_report.txt'
$snap = Join-Path $logDir 'module_registry_phase5a.json'

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

Write-Host "== Phase 5A live ext resolve Seconds=$Seconds =="
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

Assert-Log 'mrc_loader_exact' (
  $all -match 'requested=mrc_loader\.ext.*strategy=exact' -or
  $all -match '\[EXT_RESOLVE\].*mrc_loader\.ext.*strategy=exact'
) 'mrc_loader.ext exact HIT missing'

Assert-Log 'cfunction_alias' (
  $all -match 'requested=cfunction\.ext.*strategy=profile_alias.*robotol\.ext' -or
  $all -match '\[EXT_RESOLVE\].*cfunction\.ext.*profile_alias'
) 'cfunction.ext profile_alias → robotol.ext missing'

Assert-Log 'robotol_extracted' (
  $all -match '\[MODULE_REGISTRY\].*robotol\.ext.*EXTRACTED' -or
  $all -match 'unpacked_size=253420' -or
  (Test-Path $snap)
) 'robotol EXTRACTED / snapshot missing'

Assert-Log 'no_code_3006' (-not ($all -match 'cfunction\.ext.*3006')) 'guest still reports cfunction.ext err 3006'

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash

Assert-Log 'dsm_cfunction_present' (Test-Path (Join-Path $RunDir 'cfunction.ext')) 'DSM host cfunction.ext missing'

# Original resource unchanged
$origHash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
Assert-Log 'resource_hash_unchanged' ($origHash -eq $ExpectedHash) $origHash

$failed = @($checks | Where-Object { -not $_.ok })
$lines = @('Phase 5A live ext resolve', "jjfb_sha256=$hash")
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "report=$report"

if ($failed.Count -gt 0) {
  throw ("phase5a live failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_EXT_RESOLVE complete'
