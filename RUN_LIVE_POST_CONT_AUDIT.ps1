# Phase 6D-A: Post-Continuation Runtime Progress Audit (observe-only).
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

$banned = Select-String -Path (Join-Path $Root 'src\runtime\ext_post_cont_audit.c') `
  -Pattern '0x2B1858|0x280400|0x304AED|0x303B92|0x2AEB34|0x2D8DF4|appid\s*==\s*400101|ui_mode|force_entry' -ErrorAction SilentlyContinue
if ($banned) { throw "absolute ERW/PC/appid or force literals in post_cont core: $($banned.Line)" }

$logDir = Join-Path $Root 'logs'
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
$stdout = Join-Path $logDir 'phase6d_a_live_stdout.txt'
$report = Join-Path $logDir 'phase6d_a_post_cont_report.txt'
$snap = Join-Path $logDir 'module_registry_phase6d_a.json'

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
  Remove-Item Env:GWY_ENTRY_RECONCILE -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $outLog -RedirectStandardError $errLog -PassThru
  $deadline = (Get-Date).AddSeconds([Math]::Max(1, $secs))
  $ready = $false
  $armed = $false
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $outLog)) { continue }
    if (-not $armed -and (Select-String -Path $outLog -Pattern '\[POST_CONT_ARM\]|event_type=ROBOTOL_CONTINUATION' -Quiet)) {
      $armed = $true
    }
    # Marker-driven: stop after POST_CONT_SUMMARY, or after continuation + post boundary.
    if ((Select-String -Path $outLog -Pattern '\[POST_CONT_SUMMARY\]' -Quiet)) {
      Start-Sleep -Seconds 1
      $ready = $true
      break
    }
    if ($armed -and (Select-String -Path $outLog -Pattern '\[POST_CONT_PLATFORM_API\]|\[POST_CONT_FAULT\]|\[POST_CONT_RETURN\]|\[ROBOTOL_RUNTIME_READY\]' -Quiet)) {
      # Give a short window for summary emission after first boundary.
      Start-Sleep -Seconds 2
      if ((Select-String -Path $outLog -Pattern '\[POST_CONT_SUMMARY\]' -Quiet) -or
          (Select-String -Path $outLog -Pattern 'event_type=(NORMAL_RETURN|PLATFORM_API|FILE_REQUEST|TIMER_REQUEST|DRAW_CALL|REFRESH_CALL|NEW_FAULT|EVENT_WAIT)' -Quiet)) {
        $ready = $true
        break
      }
    }
  }
  if (-not $p.HasExited) {
    # Best-effort: process may not call finalize; harness classifies timeout.
    try { Stop-Process -Id $p.Id -Force } catch {}
    Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 600
  }
  if (-not $ready -and -not (Test-Path $outLog)) {
    throw "live stdout missing: $outLog"
  }
}

Write-Host "== Phase 6D-A JJFB post-cont audit Seconds=$Seconds =="
Invoke-Live $stdout (Join-Path $logDir 'phase6d_a_live_stderr.txt') $ResourceRoot 'gwy/jjfb.mrp' `
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
Assert-Log 'post_cont_arm' ($all -match '\[POST_CONT_ARM\]|event_type=ROBOTOL_CONTINUATION') 'POST_CONT not armed'
Assert-Log 'nested_r9_scope_gate' ($all -match 'nested_r9_scope_gate=open') 'nested gate'
Assert-Log 'module_r9_switch_gate' ($all -match 'module_r9_switch_gate=open') 'module gate'
Assert-Log 'guest_callback_frame_gate' ($all -match 'guest_callback_frame_gate=blocked') 'callback gate'
Assert-Log 'bootstrap_entry_r9_gate' ($all -match 'bootstrap_entry_r9_gate=blocked') 'bootstrap'
Assert-Log 'phase6b_b_gate' ($all -match 'phase6b_b_gate=blocked') 'phase6b_b'
Assert-Log 'er_rw_timing_gate' ($all -match 'er_rw_metadata_timing_gate=blocked') 'er_rw timing'
Assert-Log 'no_forbidden_fix' (
  -not ($all -match 'force_entry_r0|dsm_module_record_set_dispatch|ext_platform_context_create|GuestCallbackFrame.*restore|BOOTSTRAP_TEMP_R9|force_ui_mode')
) 'forbidden fix suspected'
Assert-Log 'old_fault_274_gone' (
  -not ($all -match 'invalid_address=0x274|mem_invalid[^\r\n]*at 0x274')
) 'old 0x274 returned'

