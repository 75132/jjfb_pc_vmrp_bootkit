# E10A-3.1r research: live provenance for cfunction PC 0xA1B8C.
# Research-track ONLY. Builds GwyResearch. Does not patch or write SMSCFG.
param(
  [int]$Seconds = 90,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

Write-Host '== E10A-3.1r: 0xA1B8C DSM/cfunction provenance (research) =='
Write-Host 'Static artifacts already under research/e10a31r/'
Write-Host 'This runner stages GwyResearch; extend with insn trace when ready.'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'GwyResearch build failed' }
}

$outDir = Join-Path $Root 'research\e10a31r'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

# Record run metadata; live BL/BLX capture hooks belong in research_gwy_shell
# (future e10a31r probe) — not launcher_core.
$meta = Join-Path $outDir 'a1b8c_run_meta.txt'
@"
stage=e10a31r
failure_pc=0xA1B8C
file_off=0x21B8C
cfunction_sha256=8f85e3cf8f0ed4a8e09eb658f9daba566989fbc06c510e4e76cd474dd275cad5
static_verdict=BLX_r1_indirect_not_imm_neg1
source_class_candidate=PLATFORM_TABLE
promotion=blocked_pending_live_callee_and_cross_target
seconds=$Seconds
"@ | Set-Content -Path $meta -Encoding utf8

Write-Host "meta=$meta"
Write-Host '[OK] E10A-3.1r static package ready; live CSV row remains STATIC_ONLY until probe lands in research lib'
