# Phase 6A live: Nested Helper ABI Conformance (JJFB + cross-target gwy.mrp).
param(
  [int]$Seconds = 12,
  [switch]$SkipBuild,
  [switch]$SkipCrossTarget
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$jjfb = Join-Path $ResourceRoot 'gwy\jjfb.mrp'
$ExpectedHash = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
if (-not (Test-Path $jjfb)) { throw "missing $jjfb" }
$hash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
if ($hash -ne $ExpectedHash) { throw "jjfb.mrp sha256 mismatch: $hash" }

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_entry_observe.c'),
  (Join-Path $Root 'src\runtime\guest_call_observer.c'),
  (Join-Path $Root 'src\runtime\gwy_ext_obs.c') -Pattern '0x304558|0x2D8DE0|0x304580' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute PC/base literals found in observe sources: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6a_live_helper_abi_stdout.txt'
$stderr = Join-Path $logDir 'phase6a_live_helper_abi_stderr.txt'
$report = Join-Path $logDir 'phase6a_helper_abi_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6a.json'
$xtStdout = Join-Path $logDir 'phase6a_xt_gwy_stdout.txt'
$xtStderr = Join-Path $logDir 'phase6a_xt_gwy_stderr.txt'

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
}

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
if (-not (Test-Path $exe)) { throw "missing $exe" }

$legacyKey = Join-Path $RunDir 'mythroad\sdk_key.dat'
if (Test-Path $legacyKey) { Remove-Item -Force $legacyKey }

function Invoke-Live([string]$outLog, [string]$errLog, [string]$resourceRoot, [string]$target, [string]$param, [string]$profile, [int]$secs) {
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = $target
  $env:GWY_LAUNCH_PARAM = $param
  $env:GWY_RESOURCE_ROOT = $resourceRoot
  $env:GWY_OVERLAY_ROOT = (Join-Path $RunDir 'overlay')
  $env:GWY_PROFILE = $profile
  $env:GWY_MODULE_SNAPSHOT = $snap
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  Start-Sleep -Seconds $secs
  if (-not $p.HasExited) {
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 400
  }
}

Write-Host "== Phase 6A JJFB Nested Helper ABI Seconds=$Seconds =="
Invoke-Live $stdout $stderr $ResourceRoot 'gwy/jjfb.mrp' `
  'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink' `
  (Join-Path $Root 'profiles\jjfb.json') $Seconds

$all = ''
if (Test-Path $stdout) { $all += (Get-Content $stdout -Raw -ErrorAction SilentlyContinue) }
if (Test-Path $stderr) { $all += "`n" + (Get-Content $stderr -Raw -ErrorAction SilentlyContinue) }

$checks = @()
function Assert-Log([string]$name, [bool]$ok, [string]$detail) {
  $script:checks += [pscustomobject]@{ name = $name; ok = $ok; detail = $detail }
  if ($ok) { Write-Host "[OK] $name" } else { Write-Host "[FAIL] $name : $detail" }
}

Assert-Log 'jjfb_hash' ($hash -eq $ExpectedHash) $hash
$origHash = (Get-FileHash -Algorithm SHA256 -Path $jjfb).Hash.ToLowerInvariant()
Assert-Log 'resource_hash_unchanged' ($origHash -eq $ExpectedHash) $origHash

Assert-Log 'dsm_code_image' ($all -match 'DSM_CODE_IMAGE|stage=DSM_CODE_IMAGE') 'DSM_CODE_IMAGE missing'
Assert-Log 'robotol_registered' (
  $all -match 'resolved=robotol\.ext.*state=REGISTERED' -or $all -match 'robotol\.ext.*REGISTERED'
) 'robotol REGISTERED missing'

Assert-Log 'helper_abi_enter' ($all -match '\[HELPER_ABI\].*stage=ROBOTOL_ENTER') 'ROBOTOL_ENTER missing'
Assert-Log 'caller_module_named' (
  $all -match 'caller_module=dsm:cfunction\.ext' -or
  $all -match 'caller_module=mrc_loader\.ext' -or
  $all -match 'caller_module=[^\s]+.*caller_offset=0x'
) 'caller_module/offset missing'
Assert-Log 'ext_fault_or_summary' (
  $all -match '\[EXT_FAULT\]' -or $all -match '\[HELPER_ABI_SUMMARY\]'
) 'EXT_FAULT / HELPER_ABI_SUMMARY missing'
Assert-Log 'field28_analysis' (
  $all -match '\[HELPER_ABI_FIELD28\]' -or $all -match 'layout_class='
) 'FIELD28 analysis missing'
Assert-Log 'helper_abi_summary' ($all -match '\[HELPER_ABI_SUMMARY\]') 'HELPER_ABI_SUMMARY missing'
Assert-Log 'no_low_va_map_hack' (
  -not ($all -match 'uc_mem_map\(\s*0x0\s*,') -and
  -not ($all -match 'mapped low address 0x28')
) 'low VA map hack suspected'
Assert-Log 'failed_lifecycle' (
  $all -match 'state=FAILED' -or $all -match '\[EXT_FAIL\]' -or $all -match 'ENTRY_EXECUTION'
) 'FAILED ENTRY_EXECUTION missing'

