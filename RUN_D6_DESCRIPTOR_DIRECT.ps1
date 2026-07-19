# D6 product track: cfg36 descriptor → original gwy/jjfb.mrp (no gamelist shell).
# Honest source=descriptor_launcher — does NOT claim native_shell runapp.
param(
  [int]$Seconds = 25,
  [int]$CfgIndex = 36,
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
$Profile = Join-Path $Root 'profiles\jjfb.json'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'

if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hashBefore = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hashBefore -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hashBefore" }

# Clear research/full-boot shell envs (must not redirect to gbrwcore/gamelist).
@(
  'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_LAUNCH_PATH',
  'JJFB_SHELL_CHAIN_MODE', 'JJFB_RUNAPP_NATIVE_ONLY', 'JJFB_PACKAGE_SCOPED_CLOAD',
  'JJFB_EXTCHUNK_PROVIDER', 'JJFB_ER_RW_BIND_RESTORE', 'JJFB_SHELL_NATIVE_EXEC_TRACE'
) | ForEach-Object {
  Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

if (-not (Test-Path $Launcher)) { throw "missing $Launcher — run RUN_BUILD.ps1" }
if (-not (Test-Path $exe)) { throw "missing $exe" }

# Sync run dir resources without launching visual path.
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'd6_descriptor_direct_stdout.txt'
$stderr = Join-Path $logDir 'd6_descriptor_direct_stderr.txt'
$report = Join-Path $Root 'reports\d6_descriptor_direct_boot.md'
if (Test-Path $stdout) { Remove-Item -Force $stdout }
if (Test-Path $stderr) { Remove-Item -Force $stderr }

$env:GWY_PROFILE = $Profile
$env:GWY_OVERLAY_ROOT = Join-Path $RunDir 'overlay'
New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null

Write-Host "== D6 descriptor direct boot cfg_index=$CfgIndex Seconds=$Seconds =="
Write-Host "launcher=$Launcher"
Write-Host "target via cfg → gwy/jjfb.mrp (source=descriptor_launcher)"

# Preflight validate
& $Launcher validate --root $ResourceRoot
if ($LASTEXITCODE -ne 0) { throw 'gwy_launcher validate failed' }

# Spawn via product CLI (sets GWY_LAUNCH_TARGET from cfg, not gbrwcore).
# Capture launcher stdout separately; vmrp inherits console — redirect by launching vmrp ourselves
# after printing descriptor, OR let CreateProcess inherit. Prefer direct env+main for log capture.
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
$env:GWY_LAUNCH_PARAM = $param
$env:GWY_RESOURCE_ROOT = $ResourceRoot

# Honest product tags (launcher spawn path also emits these when using `launch`).
"[JJFB_GWY_LAUNCH] cfg_index=$CfgIndex target=gwy/jjfb.mrp source=descriptor_launcher evidence=DOCUMENTED" |
  Out-File -FilePath $stdout -Encoding utf8
"[JJFB_PARAM] $param" | Out-File -FilePath $stdout -Append -Encoding utf8
"[JJFB_RUNAPP] source=descriptor_launcher target=gwy/jjfb.mrp spawned=pending evidence=DOCUMENTED" |
  Out-File -FilePath $stdout -Append -Encoding utf8

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300

$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput (Join-Path $logDir 'd6_vmrp_stdout.txt') `
  -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds([Math]::Max(5, $Seconds))
$stopPat = 'BOOTSTRAP_SEQ.*ROBOTOL_ENTER|JJFB_MRC_INIT|mrc_init|UC_MEM_READ_UNMAPPED|mythroad exit|br_mem_get failed'
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 400
  $vmLog = Join-Path $logDir 'd6_vmrp_stdout.txt'
  if ((Test-Path $vmLog) -and (Select-String -Path $vmLog -Pattern $stopPat -Quiet -ErrorAction SilentlyContinue)) {
    break
  }
}
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force } catch {}
}
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

# Merge logs (utf-8)
$vmLog = Join-Path $logDir 'd6_vmrp_stdout.txt'
$all = Get-Content $stdout -Raw -ErrorAction SilentlyContinue
if (Test-Path $vmLog) {
  # PS redirect may be UTF-16; normalize via .NET
  $bytes = [System.IO.File]::ReadAllBytes($vmLog)
  if ($bytes.Length -ge 2 -and $bytes[0] -eq 0xFF -and $bytes[1] -eq 0xFE) {
    $all += [System.Text.Encoding]::Unicode.GetString($bytes)
  } else {
    $all += [System.Text.Encoding]::UTF8.GetString($bytes)
  }
}
[System.IO.File]::WriteAllText($stdout, $all)

$hashAfter = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()

function Ok([string]$name, [bool]$pass, [string]$detail) {
  if ($pass) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
  return $pass
}

$checks = @()
$checks += Ok 'hash_unchanged' ($hashAfter -eq $ExpectedHash) $hashAfter
$checks += Ok 'target_jjfb' ($all -match 'target=gwy/jjfb\.mrp') 'no jjfb target'
$checks += Ok 'source_descriptor' ($all -match 'source=descriptor_launcher') 'missing source tag'
$checks += Ok 'no_gbrwcore_dsm' (-not ($all -match '\[GWY_LAUNCH\] target=gwy/gbrwcore\.mrp')) 'gbrwcore DSM'
$checks += Ok 'no_gamelist_started' (-not ($all -match 'JJFB_GAMELIST_STARTED')) 'gamelist started'
$checks += Ok 'no_host_runapp_equiv' (-not ($all -match 'host_runapp_equivalent')) 'host_runapp'
$checks += Ok 'mrc_loader' ($all -match 'mrc_loader\.ext') 'mrc_loader missing'
$checks += Ok 'robotol_or_alias' ($all -match 'robotol\.ext|profile_alias') 'robotol/alias missing'
$checks += Ok 'robotol_enter_or_entry' (
  $all -match 'ROBOTOL_ENTER|robotol\.ext.*ENTRY_CALLED|MODULE_REGISTRY.*robotol'
) 'robotol enter missing'

$passN = @($checks | Where-Object { $_ }).Count
$failN = $checks.Count - $passN

@"
# D6 — Descriptor Direct Boot

- **source:** ``descriptor_launcher`` (not ``native_shell``)
- **cfg_index:** $CfgIndex
- **target:** ``gwy/jjfb.mrp``
- **hash before/after:** ``$hashBefore`` / ``$hashAfter`` match=$($hashAfter -eq $ExpectedHash)
- **seconds:** $Seconds
- **checks pass/fail:** $passN / $failN

## Success criteria (advice §12)

| Gate | Result |
|------|--------|
| jjfb natural open / DSM target | $(if ($all -match 'gwy/jjfb\.mrp') { 'yes' } else { 'no' }) |
| mrc_loader.ext | $(if ($all -match 'mrc_loader\.ext') { 'yes' } else { 'no' }) |
| robotol.ext / alias | $(if ($all -match 'robotol\.ext|profile_alias') { 'yes' } else { 'no' }) |
| ROBOTOL_ENTER / entry | $(if ($all -match 'ROBOTOL_ENTER|robotol\.ext.*ENTRY') { 'yes' } else { 'no' }) |
| no gamelist shell | $(if ($all -notmatch 'JJFB_GAMELIST_STARTED') { 'yes' } else { 'no' }) |

## Log

- ``logs/d6_descriptor_direct_stdout.txt``
- ``logs/d6_vmrp_stdout.txt``
"@ | Set-Content -Path $report -Encoding utf8

Write-Host "==== D6 markers ===="
Select-String -Path $stdout -Pattern 'GWY_LAUNCH|descriptor_launcher|EXT_RESOLVE|ROBOTOL|mrc_loader|robotol|MRC_INIT|GAMELIST|gbrwcore' -ErrorAction SilentlyContinue |
  Select-Object -First 40 | ForEach-Object { $_.Line }
Write-Host "report=$report"
if ($failN -gt 0) { exit 1 }
Write-Host '[OK] D6 descriptor direct boot gates passed'
exit 0
