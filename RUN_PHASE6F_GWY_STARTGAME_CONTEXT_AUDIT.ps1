# Phase 6F: GWY startGame/runapp Context Audit (observe-only).
param(
  [int]$Seconds = 50,
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_gwy_startgame_audit.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x304558|0x2AC8DC|ui_mode|force_entry|uc_reg_write|fake.?chunk|P\+0xC\s*=' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden literals in startgame audit core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$packDir = Join-Path $Root 'packages'
New-Item -ItemType Directory -Force -Path $logDir,$reportDir,$packDir | Out-Null
$stdout = Join-Path $logDir 'phase6f_gwy_startgame_context_stdout.txt'
$report = Join-Path $logDir 'phase6f_gwy_startgame_context_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6f.json'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
if (-not (Test-Path $exe)) { throw "missing $exe" }

function Invoke-Live([string]$outLog, [string]$errLog, [string]$resourceRoot, [string]$target, [string]$param, [string]$profile, [int]$secs) {
  $legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
  if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = $target
  $env:GWY_LAUNCH_PARAM = $param
  $env:GWY_RESOURCE_ROOT = $resourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = $profile
  $env:GWY_MODULE_SNAPSHOT = $snap
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
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  $ready = $false
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (Select-String -Path $outLog -Pattern '\[JJFB_GWY_CONTEXT_SUMMARY\]' -Quiet) {
      Start-Sleep -Seconds 1
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

Write-Host "== Phase 6F JJFB GWY startGame context audit root=240x320 Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6f_gwy_startgame_context_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
  'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink' `
  (Join-Path $Root 'profiles\jjfb.json') $Seconds

$all = ''
if (Test-Path $stdout) { $all += (Get-Content $stdout -Raw -ErrorAction SilentlyContinue) }

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if ($ok) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
}

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
Assert-Log 'resource_240x320' ($ResourceRoot -match '240x320') $ResourceRoot
Assert-Log 'launch_context' ($all -match '\[JJFB_LAUNCH_CONTEXT\]') 'launch context missing'
Assert-Log 'cfg36' ($all -match '\[JJFB_CFG36_CONTRACT\]') 'cfg36 missing'
Assert-Log 'startgame_equiv' ($all -match '\[JJFB_STARTGAME_EQUIV\]') 'start_dsm missing'
Assert-Log 'shell_bypass' ($all -match '\[JJFB_SHELL_BYPASS\]') 'shell bypass missing'
Assert-Log 'summary' ($all -match '\[JJFB_GWY_CONTEXT_SUMMARY\]') 'summary missing'
Assert-Log 'gates_blocked' (
  ($all -match 'gwy_startgame_context_gate=blocked') -and ($all -match 'p_extchunk_gate=blocked')
) 'gates'
Assert-Log 'no_forbidden_fix' (
  -not ($all -match 'force_entry_r0|force_ui_mode|fake_extchunk|promotion_performed=yes')
) 'forbidden fix'
Assert-Log 'observe_only' ($all -match 'note=observe_only') 'observe tag'

$class = 'UNKNOWN'
$nextFix = 'NONE'
$bypass = '?'
$writes = '0'
if ($all -match '\[JJFB_GWY_CONTEXT_SUMMARY\][^\r\n]*gwy_context_class=(\S+)') { $class = $Matches[1] }
if ($all -match '\[JJFB_GWY_CONTEXT_SUMMARY\][^\r\n]*next_allowed_fix=(\S+)') { $nextFix = $Matches[1] }
if ($all -match '\[JJFB_GWY_CONTEXT_SUMMARY\][^\r\n]*shell_bypassed=(\S+)') { $bypass = $Matches[1] }
if ($all -match '\[JJFB_GWY_CONTEXT_SUMMARY\][^\r\n]*pxc_writes_seen=(\d+)') { $writes = $Matches[1] }

$allowedClass = @(
  'SHELL_BYPASSED_DIRECT_JJFB','SHELL_LOADED_BUT_NO_EXTCHUNK','PARAM_MISMATCH',
  'RESOURCE_MISS_BLOCKS_CONTEXT','UNKNOWN'
)
Assert-Log 'class_allowed' ($allowedClass -contains $class) "class=$class"
$allowedFix = @(
  'RESTORE_GWY_STARTGAME_RUNAPP_CONTEXT','SHELL_PUBLICATION_ROUTINE_AUDIT',
  'FIX_CFG36_PARAM_PIPELINE','FIX_RESOURCE_ROOT_OR_PATH','NONE'
)
Assert-Log 'single_next_fix' ($allowedFix -contains $nextFix) "next=$nextFix"

$reportBody = @()
$reportBody += 'Phase 6F GWY startGame/runapp Context Audit'
$reportBody += "jjfb_sha256=$hash"
$reportBody += 'resource_root=game_files/mythroad/240x320'
$reportBody += 'gwy_startgame_context_gate=blocked'
$reportBody += 'p_extchunk_gate=blocked'
$reportBody += 'post_cfn_r9_gate=blocked'
$reportBody += "gwy_context_class=$class"
$reportBody += "shell_bypassed=$bypass"
$reportBody += "pxc_writes_seen=$writes"
$reportBody += "next_allowed_fix=$nextFix"
$reportBody += 'mode=observe_only'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class bypass=$bypass writes=$writes next=$nextFix"

& python (Join-Path $Root 'tools\phase6f_gwy_context_reports.py') $stdout $reportDir $GwyRoot $report
if ($LASTEXITCODE -ne 0) { throw 'phase6f report tool failed' }

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$zip = Join-Path $packDir ("JJFB_phase6f_gwy_startgame_context_audit_pack_$stamp.zip")
$toZip = @(
  $stdout,
  $report,
  (Join-Path $reportDir 'phase6f_cfg36_contract.md'),
  (Join-Path $reportDir 'phase6f_gwy_startgame_runapp_chain.md'),
  (Join-Path $reportDir 'phase6f_p_extchunk_context_map.md'),
  (Join-Path $reportDir 'phase6f_mrc_init_gap.md'),
  (Join-Path $reportDir 'phase6f_fileopen_mapping.md')
) | Where-Object { Test-Path $_ }
Compress-Archive -Path $toZip -DestinationPath $zip -Force
Write-Host "pack=$zip"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6f failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_PHASE6F_GWY_STARTGAME_CONTEXT_AUDIT complete'
