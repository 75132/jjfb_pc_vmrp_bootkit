# Phase 6M: cfunction publication / chunk_field_04 Source Audit
param(
  [int]$Seconds = 55,
  [switch]$SkipBuild,
  [switch]$SkipWxjwq
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
$GwyRoot = Join-Path $ResourceRoot 'gwy'
$jjfb = Join-Path $GwyRoot 'jjfb.mrp'
$wxjwq = Join-Path $GwyRoot 'wxjwq.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_cfunction_publication_audit.c') `
  -Pattern 'uc_mem_write\s*\(|force_entry|ui_mode\s*=' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden patterns: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $logDir,$reportDir | Out-Null

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$unit = Join-Path $Root 'build-i686\test_ext_cfunction_publication_audit.exe'
if (Test-Path $unit) {
  & $unit
  if ($LASTEXITCODE -ne 0) { throw 'unit test failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
if (-not (Test-Path $exe)) { throw "missing $exe" }

function Invoke-6MLive([string]$outLog, [string]$errLog, [string]$nmrpname, [int]$secs) {
  $legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
  if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  $env:GWY_LAUNCH_PARAM = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=${nmrpname}_gwyblink"
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
  $env:GWY_MODULE_SNAPSHOT = (Join-Path $logDir 'module_registry_phase6m.json')
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_LAUNCH_PATH = 'gwy_guest_native_runapp'
  $env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  $env:JJFB_PUBLICATION_AUDIT = '1'
  $env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'gbrwcore_only'
  $env:JJFB_CFUNCTION_PUBLICATION_AUDIT = '1'
  $env:JJFB_CHUNK_FIELD04_AUDIT = '1'
  $env:JJFB_P_TIMELINE_TRACE = '1'
  $env:JJFB_MULTI_TARGET_MIN_COMPARE = '1'
  $env:JJFB_SHELL_CHAIN_TARGET = $nmrpname
  $env:GWY_CONTEXT_WRITE_WATCH = '1'
  $env:GWY_CHUNK_PROVENANCE = '1'
  $env:GWY_OBJECT_IDENTITY = '1'
  $env:GWY_DISPATCH_TRACE = '1'
  $env:GWY_HELPER_HANDOFF = '1'
  $env:GWY_DSM_RECORD_CONTRACT = '1'
  $env:GWY_MODULE_ENTRY_ABI = '1'
  $env:GWY_ENTRY_NULL_CONTRACT = '1'
  $env:GWY_MODULE_DATA_INIT = '1'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_ER_RW_PRODUCER = '1'
  $env:GWY_BOOTSTRAP_ABI = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:GWY_NESTED_R9_SCOPE = '1'
  $env:GWY_POST_CONT_AUDIT = '1'
  $env:GWY_POST_CFN_R9_AUDIT = '1'
  $env:GWY_P_EXTCHUNK_AUDIT = '1'
  $env:GWY_GWY_STARTGAME_AUDIT = '1'
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_ENTRY_ABI_AUDIT -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  if (Test-Path $outLog) { Remove-Item -Force $outLog }
  if (Test-Path $errLog) { Remove-Item -Force $errLog }
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (Select-String -Path $outLog -Pattern '\[JJFB_6M_SUMMARY\]|mythroad exit|UC_MEM_READ_UNMAPPED' -Quiet) {
      Start-Sleep -Seconds 2
      break
    }
  }
  if (-not $p.HasExited) {
    Start-Sleep -Seconds 3
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 600
  }
}

Write-Host "== Phase 6M CFunction Publication Source Audit Seconds=$Seconds =="

$stdoutJj = Join-Path $logDir 'phase6m_cfunction_publication_stdout.txt'
$stderrJj = Join-Path $logDir 'phase6m_cfunction_publication_stderr.txt'
Invoke-6MLive $stdoutJj $stderrJj 'gwy/jjfb.mrp' $Seconds

$stdoutWx = Join-Path $logDir 'phase6m_cfunction_publication_wxjwq_stdout.txt'
$stderrWx = Join-Path $logDir 'phase6m_cfunction_publication_wxjwq_stderr.txt'
if (-not $SkipWxjwq -and (Test-Path $wxjwq)) {
  Write-Host '-- wxjwq cross-target --'
  Invoke-6MLive $stdoutWx $stderrWx 'gwy/wxjwq.mrp' $Seconds
}

# Find a host MRP that may contain cfunction.ext for offline disasm
$cfnMrp = $null
foreach ($cand in @(
  (Join-Path $ResourceRoot 'cfunction.mrp'),
  (Join-Path $ResourceRoot 'mythroad.mrp'),
  (Join-Path $RunDir 'overlay\mrp_member_view\shell_gbrwcore_cfunction.mrp')
)) {
  if (Test-Path $cand) { $cfnMrp = $cand; break }
}

$disasmArgs = @($stdoutJj, (Join-Path $reportDir 'phase6m_cfunction_disasm_89cf4_94f04.md'))
if ($cfnMrp) { $disasmArgs += @('--mrp', $cfnMrp) }
python (Join-Path $Root 'tools\phase6m_cfunction_disasm.py') @disasmArgs
if ($LASTEXITCODE -ne 0) { throw 'phase6m_cfunction_disasm failed' }

$reportArgs = @($stdoutJj, $reportDir)
if (Test-Path $stdoutWx) { $reportArgs += @('--wx-stdout', $stdoutWx) }
python (Join-Path $Root 'tools\phase6m_reports.py') @reportArgs
if ($LASTEXITCODE -ne 0) { throw 'phase6m_reports failed' }

python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
if ($LASTEXITCODE -ne 0) { throw 'audit_launcher_core failed' }

$hashAfter = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hashAfter -ne $ExpectedHash) { throw "jjfb.mrp hash changed: $hashAfter" }

Copy-Item -Force (Join-Path $reportDir 'CONCLUSION.md') (Join-Path $Root 'CONCLUSION_PHASE6M.md')

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$pack = Join-Path $Root "JJFB_phase6m_cfunction_publication_source_pack_$stamp.zip"
$items = @()
$items += Get-ChildItem (Join-Path $logDir 'phase6m_*') -File -ErrorAction SilentlyContinue
$items += Get-ChildItem (Join-Path $reportDir 'phase6m_*.md') -File -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $reportDir 'CONCLUSION.md') -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $Root 'CONCLUSION_PHASE6M.md') -ErrorAction SilentlyContinue
Compress-Archive -Path ($items | ForEach-Object FullName) -DestinationPath $pack -Force

$report = Join-Path $logDir 'phase6m_cfunction_publication_report.txt'
@(
  "phase=6M",
  "jjfb_hash=$hashAfter",
  "pack=$pack",
  "",
  (Get-Content (Join-Path $reportDir 'phase6m_verdict.md') -Raw)
) | Set-Content -Encoding utf8 $report

Write-Host "Phase 6M done. pack=$pack"
Write-Host "==== CONCLUSION ===="
Get-Content (Join-Path $reportDir 'phase6m_verdict.md')
exit 0
