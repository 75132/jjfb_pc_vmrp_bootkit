# 120s observe-only JJFB visual-stable diagnostic (research runner).
# Does NOT inject Event15/E6C/B70. Layer-2 dual-screenshot compare.
param(
  [int]$HoldSeconds = 120,
  [string]$ResourceRoot = "",
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root 'profiles\jjfb.json'))) {
  $Root = Split-Path -Parent $Root
}
Set-Location $Root
. (Join-Path $Root 'tools\JjfbLayer1Gate.ps1')

if (-not $ResourceRoot) {
  $ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
}
$RunDir = Join-Path $Root 'out\vmrp_run'
$shotDir = Join-Path $RunDir 'screenshots'
$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$archive = Join-Path $Root "out\visual_baseline\diag_$stamp"
New-Item -ItemType Directory -Force -Path $archive, $shotDir | Out-Null

Write-Host "== JJFB visual-stable diag HoldSeconds=$HoldSeconds =="
$args = @('-HoldSeconds', "$HoldSeconds", '-ResourceRoot', $ResourceRoot, '-RequireCatalog', '-Diagnostic')
if ($SkipBuild) { $args += '-SkipBuild' }
if ($SkipVmrpBuild) { $args += '-SkipVmrpBuild' }

# Mid-hold second screenshot: start launcher via helper then poll.
# RUN_JJFB_LAUNCHER already holds; we post-process archives after.

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_JJFB_LAUNCHER.ps1') @args
$gateExit = $LASTEXITCODE

# Collect observe signals from last progress (launcher archived under out/visual_baseline/gate_*)
$latestGate = Get-ChildItem (Join-Path $Root 'out\visual_baseline') -Directory -Filter 'gate_*' |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1
$prog = if ($latestGate) { Join-Path $latestGate.FullName 'runtime_progress.jsonl' } else { Join-Path $RunDir 'runtime_progress.jsonl' }
$txt = if (Test-Path $prog) { Get-Content $prog -Raw } else { '' }

$members = @()
[regex]::Matches($txt, '"details":"([^"]+\.bmp)"') | ForEach-Object { $members += $_.Groups[1].Value }
$members = $members | Select-Object -Unique

$obs = [ordered]@{
  gate_exit          = $gateExit
  distinct_members   = @($members)
  member_count       = $members.Count
  jjfbol_scope       = [bool]($txt -match 'JJFBOL_SCOPE|jjfbol_scope')
  pending_fifo       = [bool]($txt -match 'pending_bitmap_')
  plat_11f00         = [bool]($txt -match '0x11[Ff]00|plat_11f00|11F00')
  b54_code15         = [bool]($txt -match 'B54.*15|event_code.?=.?15|code=15')
  e6c_natural        = [bool]($txt -match 'E6C_NATURAL|e6c_store|2E5E60')
  path_2e2520        = [bool]($txt -match '2E2520|2e2520')
  inject_forbidden   = [bool]($txt -match 'host_enqueue_15|hardwrite_E6C|FORCE_UI_MODE')
}
($obs | ConvertTo-Json -Depth 5) | Set-Content (Join-Path $archive 'diag_observe.json') -Encoding utf8
Write-Host ($obs | ConvertTo-Json -Compress)

# Layer-2: compare two screenshots if present
$shots = @(Get-ChildItem $shotDir -Filter '*.bmp' -ErrorAction SilentlyContinue)
$hashes = @()
foreach ($s in $shots) {
  $hashes += (Get-FileHash $s.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
  Copy-Item $s.FullName $archive -Force
}
$layer2 = @{
  shot_count = $shots.Count
  distinct_sha = @($hashes | Select-Object -Unique).Count
  layer2_changed = ((@($hashes | Select-Object -Unique).Count) -gt 1)
}
($layer2 | ConvertTo-Json) | Set-Content (Join-Path $archive 'layer2_shots.json') -Encoding utf8
Write-Host "LAYER2 shots=$($layer2.shot_count) distinct_sha=$($layer2.distinct_sha) changed=$($layer2.layer2_changed)"

if ($obs.inject_forbidden) {
  throw 'DIAG_FAIL: injection markers observed (forbidden)'
}
if ($gateExit -ne 0) {
  throw "DIAG_FAIL: Layer-1 gate exit=$gateExit"
}
Write-Host '[OK] JJFB visual-stable diag complete (observe-only)'
exit 0
