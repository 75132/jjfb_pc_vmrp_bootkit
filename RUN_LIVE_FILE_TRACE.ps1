# Phase 4C live loader file-trace smoke: Gwy runtime + GWY_LAUNCH=1, short run.
param(
  [int]$Seconds = 8,
  [switch]$SkipBuild,
  [switch]$ProveNoCwdTree
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }

$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase4c_live_file_trace_stdout.txt'
$stderr = Join-Path $logDir 'phase4c_live_file_trace_stderr.txt'
$report = Join-Path $logDir 'phase4c_live_file_trace_report.txt'
$cfunctionMiss = Join-Path $logDir 'phase4c_cfunction_miss.txt'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
if (-not (Test-Path $exe)) { throw "missing $exe" }

# Ensure no cwd sdk_key
$myth = Join-Path $RunDir 'mythroad'
$legacyKey = Join-Path $myth 'sdk_key.dat'
if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }

$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_RESOURCE_ROOT = $ResourceRoot
$env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

if ($ProveNoCwdTree) {
  # Optional strong proof: empty/missing cwd mythroad tree; still read via GWY_RESOURCE_ROOT
  if (Test-Path $myth) {
    $bak = Join-Path $Root 'out\vmrp_run_mythroad_bak'
    if (Test-Path $bak) { Remove-Item -Recurse -Force $bak }
    Move-Item -Force $myth $bak
  }
  New-Item -ItemType Directory -Force -Path $myth | Out-Null
}

Write-Host "== live file trace Seconds=$Seconds =="
$p = Start-Process -FilePath $exe `
  -WorkingDirectory $RunDir `
  -RedirectStandardOutput $stdout `
  -RedirectStandardError $stderr `
  -PassThru

Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force } catch {}
  Start-Sleep -Milliseconds 400
}

$outText = ''
if (Test-Path $stdout) { $outText = Get-Content $stdout -Raw -ErrorAction SilentlyContinue }
if (-not $outText) { $outText = '' }
$errText = ''
if (Test-Path $stderr) { $errText = Get-Content $stderr -Raw -ErrorAction SilentlyContinue }
$all = $outText + "`n" + $errText

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if (-not $ok) { Write-Host "[FAIL] $name : $detail" } else { Write-Host "[OK] $name" }
}

Assert-Log 'bind_evidence' (
  ($all -match 'GuestVFS bound') -or ($all -match '\[VM_FILE_BIND\]') -or ($all -match 'gwy_vm_file bound')
) 'missing GuestVFS/gwy_vm_file bind log'

Assert-Log 'jjfb_package_hit' (
  ($all -match '\[VM_FILE_OPEN\].*jjfb\.mrp.*backend=canonical.*result=HIT') -or
  ($all -match '\[JJFB_FILEOPEN\].*jjfb\.mrp.*backend=canonical') -or
  (($all -match '\[VM_FILE_OPEN\].*jjfb\.mrp.*backend=generated.*result=HIT') -and
   ($all -match '\[MRP_MEMBER_VIEW\] installed.*original_sha256=52c13182'))
) 'jjfb.mrp HIT via canonical or member-view generated missing'

Assert-Log 'sdk_key_generated' (
  ($all -match '\[VM_FILE_OPEN\].*sdk_key\.dat.*backend=generated.*result=HIT') -or
  ($all -match '\[JJFB_FILEOPEN\].*sdk_key\.dat.*backend=generated') -or
  ($all -match '\[JJFB_SDK_KEY\].*backend=generated')
) 'sdk_key generated HIT not found'

Assert-Log 'no_cwd_sdk_key' (-not (Test-Path $legacyKey)) "cwd mythroad/sdk_key.dat must not exist: $legacyKey"

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash

# Fail-fast: plain unbound with GWY_LAUNCH must refuse
Write-Host '== fail-fast unbound plain =='
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Plain
if ($LASTEXITCODE -ne 0) { throw 'Plain build failed' }
$plainDir = Join-Path $Root 'out\vmrp_plain'
$plainExe = Join-Path $plainDir 'main.exe'
$plainOut = Join-Path $logDir 'phase4c_failfast_stdout.txt'
$plainErr = Join-Path $logDir 'phase4c_failfast_stderr.txt'
Copy-Item -Force (Join-Path $Root 'third_party\vmrp_upstream\windows\SDL2-2.0.10\i686-w64-mingw32\bin\SDL2.dll') $plainDir -ErrorAction SilentlyContinue
Copy-Item -Force (Join-Path $Root 'third_party\vmrp_upstream\windows\unicorn-1.0.2-win32\unicorn.dll') $plainDir -ErrorAction SilentlyContinue
$env:GWY_LAUNCH = '1'
$env:GWY_RESOURCE_ROOT = $ResourceRoot
$env:GWY_OVERLAY_ROOT = (Join-Path $plainDir 'overlay')
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
$pf = Start-Process -FilePath $plainExe -WorkingDirectory $plainDir `
  -RedirectStandardOutput $plainOut -RedirectStandardError $plainErr -PassThru
Start-Sleep -Seconds 4
if (-not $pf.HasExited) { try { Stop-Process -Id $pf.Id -Force } catch {} }
$pfText = ''
if (Test-Path $plainOut) { $pfText = Get-Content $plainOut -Raw -ErrorAction SilentlyContinue }
if (Test-Path $plainErr) { $pfText += "`n" + (Get-Content $plainErr -Raw -ErrorAction SilentlyContinue) }
Assert-Log 'failfast_unbound' ($pfText -match 'fail-fast|GuestVFS not bound|BLOCKER') 'plain+GWY_LAUNCH should fail-fast'

# Record cfunction.ext miss evidence only (no alias)
if ($all -match 'cfunction\.ext') {
  $lines = ($all -split "`n") | Where-Object { $_ -match 'cfunction\.ext' }
  Set-Content -Path $cfunctionMiss -Value ($lines -join "`n") -Encoding utf8
  Write-Host "[INFO] cfunction.ext evidence -> $cfunctionMiss (alias NOT implemented)"
}

$failed = @($checks | Where-Object { -not $_.ok })
$lines = @()
$lines += 'Phase 4C live file trace'
$lines += "jjfb_sha256=$hash"
$lines += "seconds=$Seconds"
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
$lines += "stdout=$stdout"
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "report=$report"

if ($failed.Count -gt 0) {
  throw ("live file trace failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}

# Rebuild Gwy as default formal artifact after plain smoke
if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'restore Gwy build failed' }
}

Write-Host '[OK] RUN_LIVE_FILE_TRACE complete'
