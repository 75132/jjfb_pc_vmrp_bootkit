# Phase 6D-B: Post-CFN Runtime R9 Promotion Audit (observe-only).
param(
  [int]$Seconds = 45,
  [switch]$SkipBuild
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_post_cfn_r9_audit.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x2AEB34|0x2D8DF4|appid\s*==\s*400101|ui_mode|force_entry|uc_reg_write' -ErrorAction SilentlyContinue
if ($banned) { throw "forbidden literals or uc_reg_write in post_cfn core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6d_b_live_stdout.txt'
$report = Join-Path $logDir 'phase6d_b_post_cfn_r9_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6d_b.json'

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
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  $ready = $false
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (Select-String -Path $outLog -Pattern '\[POST_CFN_SUMMARY\]' -Quiet) {
      Start-Sleep -Seconds 1
      $ready = $true
      break
    }
    if (Select-String -Path $outLog -Pattern '\[POST_CFN_FAULT\]' -Quiet) {
      Start-Sleep -Seconds 2
      if (Select-String -Path $outLog -Pattern '\[POST_CFN_SUMMARY\]' -Quiet) {
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

Write-Host "== Phase 6D-B JJFB post-CFN R9 audit Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6d_b_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'post_cfn_arm' ($all -match '\[POST_CFN_ARM\]') 'POST_CFN_ARM missing'
Assert-Log 'p_snapshot' ($all -match '\[CFUNCTION_P_SNAPSHOT\]') 'P snapshot missing'
Assert-Log 'fixr9_contract' ($all -match '\[FIXR9_STAGE_CONTRACT\]') 'fixR9 contracts missing'
Assert-Log 'fault_or_summary' (
  ($all -match '\[POST_CFN_FAULT\]') -or ($all -match '\[POST_CFN_SUMMARY\]')
) 'no fault/summary'
Assert-Log 'scope_stack' ($all -match '\[R9_STACK_SNAPSHOT\]') 'scope stack missing'
Assert-Log 'promotion_candidate' ($all -match '\[R9_SCOPE_PROMOTION_CANDIDATE\]') 'promotion candidate missing'
Assert-Log 'nested_r9_scope_gate' ($all -match 'nested_r9_scope_gate=open') 'nested gate'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module gate'
Assert-Log 'guest_callback_frame_gate' ($all -match 'guest_callback_frame_gate=blocked') 'callback gate'
Assert-Log 'bootstrap_entry_r9_gate' ($all -match 'bootstrap_entry_r9_gate=blocked') 'bootstrap'
Assert-Log 'no_forbidden_fix' (
  -not ($all -match 'force_entry_r0|force_ui_mode|BOOTSTRAP_TEMP_R9|promotion_performed=yes')
) 'forbidden fix suspected'
Assert-Log 'old_fault_274_gone' (
  -not ($all -match 'invalid_address=0x274|mem_invalid[^\r\n]*at 0x274')
) 'old 0x274 returned'

$class = 'UNKNOWN'
$faultInsn = 'none'
$r0Writer = '0'
$r0Src = 'UNKNOWN'
$erExact = '0'
$erBefore = 'no'
$regPend = 'UNKNOWN'
$scopeStack = 'see_log'
$nextFix = 'NONE'
$gate = 'blocked'
$funcStart = '0'
$memPc = '0'

if ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*post_cfn_r9_class=(\S+)') { $class = $Matches[1] }
if ($all -match '\[POST_CFN_FAULT\][^\r\n]*address_expr=(\S+)') { $faultInsn = $Matches[1] }
elseif ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*fault_instruction=(\S+)') { $faultInsn = $Matches[1] }
if ($all -match '\[POST_CFN_R0_SOURCE\][^\r\n]*last_writer_pc=(0x[0-9A-Fa-f]+)') { $r0Writer = $Matches[1] }
if ($all -match '\[POST_CFN_R0_SOURCE\][^\r\n]*source=(\S+)') { $r0Src = $Matches[1] }
if ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*robotol_er_rw_exact=(0x[0-9A-Fa-f]+)') { $erExact = $Matches[1] }
elseif ($all -match '\[ROBOTOL_ER_RW_READY\][^\r\n]*er_rw=(0x[0-9A-Fa-f]+)') { $erExact = $Matches[1] }
if ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*er_rw_ready_before_continuation=(\S+)') { $erBefore = $Matches[1] }
if ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*registry_pending_reason=(\S+)') { $regPend = $Matches[1] }
elseif ($all -match '\[POST_CFN_REGISTRY\][^\r\n]*pending_reason=(\S+)') { $regPend = $Matches[1] }
if ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*next_allowed_fix=(\S+)') { $nextFix = $Matches[1] }
if ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*function_start=(0x[0-9A-Fa-f]+)') { $funcStart = $Matches[1] }
if ($all -match '\[POST_CFN_SUMMARY\][^\r\n]*memory_access_pc=(0x[0-9A-Fa-f]+)') { $memPc = $Matches[1] }
if ($all -match 'post_cfn_r9_gate=open') { $gate = 'open' }

$scopeParts = @()
if ($all -match '\[R9_STACK_SNAPSHOT\] depth=(\d+) active_r9=(0x[0-9A-Fa-f]+)') {
  $scopeParts += ("depth={0};active={1}" -f $Matches[1], $Matches[2])
}
$frameMatches = [regex]::Matches($all, '\[R9_STACK_SNAPSHOT\] frame\[(\d+)\][^\r\n]*caller=(\S+) callee=(\S+) saved_r9=(0x[0-9A-Fa-f]+) callee_r9=(0x[0-9A-Fa-f]+)')
foreach ($m in $frameMatches) {
  $scopeParts += ("[{0}]{1}->{2}:saved={3}:callee={4}" -f $m.Groups[1].Value, $m.Groups[2].Value, $m.Groups[3].Value, $m.Groups[4].Value, $m.Groups[5].Value)
}
if ($scopeParts.Count -gt 0) { $scopeStack = ($scopeParts -join ';') }

$allowedClass = @(
  'POST_CFN_R9_PROMOTION_REQUIRED','ER_RW_REGISTRY_PUBLICATION_LATE','GUEST_FIXR9_PATH_MISSING',
  'CONTINUATION_SCOPE_NOT_PROMOTED','R0_SOURCE_INDEPENDENT_OF_R9','P_MODULE_ASSOCIATION_MISSING','UNKNOWN'
)
Assert-Log 'class_allowed' ($allowedClass -contains $class) "class=$class"
$allowedFix = @(
  'CURRENT_SCOPE_R9_PROMOTION_ONLY','ROBOTOL_ER_RW_REGISTRY_PUBLISH_ONLY','NONE'
)
Assert-Log 'single_next_fix' ($allowedFix -contains $nextFix) "next=$nextFix"
Assert-Log 'observe_only' ($all -match 'promotion_performed=no|note=observe_only') 'observe tag'

$reportBody = @()
$reportBody += 'Phase 6D-B Post-CFN Runtime R9 Promotion Audit'
$reportBody += "jjfb_sha256=$hash"
$reportBody += "post_cfn_r9_gate=$gate"
$reportBody += 'post_continuation_gate=open'
$reportBody += 'graphics_gate=blocked'
$reportBody += 'event_scheduler_gate=blocked'
$reportBody += 'nested_r9_scope_gate=open'
$reportBody += 'module_r9_switch_gate=open'
$reportBody += 'guest_callback_frame_gate=blocked'
$reportBody += 'bootstrap_entry_r9_gate=blocked'
$reportBody += 'phase6b_b_gate=blocked'
$reportBody += 'er_rw_metadata_timing_gate=blocked'
$reportBody += "fault_instruction=$faultInsn"
$reportBody += "function_start=$funcStart"
$reportBody += "memory_access_pc=$memPc"
$reportBody += "r0_last_writer=$r0Writer"
$reportBody += "r0_source_class=$r0Src"
$reportBody += "robotol_er_rw_exact=$erExact"
$reportBody += "er_rw_ready_before_continuation=$erBefore"
$reportBody += "registry_pending_reason=$regPend"
$reportBody += "scope_stack=$scopeStack"
$reportBody += "post_cfn_r9_class=$class"
$reportBody += "next_allowed_fix=$nextFix"
$reportBody += 'mode=observe_only'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class fault=$faultInsn r0src=$r0Src er=$erExact before=$erBefore reg=$regPend next=$nextFix gate=$gate"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6d-b failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_POST_CFN_R9_AUDIT complete'
