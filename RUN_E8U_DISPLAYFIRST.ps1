# Stage E8U-DisplayFirst: assisted real-render path to FIRST_REAL_FRAME
# NOT product success. No fake DRAW / no framebuffer paint / no C9D/CF5 poke.
param(
  [int]$Seconds = 90,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8u_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$shotDir = Join-Path $Root 'screenshots'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir, $shotDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 20
$summaryPath = Join-Path $reportDir 'stage_e8u_displayfirst_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (-not (Test-Path $flagMap)) {
  py -3 (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp') | Out-Host
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
foreach ($need in @('2256','3229','3317')) {
  if ($offsets -notmatch "(^|,)$need(,|$)") {
    $offsets = if ($offsets) { "$offsets,$need" } else { $need }
  }
}

$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x2F4E82','p:0x3046A8',
  'p:0x2E4788','p:0x2E4840','p:0x2E2F50','p:0x2E2520','p:0x30DDE2','p:0x2DB9DC',
  'p:0x3066B8','p:0x3066C6','p:0x3066DA','p:0x306740','p:0x2E88CC',
  'p:0x30AA46','p:0x3115BA','p:0x2E7DC2','p:0x2F7F2C','p:0x30213E','p:0x301848'
) -join ','

$insnDefault = '500000'

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE','JJFB_E8U_MODE',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3','JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_FAST_UI_INIT_CALL','JJFB_FAST_UI_STATE','JJFB_FAST_UI_ED8','JJFB_FAST_UI_CA3',
    'JJFB_FAST_UI_UPSTREAM','JJFB_FAST_UI_OBJECT_R0','JJFB_FAST_C9D_UNLOCK_CALL',
    'JJFB_DISPLAY_FIRST','JJFB_BYPASS_C9D_GATE','JJFB_BYPASS_CF5_GATE','JJFB_E8U_SCREENSHOT'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Stop-E8UChildren {
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -match '^(main|jjfb|vmrp|gwy)' -or
      ($_.Name -eq 'powershell.exe' -and $_.CommandLine -match 'ROBOTOL_MRCINIT|stage_e_product')
    } |
    ForEach-Object {
      try { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } catch {}
    }
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

function Analyze-Hits([string]$log) {
  $h = @{
    hit_2E4788 = $false; hit_2E4840 = $false; hit_2E2520 = $false; hit_2E2F50 = $false
    hit_2FC8C0 = $false; hit_2FC8CE = $false
    hit_c9d_assist = $false; hit_cf5_assist = $false; hit_idle_success = $false
    hit_DRAW = $false; hit_first_frame = $false; hit_SVC_AB = $false
    hit_SVC_AB_real = $false; hit_new_platform = $false
    ui_obj = $false; fault_pc = ''; c44nz = $false; c9dnz = $false; cf5nz = $false
    svc_note = ''
  }
  if (-not $log -or -not (Test-Path $log)) { return $h }
  $h.hit_2E4788 = [bool](Select-String -Path $log -Pattern 'tag=p2E4788\b|FAST_UI_INIT_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2E4840 = [bool](Select-String -Path $log -Pattern 'tag=p2E4840\b' -Quiet -EA SilentlyContinue)
  $h.hit_2E2520 = [bool](Select-String -Path $log -Pattern 'tag=p2E2520\b|FAST_UI_UPSTREAM_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2E2F50 = [bool](Select-String -Path $log -Pattern 'tag=p2E2F50\b' -Quiet -EA SilentlyContinue)
  $h.hit_2FC8C0 = [bool](Select-String -Path $log -Pattern 'tag=p2FC8C0\b|FAST_UNLOCK_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2FC8CE = [bool](Select-String -Path $log -Pattern 'tag=p2FC8CE\b|E8R_C44_UNLOCKED|C44_after=0x1' -Quiet -EA SilentlyContinue)
  $h.hit_c9d_assist = [bool](Select-String -Path $log -Pattern 'DISPLAY_FIRST_BRANCH_ASSIST\][^\r\n]*gate=C9D' -Quiet -EA SilentlyContinue)
  $h.hit_cf5_assist = [bool](Select-String -Path $log -Pattern 'DISPLAY_FIRST_BRANCH_ASSIST\][^\r\n]*gate=CF5' -Quiet -EA SilentlyContinue)
  $h.hit_idle_success = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_IDLE_SUCCESS\]' -Quiet -EA SilentlyContinue)
  $h.hit_DRAW = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\]|JJFB_E8U_DRAW\]' -Quiet -EA SilentlyContinue)
  $h.hit_first_frame = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_FIRST_REAL_FRAME\]' -Quiet -EA SilentlyContinue)
  # Lane E: distinguish real SVC dump vs stale/mis-tag
  $svcFast = Select-String -Path $log -Pattern 'JJFB_FAST_SVC_AB\]' -EA SilentlyContinue | Select-Object -First 1
  $svcE8h = Select-String -Path $log -Pattern 'JJFB_E8H_SVC_AB\]' -EA SilentlyContinue | Select-Object -First 1
  $h.hit_SVC_AB = [bool]$svcFast
  $h.hit_SVC_AB_real = [bool]$svcE8h
  if ($svcE8h) { $h.svc_note = $svcE8h.Line.Substring(0, [Math]::Min(160, $svcE8h.Line.Length)) }
  elseif ($svcFast) { $h.svc_note = 'FAST_SVC_AB_without_E8H_line' }
  else { $h.svc_note = 'none' }
  $h.ui_obj = [bool](Select-String -Path $log -Pattern 'JJFB_E8U_UI_OBJECT\]' -Quiet -EA SilentlyContinue)
  $h.c44nz = [bool](Select-String -Path $log -Pattern 'C44_after=0x1|E8R_C44_UNLOCKED|off=0xC44.*to=0x1' -Quiet -EA SilentlyContinue)
  $h.c9dnz = [bool](Select-String -Path $log -Pattern 'off=0xC9D[^\r\n]*to=0x(?!0\b)[0-9A-Fa-f]|C9D=0x(?!0\b)[0-9A-Fa-f]' -Quiet -EA SilentlyContinue)
  $h.cf5nz = [bool](Select-String -Path $log -Pattern 'off=0xCF5[^\r\n]*to=0x(?!0\b)[0-9A-Fa-f]|CF5=0x(?!0\b)[0-9A-Fa-f]' -Quiet -EA SilentlyContinue)
  $h.hit_new_platform = [bool](Select-String -Path $log -Pattern 'JJFB_PLAT_CALL\][^\r\n]*code=0x1E2|new_platform|UNSUPPORTED' -Quiet -EA SilentlyContinue)
  $fm = Select-String -Path $log -Pattern 'UC_MEM_.*UNMAPPED|FAULT.*pc=0x([0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($fm) { $h.fault_pc = $fm.Line.Substring(0, [Math]::Min(80, $fm.Line.Length)) }
  return $h
}

