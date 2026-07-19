# Native GWY/JJFB Full Boot — one-shot shell chain to jjfb (CursorPack)
param(
  [int]$Seconds = 120,
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
  (Join-Path $Root 'src\runtime\package_scope.c'),
  (Join-Path $Root 'src\runtime\ext_loader.c'),
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
  # Rebuild launcher_core targets that Full Boot depends on
  $cmakeBuild = Join-Path $Root 'build-i686'
  if (Test-Path $cmakeBuild) {
    & cmake --build $cmakeBuild --target launcher_core test_package_scope test_ext_chunk_provider test_ext_er_rw_bind_restore test_ext_gwy_shell_shim
    if ($LASTEXITCODE -ne 0) { throw 'cmake unit target build failed' }
  }
}

$unitTargets = @('test_package_scope', 'test_ext_chunk_provider', 'test_ext_er_rw_bind_restore', 'test_ext_gwy_shell_shim', 'test_mrp_reg_primary')
foreach ($t in $unitTargets) {
  $exeU = Join-Path $Root "build-i686\$t.exe"
  if (-not (Test-Path $exeU)) {
    & cmake --build (Join-Path $Root 'build-i686') --target $t
    if ($LASTEXITCODE -ne 0) { throw "unit target build failed: $t" }
  }
  & $exeU
  if ($LASTEXITCODE -ne 0) { throw "unit test failed: $t" }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
if (-not (Test-Path $exe)) { throw "missing $exe" }

$stdoutJj = Join-Path $logDir 'fullboot_native_gwy_jjfb_stdout.txt'
$stderrJj = Join-Path $logDir 'fullboot_native_gwy_jjfb_stderr.txt'

$legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }

# Full Boot env (MasterPlan / CursorPack)
$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
$env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_RESOURCE_ROOT = $ResourceRoot
$env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
$env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
$env:GWY_MODULE_SNAPSHOT = (Join-Path $logDir 'module_registry_fullboot.json')

$env:JJFB_NATIVE_BOOT_FULL = '1'
$env:JJFB_GWY_LAUNCHER_MODE = '1'
$env:JJFB_LAUNCH_PATH = 'gwy_native_full_shell'
$env:JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
$env:JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
$env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
$env:JJFB_EXTCHUNK_PROVIDER = 'shell_and_game'
$env:JJFB_ER_RW_BIND_RESTORE = 'shell_and_game'
$env:JJFB_GWY_UPDATE_STUB = 'no_update_native_branch'
$env:JJFB_RUNAPP_NATIVE_ONLY = '1'
$env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
$env:JJFB_CONTROL_TARGET = 'gwy/wxjwq.mrp'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_GAME_SELF_PATCH = '0'
$env:JJFB_SHELL_CHAIN_TARGET = 'gwy/jjfb.mrp'
$env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
$env:JJFB_PUBLICATION_AUDIT = '1'
$env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'shell'
$env:JJFB_CFUNCTION_PUBLICATION_AUDIT = '1'
$env:JJFB_CHUNK_FIELD04_AUDIT = '1'
$env:JJFB_P_TIMELINE_TRACE = '1'
$env:JJFB_DSM_CFUNCTION_HELPER_ABI_AUDIT = '1'

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

New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
if (Test-Path $stdoutJj) { Remove-Item -Force $stdoutJj }
if (Test-Path $stderrJj) { Remove-Item -Force $stderrJj }

