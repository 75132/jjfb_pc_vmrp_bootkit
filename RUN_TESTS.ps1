# Product-track preflight + unit tests. No GUI / no E10A live provenance.
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

Write-Host '== unit tests (product) =='
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
  'test_platform_userinfo.exe',
  'test_platform_timer.exe',
  'test_platform_send_app_event.exe',
  'test_platform_handler_registry.exe',
  'test_vm_runtime.exe',
  'test_guest_memory.exe',
  'test_vm_file_service.exe',
  'test_module_registry.exe',
  'test_ext_loader.exe',
  'test_mrp_member_view.exe',
  'test_mrp_reg_primary.exe',
  'test_package_scope.exe',
  'test_package_metadata.exe'
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
}

Write-Host "[OK] RUN_TESTS complete (product track; GWY_FIXTURE_ROOT=$FixtureRoot)"
Write-Host "     Research live/E10A runners: .\RUN_RESEARCH_GWY_SHELL.ps1"