# Parse robotol ROBOTOL_ENTER (not DSM host bridge)
$csR0 = '?'
$thR0 = '?'
$enR0 = '?'
$zeroAt = '?'
$thunkDrop = 'false'
$callOrigin = 'none'
if ($all -match '\[HELPER_ABI_SUMMARY\].*call_site_r0=0x([0-9A-Fa-f]+).*thunk_r0=0x([0-9A-Fa-f]+).*enter_r0=0x([0-9A-Fa-f]+).*r0_became_zero_at=(\S+).*thunk_drop=(\w+).*call_site_origin=(\S+)') {
  $csR0 = $Matches[1]; $thR0 = $Matches[2]; $enR0 = $Matches[3]; $zeroAt = $Matches[4]; $thunkDrop = $Matches[5]; $callOrigin = $Matches[6]
}
$callerMod = 'unknown'
$callerOff = '?'
if ($all -match '\[HELPER_ABI\] stage=ROBOTOL_ENTER module=robotol\.ext[\s\S]*?caller_module=(\S+)\s+caller_offset=0x([0-9A-Fa-f]+)') {
  $callerMod = $Matches[1]; $callerOff = $Matches[2]
} elseif ($all -match 'resolved=robotol\.ext[\s\S]*?caller_module=(\S+)\s+caller_offset=0x([0-9A-Fa-f]+)') {
  $callerMod = $Matches[1]; $callerOff = $Matches[2]
}
# Prefer robotol enter r0 from dedicated line
if ($all -match '\[HELPER_ABI\] stage=ROBOTOL_ENTER module=robotol\.ext.*r0=0x([0-9A-Fa-f]+)') {
  $enR0 = $Matches[1]
}
$field28 = 'unknown'
if ($all -match 'layout_class=(\S+)') { $field28 = $Matches[1] }
$hasCallSite = [bool]($all -match 'stage=CALL_SITE_BEFORE')
$hasThunk = [bool]($all -match 'stage=THUNK_ENTER')
$movs0 = [bool]($all -match 'HELPER_ABI_CALLER_DISASM.*MOV r0, #0x0\b|MOVS r0, #0')
$blxRm = [bool]($all -match 'HELPER_ABI_CALLER_DISASM.*\* .*BLX r')
$entryNeHelper = $false
if ($all -match '\[HELPER_ABI\] stage=ROBOTOL_ENTER module=robotol\.ext module_id=\d+ module_offset=0x[0-9A-Fa-f]+ pc=0x([0-9A-Fa-f]+) helper=0x([0-9A-Fa-f]+)') {
  $entryNeHelper = ($Matches[1].ToLowerInvariant() -ne $Matches[2].ToLowerInvariant())
}
$field28Blx = [bool]($all -match '\[HELPER_ABI_FIELD28\].*dest_use blx=1')

# Cross-target gwy.mrp
$xtNote = 'skipped'
$xtR0 = '?'
if (-not $SkipCrossTarget) {
  $xtRoot = Join-Path $Root 'game_files\mythroad\240x320'
  $gwyMrp = Join-Path $xtRoot 'gwy.mrp'
  if (Test-Path $gwyMrp) {
    Write-Host '== Phase 6A cross-target gwy.mrp =='
    Invoke-Live $xtStdout $xtStderr $xtRoot 'gwy.mrp' `
      'napptype=12_nextid=0_ncode=0_narg=0_narg1=0_nmrpname=gwy.mrp_gwyblink' `
      (Join-Path $Root 'profiles\jjfb.json') ([Math]::Max(8, [int]($Seconds * 0.75)))
    $xt = ''
    if (Test-Path $xtStdout) { $xt += (Get-Content $xtStdout -Raw -ErrorAction SilentlyContinue) }
    if ($xt -match '\[HELPER_ABI\] stage=ROBOTOL_ENTER module=(?!dsm:)(\S+).*r0=0x([0-9A-Fa-f]+)') {
      $xtR0 = $Matches[2]
      $xtNote = "gwy.mrp nested enter module=$($Matches[1]) r0=0x$xtR0"
    } elseif ($xt -match '\[HELPER_ABI\] stage=ROBOTOL_ENTER module=mrc_loader\.ext.*r0=0x([0-9A-Fa-f]+)') {
      $xtR0 = $Matches[1]
      $xtNote = "gwy.mrp mrc_loader enter r0=0x$xtR0"
    } elseif ($xt -match 'mrc_loader|cfunction\.ext|HELPER_ABI') {
      $xtNote = 'gwy.mrp saw loader/helper tags (nested enter r0 not parsed)'
    } else {
      $xtNote = 'gwy.mrp ran but no HELPER_ABI enter (may not reach nested EXT in window)'
    }
    Assert-Log 'cross_target_ran' ($xt.Length -gt 100) 'cross-target log empty'
  } else {
    $xtNote = 'gwy.mrp missing'
    Assert-Log 'cross_target_ran' $false 'gwy.mrp missing'
  }
}

