# Phase 6K: Fix MRPGCMAP entry order (observe + gbrwcore_only; no fake P+0xC).
param(
  [int]$Seconds = 55,
  [switch]$SkipBuild,
  [switch]$SkipWxjwq
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
$GwyRoot = Join-Path $ResourceRoot 'gwy'
$jjfb = Join-Path $GwyRoot 'jjfb.mrp'
$wxjwq = Join-Path $GwyRoot 'wxjwq.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path @(
  (Join-Path $Root 'src\runtime\ext_mrpgcmap_entry_order.c')
) -Pattern 'uc_mem_write\s*\(|force_entry|ui_mode\s*=' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden patterns in entry_order: $($banned.Line)" }
$hdrSym = Select-String -Path (Join-Path $Root 'src\runtime\ext_mrpgcmap_entry_order.c') `
  -Pattern 'header_entry_candidate' -ErrorAction SilentlyContinue |
  Where-Object { $_.Line -notmatch '^\s*/\*|^\s*\*|not use|never use' }
if ($hdrSym) { throw "header_entry_candidate used in entry_order code: $($hdrSym.Line)" }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$packDir = Join-Path $Root 'packages'
New-Item -ItemType Directory -Force -Path $logDir,$reportDir,$packDir | Out-Null

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
if (-not (Test-Path $exe)) { throw "missing $exe" }

function Invoke-6KLive([string]$outLog, [string]$errLog, [string]$nmrpname, [string]$entryMode, [int]$secs) {
  $legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
  if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  $env:GWY_LAUNCH_PARAM = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=${nmrpname}_gwyblink"
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
  $env:GWY_MODULE_SNAPSHOT = (Join-Path $logDir "module_registry_phase6k_${entryMode}.json")
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_LAUNCH_PATH = 'gwy_guest_native_runapp'
  $env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  $env:JJFB_PUBLICATION_AUDIT = '1'
  $env:JJFB_FIX_MRPGCMAP_ENTRY_ORDER = $entryMode
  $env:JJFB_MULTI_TARGET_MIN_COMPARE = '1'
  $env:JJFB_SHELL_CHAIN_TARGET = $nmrpname
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
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  if (Test-Path $outLog) { Remove-Item -Force $outLog }
  if (Test-Path $errLog) { Remove-Item -Force $errLog }
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (Select-String -Path $outLog -Pattern '\[JJFB_6K_ENTRY_SUMMARY\]|\[JJFB_PUBLICATION_SUMMARY\]|\[JJFB_EXTCHUNK_FAULT\]|UC_MEM_READ_UNMAPPED|mythroad exit' -Quiet) {
      Start-Sleep -Seconds 2
      break
    }
  }
  if (-not $p.HasExited) {
    Start-Sleep -Seconds 3
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 600
  }
}

Write-Host "== Phase 6K Fix MRPGCMAP Entry Order Seconds=$Seconds =="

# --- 6K-A observe (jjfb) ---
$stdoutObs = Join-Path $logDir 'phase6k_observe_stdout.txt'
$stderrObs = Join-Path $logDir 'phase6k_observe_stderr.txt'
Invoke-6KLive $stdoutObs $stderrObs 'gwy/jjfb.mrp' 'observe' $Seconds

# --- 6K-B gbrwcore_only (jjfb) ---
$stdoutB = Join-Path $logDir 'phase6k_gbrwcore_only_stdout.txt'
$stderrB = Join-Path $logDir 'phase6k_gbrwcore_only_stderr.txt'
Invoke-6KLive $stdoutB $stderrB 'gwy/jjfb.mrp' 'gbrwcore_only' $Seconds

$stdoutWx = Join-Path $logDir 'phase6k_gbrwcore_only_wxjwq_stdout.txt'
$stderrWx = Join-Path $logDir 'phase6k_gbrwcore_only_wxjwq_stderr.txt'

$textB = ''
if (Test-Path $stdoutB) { $textB = Get-Content $stdoutB -Raw -ErrorAction SilentlyContinue }
$entryHit = $textB -match '\[JJFB_MRPGCMAP_ENTRY_HIT\]'
$pxc = $textB -match '\[JJFB_P_FIELD_WRITE\][^\r\n]*off=0x0C[^\r\n]*new=0x[1-9A-Fa-f]' -or
       $textB -match '\[JJFB_P_WRITE\][^\r\n]*off=0x[Cc][^\r\n]*new=0x[1-9A-Fa-f]'
$midOk = $entryHit -and $pxc

# 6K-C/D only after mid success
if ($midOk -and -not $SkipWxjwq) {
  Write-Host '== 6K mid success: running wxjwq cross-target (6K-D) =='
  Invoke-6KLive $stdoutWx $stderrWx 'gwy/wxjwq.mrp' 'gbrwcore_only' $Seconds
} else {
  Write-Host '== 6K-C/D skipped (need ENTRY_HIT + natural P+0xC) =='
}

python (Join-Path $Root 'tools\phase6k_reports.py') $stdoutB $reportDir --wx-stdout $stdoutWx
if ($LASTEXITCODE -ne 0) { throw 'phase6k_reports failed' }
python (Join-Path $Root 'tools\phase6k_init_writer_reachability.py') $stdoutB (Join-Path $reportDir 'phase6k_gbrwcore_entry_result.md')
if ($LASTEXITCODE -ne 0) { throw 'phase6k_init_writer_reachability failed' }

# Observe-side note
$obsNote = Join-Path $reportDir 'phase6k_observe_note.md'
$obsText = if (Test-Path $stdoutObs) { Get-Content $stdoutObs -Raw } else { '' }
$obsAudit = if ($obsText -match '\[JJFB_6K_ENTRY_AUDIT\]') { 'yes' } else { 'no' }
$obsRan = if ($obsText -match 'result=EMU_OK|result=EMU_ERR') { 'yes' } else { 'no' }
@"
# Phase 6K — observe pass

- 6K_ENTRY_AUDIT present: ``$obsAudit``
- entry emu attempted in observe: ``$obsRan`` (must be no)
- log: ``logs/phase6k_observe_stdout.txt``
"@ | Set-Content -Encoding utf8 $obsNote

python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
if ($LASTEXITCODE -ne 0) { throw 'audit_launcher_core failed' }

$hashAfter = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hashAfter -ne $ExpectedHash) { throw "jjfb.mrp hash changed after run: $hashAfter" }

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$pack = Join-Path $packDir "JJFB_phase6k_entry_order_pack_$stamp.zip"
$packItems = @(
  $stdoutObs, $stderrObs, $stdoutB, $stderrB,
  (Join-Path $reportDir 'phase6k_entry_order_change.md'),
  (Join-Path $reportDir 'phase6k_p_extchunk_publication.md'),
  (Join-Path $reportDir 'phase6k_next_fault_classification.md'),
  (Join-Path $reportDir 'phase6k_cross_target_after_entry_fix.md'),
  (Join-Path $reportDir 'phase6k_gbrwcore_entry_result.md'),
  (Join-Path $reportDir 'phase6k_summary.md'),
  $obsNote
)
if (Test-Path $stdoutWx) { $packItems += @($stdoutWx, $stderrWx) }
Compress-Archive -Path ($packItems | Where-Object { Test-Path $_ }) -DestinationPath $pack -Force

$report = Join-Path $logDir 'phase6k_entry_order_report.txt'
@(
  "phase=6K",
  "jjfb_hash=$hashAfter",
  "observe_audit=$obsAudit",
  "observe_emu_attempted=$obsRan",
  "entry_hit=$entryHit",
  "natural_pxc=$pxc",
  "mid_success=$midOk",
  "pack=$pack"
) | Set-Content -Encoding utf8 $report

Write-Host "Phase 6K done. entry_hit=$entryHit pxc=$pxc mid=$midOk pack=$pack"
if (-not $entryHit) { Write-Host 'NOTE: no ENTRY_HIT — check inject path / code base' }
if ($textB -match 'fault_during_entry|NEW_ENTRY_FAULT') {
  Write-Host 'NOTE: NEW_ENTRY_FAULT during entry — see phase6k_next_fault_classification.md'
}
exit 0
