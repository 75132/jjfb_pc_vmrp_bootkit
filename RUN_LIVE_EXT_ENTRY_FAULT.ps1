# Phase 5D live: robotol entry ABI + UC_MEM_READ_UNMAPPED@0x28 fault classification.
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

# Gate: no hard-coded absolute fault PC in new observe sources
$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_entry_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\gwy_ext_obs.c') -Pattern '0x304558|0x2D8DE0' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC/base literals found in observe sources: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase5d_live_entry_stdout.txt'
$stderr = Join-Path $logDir 'phase5d_live_entry_stderr.txt'
$report = Join-Path $logDir 'phase5d_entry_fault_report.txt'
$snap = Join-Path $logDir 'module_registry_phase5d.json'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
if (-not (Test-Path $exe)) { throw "missing $exe" }

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

Write-Host "== Phase 5D live entry fault Seconds=$Seconds =="
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

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
$origHash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
Assert-Log 'resource_hash_unchanged' ($origHash -eq $ExpectedHash) $origHash

Assert-Log 'robotol_registered' (
  $all -match 'resolved=robotol\.ext.*state=REGISTERED' -or
  $all -match 'robotol\.ext.*REGISTERED'
) 'robotol REGISTERED missing'

Assert-Log 'entry_called_or_ctx' (
  $all -match '\[EXT_ENTRY\]' -or
  $all -match '\[EXT_ENTRY_CTX\]' -or
  $all -match 'state=ENTRY_CALLED'
) 'ENTRY_CALLED / EXT_ENTRY_CTX missing'

Assert-Log 'ext_fault_present' ($all -match '\[EXT_FAULT\]') 'EXT_FAULT missing'

Assert-Log 'module_relative_offset' (
  $all -match 'module_offset=0x[0-9A-Fa-f]+'
) 'module_offset missing'

Assert-Log 'fault_insn_or_regs' (
  $all -match '\[EXT_FAULT_INSN\]' -or
  $all -match 'base_register=r'
) 'fault insn decode missing'

Assert-Log 'failed_lifecycle' (
  $all -match '\[EXT_FAIL\].*ENTRY_EXECUTION' -or
  $all -match 'state=FAILED'
) 'FAILED ENTRY_EXECUTION missing'

Assert-Log 'root_cause_class' (
  $all -match '\[EXT_FAULT_CLASS\] root_cause=(ENTRY_ARGUMENT|ER_RW_INIT|RELOCATION|PLATFORM_CONTEXT|WRONG_ENTRY|OTHER)'
) 'root_cause class missing'

Assert-Log 'no_low_va_map_hack' (
  -not ($all -match 'uc_mem_map\(\s*0x0\s*,') -and
  -not ($all -match 'mapped low address 0x28')
) 'low VA map hack suspected'

# Prefer EXT_FAULT line for offsets (not earlier ENTRY_CTX noise)
$rootCause = 'UNKNOWN'
if ($all -match '\[EXT_FAULT_CLASS\] root_cause=(\w+)') { $rootCause = $Matches[1] }
$baseReg = '?'
$baseVal = '?'
if ($all -match '\[EXT_FAULT_INSN\].*base_register=r(\d+) base_value=0x([0-9A-Fa-f]+)') {
  $baseReg = $Matches[1]
  $baseVal = $Matches[2]
}
$modOff = '?'
$faultPc = '?'
if ($all -match '\[EXT_FAULT\].*fault_pc=0x([0-9A-Fa-f]+).*module_offset=0x([0-9A-Fa-f]+)') {
  $faultPc = $Matches[1]
  $modOff = $Matches[2]
}
$faultInsn = '?'
if ($all -match '\[EXT_FAULT_INSN\] fault_instruction=([^\r\n]+)') {
  $faultInsn = ($Matches[1] -split ' base_')[0]
}

$phase6a = ''
if ($rootCause -eq 'PLATFORM_CONTEXT') {
  $phase6a = 'NEXT=Phase6A_Minimal_PlatformRegistry; list platform slots touched in EXT_FAULT_RING'
} elseif ($rootCause -eq 'ENTRY_ARGUMENT') {
  $phase6a = 'NEXT=fix_generic_helper_dispatch_or_guest_nested_P_arg; no PlatformRegistry yet'
}

$failed = @($checks | Where-Object { -not $_.ok })
$lines = @(
  'Phase 5D robotol entry ABI / null-context fault',
  "jjfb_sha256=$hash",
  "root_cause=$rootCause",
  "fault_pc=0x$faultPc",
  "module_offset=0x$modOff",
  "fault_instruction=$faultInsn",
  "base_register=r$baseReg",
  "base_value=0x$baseVal",
  "phase6a_note=$phase6a",
  'note=observe-only fault path; no alias/3004 changes; no low-VA map'
)
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "root_cause=$rootCause fault_pc=0x$faultPc module_offset=0x$modOff insn=$faultInsn base=r$baseReg=0x$baseVal"
Write-Host "report=$report"

if ($failed.Count -gt 0) {
  throw ("phase5d live failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_EXT_ENTRY_FAULT complete'