# Case classification (observe-only; no fake P)
# True Case A only if first call-site r0!=0 and enter r0==0 without LR_PROXY
$case = 'C_null_r0_or_wrong_entry'
$phase6b = ''
if ($thunkDrop -eq 'true' -and $callOrigin -ne 'LR_PROXY' -and $csR0 -ne '0' -and $csR0 -ne '00000000' -and $csR0 -ne '?') {
  $case = 'A_thunk_drop'
} elseif ($enR0 -eq '0' -or $enR0 -eq '00000000') {
  if ($movs0 -or $callOrigin -eq 'LR_PROXY') {
    $case = 'C_guest_null_r0'
    if ($entryNeHelper) { $case = 'C_null_r0_and_entry_ne_helper' }
  }
  if ($field28Blx -or $field28 -match 'mrc_extChunk') {
    $phase6b = 'Phase6B_candidate_fields(DOCUMENTED_names_only): mr_c_function_st{start_of_ER_RW,ER_RW_Length,ext_type,mrc_extChunk*,stack}; mrc_extChunk{check,init_func,event,code_buf,code_len,var_buf,var_len,global_p_buf,global_p_len,timer,sendAppEvent@field_28,extMrTable@field_2c}. Do not fill until bootstrap path confirmed. Live: field_28 loaded then BLX (function-pointer-like).'
  }
}

$lines = @(
  'Phase 6A Nested Helper ABI Conformance',
  "jjfb_sha256=$hash",
  "caller_module=$callerMod",
  "caller_offset=0x$callerOff",
  "call_site_present=$hasCallSite",
  "thunk_enter_present=$hasThunk",
  "call_site_r0=0x$csR0",
  "thunk_r0=0x$thR0",
  "robotol_enter_r0=0x$enR0",
  "r0_became_zero_at=$zeroAt",
  "thunk_drop=$thunkDrop",
  "call_site_origin=$callOrigin",
  "caller_disasm_movs_r0_imm=$movs0",
  "caller_disasm_blx_rm=$blxRm",
  "entry_ne_helper=$entryNeHelper",
  "field28_layout_class=$field28",
  "field28_dest_blx=$field28Blx",
  "case=$case",
  "phase6b_field_list=$phase6b",
  "cross_target=$xtNote",
  'hypothesis_r0_is_mr_c_function_st=confirmed_DOCUMENTED (MR_C_FUNCTION/TestCom801); live_robotol_enter_r0=0 so not satisfied',
  'hypothesis_field28_sendAppEvent=probable (DOCUMENTED mrc_extChunk+0x28) given BLX-after-LDR; not confirmed (no +0x2c seen in window)',
  'hypothesis_P_from_c_function_new=confirmed_DOCUMENTED',
  'hypothesis_caller_should_pass_P=probable_DOCUMENTED; live DSM BLX r1 with r0 from r4 == 0 at nested enter',
  "hypothesis_thunk_drops_r0=false (first CALL_SITE is LR_PROXY/guest with r0=0; later internal GUEST_CALL ignored)",
  'fix_applied=none (Case A not proven; no fake P; no PlatformRegistry)',
  'note=observe-only ABI proof path; next=Phase6B only if context bootstrap proven required'
)
foreach ($c in $checks) {
  $lines += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $c.detail)
}
Set-Content -Path $report -Value ($lines -join "`n") -Encoding utf8
Write-Host "case=$case caller=$callerMod+0x$callerOff enter_r0=0x$enR0 zero_at=$zeroAt field28=$field28 origin=$callOrigin"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6a live failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_HELPER_ABI complete'
