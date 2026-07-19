# JJFB GWY Launcher — primary entrypoint
# Forwards to the current clean baseline runner.
param(
  [switch]$SkipBuild,
  [switch]$SkipResourceCopy,
  [string]$VmVer = "1968",
  [string]$Imei = "864086040622841",
  [string]$HsMan = "vmrp",
  [string]$HsType = "vmrp"
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$impl = Join-Path $here "RUN_V71_PRESENT_COALESCE.ps1"
if (-not (Test-Path $impl)) {
  throw "missing implementation runner: $impl"
}

& $impl -SkipBuild:$SkipBuild -SkipResourceCopy:$SkipResourceCopy `
  -VmVer $VmVer -Imei $Imei -HsMan $HsMan -HsType $HsType
