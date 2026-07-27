# Local integration: real game_files jjfbol counts.
param(
  [string]$ResourceRoot = ""
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
# research/runners -> repo root
if (-not (Test-Path (Join-Path $Root 'profiles\jjfb.json'))) {
  $Root = Split-Path -Parent $Root
}
Set-Location $Root
if (-not $ResourceRoot) {
  $ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
}
$jjfbol = Join-Path $ResourceRoot 'gwy\jjfbol'
if (-not (Test-Path $jjfbol)) { throw "missing $jjfbol" }

$mrps = @(Get-ChildItem $jjfbol -Filter '*.mrp' -File)
$vs = @(Get-ChildItem $jjfbol -Filter '*.v' -File)
$down = Join-Path $jjfbol 'downVersion'
if (-not (Test-Path $down)) { throw 'missing downVersion' }
$bytes = [System.IO.File]::ReadAllBytes($down)
if ($bytes.Length -lt 4) { throw 'downVersion too short' }
$raw = ([int]$bytes[0] -shl 24) -bor ([int]$bytes[1] -shl 16) -bor ([int]$bytes[2] -shl 8) -bor [int]$bytes[3]
Write-Host "packages=$($mrps.Count) versions=$($vs.Count) downVersion=$raw raw=$('{0:X8}' -f $raw)"
if ($mrps.Count -ne 59) { throw "expected 59 mrp, got $($mrps.Count)" }
if ($vs.Count -ne 60) { throw "expected 60 .v, got $($vs.Count)" }
if ($raw -ne 1006) { throw "expected downVersion 1006, got $raw" }
Write-Host '[OK] test_jjfbol_catalog_real'
