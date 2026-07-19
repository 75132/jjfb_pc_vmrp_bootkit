# Phase 6J: Shell Publication Routine Audit (observe-only; does not invent extChunk).
param(
  [int]$Seconds = 55,
  [switch]$SkipBuild
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
if (-not (Test-Path $wxjwq)) { throw "missing $wxjwq" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path @(
  (Join-Path $Root 'src\runtime\ext_shell_publication_audit.c')
) -Pattern 'uc_mem_write\s*\(|uc_reg_write\s*\(|force_entry|ui_mode\s*=' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden mutation APIs in publication audit: $($banned.Line)" }

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

function Invoke-PubLive([string]$outLog, [string]$errLog, [string]$nmrpname, [int]$secs) {
  $legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
  if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  $env:GWY_LAUNCH_PARAM = "napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=${nmrpname}_gwyblink"
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
  $env:GWY_MODULE_SNAPSHOT = (Join-Path $logDir 'module_registry_phase6j.json')
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_LAUNCH_PATH = 'gwy_guest_native_runapp'
  $env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  $env:JJFB_PUBLICATION_AUDIT = '1'
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
    if (Select-String -Path $outLog -Pattern '\[JJFB_PUBLICATION_SUMMARY\]|\[JJFB_EXTCHUNK_FAULT\]|UC_MEM_READ_UNMAPPED|mythroad exit' -Quiet) {
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

Write-Host "== Phase 6J Shell Publication Audit Seconds=$Seconds =="

$stdoutJj = Join-Path $logDir 'phase6j_shell_publication_stdout.txt'
$stderrJj = Join-Path $logDir 'phase6j_shell_publication_stderr.txt'
$stdoutWx = Join-Path $logDir 'phase6j_shell_publication_wxjwq_stdout.txt'
$stderrWx = Join-Path $logDir 'phase6j_shell_publication_wxjwq_stderr.txt'
$report = Join-Path $logDir 'phase6j_shell_publication_report.txt'

Invoke-PubLive $stdoutJj $stderrJj 'gwy/jjfb.mrp' $Seconds
Invoke-PubLive $stdoutWx $stderrWx 'gwy/wxjwq.mrp' $Seconds

$all = ''
if (Test-Path $stdoutJj) { $all += (Get-Content $stdoutJj -Raw -ErrorAction SilentlyContinue) }

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if ($ok) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
}

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
Assert-Log 'launch_banner' ($all -match '\[JJFB_GWY_LAUNCH\] mode=gwy_guest_native_runapp') 'banner'
Assert-Log 'publication_audit' ($all -match '\[JJFB_PUBLICATION_SUMMARY\]|\[JJFB_P_FIELD_WRITE\]|\[JJFB_P_PROVIDER\]') 'pub tags'
Assert-Log 'no_host_equivalent' (-not ($all -match 'host_runapp_equivalent_after_no_update')) 'host eq'
Assert-Log 'shell_guest_pc' ($all -match '\[JJFB_SHELL_GUEST_PC\]') 'GUEST_PC'
Assert-Log 'extchunk_fault_or_summary' ($all -match '\[JJFB_EXTCHUNK_FAULT\]|\[JJFB_PUBLICATION_SUMMARY\]') 'fault/summary'
Assert-Log 'wrote_048_missing_c' (
  (
    ($all -match 'wrote_0=yes') -or ($all -match 'wrote_4=yes') -or ($all -match 'wrote_8=yes') -or
    ($all -match '\[CONTEXT_FIELD_WRITE\][^\r\n]*base=0x2AC8DC[^\r\n]*offset=0x[048]\b')
  ) -and (
    ($all -match 'wrote_C=no') -or ($all -match 'missing P\+0x0C') -or
    ($all -match 'P\+0xC=0x0')
  )
) '048 without C'

$reportBody = @()
$reportBody += 'Phase 6J Shell Publication Routine Audit'
$reportBody += "jjfb_sha256=$hash"
$reportBody += 'mode=gwy_guest_native_runapp+JJFB_PUBLICATION_AUDIT'
$reportBody += 'targets=gbrwcore+jjfb,gbrwcore+wxjwq'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8

# Static tools
Push-Location (Join-Path $Root 'tools')
try {
  & python phase6j_p_field_writer_xref.py $GwyRoot (Join-Path $reportDir 'phase6j_p_field_writer_xref.md')
  if ($LASTEXITCODE -ne 0) { throw 'xref failed' }
  & python phase6j_extchunk_abi_users.py $GwyRoot (Join-Path $reportDir 'phase6j_extchunk_abi_users.md')
  if ($LASTEXITCODE -ne 0) { throw 'abi users failed' }
  & python phase6j_mrpgcmap_decode.py $GwyRoot (Join-Path $reportDir 'phase6j_mrpgcmap_entry_decode.md') --stdout $stdoutJj
  if ($LASTEXITCODE -ne 0) { throw 'mrpgcmap failed' }
  & python phase6j_reports.py $reportDir $GwyRoot --primary-stdout $stdoutJj --extra-stdout ("gbrwcore_wxjwq=$stdoutWx")
  if ($LASTEXITCODE -ne 0) { throw 'phase6j reports failed' }
} finally {
  Pop-Location
}

$verdict = 'UNKNOWN'
if (Test-Path (Join-Path $reportDir 'phase6j_publication_verdict.md')) {
  $vt = Get-Content (Join-Path $reportDir 'phase6j_publication_verdict.md') -Raw
  if ($vt -match 'Verdict:\s*\*\*([ABCD])\*\*') { $verdict = $Matches[1] }
}
Add-Content -Path $report -Value "verdict=$verdict"
Write-Host "verdict=$verdict"

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$zip = Join-Path $packDir ("JJFB_phase6j_shell_publication_audit_pack_$stamp.zip")
$toZip = @(
  $stdoutJj, $stderrJj, $stdoutWx, $stderrWx, $report,
  (Join-Path $reportDir 'phase6j_p_field_writer_xref.md'),
  (Join-Path $reportDir 'phase6j_extchunk_abi_users.md'),
  (Join-Path $reportDir 'phase6j_mrpgcmap_entry_decode.md'),
  (Join-Path $reportDir 'phase6j_entry_selection_vs_publication.md'),
  (Join-Path $reportDir 'phase6j_minimal_cross_target_publication_compare.md'),
  (Join-Path $reportDir 'phase6j_publication_verdict.md')
) | Where-Object { Test-Path $_ }
Compress-Archive -Path $toZip -DestinationPath $zip -Force
Write-Host "pack=$zip"

$fail = @($checks | Where-Object { -not $_.ok })
if ($fail.Count -gt 0) {
  Write-Host "[WARN] some asserts failed; pack still written for evidence"
}
Write-Host '[OK] RUN_PHASE6J_SHELL_PUBLICATION_AUDIT complete'