function Case-Verdict([hashtable]$h) {
  if ($h.hit_first_frame) { return 'FIRST_REAL_FRAME_REACHED' }
  if ($h.hit_DRAW) { return 'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' }
  if ($h.hit_idle_success -or ($h.hit_c9d_assist -and $h.hit_cf5_assist)) {
    return 'C9D_GATE_BYPASSED_NEXT_GAP'
  }
  if ($h.hit_c9d_assist) { return 'C9D_GATE_BYPASSED_NEXT_GAP' }
  if ($h.hit_cf5_assist -and -not $h.hit_idle_success -and -not $h.hit_DRAW) {
    return 'DISPLAY_FIRST_BLOCKED_BY_CF5'
  }
  if ($h.hit_new_platform -and $h.hit_c9d_assist) { return 'DISPLAY_FIRST_BLOCKED_BY_NEW_PLATFORM_API' }
  if ($h.hit_SVC_AB_real -and $h.hit_c9d_assist -and -not $h.hit_idle_success) {
    return 'DISPLAY_FIRST_BLOCKED_BY_SVC_AB'
  }
  if ($h.hit_2E4840) { return 'UI_INIT_REACHED_2E4840_NEXT_GAP' }
  if ($h.ui_obj -and -not $h.hit_2E4840) { return 'UI_OBJECT_ACQUIRED_NEXT_GAP' }
  if ($h.fault_pc -match 'UNMAPPED@0x4|UI') { return 'DISPLAY_FIRST_BLOCKED_BY_UI_OBJECT' }
  if ($h.c44nz -and -not $h.hit_c9d_assist) { return 'DISPLAY_FIRST_STILL_BLANK' }
  return 'DISPLAY_FIRST_STILL_BLANK'
}

