# Phase 6H: Guest-native GWY runapp/startGame (no invented extChunk / host equivalent).
param(
  [int]$Seconds = 70,
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

$banned = Select-String -Path @(
  (Join-Path $Root 'src\runtime\ext_gwy_shell_shim.c'),
  (Join-Path $Root 'src\runtime\ext_gwy_shell_native_exec.c')
) -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x304558|0x2AC8DC|ui_mode|force_entry|uc_reg_write|fake.?chunk|P\+0xC\s*=' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden literals in shell native modules: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$packDir = Join-Path $Root 'packages'
New-Item -ItemType Directory -Force -Path $logDir,$reportDir,$packDir | Out-Null
$stdout = Join-Path $logDir 'phase6h_gwy_guest_runapp_stdout.txt'
$report = Join-Path $logDir 'phase6h_gwy_guest_runapp_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6h.json'
$stderr = Join-Path $logDir 'phase6h_gwy_guest_runapp_stderr.txt'

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
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = (Join-Path $Root 'profiles\jjfb.json')
  $env:GWY_MODULE_SNAPSHOT = $snap
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_LAUNCH_PATH = 'gwy_guest_native_runapp'
  $env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
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
  $sawGuestPc = $false
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (-not $sawGuestPc -and (Select-String -Path $outLog -Pattern '\[JJFB_SHELL_GUEST_PC\]' -Quiet)) {
      $sawGuestPc = $true
    }
    # Terminal only: export call, real SUMMARY, process exit markers, or mem fault.
    # Do NOT stop on GATE-open progress alone (that cut off the P+0xC fault path).
    if (Select-String -Path $outLog -Pattern '\[JJFB_SHELL_EXPORT_CALL\]|mythroad exit|EXTCHUNK_FAULT|UC_MEM_FAULT|UC_ERR|fault_addr=' -Quiet) {
      Start-Sleep -Seconds 2
      $ready = $true
      break
    }
    if (Select-String -Path $outLog -Pattern '\[JJFB_SHELL_NATIVE_SUMMARY\].*stop=(shell_ext_fault|mem_fault|export|exit)' -Quiet) {
      Start-Sleep -Seconds 1
      $ready = $true
      break
    }
  }
  if (-not $p.HasExited) {
    # Grace for late SUMMARY/fault flush, then stop for packaging.
    if ($sawGuestPc) { Start-Sleep -Seconds 8 }
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 800
    $ready = $true
  }
  if (-not $ready -and -not (Test-Path $outLog)) {
    throw "live stdout missing: $outLog"
  }
}

Write-Host "== Phase 6H guest-native GWY runapp root=240x320 Seconds=$Seconds =="
Invoke-Live $stdout $stderr $Seconds

$all = ''
if (Test-Path $stdout) { $all += (Get-Content $stdout -Raw -ErrorAction SilentlyContinue) }
if (Test-Path $stderr) { $all += "`n" + (Get-Content $stderr -Raw -ErrorAction SilentlyContinue) }

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if ($ok) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
}

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
Assert-Log 'launch_banner' ($all -match '\[JJFB_GWY_LAUNCH\] mode=gwy_guest_native_runapp') 'banner'
Assert-Log 'gwy_root_log' ($all -match '\[JJFB_GWY_ROOT\] mythroad_root=') 'root'
Assert-Log 'shell_exec_tag' ($all -match '\[JJFB_SHELL_EXEC\]') 'SHELL_EXEC'
Assert-Log 'no_host_equivalent' (-not ($all -match 'host_runapp_equivalent_after_no_update')) 'still host equivalent'
Assert-Log 'no_alias_direct' (
  -not ($all -match 'host="[^"]*jjfb_alias\.mrp"[^"]*ok=1') -and
  ($all -match 'vfs_remap=disabled' -or $all -match 'alias_direct=disabled')
) 'alias'
Assert-Log 'not_shell_bypassed' (-not ($all -match 'class=SHELL_BYPASSED_DIRECT_JJFB')) 'bypassed'
Assert-Log 'reg_primary_view' ($all -match 'reg_primary_installed.*gbrwcore') 'shell cfunction view'
Assert-Log 'no_cfunction_3006' (-not ($all -match 'read file\s+\"cfunction\.ext\" err, code=3006')) 'cfunction.ext still 3006'

Assert-Log 'shell_ext_loaded' ($all -match '\[JJFB_SHELL_EXT\].*loaded=yes base=0x') 'SHELL_EXT'
Assert-Log 'shell_export_reg' ($all -match '\[JJFB_SHELL_EXPORT\].*lib\.runapp') 'EXPORT'
Assert-Log 'shell_guest_pc' ($all -match '\[JJFB_SHELL_GUEST_PC\]') 'GUEST_PC'
Assert-Log 'shell_gate_open_tag' (
  ($all -match '\[JJFB_SHELL_NATIVE_GATE\].*shell_native_exec_gate=open') -or
  ($all -match '\[JJFB_SHELL_NATIVE_SUMMARY\].*shell_native_exec_gate=open')
) 'NATIVE_GATE'

