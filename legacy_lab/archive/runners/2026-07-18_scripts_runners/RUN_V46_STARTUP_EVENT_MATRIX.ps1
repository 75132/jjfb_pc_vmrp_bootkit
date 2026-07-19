# JJFB v46 — event matrix focused on startup UI animation (not login skip)
param(
  [int]$Seconds = 14,
  [string]$Tag = "event_matrix"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}

# Baseline: natural AC8 + progress scan to see if bar can draw when count moves.
& (Join-Path $root "RUN_V46_STARTUP_CHECK_UI.ps1") `
  -Ac8Mode natural -Mode 45 -ProgressScan 1 -Ret 1 -Tag "${Tag}_scan" -Seconds $Seconds

# AC8 pulse then loading: slogo hold then release
& (Join-Path $root "RUN_V46_STARTUP_CHECK_UI.ps1") `
  -Ac8Mode pulse -Mode 45 -Ac8PulseTicks 5 -Ret 1 -Tag "${Tag}_pulse5" -Seconds $Seconds

# slogo once then release
& (Join-Path $root "RUN_V46_STARTUP_CHECK_UI.ps1") `
  -Ac8Mode force_slogo_once_then_release -Mode 45 -Ret 1 -Tag "${Tag}_slogo_once" -Seconds $Seconds

Write-Host "=== event matrix done; see logs/v46_${Tag}_* ==="
