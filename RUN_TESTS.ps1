# Local preflight + unit tests. No GUI required.
param(
  [string]$BuildDir = "build-i686",
  [string]$FixtureRoot = "",
  [switch]$SkipBuild,
  [switch]$SkipNegativeAudit
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;" + $env:Path

if (-not $FixtureRoot) {
  $candidate = Join-Path $Root 'game_files\mythroad\320x480'
  if (Test-Path $candidate) { $FixtureRoot = $candidate }
}
$env:GWY_FIXTURE_ROOT = $FixtureRoot
$env:GWY_RESOURCE_ROOT = $FixtureRoot

New-Item -ItemType Directory -Force -Path (Join-Path $Root 'logs') | Out-Null
$auditJson = Join-Path $Root 'logs\audit_preflight.json'

Write-Host '== audit_launcher_core =='
& python (Join-Path $Root 'tools\audit_launcher_core.py') $Root --json $auditJson
if ($LASTEXITCODE -ne 0) { throw "anti-drift audit failed" }

if (-not $SkipNegativeAudit) {
  Write-Host '== audit negative control =='
  & python (Join-Path $Root 'tools\test_audit_gate.py')
  if ($LASTEXITCODE -ne 0) { throw "audit gate self-test failed" }
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir $BuildDir
  if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

Write-Host '== unit tests =='
$env:GWY_FIXTURE_ROOT = $FixtureRoot
@(
  'test_smoke.exe',
  'test_guest_vfs.exe',
  'test_mrp_jjfb.exe',
  'test_cfg36.exe',
  'test_game_catalog.exe',
  'test_launch_descriptor.exe',
  'test_ext_resolver.exe',
  'test_platform_identity.exe',
  'test_vm_runtime.exe',
  'test_guest_memory.exe',
  'test_vm_file_service.exe',
  'test_module_registry.exe',
  'test_ext_loader.exe',
  'test_mrp_member_view.exe',
  'test_ext_entry_observe.exe',
  'test_ext_object_observe.exe',
  'test_ext_helper_handoff.exe',
  'test_ext_dsm_record_observe.exe',
  'test_ext_callback_frame.exe',
  'test_ext_post_cont_audit.exe',
  'test_ext_post_cfn_r9_audit.exe',
  'test_ext_p_extchunk_audit.exe',
  'test_ext_gwy_startgame_audit.exe'
) | ForEach-Object {
  $p = Join-Path $Root "$BuildDir\$_"
  if (-not (Test-Path $p)) { throw "missing $_" }
  if ($_ -eq 'test_smoke.exe' -or $_ -eq 'test_guest_vfs.exe' -or $FixtureRoot) {
    Write-Host "run $_"
    if ($_ -eq 'test_ext_resolver.exe') {
      $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
      $env:GWY_MODULE_SNAPSHOT = Join-Path $Root 'logs\module_registry_jjfb.json'
    }
    & $p
    if ($LASTEXITCODE -ne 0) { throw "$_ failed" }
  }
}

if ($FixtureRoot -and (Test-Path (Join-Path $FixtureRoot 'gwy\jjfb.mrp'))) {
  Write-Host '== mrp_inspect golden smoke =='
  $out = Join-Path $Root 'logs\jjfb_mrp_inspect.json'
  & python (Join-Path $Root 'tools\mrp_inspect.py') (Join-Path $FixtureRoot 'gwy\jjfb.mrp') --json $out | Out-Null
  if ($LASTEXITCODE -ne 0) { throw "mrp_inspect failed" }
  $report = Get-Content $out -Raw | ConvertFrom-Json
  if ($report.sha256 -ne '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036') {
    throw "jjfb.mrp sha256 mismatch: $($report.sha256)"
  }
  Write-Host '[OK] jjfb.mrp hash matches evidence'

  Write-Host '== gwy_launcher validate =='
  $exe = Join-Path $Root "$BuildDir\gwy_launcher.exe"
  & $exe validate --root $FixtureRoot
  if ($LASTEXITCODE -ne 0) { throw "gwy_launcher validate failed" }

  Write-Host '== gwy_launcher vfs-check =='
  $ov = Join-Path $Root 'logs\vfs_overlay_check'
  & $exe vfs-check --root $FixtureRoot --overlay $ov
  if ($LASTEXITCODE -ne 0) { throw "gwy_launcher vfs-check failed" }

  Write-Host '== sdk_key cwd absence gate =='
  $runMyth = Join-Path $Root 'out\vmrp_run\mythroad\sdk_key.dat'
  if (Test-Path $runMyth) { Remove-Item -Force $runMyth }
  $ov2 = Join-Path $Root 'logs\vfs_overlay_phase4b'
  New-Item -ItemType Directory -Force -Path $ov2 | Out-Null
  & $exe vfs-check --root $FixtureRoot --overlay $ov2 | Out-Null
  if ($LASTEXITCODE -ne 0) { throw 'vfs-check overlay install failed' }
  if (Test-Path $runMyth) { throw "cwd mythroad/sdk_key.dat must not exist after VFS install" }
  Write-Host '[OK] no cwd mythroad/sdk_key.dat'

  Write-Host '== Phase 4C live file trace (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_FILE_TRACE.ps1') -Seconds 6
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_FILE_TRACE failed' }

  Write-Host '== Phase 5A live ext resolve (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_EXT_RESOLVE.ps1') -Seconds 6
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_EXT_RESOLVE failed' }

  Write-Host '== Phase 5D live entry fault (short) =='
  # 14s: observe chain through robotol second-hop fault needs headroom under A5–A7 hooks.
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_EXT_ENTRY_FAULT.ps1') -Seconds 14
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_EXT_ENTRY_FAULT failed' }

  Write-Host '== Phase 6A nested helper ABI (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_HELPER_ABI.ps1') -Seconds 14
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_HELPER_ABI failed' }

  Write-Host '== Phase 6B-A EXT bootstrap evidence (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_EXT_BOOTSTRAP.ps1') -Seconds 14
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_EXT_BOOTSTRAP failed' }

  Write-Host '== Phase 6B-A2 entry reconcile (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_ENTRY_RECONCILE.ps1') -Seconds 14
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_ENTRY_RECONCILE failed' }

  Write-Host '== Phase 6B-A3 chunk provenance (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_CHUNK_PROVENANCE.ps1') -Seconds 14
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_CHUNK_PROVENANCE failed' }

  Write-Host '== Phase 6B-A4 object identity (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_OBJECT_IDENTITY.ps1') -Seconds 14
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_OBJECT_IDENTITY failed' }

  Write-Host '== Phase 6B-A5 dispatch hop (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_DISPATCH_HOP.ps1') -Seconds 14 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_DISPATCH_HOP failed' }

  Write-Host '== Phase 6B-A6 helper handoff (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_HELPER_HANDOFF.ps1') -Seconds 14 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_HELPER_HANDOFF failed' }

  Write-Host '== Phase 6B-A7 DSM handoff contract (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_DSM_HANDOFF_CONTRACT.ps1') -Seconds 14 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_DSM_HANDOFF_CONTRACT failed' }

  Write-Host '== Phase 6B-A8 CODE_IMAGE entry ABI (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_MODULE_ENTRY_ABI.ps1') -Seconds 20 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_MODULE_ENTRY_ABI failed' }

  Write-Host '== Phase 6B-A9 NULL contract discrimination (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_MODULE_ENTRY_NULL_CONTRACT.ps1') -Seconds 18 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_MODULE_ENTRY_NULL_CONTRACT failed' }

  Write-Host '== Phase 6B-A10 module data init contract (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_MODULE_DATA_INIT.ps1') -Seconds 18 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_MODULE_DATA_INIT failed' }

  Write-Host '== Phase 6C-A nested EXT R9 switch (short) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_MODULE_R9_SWITCH.ps1') -Seconds 18 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_MODULE_R9_SWITCH failed' }

  Write-Host '== Live: Phase 6C-B ER_RW producer timing =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_ER_RW_PRODUCER.ps1') -Seconds 18 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_ER_RW_PRODUCER failed' }

  Write-Host '== Live: Phase 6C-C bootstrap entry ABI =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_BOOTSTRAP_ENTRY_ABI.ps1') -Seconds 18 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_BOOTSTRAP_ENTRY_ABI failed' }

  Write-Host '== Live: Phase 6C-D1 callback continuation frame =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_CALLBACK_FRAME.ps1') -Seconds 28 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_CALLBACK_FRAME failed' }

  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_NESTED_R9_SCOPE.ps1') -Seconds 24 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_NESTED_R9_SCOPE failed' }

  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_TOKENIZED_R9_SCOPE.ps1') -Seconds 28 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_TOKENIZED_R9_SCOPE failed' }

  Write-Host '== Live: Phase 6D-A post-continuation progress audit =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_POST_CONT_AUDIT.ps1') -Seconds 45 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_POST_CONT_AUDIT failed' }

  Write-Host '== Live: Phase 6D-B post-CFN R9 promotion audit =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_LIVE_POST_CFN_R9_AUDIT.ps1') -Seconds 45 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_LIVE_POST_CFN_R9_AUDIT failed' }

  Write-Host '== Live: Phase 6E P.mrc_extChunk provider audit =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_PHASE6E_P_EXTCHUNK_AUDIT.ps1') -Seconds 45 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_PHASE6E_P_EXTCHUNK_AUDIT failed' }

  Write-Host '== Live: Phase 6F GWY startGame/runapp context audit =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_PHASE6F_GWY_STARTGAME_CONTEXT_AUDIT.ps1') -Seconds 50 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_PHASE6F_GWY_STARTGAME_CONTEXT_AUDIT failed' }

  Write-Host '== Live: Phase 6G restore GWY startGame/runapp context =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_PHASE6G_RESTORE_GWY_CONTEXT.ps1') -Seconds 55 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_PHASE6G_RESTORE_GWY_CONTEXT failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_PHASE6H_GWY_GUEST_RUNAPP.ps1') -Seconds 70 -SkipBuild
  if ($LASTEXITCODE -ne 0) { throw 'RUN_PHASE6H_GWY_GUEST_RUNAPP failed' }
}

Write-Host "[OK] RUN_TESTS complete (GWY_FIXTURE_ROOT=$FixtureRoot)"
