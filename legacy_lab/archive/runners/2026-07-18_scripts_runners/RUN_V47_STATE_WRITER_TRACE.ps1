# JJFB v47 — writer trace (progress/AC8/ui_mode); no progress driver
param(
  [string]$Ac8Mode = "natural",
  [string]$Mode = "45",
  [string]$Tag = "A_writer",
  [int]$Seconds = 16
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
# scripts/runners -> project root
if ((Split-Path -Leaf $root) -eq 'runners') {
  $root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}
& (Join-Path $root "RUN_V47_STARTUP_PROGRESS_DRIVER.ps1") `
  -Ac8Mode $Ac8Mode -Mode $Mode -Driver off -Tag $Tag -Seconds $Seconds