# Parse summary fields (or synthesize from markers if process killed before finalize).
$class = 'UNKNOWN'
$instr = '0'
$ready = 'no'
$firstApi = 'none'
$draw = 'no'
$refresh = 'no'
$stop = 'UNKNOWN'
$nextFix = 'NONE'
$postGate = 'blocked'
$graphicsGate = 'blocked'
$eventGate = 'blocked'

if ($all -match '\[POST_CONT_SUMMARY\][^\r\n]*post_continuation_class=(\S+)') { $class = $Matches[1] }
if ($all -match '\[POST_CONT_SUMMARY\][^\r\n]*continuation_instruction_count=(\d+)') { $instr = $Matches[1] }
elseif ($all -match '\[POST_CONT\][^\r\n]*instr=(\d+)') {
  # last instr= in POST_CONT lines
  $ms = [regex]::Matches($all, '\[POST_CONT\][^\r\n]*instr=(\d+)')
  if ($ms.Count -gt 0) { $instr = $ms[$ms.Count - 1].Groups[1].Value }
}
if ($all -match '\[ROBOTOL_RUNTIME_READY\]') { $ready = 'yes' }
elseif ($all -match '\[POST_CONT_SUMMARY\][^\r\n]*robotol_runtime_ready=(\S+)') { $ready = $Matches[1] }
if ($all -match '\[POST_CONT_PLATFORM_API\][^\r\n]*api=(\S+)') { $firstApi = $Matches[1] }
elseif ($all -match '\[POST_CONT_SUMMARY\][^\r\n]*first_platform_api=(\S+)') { $firstApi = $Matches[1] }
if ($all -match 'draw_seen=yes|event_type=DRAW_CALL|category=DRAW|category=BITMAP') { $draw = 'yes' }
if ($all -match 'refresh_seen=yes|event_type=REFRESH_CALL|category=REFRESH') { $refresh = 'yes' }

if ($all -match '\[POST_CONT_SUMMARY\][^\r\n]*stop_reason=(\S+)') {
  $stop = $Matches[1]
} elseif ($all -match '\[POST_CONT_FAULT\]') {
  $stop = 'NEW_FAULT'
  if ($class -eq 'UNKNOWN') { $class = 'NEW_ABI_FAULT' }
} elseif ($all -match '\[POST_CONT_RETURN\]') {
  $stop = 'NORMAL_RETURN'
} elseif ($all -match '\[POST_CONT_PLATFORM_API\]') {
  $stop = 'PLATFORM_API'
  if ($class -eq 'UNKNOWN') { $class = 'RUNTIME_PROGRESS_TO_PLATFORM_API' }
} elseif ($all -match '\[POST_CONT_ARM\]') {
  $stop = 'HARNESS_TIMEOUT'
  if ($class -eq 'UNKNOWN') {
    if ($ready -ne 'yes' -and [int64]$instr -ge 64) { $class = 'MODULE_ER_RW_RUNTIME_GAP' }
    elseif ([int64]$instr -lt 64) { $class = 'HARNESS_WINDOW_TOO_SHORT' }
    else { $class = 'MISSING_EVENT_SCHEDULING' }
  }
}