function Write-CaseDone([string]$Name, [string]$Verdict, [double]$Elapsed, [hashtable]$Hits) {
  $line = "== E8U_CASE_DONE name=$Name verdict=$Verdict elapsed=$([Math]::Round($Elapsed,1))"
  Write-Host $line
  [Console]::Out.Flush()
  $obj = [ordered]@{
    case_name = $Name
    verdict = $Verdict
    elapsed_sec = [Math]::Round($Elapsed, 2)
    hit_2E4788 = [bool]$Hits.hit_2E4788
    hit_2E4840 = [bool]$Hits.hit_2E4840
    hit_2E2520 = [bool]$Hits.hit_2E2520
    hit_2FC8C0 = [bool]$Hits.hit_2FC8C0
    hit_c9d_assist = [bool]$Hits.hit_c9d_assist
    hit_cf5_assist = [bool]$Hits.hit_cf5_assist
    hit_idle_success = [bool]$Hits.hit_idle_success
    hit_DRAW = [bool]$Hits.hit_DRAW
    hit_first_frame = [bool]$Hits.hit_first_frame
    hit_SVC_AB = [bool]$Hits.hit_SVC_AB
    hit_SVC_AB_real = [bool]$Hits.hit_SVC_AB_real
    svc_note = $Hits.svc_note
    ui_obj = [bool]$Hits.ui_obj
    fault_pc = $Hits.fault_pc
    timeout = ($Verdict -eq 'TIMEOUT')
    c44nz = [bool]$Hits.c44nz
    c9dnz = [bool]$Hits.c9dnz
    cf5nz = [bool]$Hits.cf5nz
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content -Path $summaryPath -Encoding utf8
}

function Invoke-E8UCase([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8U $Label =="
  [Console]::Out.Flush()
  $t0 = Get-Date
  Clear-E8Modes
  $env:JJFB_E8U_MODE = '1'
  $env:JJFB_DISPLAY_FIRST = '1'
  $env:JJFB_BYPASS_C9D_GATE = '1'
  $env:JJFB_E8U_SCREENSHOT = (Join-Path $shotDir "e8u_${Label}_frame.bmp")
  $env:JJFB_E8C_IDLE_WATCH = '1'
  $env:JJFB_E8C_WATCH_OFFSETS = $offsets
  $env:JJFB_E8D_EARLY_WATCH = '1'
  $env:JJFB_E8J_CLUSTER_BP = '1'
  $env:JJFB_E8J_BP_SPEC = $bpSpec
  $env:JJFB_E8I_STATE_WATCH = '1'
  $env:JJFB_PLAT_CENSUS = '1'
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_LAUNCH_SOURCE = 'descriptor_launcher'
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_PRIMARY_TARGET = ($Target -replace '\\', '/')
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:JJFB_GAME_PACKAGE_ER_RW_SOURCE = 'module_map_or_mrpgcmap'
  $env:JJFB_GAME_PACKAGE_CONTEXT_PROVIDER = '1'
  $env:JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
  $env:JJFB_MODULE_REGISTRY_TRACE = '1'
  $env:JJFB_ROBOTOL_ENTRY_TRACE = '1'
  $env:JJFB_MRC_INIT_TRACE = '1'
  $env:JJFB_LIFECYCLE_EVENT_TRACE = '1'
  $env:JJFB_E7_LIFECYCLE_MODE = '1'
  $env:JJFB_POST_START_SCHEDULER_TRACE = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:GWY_PACKAGE_APPID = '400101'
  $env:GWY_PACKAGE_APPVER = '12'
  foreach ($k in $ExtraEnv.Keys) { Set-Item -Path "Env:$k" -Value $ExtraEnv[$k] }

  $caseLog = Join-Path $logDir "stage_e8u_${Label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e8u_${Label}_stderr.txt"
  $prodScript = Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'
  $argList = @(
    '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $prodScript,
    '-Target', $Target, '-Seconds', "$CASE_TIMEOUT_SEC"
  )
  if ($script:SkipBuildNext) { $argList += '-SkipBuild' }

  $verdict = 'OK'
  try {
    $p = Start-Process -FilePath 'powershell.exe' -ArgumentList $argList `
      -WorkingDirectory $Root -PassThru `
      -RedirectStandardOutput $caseLog -RedirectStandardError $caseErr
    $ok = $p.WaitForExit($OUTER_KILL_SEC * 1000)
    if (-not $ok) {
      $verdict = 'TIMEOUT'
      try { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } catch {}
      Stop-E8UChildren
      Start-Sleep -Milliseconds 200
    } elseif ($p.ExitCode -ne 0) {
      $verdict = 'FATAL_ERROR'
    }
  } catch {
    $verdict = 'FATAL_ERROR'
    Write-Host "E8U case exception: $_"
    Stop-E8UChildren
  }

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  if ((Test-Path $src) -and ((Get-Item $src).Length -gt 5000)) {
    Copy-Item -Force $src $caseLog
  } elseif ((Test-Path $vm) -and ((Get-Item $caseLog -EA SilentlyContinue).Length -lt 5000)) {
    Copy-Item -Force $vm $caseLog
  }

  $elapsed = ((Get-Date) - $t0).TotalSeconds
  $hits = Analyze-Hits $caseLog
  # Prefer observed progress over FATAL_ERROR from product wrapper exit code.
  if ($verdict -eq 'FATAL_ERROR' -or $verdict -eq 'TIMEOUT' -or $verdict -eq 'OK') {
    $v2 = Case-Verdict $hits
    if ($v2 -ne 'DISPLAY_FIRST_STILL_BLANK' -or $hits.c44nz -or $hits.hit_c9d_assist) {
      $verdict = $v2
    }
  }
  Write-CaseDone -Name $Label -Verdict $verdict -Elapsed $elapsed -Hits $hits
  $script:SkipBuildNext = $true

  # Stop remaining cases after first real frame/draw.
  $stopMore = ($hits.hit_first_frame -or $hits.hit_DRAW)
  return @{ log = $caseLog; verdict = $verdict; hits = $hits; elapsed = $elapsed; stopMore = $stopMore }
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}
$script:SkipBuildNext = [bool]$SkipBuild

$fastBase = @{
  JJFB_FAST_ASSIST = '1'
  # return0: continue past SVC so idle poll can hit C9D gate after real C44 unlock
  JJFB_FAST_SVC_AB = 'return0'
  JJFB_FAST_STATE = '38'
  JJFB_FAST_CASE156_R1 = '20'
  JJFB_FAST_SEQUENCE = 'case156'
  JJFB_FAST_C6C22 = '1'
  JJFB_FAST_DEC30 = '1'
  JJFB_FAST_INSN_LIMIT = $insnDefault
  JJFB_FAST_UNLOCK_CALL = '1'
  JJFB_FAST_UNLOCK_WHEN = 'before'
  JJFB_DISPLAY_FIRST = '1'
  JJFB_BYPASS_C9D_GATE = '1'
}

function New-FastExtra([hashtable]$Overlay) {
  $h = @{}
  foreach ($k in $fastBase.Keys) { $h[$k] = $fastBase[$k] }
  foreach ($k in $Overlay.Keys) { $h[$k] = $Overlay[$k] }
  return $h
}

# Lane D: max 3 quick cases
$matrix = @(
  @{ L = 'A_c44_bypass_c9d'; Extra = (New-FastExtra @{}) },
  @{ L = 'B_bypass_plus_ui_upstream'; Extra = (New-FastExtra @{
      JJFB_FAST_UI_UPSTREAM = '2E2520'
      JJFB_FAST_UI_STATE = '20'
      JJFB_FAST_UI_CA3 = '1'
      JJFB_BYPASS_CF5_GATE = '1'
    })
  },
  @{ L = 'C_state20_ui_init_bypass'; Extra = (New-FastExtra @{
      JJFB_FAST_UI_INIT_CALL = '1'
      JJFB_FAST_UI_STATE = '20'
      JJFB_FAST_UI_CA3 = '1'
      JJFB_BYPASS_CF5_GATE = '1'
    })
  }
)

Write-Host "E8U-DisplayFirst timeout=${CASE_TIMEOUT_SEC}s cases=$($matrix.Count) NOT_PRODUCT_SUCCESS"
[Console]::Out.Flush()

$results = @{}
foreach ($m in $matrix) {
  $results[$m.L] = Invoke-E8UCase -Label $m.L -ExtraEnv $m.Extra
  if ($results[$m.L].stopMore) {
    Write-Host "E8U stop-early: first real draw/frame in $($m.L)"
    break
  }
}

# Aggregate verdict priority
$best = 'DISPLAY_FIRST_STILL_BLANK'
$rank = @{
  'FIRST_REAL_FRAME_REACHED' = 100
  'FIRST_REAL_DRAW_API_REACHED_NEXT_GAP' = 90
  'C9D_GATE_BYPASSED_NEXT_GAP' = 80
  'UI_INIT_REACHED_2E4840_NEXT_GAP' = 55
  'UI_OBJECT_ACQUIRED_NEXT_GAP' = 50
  'DISPLAY_FIRST_BLOCKED_BY_CF5' = 40
  'DISPLAY_FIRST_BLOCKED_BY_NEW_PLATFORM_API' = 40
  'DISPLAY_FIRST_BLOCKED_BY_SVC_AB' = 40
  'DISPLAY_FIRST_BLOCKED_BY_UI_OBJECT' = 30
  'DISPLAY_FIRST_BLOCKED_BY_RESOURCE_LOAD' = 30
  'DISPLAY_FIRST_STILL_BLANK' = 10
}
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $v = $results[$m.L].verdict
  $rv = if ($rank.ContainsKey($v)) { $rank[$v] } else { 5 }
  $rb = if ($rank.ContainsKey($best)) { $rank[$best] } else { 0 }
  if ($rv -gt $rb) { $best = $v }
}

# Promote canonical screenshot if any case saved a frame
$canon = Join-Path $shotDir 'e8u_first_real_frame.png'
$canonBmp = Join-Path $shotDir 'e8u_first_real_frame.bmp'
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $cand = Join-Path $shotDir "e8u_$($m.L)_frame.bmp"
  if ((Test-Path $cand) -and ((Get-Item $cand).Length -gt 1000)) {
    Copy-Item -Force $cand $canonBmp
    # Keep .bmp as primary; note png may be absent (SDL_SaveBMP).
    break
  }
}

