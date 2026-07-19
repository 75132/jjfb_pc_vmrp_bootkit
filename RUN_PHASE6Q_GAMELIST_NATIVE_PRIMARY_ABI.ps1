# Phase 6Q: Gamelist Native Primary + cfunction Helper ABI
param(
  [int]$Seconds = 75,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
$GwyRoot = Join-Path $ResourceRoot 'gwy'
$jjfb = Join-Path $GwyRoot 'jjfb.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path @(
  (Join-Path $Root 'src\formats\mrp_reg_primary.c'),
  (Join-Path $Root 'src\runtime\ext_entry_observe.c'),
  (Join-Path $Root 'third_party\vmrp_upstream\bridge.c')
) -Pattern 'force_entry|ui_mode\s*=' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden patterns: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$packDir = Join-Path $Root 'packages'
New-Item -ItemType Directory -Force -Path $logDir,$reportDir,$packDir | Out-Null

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$unitTargets = @('test_mrp_reg_primary', 'test_ext_gwy_shell_shim', 'test_ext_er_rw_bind_restore')
foreach ($t in $unitTargets) {
  $exe = Join-Path $Root "build-i686\$t.exe"
  if (-not (Test-Path $exe)) {
    & cmake --build (Join-Path $Root 'build-i686') --target $t
    if ($LASTEXITCODE -ne 0) { throw "unit target build failed: $t" }
  }
  & $exe
  if ($LASTEXITCODE -ne 0) { throw "unit test failed: $t" }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
if (-not (Test-Path $exe)) { throw "missing $exe" }

$stdoutJj = Join-Path $logDir 'phase6q_gamelist_native_primary_stdout.txt'
$stderrJj = Join-Path $logDir 'phase6q_gamelist_native_primary_stderr.txt'

$legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
$env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_RESOURCE_ROOT = $ResourceRoot
$env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
$env:GWY_MODULE_SNAPSHOT = (Join-Path $logDir 'module_registry_phase6q.json')
$env:JJFB_GWY_LAUNCHER_MODE = '1'
$env:JJFB_LAUNCH_PATH = 'gwy_shell_core_continue'
$env:JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
$env:JJFB_GAMELIST_MEMBER_VIEW_FIX = '1'
$env:JJFB_DSM_CFUNCTION_HELPER_ABI_AUDIT = '1'
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
$env:JJFB_GWY_UPDATE_STUB = 'no_update'
$env:JJFB_GAME_SELF_PATCH = '0'
$env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
$env:JJFB_PUBLICATION_AUDIT = '1'
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'shell'
$env:JJFB_EXTCHUNK_PROVIDER = 'shell_core'
$env:JJFB_ER_RW_BIND_RESTORE = 'shell_core'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_CFUNCTION_PUBLICATION_AUDIT = '1'
$env:JJFB_CHUNK_FIELD04_AUDIT = '1'
$env:JJFB_P_TIMELINE_TRACE = '1'
$env:JJFB_SHELL_CHAIN_TARGET = 'gwy/jjfb.mrp'
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
if (Test-Path $stdoutJj) { Remove-Item -Force $stdoutJj }
if (Test-Path $stderrJj) { Remove-Item -Force $stderrJj }

Write-Host "== Phase 6Q Gamelist Native Primary ABI Seconds=$Seconds =="
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $stdoutJj -RedirectStandardError $stderrJj -PassThru
$deadline = (Get-Date).AddSeconds([Math]::Max(1, $Seconds))
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
  if (-not (Test-Path $stdoutJj)) { continue }
  if (Select-String -Path $stdoutJj -Pattern '\[JJFB_GAMELIST_CFG36_BUILD\]|\[JJFB_SHELL_EXPORT_CALL\]|mythroad exit|UC_MEM_READ_UNMAPPED|fault_pc=0x8CC00|\[JJFB_HELPER_CALL_ROUTE\]' -Quiet) {
    if (Select-String -Path $stdoutJj -Pattern 'mythroad exit|UC_MEM_READ_UNMAPPED|fault_pc=0x8CC00|\[JJFB_GAMELIST_CFG36_BUILD\]' -Quiet) {
      Start-Sleep -Seconds 3
      break
    }
  }
}
if (-not $p.HasExited) {
  Start-Sleep -Seconds 4
  try { Stop-Process -Id $p.Id -Force } catch {}
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 600
}

python (Join-Path $Root 'tools\phase6q_reports.py') $stdoutJj $reportDir --stderr $stderrJj
if ($LASTEXITCODE -ne 0) { throw 'phase6q_reports failed' }

python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
if ($LASTEXITCODE -ne 0) { throw 'audit_launcher_core failed' }

$hashAfter = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hashAfter -ne $ExpectedHash) { throw "jjfb.mrp hash changed: $hashAfter" }

Copy-Item -Force (Join-Path $reportDir 'CONCLUSION.md') (Join-Path $Root 'CONCLUSION.md')
Copy-Item -Force (Join-Path $reportDir 'phase6q_verdict.md') (Join-Path $Root 'phase6q_verdict.md')

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$pack = Join-Path $packDir "JJFB_phase6q_gamelist_native_primary_abi_pack_$stamp.zip"
$items = @()
$items += Get-ChildItem (Join-Path $logDir 'phase6q_*') -File -ErrorAction SilentlyContinue
$items += Get-ChildItem (Join-Path $reportDir 'phase6q_*.md') -File -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $reportDir 'CONCLUSION.md') -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $Root 'CONCLUSION.md') -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $Root 'phase6q_verdict.md') -ErrorAction SilentlyContinue
Compress-Archive -Path ($items | ForEach-Object FullName) -DestinationPath $pack -Force

@(
  "phase=6Q",
  "jjfb_hash=$hashAfter",
  "pack=$pack",
  "",
  (Get-Content (Join-Path $reportDir 'phase6q_verdict.md') -Raw)
) | Set-Content -Encoding utf8 (Join-Path $logDir 'phase6q_gamelist_native_primary_report.txt')

Write-Host "Phase 6Q done. pack=$pack"
Write-Host "==== CONCLUSION ===="
Get-Content (Join-Path $reportDir 'phase6q_verdict.md')
exit 0