if ($all -match '\[POST_CONT_SUMMARY\][^\r\n]*next_allowed_fix=(\S+)') {
  $nextFix = $Matches[1]
} else {
  switch ($class) {
    'NEW_ABI_FAULT' { $nextFix = 'NEW_FAULT_ANALYSIS_ONLY' }
    'MODULE_ER_RW_RUNTIME_GAP' { $nextFix = 'MODULE_ER_RW_RUNTIME_ONLY' }
    'MISSING_EVENT_SCHEDULING' { $nextFix = 'EVENT_SCHEDULER_ONLY' }
    'MISSING_GRAPHICS_REFRESH' { $nextFix = 'GRAPHICS_REFRESH_ONLY' }
    'RUNTIME_PROGRESS_TO_PLATFORM_API' {
      if ($all -match 'Not yet implemented|POST_CONT_UNIMPLEMENTED') { $nextFix = 'FIRST_MISSING_PLATFORM_API_ONLY' }
      elseif ($all -match 'category=TIMER|category=EVENT') { $nextFix = 'EVENT_SCHEDULER_ONLY' }
      else { $nextFix = 'FIRST_MISSING_PLATFORM_API_ONLY' }
    }
    'GRAPHICS_PIPELINE_ACTIVE' { $nextFix = 'NONE' }
    'NORMAL_IDLE' { $nextFix = 'NONE' }
    'HARNESS_WINDOW_TOO_SHORT' { $nextFix = 'NONE' }
    default { $nextFix = 'NONE' }
  }
}

if ($nextFix -ne 'NONE') { $postGate = 'open' }
if ($nextFix -eq 'GRAPHICS_REFRESH_ONLY') { $graphicsGate = 'open' }
if ($nextFix -eq 'EVENT_SCHEDULER_ONLY') { $eventGate = 'open' }
if ($all -match 'post_continuation_gate=open') { $postGate = 'open' }

Assert-Log 'has_class' ($class -ne 'UNKNOWN' -or ($all -match '\[POST_CONT_ARM\]')) "class=$class"
Assert-Log 'single_next_fix' (
  @('FIRST_MISSING_PLATFORM_API_ONLY','EVENT_SCHEDULER_ONLY','GRAPHICS_REFRESH_ONLY',
    'MODULE_ER_RW_RUNTIME_ONLY','NEW_FAULT_ANALYSIS_ONLY','NONE') -contains $nextFix
) "next=$nextFix"

$reportBody = @()
$reportBody += 'Phase 6D-A Post-Continuation Runtime Progress Audit'
$reportBody += "jjfb_sha256=$hash"
$reportBody += "post_continuation_gate=$postGate"
$reportBody += "graphics_gate=$graphicsGate"
$reportBody += "event_scheduler_gate=$eventGate"
$reportBody += 'nested_r9_scope_gate=open'
$reportBody += 'module_r9_switch_gate=open'
$reportBody += 'guest_callback_frame_gate=blocked'
$reportBody += 'bootstrap_entry_r9_gate=blocked'
$reportBody += 'phase6b_b_gate=blocked'
$reportBody += 'er_rw_metadata_timing_gate=blocked'
$reportBody += "post_continuation_class=$class"
$reportBody += "continuation_instruction_count=$instr"
$reportBody += "robotol_runtime_ready=$ready"
$reportBody += "first_platform_api=$firstApi"
$reportBody += "draw_seen=$draw"
$reportBody += "refresh_seen=$refresh"
$reportBody += "stop_reason=$stop"
$reportBody += "next_allowed_fix=$nextFix"
$reportBody += 'mode=observe_only'
foreach ($c in $checks) {
  $detail = if ($c.ok) { 'ok' } else { $c.detail }
  $reportBody += ("{0}={1} {2}" -f $c.name, $(if ($c.ok) { 'PASS' } else { 'FAIL' }), $detail)
}
$reportBody | Set-Content -Path $report -Encoding utf8
Write-Host "class=$class instr=$instr ready=$ready firstApi=$firstApi draw=$draw refresh=$refresh stop=$stop next=$nextFix"
Write-Host "report=$report"

$failed = @($checks | Where-Object { -not $_.ok })
if ($failed.Count -gt 0) {
  throw ("phase6d-a failed: " + (($failed | ForEach-Object { $_.name }) -join ', '))
}
Write-Host '[OK] RUN_LIVE_POST_CONT_AUDIT complete'