$envDump = @(
  "JJFB_NATIVE_BOOT_FULL=$($env:JJFB_NATIVE_BOOT_FULL)",
  "JJFB_LAUNCH_PATH=$($env:JJFB_LAUNCH_PATH)",
  "JJFB_PACKAGE_SCOPED_CLOAD=$($env:JJFB_PACKAGE_SCOPED_CLOAD)",
  "JJFB_MEMBER_VIEW_PRIMARY=$($env:JJFB_MEMBER_VIEW_PRIMARY)",
  "JJFB_EXTCHUNK_PROVIDER=$($env:JJFB_EXTCHUNK_PROVIDER)",
  "JJFB_ER_RW_BIND_RESTORE=$($env:JJFB_ER_RW_BIND_RESTORE)",
  "JJFB_GWY_UPDATE_STUB=$($env:JJFB_GWY_UPDATE_STUB)",
  "JJFB_RUNAPP_NATIVE_ONLY=$($env:JJFB_RUNAPP_NATIVE_ONLY)",
  "GWY_LAUNCH_TARGET=$($env:GWY_LAUNCH_TARGET)",
  "GWY_LAUNCH_PARAM=$($env:GWY_LAUNCH_PARAM)"
) -join "`n"

Write-Host "== Native GWY/JJFB Full Boot Seconds=$Seconds =="
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $stdoutJj -RedirectStandardError $stderrJj -PassThru
$deadline = (Get-Date).AddSeconds([Math]::Max(1, $Seconds))
$stopPat = '\[JJFB_MRC_INIT\]|\[JJFB_RUNAPP\].*source=native_shell|\[JJFB_DRAW\]|mythroad exit|UC_MEM_READ_UNMAPPED|br_mem_get failed=no_memory'
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
  if (-not (Test-Path $stdoutJj)) { continue }
  if (Select-String -Path $stdoutJj -Pattern $stopPat -Quiet) {
    if (Select-String -Path $stdoutJj -Pattern '\[JJFB_MRC_INIT\]|\[JJFB_DRAW\]|mythroad exit|br_mem_get failed=no_memory' -Quiet) {
      Start-Sleep -Seconds 4
      break
    }
    # native runapp: give a bit more time for jjfb open / mrc_init
    if (Select-String -Path $stdoutJj -Pattern '\[JJFB_RUNAPP\].*source=native_shell' -Quiet) {
      Start-Sleep -Seconds 8
      if (Select-String -Path $stdoutJj -Pattern '\[JJFB_MRC_INIT\]|\[JJFB_DRAW\]|mythroad exit' -Quiet) { break }
    }
  }
}
if (-not $p.HasExited) {
  Start-Sleep -Seconds 5
  try { Stop-Process -Id $p.Id -Force } catch {}
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 600
}

python (Join-Path $Root 'tools\fullboot_reports.py') $stdoutJj $reportDir --stderr $stderrJj --env-dump $envDump
if ($LASTEXITCODE -ne 0) { throw 'fullboot_reports failed' }

if (Test-Path (Join-Path $Root 'tools\audit_launcher_core.py')) {
  python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
  if ($LASTEXITCODE -ne 0) { throw 'audit_launcher_core failed' }
}

$hashAfter = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hashAfter -ne $ExpectedHash) { throw "jjfb.mrp hash changed: $hashAfter" }

Copy-Item -Force (Join-Path $reportDir 'CONCLUSION.md') (Join-Path $Root 'CONCLUSION.md')
Copy-Item -Force (Join-Path $reportDir 'fullboot_10_final_verdict.md') (Join-Path $Root 'fullboot_verdict.md')

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$pack = Join-Path $packDir "JJFB_fullboot_native_gwy_jjfb_pack_$stamp.zip"
$items = @()
$items += Get-ChildItem (Join-Path $logDir 'fullboot_*') -File -ErrorAction SilentlyContinue
$items += Get-ChildItem (Join-Path $reportDir 'fullboot_*.md') -File -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $reportDir 'CONCLUSION.md') -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $Root 'CONCLUSION.md') -ErrorAction SilentlyContinue
$items += Get-Item (Join-Path $Root 'fullboot_verdict.md') -ErrorAction SilentlyContinue
Compress-Archive -Path ($items | ForEach-Object FullName) -DestinationPath $pack -Force

Write-Host "Full Boot done. pack=$pack"
Write-Host "==== VERDICT ===="
Get-Content (Join-Path $reportDir 'fullboot_10_final_verdict.md')
exit 0
