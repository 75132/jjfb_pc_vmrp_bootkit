# Research-track: E10A / GWY shell live provenance runners.
# Requires explicit opt-in. Builds GwyResearch runtime (real research_gwy_shell).
param(
  [string]$BuildDir = "build-i686",
  [string]$FixtureRoot = "",
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild,
  [int]$ShortSeconds = 14
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

if (-not $FixtureRoot -or -not (Test-Path (Join-Path $FixtureRoot 'gwy\jjfb.mrp'))) {
  throw "RUN_RESEARCH_GWY_SHELL requires fixture root with gwy/jjfb.mrp"
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir $BuildDir
  if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

if (-not $SkipVmrpBuild) {
  Write-Host '== build GwyResearch runtime =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch -LauncherBuildDir $BuildDir
  if ($LASTEXITCODE -ne 0) { throw "GwyResearch vmrp build failed" }
}

Write-Host '== research unit: shell shim =='
$shim = Join-Path $Root "$BuildDir\test_ext_gwy_shell_shim.exe"
if (Test-Path $shim) {
  & $shim
  if ($LASTEXITCODE -ne 0) { throw "test_ext_gwy_shell_shim failed" }
}

$S = $ShortSeconds
$runners = @(
  @{ Name = 'Phase 4C live file trace'; File = 'RUN_LIVE_FILE_TRACE.ps1'; Sec = 6; Skip = $false },
  @{ Name = 'Phase 5A live ext resolve'; File = 'RUN_LIVE_EXT_RESOLVE.ps1'; Sec = 6; Skip = $false },
  @{ Name = 'Phase 5D live entry fault'; File = 'RUN_LIVE_EXT_ENTRY_FAULT.ps1'; Sec = 14; Skip = $false },
  @{ Name = 'Phase 6A nested helper ABI'; File = 'RUN_LIVE_HELPER_ABI.ps1'; Sec = 14; Skip = $false },
  @{ Name = 'Phase 6B-A EXT bootstrap'; File = 'RUN_LIVE_EXT_BOOTSTRAP.ps1'; Sec = 14; Skip = $false },
  @{ Name = 'Phase 6B-A2 entry reconcile'; File = 'RUN_LIVE_ENTRY_RECONCILE.ps1'; Sec = 14; Skip = $false },
  @{ Name = 'Phase 6B-A3 chunk provenance'; File = 'RUN_LIVE_CHUNK_PROVENANCE.ps1'; Sec = 14; Skip = $false },
  @{ Name = 'Phase 6B-A4 object identity'; File = 'RUN_LIVE_OBJECT_IDENTITY.ps1'; Sec = 14; Skip = $false },
  @{ Name = 'Phase 6B-A5 dispatch hop'; File = 'RUN_LIVE_DISPATCH_HOP.ps1'; Sec = 14; Skip = $true },
  @{ Name = 'Phase 6B-A6 helper handoff'; File = 'RUN_LIVE_HELPER_HANDOFF.ps1'; Sec = 14; Skip = $true },
  @{ Name = 'Phase 6B-A7 DSM handoff'; File = 'RUN_LIVE_DSM_HANDOFF_CONTRACT.ps1'; Sec = 14; Skip = $true },
  @{ Name = 'Phase 6B-A8 CODE_IMAGE entry ABI'; File = 'RUN_LIVE_MODULE_ENTRY_ABI.ps1'; Sec = 20; Skip = $true },
  @{ Name = 'Phase 6B-A9 NULL contract'; File = 'RUN_LIVE_MODULE_ENTRY_NULL_CONTRACT.ps1'; Sec = 18; Skip = $true },
  @{ Name = 'Phase 6B-A10 module data init'; File = 'RUN_LIVE_MODULE_DATA_INIT.ps1'; Sec = 18; Skip = $true },
  @{ Name = 'Phase 6C-A nested EXT R9 switch'; File = 'RUN_LIVE_MODULE_R9_SWITCH.ps1'; Sec = 18; Skip = $true },
  @{ Name = 'Phase 6C-B ER_RW producer'; File = 'RUN_LIVE_ER_RW_PRODUCER.ps1'; Sec = 18; Skip = $true },
  @{ Name = 'Phase 6C-C bootstrap entry ABI'; File = 'RUN_LIVE_BOOTSTRAP_ENTRY_ABI.ps1'; Sec = 18; Skip = $true },
  @{ Name = 'Phase 6C-D1 callback frame'; File = 'RUN_LIVE_CALLBACK_FRAME.ps1'; Sec = 28; Skip = $true },
  @{ Name = 'nested R9 scope'; File = 'RUN_LIVE_NESTED_R9_SCOPE.ps1'; Sec = 24; Skip = $true },
  @{ Name = 'tokenized R9 scope'; File = 'RUN_LIVE_TOKENIZED_R9_SCOPE.ps1'; Sec = 28; Skip = $true },
  @{ Name = 'Phase 6D-A post-cont audit'; File = 'RUN_LIVE_POST_CONT_AUDIT.ps1'; Sec = 45; Skip = $true },
  @{ Name = 'Phase 6D-B post-CFN R9 audit'; File = 'RUN_LIVE_POST_CFN_R9_AUDIT.ps1'; Sec = 45; Skip = $true },
  @{ Name = 'Phase 6E P.extChunk audit'; File = 'RUN_PHASE6E_P_EXTCHUNK_AUDIT.ps1'; Sec = 45; Skip = $true },
  @{ Name = 'Phase 6F GWY startGame audit'; File = 'RUN_PHASE6F_GWY_STARTGAME_CONTEXT_AUDIT.ps1'; Sec = 50; Skip = $true },
  @{ Name = 'Phase 6G restore GWY context'; File = 'RUN_PHASE6G_RESTORE_GWY_CONTEXT.ps1'; Sec = 55; Skip = $true },
  @{ Name = 'Phase 6H GWY guest runapp'; File = 'RUN_PHASE6H_GWY_GUEST_RUNAPP.ps1'; Sec = 70; Skip = $true }
)

foreach ($r in $runners) {
  $script = Join-Path $Root $r.File
  if (-not (Test-Path $script)) {
    Write-Host "[SKIP] missing $($r.File)"
    continue
  }
  Write-Host "== $($r.Name) =="
  $args = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $script, '-Seconds', $r.Sec)
  if ($r.Skip) { $args += '-SkipBuild' }
  & powershell @args
  if ($LASTEXITCODE -ne 0) { throw "$($r.File) failed" }
}

Write-Host '[OK] RUN_RESEARCH_GWY_SHELL complete'
