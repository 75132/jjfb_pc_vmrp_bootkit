# Phase 6E: P.mrc_extChunk Provider Audit (observe-only).
param(
  [int]$Seconds = 45,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) '..\..')).Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_p_extchunk_audit.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x304558|0x2AC8DC|0x2AEB34|0x2D8DF4|appid\s*==\s*400101|ui_mode|force_entry|uc_reg_write|fake.?chunk' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden literals or writes in p_extchunk core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$packDir = Join-Path $Root 'packages'
New-Item -ItemType Directory -Force -Path $logDir,$reportDir,$packDir | Out-Null
$stdout = Join-Path $logDir 'phase6e_p_extchunk_audit_stdout.txt'
$report = Join-Path $logDir 'phase6e_p_extchunk_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6e.json'

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
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  $ready = $false
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (Select-String -Path $outLog -Pattern '\[JJFB_EXTCHUNK_SUMMARY\]' -Quiet) {
      Start-Sleep -Seconds 1
      $ready = $true
      break
    }
    if (Select-String -Path $outLog -Pattern '\[JJFB_EXTCHUNK_FAULT\]' -Quiet) {
      Start-Sleep -Seconds 2
      if (Select-String -Path $outLog -Pattern '\[JJFB_EXTCHUNK_SUMMARY\]' -Quiet) {
        $ready = $true
        break
      }
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

Write-Host "== Phase 6E JJFB P.mrc_extChunk audit Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6e_p_extchunk_audit_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'p_struct' ($all -match '\[JJFB_P_STRUCT\]') 'P struct missing'
Assert-Log 'extchunk_phase' ($all -match '\[JJFB_EXTCHUNK_PHASE\]') 'phase missing'
Assert-Log 'fault_or_summary' (
  ($all -match '\[JJFB_EXTCHUNK_FAULT\]') -or ($all -match '\[JJFB_EXTCHUNK_SUMMARY\]')
) 'no fault/summary'
Assert-Log 'summary' ($all -match '\[JJFB_EXTCHUNK_SUMMARY\]') 'summary missing'
Assert-Log 'post_cfn_r9_gate' ($all -match 'post_cfn_r9_gate=blocked') 'r9 gate'
Assert-Log 'p_extchunk_gate' ($all -match 'p_extchunk_gate=blocked') 'extchunk gate'
Assert-Log 'no_forbidden_fix' (
  -not ($all -match 'force_entry_r0|force_ui_mode|fake_extchunk|promotion_performed=yes|uc_reg_write')
) 'forbidden fix suspected'
Assert-Log 'old_fault_274_gone' (
  -not ($all -match 'invalid_address=0x274|mem_invalid[^\r\n]*at 0x274')
) 'old 0x274 returned'
Assert-Log 'observe_only' ($all -match 'note=observe_only') 'observe tag'

$class = 'UNKNOWN'
$writes = '0'
$reads = '0'
$nextFix = 'NONE'
$gate = 'blocked'
$pBase = '0'
$funcStart = '0'
$memPc = '0'

if ($all -match '\[JJFB_EXTCHUNK_SUMMARY\][^\r\n]*extchunk_class=(\S+)') { $class = $Matches[1] }
if ($all -match '\[JJFB_EXTCHUNK_SUMMARY\][^\r\n]*writes_seen=(\d+)') { $writes = $Matches[1] }
if ($all -match '\[JJFB_EXTCHUNK_SUMMARY\][^\r\n]*reads_seen=(\d+)') { $reads = $Matches[1] }
if ($all -match '\[JJFB_EXTCHUNK_SUMMARY\][^\r\n]*next_allowed_fix=(\S+)') { $nextFix = $Matches[1] }
if ($all -match '\[JJFB_EXTCHUNK_SUMMARY\][^\r\n]*P=(0x[0-9A-Fa-f]+)') { $pBase = $Matches[1] }
if ($all -match '\[JJFB_EXTCHUNK_SUMMARY\][^\r\n]*function_start=(0x[0-9A-Fa-f]+)') { $funcStart = $Matches[1] }
if ($all -match '\[JJFB_EXTCHUNK_SUMMARY\][^\r\n]*memory_access_pc=(0x[0-9A-Fa-f]+)') { $memPc = $Matches[1] }
if ($all -match 'p_extchunk_gate=open') { $gate = 'open' }

$allowedClass = @(
  'EXTCHUNK_NEVER_WRITTEN','EXTCHUNK_WRITE_AFTER_FAULT_WINDOW','EXTCHUNK_PROVIDER_PATH_SKIPPED',
  'EXTCHUNK_GWY_CONTEXT_HYPOTHESIS','EXTCHUNK_FILLED_BUT_NULL_AT_USE','UNKNOWN'
)
Assert-Log 'class_allowed' ($allowedClass -contains $class) "class=$class"
$allowedFix = @(
  'MISSING_EXTCHUNK_PROVIDER_PATH','PROVIDER_ORDERING_AUDIT','RESTORE_PLUGIN_OR_GWY_LOAD_PATH',
  'GWY_STARTGAME_RUNAPP_CONTEXT_AUDIT','EXTCHUNK_LIFETIME_AUDIT','NONE'
)
Assert-Log 'single_next_fix' ($allowedFix -contains $nextFix) "next=$nextFix"

$phases = @()
foreach ($m in [regex]::Matches($all, '\[JJFB_EXTCHUNK_PHASE\] phase=(\S+) P=(0x[0-9A-Fa-f]+) P\+0xC=(0x[0-9A-Fa-f]+)')) {
  $phases += ("{0}:{1}->{2}" -f $m.Groups[1].Value, $m.Groups[2].Value, $m.Groups[3].Value)
}
$phaseLine = if ($phases.Count -gt 0) { ($phases -join ';') } else { 'none' }

$reportBody = @()
$reportBody += 'Phase 6E P.mrc_extChunk Provider Audit'
$reportBody += "jjfb_sha256=$hash"
$reportBody += "p_extchunk_gate=$gate"
$reportBody += 'post_cfn_r9_gate=blocked'
$reportBody += 'post_continuation_gate=open'
$reportBody += 'graphics_gate=blocked'
$reportBody += 'event_scheduler_gate=blocked'
$reportBody += "P=$pBase"
$reportBody += "writes_seen=$writes"
$reportBody += "reads_seen=$reads"
$reportBody += "function_start=$funcStart"
$reportBody += "memory_access_pc=$memPc"
$reportBody += "phases=$phaseLine"
$reportBody += "extchunk_class=$class"
$reportBody += "next_allowed_fix=$nextFix"
$reportBody += 'mode=observe_only'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class writes=$writes reads=$reads next=$nextFix gate=$gate"
Write-Host "report=$report"

& python (Join-Path $Root 'tools\phase6e_p_extchunk_reports.py') $stdout $reportDir $report
if ($LASTEXITCODE -ne 0) { throw 'phase6e report tool failed' }

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$zip = Join-Path $packDir ("JJFB_phase6e_p_extchunk_audit_pack_$stamp.zip")
$toZip = @(
  $stdout,
  $report,
  (Join-Path $reportDir 'phase6e_p_extchunk_audit.md'),
  (Join-Path $reportDir 'phase6e_304558_disasm.md'),
  (Join-Path $reportDir 'phase6e_p_field_xref.md')
) | Where-Object { Test-Path $_ }
Compress-Archive -Path $toZip -DestinationPath $zip -Force
Write-Host "pack=$zip"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6e failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_PHASE6E_P_EXTCHUNK_AUDIT complete'