# Lane E audit snippet
$svcAudit = 'not_checked'
if ($results.ContainsKey('A_c44_bypass_c9d')) {
  $ha = $results['A_c44_bypass_c9d'].hits
  if ($ha.hit_SVC_AB_real) {
    $svcAudit = "REAL — E8H_SVC_AB observed. $($ha.svc_note)"
  } elseif ($ha.hit_SVC_AB) {
    $svcAudit = "FAST_ONLY — hit_SVC_AB true from JJFB_FAST_SVC_AB tag (E8T A used observe+stop)."
  } else {
    $svcAudit = 'NONE in E8U Case A (return0 continues; no stop-on-observe).'
  }
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8U-DisplayFirst Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** DISPLAY_FIRST is runtime branch assist only — no C9D/CF5 poke, no fake DRAW, no host framebuffer paint.')
$md.Add('')
$md.Add('## Rules')
$md.Add('')
$md.Add('- Real C44 unlock via ``0x2FC8C0``')
$md.Add('- Runtime-only C9D BNE assist @ ``0x3066C6`` → continue ``0x3066C8``')
$md.Add('- Optional CF5 assist only after C9D assist + ``JJFB_BYPASS_CF5_GATE=1``')
$md.Add('- UI-init refuses ``r0=0``; needs live/captured object')
$md.Add('')
$md.Add('## Lane E — SVC_AB audit (E8T discrepancy)')
$md.Add('')
$md.Add("- E8T ``A_unlock_c9d_watch`` ``hit_SVC_AB=true``: **real**, not stale. Log shows ``JJFB_E8H_SVC_AB`` + ``JJFB_FAST_SVC_AB`` with ``mode=observe`` then ``SVC_AB_STOP``.")
$md.Add("- Main report deprioritized SVC because it was on the FAST fire path before idle C9D, not the post-C9D draw blocker.")
$md.Add("- E8U Case A uses ``JJFB_FAST_SVC_AB=return0`` so idle poll can reach the C9D gate.")
$md.Add("- This run Case A: $svcAudit")
$md.Add('')
$md.Add('## Cases')
$md.Add('')
$md.Add('| Case | Verdict | Elapsed | C44 | C9D-assist | IdleOK | DRAW | Frame | UI-obj |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- |')
foreach ($m in $matrix) {
  if (-not $results.ContainsKey($m.L)) { continue }
  $r = $results[$m.L]
  $h = $r.hits
  $md.Add("| $($m.L) | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($h.c44nz) | $($h.hit_c9d_assist) | $($h.hit_idle_success) | $($h.hit_DRAW) | $($h.hit_first_frame) | $($h.ui_obj) |")
}
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``reports/stage_e8u_displayfirst_summary.jsonl``')
$md.Add('- ``logs/stage_e8u_*_stdout.txt``')
$md.Add('- ``screenshots/e8u_first_real_frame.bmp`` (if frame appeared)')
$md.Add('- ``logs/e8u_first_real_draw_stdout.txt`` (best draw case copy)')
$md.Add('')

# Copy best draw log
foreach ($name in @('A_c44_bypass_c9d','B_bypass_plus_ui_upstream','C_state20_ui_init_bypass')) {
  if (-not $results.ContainsKey($name)) { continue }
  $h = $results[$name].hits
  if ($h.hit_DRAW -or $h.hit_first_frame -or $h.hit_idle_success -or $h.hit_c9d_assist) {
    Copy-Item -Force $results[$name].log (Join-Path $logDir 'e8u_first_real_draw_stdout.txt')
    break
  }
}

$reportPath = Join-Path $reportDir 'stage_e8u_displayfirst_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8U-DisplayFirst complete (NOT product success)."
[Console]::Out.Flush()