$class = 'UNKNOWN'
if ($all -match '\[JJFB_SHELL_NATIVE_SUMMARY\][^\r\n]*class=(\S+)') { $class = $Matches[1] }
elseif ($all -match '\[JJFB_SHELL_NATIVE_GATE\][^\r\n]*class=(\S+)') { $class = $Matches[1] }
elseif ($all -match '\[JJFB_GWY_SHELL_SUMMARY\][^\r\n]*class=(\S+)') { $class = $Matches[1] }
$gateOpen = ($all -match 'shell_native_exec_gate=open')
$guestPc = $all -match '\[JJFB_SHELL_GUEST_PC\]'
$exportReg = $all -match '\[JJFB_SHELL_EXPORT\]'

$reportBody = @()
$reportBody += 'Phase 6H Guest-Native GWY runapp/startGame'
$reportBody += "jjfb_sha256=$hash"
$reportBody += 'resource_root=game_files/mythroad/240x320'
$reportBody += 'mode=gwy_guest_native_runapp'
$reportBody += "native_class=$class"
$reportBody += ("shell_native_exec_gate={0}" -f $(if ($gateOpen) { 'open' } else { 'blocked_or_partial' }))
$reportBody += ("guest_pc_hit={0}" -f $(if ($guestPc) { 'yes' } else { 'no' }))
$reportBody += ("export_registered={0}" -f $(if ($exportReg) { 'yes' } else { 'no' }))
$reportBody += ("host_equivalent={0}" -f $(if ($all -match 'host_runapp_equivalent') { 'yes' } else { 'no' }))
$reportBody += 'fake_extchunk=forbidden'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class gateOpen=$gateOpen"

& python (Join-Path $Root 'tools\phase6h_export_resolution.py') $GwyRoot (Join-Path $reportDir 'phase6h_gbrwcore_export_resolution.md')
if ($LASTEXITCODE -ne 0) { throw 'phase6h export tool failed' }
& python (Join-Path $Root 'tools\phase6h_gamelist_branch.py') $GwyRoot $stdout (Join-Path $reportDir 'phase6h_gamelist_post_update_branch.md')
if ($LASTEXITCODE -ne 0) { throw 'phase6h gamelist tool failed' }
& python (Join-Path $Root 'tools\phase6h_reports.py') $stdout $reportDir $GwyRoot $report
if ($LASTEXITCODE -ne 0) { throw 'phase6h report tool failed' }

if ($all -notmatch '\[JJFB_SHELL_NATIVE_SUMMARY\].*class=(EXEC_GATE_OPEN|GUEST_RUNAPP|EXEC_PARTIAL)' -and
    $all -notmatch '\[JJFB_SHELL_NATIVE_GATE\].*class=EXEC_GATE_OPEN' -and
    $all -notmatch '\[JJFB_GWY_SHELL_SUMMARY\]') {
  # Do not clobber a real mid-line; append on its own line only if truly missing.
  if ($all -notmatch '\[JJFB_SHELL_NATIVE_SUMMARY\]' -and $all -notmatch '\[JJFB_SHELL_NATIVE_GATE\]') {
    Add-Content -Path $stdout -Value "`n[JJFB_SHELL_NATIVE_SUMMARY] class=NONE note=process_ended_before_finalize shell_native_exec_gate=blocked"
  }
}

$stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
$zip = Join-Path $packDir ("JJFB_phase6h_gwy_guest_runapp_pack_$stamp.zip")
$toZip = @(
  $stdout,
  $report,
  (Join-Path $reportDir 'phase6h_shell_native_exec.md'),
  (Join-Path $reportDir 'phase6h_gbrwcore_export_resolution.md'),
  (Join-Path $reportDir 'phase6h_gamelist_post_update_branch.md'),
  (Join-Path $reportDir 'phase6h_gbrwshell_role.md'),
  (Join-Path $reportDir 'phase6h_p_extchunk_provider.md'),
  (Join-Path $reportDir 'phase6h_launch_chain_result.md'),
  (Join-Path $reportDir 'phase6h_blocker_mid_ladder.md')
) | Where-Object { Test-Path $_ }
Compress-Archive -Path $toZip -DestinationPath $zip -Force
Write-Host "pack=$zip"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6h failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_PHASE6H_GWY_GUEST_RUNAPP complete'
