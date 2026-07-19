# Phase 6G: Restore GWY startGame/runapp context (no fake extChunk).
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
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_gwy_shell_shim.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x304558|0x2AC8DC|ui_mode|force_entry|uc_reg_write|fake.?chunk|P\+0xC\s*=' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden literals in shell shim: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$packDir = Join-Path $Root 'packages'
New-Item -ItemType Directory -Force -Path $logDir,$reportDir,$packDir | Out-Null
$stdout = Join-Path $logDir 'phase6g_restore_gwy_context_stdout.txt'
$report = Join-Path $logDir 'phase6g_restore_gwy_context_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6g.json'
$stderr = Join-Path $logDir 'phase6g_restore_gwy_context_stderr.txt'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
if (-not (Test-Path $exe)) { throw "missing $exe" }

function Invoke-Live([string]$outLog, [string]$errLog, [int]$secs) {
  $legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
  if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
  $env:GWY_LAUNCH = '1'
  # DSM prepends mythroad/; do not pass mythroad/gwy/... or opens become mythroad/mythroad/...
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
  $env:GWY_MODULE_SNAPSHOT = $snap
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_LAUNCH_PATH = 'gwy_shell_post_update'
  $env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_SHELL_CHAIN_TARGET = 'gwy/jjfb.mrp'
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
  $ready = $false
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (Select-String -Path $outLog -Pattern '\[JJFB_GWY_SHELL_SUMMARY\]|\[JJFB_GWY_CONTEXT_SUMMARY\]|\[JJFB_RUNAPP\]' -Quiet) {
      Start-Sleep -Seconds 2
      $ready = $true
      break
    }
  }
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 600
  }
  if (-not $ready -and -not (Test-Path $outLog)) {
    throw "live stdout missing: $outLog"
  }
}

Write-Host "== Phase 6G restore GWY context root=240x320 Seconds=$Seconds =="
Invoke-Live $stdout $stderr $Seconds

# Finalize summary if process died without it: append via python? Prefer log presence.
$all = ''
if (Test-Path $stdout) { $all += (Get-Content $stdout -Raw -ErrorAction SilentlyContinue) }
if (Test-Path $stderr) { $all += "`n" + (Get-Content $stderr -Raw -ErrorAction SilentlyContinue) }

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if ($ok) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
}

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
Assert-Log 'resource_240x320' ($ResourceRoot -match '240x320') $ResourceRoot
Assert-Log 'gwy_launch_banner' ($all -match '\[JJFB_GWY_LAUNCH\] mode=gwy_shell_post_update') 'banner'
Assert-Log 'gwy_root_log' ($all -match '\[JJFB_GWY_ROOT\] mythroad_root=') 'root'
Assert-Log 'update_stub' ($all -match '\[JJFB_GWY_UPDATE_STUB\].*no_update') 'stub'
Assert-Log 'cfg36' ($all -match '\[JJFB_CFG36\]') 'cfg36'
Assert-Log 'startgame' ($all -match '\[JJFB_STARTGAME\]') 'startgame'
Assert-Log 'runapp' ($all -match '\[JJFB_RUNAPP\]') 'runapp'
Assert-Log 'shell_package' (
  ($all -match 'gbrwcore') -and (
    ($all -match 'gbrwcore\.mrp') -or ($all -match 'shell_package') -or ($all -match 'gbrwcore_opened=yes')
  )
) 'shell open'
# Alias view file may still be written for ExtLoader seed; forbid guest→alias FILEOPEN host remap.
Assert-Log 'no_alias_direct' (
  -not ($all -match 'host="[^"]*jjfb_alias\.mrp"[^"]*ok=1') -and
  -not ($all -match 'host=.*jjfb_alias\.mrp.*note=ALIAS') -and
  ($all -match 'vfs_remap=disabled')
) 'alias VFS remap still active'
Assert-Log 'not_shell_bypassed' (-not ($all -match 'class=SHELL_BYPASSED_DIRECT_JJFB')) 'still bypassed'
Assert-Log 'no_forbidden_fix' (
  -not ($all -match 'force_entry_r0|force_ui_mode|fake_extchunk|promotion_performed=yes')
) 'forbidden'

$class = 'UNKNOWN'
if ($all -match '\[JJFB_GWY_SHELL_SUMMARY\][^\r\n]*class=(\S+)') { $class = $Matches[1] }
elseif ($all -match 'gwy_context_class=(\S+)') { $class = $Matches[1] }
$writes = '0'
if ($all -match 'pxc_writes_seen=(\d+)') { $writes = $Matches[1] }

$reportBody = @()
$reportBody += 'Phase 6G Restore GWY startGame/runapp Context'
$reportBody += "jjfb_sha256=$hash"
$reportBody += 'resource_root=game_files/mythroad/240x320'
$reportBody += 'mode=gwy_shell_post_update'
$reportBody += "gwy_shell_class=$class"
$reportBody += "pxc_writes_seen=$writes"
$reportBody += 'fake_extchunk=forbidden'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class writes=$writes"

& python (Join-Path $Root 'tools\phase6g_startgame_entry.py') $GwyRoot (Join-Path $reportDir 'phase6g_startgame_runapp_entry.md')
if ($LASTEXITCODE -ne 0) { throw 'phase6g entry tool failed' }
& python (Join-Path $Root 'tools\phase6g_restore_reports.py') $stdout $reportDir $GwyRoot $report
if ($LASTEXITCODE -ne 0) { throw 'phase6g report tool failed' }

# Ensure shell summary present for packaging
if ($all -notmatch '\[JJFB_GWY_SHELL_SUMMARY\]') {
  Add-Content -Path $stdout -Value '[JJFB_GWY_SHELL_SUMMARY] class=UNKNOWN note=process_ended_before_finalize'
}

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$zip = Join-Path $packDir ("JJFB_phase6g_restore_gwy_context_pack_$stamp.zip")
$toZip = @(
  $stdout,
  $report,
  (Join-Path $reportDir 'phase6g_resource_root_mapping.md'),
  (Join-Path $reportDir 'phase6g_startgame_runapp_entry.md'),
  (Join-Path $reportDir 'phase6g_gwy_shell_no_update_stub.md'),
  (Join-Path $reportDir 'phase6g_launch_chain_result.md'),
  (Join-Path $reportDir 'phase6g_p_extchunk_publication_result.md')
) | Where-Object { Test-Path $_ }
Compress-Archive -Path $toZip -DestinationPath $zip -Force
Write-Host "pack=$zip"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6g failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_PHASE6G_RESTORE_GWY_CONTEXT complete'
