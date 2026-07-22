# Stage E8T: C9D unlock writer + UI-init state requirement — quick by default
# NOT product success. No direct C9D/CF5 poke. No FAST_C9D_UNLOCK until real write_1 found.
param(
  [int]$Seconds = 75,
  [string]$Target = 'gwy/jjfb.mrp',
  [switch]$SkipBuild,
  [switch]$FullMatrix,
  [switch]$Deep
)

$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
while ($Root -and -not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $parent = Split-Path -Parent $Root
  if (-not $parent -or $parent -eq $Root) { break }
  $Root = $parent
}
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  throw "cannot locate repo root from $PSScriptRoot"
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8t_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

$CASE_TIMEOUT_SEC = [Math]::Max(60, [Math]::Min(90, $Seconds))
$OUTER_KILL_SEC = $CASE_TIMEOUT_SEC + 15
$summaryPath = Join-Path $reportDir 'stage_e8t_speed_summary.jsonl'
if (Test-Path $summaryPath) { Remove-Item -Force $summaryPath }

$quick = ($env:JJFB_E8T_QUICK -ne '0')
if ($FullMatrix -or ($env:JJFB_E8T_FULL_MATRIX -eq '1')) { $quick = $false }
if ($Deep -or ($env:JJFB_E8T_DEEP -eq '1')) { $env:JJFB_E8T_DEEP = '1' }
$env:JJFB_E8T_QUICK = if ($quick) { '1' } else { '0' }

$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (Test-Path $ext) {
  py -3 (Join-Path $Root 'tools\e8t_c9d_writer_xref.py') --ext $ext --out-dir $outDir | Out-Host
}

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
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
  'p:0x30AA46','p:0x3115BA','p:0x2FB008','p:0x2FAFFC','p:0x30D9EE','p:0x2E3A68',
  'p:0x2E7DC2','p:0x2F7F2C','p:0x30213E','p:0x301848'
) -join ','

$insnDefault = if ($env:JJFB_E8T_DEEP -eq '1') { '5000000' } else { '500000' }
$tickDefault = if ($env:JJFB_E8T_DEEP -eq '1') { 600 } else { 80 }

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE','JJFB_E8S_MODE','JJFB_E8T_MODE',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3','JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN',
    'JJFB_FAST_UI_INIT_CALL','JJFB_FAST_UI_STATE','JJFB_FAST_UI_ED8','JJFB_FAST_UI_CA3',
    'JJFB_FAST_C9D_UNLOCK_CALL'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Stop-E8TChildren {
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

function Write-CaseDone([string]$Name, [string]$Verdict, [double]$Elapsed, [hashtable]$Hits) {
  $line = "== E8T_CASE_DONE name=$Name verdict=$Verdict elapsed=$([Math]::Round($Elapsed,1))"
  Write-Host $line
  [Console]::Out.Flush()
  $obj = [ordered]@{
    case_name = $Name
    verdict = $Verdict
    elapsed_sec = [Math]::Round($Elapsed, 2)
    hit_2E4788 = [bool]$Hits.hit_2E4788
    hit_2FC8C0 = [bool]$Hits.hit_2FC8C0
    hit_2FC8CE = [bool]$Hits.hit_2FC8CE
    hit_C9D_writer = [bool]$Hits.hit_C9D_writer
    hit_CF5_writer = [bool]$Hits.hit_CF5_writer
    hit_DRAW = [bool]$Hits.hit_DRAW
    hit_SVC_AB = [bool]$Hits.hit_SVC_AB
    hit_new_platform = [bool]$Hits.hit_new_platform
    ui_accepted_continue = [bool]$Hits.ui_accepted_continue
    fault_pc = $Hits.fault_pc
    timeout = ($Verdict -eq 'TIMEOUT')
    c44nz = [bool]$Hits.c44nz
    c9dnz = [bool]$Hits.c9dnz
    cf5nz = [bool]$Hits.cf5nz
  }
  ($obj | ConvertTo-Json -Compress) | Add-Content -Path $summaryPath -Encoding utf8
}

function Analyze-Hits([string]$log) {
  $h = @{
    hit_2E4788 = $false; hit_2FC8C0 = $false; hit_2FC8CE = $false
    hit_C9D_writer = $false; hit_CF5_writer = $false
    hit_DRAW = $false; hit_SVC_AB = $false; hit_new_platform = $false
    ui_accepted_continue = $false
    fault_pc = ''; c44nz = $false; c9dnz = $false; cf5nz = $false; reset = $false
  }
  if (-not $log -or -not (Test-Path $log)) { return $h }
  $h.hit_2E4788 = [bool](Select-String -Path $log -Pattern 'tag=p2E4788\b|FAST_UI_INIT_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2FC8C0 = [bool](Select-String -Path $log -Pattern 'JJFB_E8I_PARENT_HIT\] tag=p2FC8C0\b|FAST_UNLOCK_CALL' -Quiet -EA SilentlyContinue)
  $h.hit_2FC8CE = [bool](Select-String -Path $log -Pattern 'JJFB_E8I_PARENT_HIT\] tag=p2FC8CE\b|E8R_C44_UNLOCKED|C44_after=0x1' -Quiet -EA SilentlyContinue)
  $h.hit_C9D_writer = [bool](Select-String -Path $log -Pattern 'FLAG_TRANSITION\].*off=0xC9D|E8T_C9D_WRITER\]|tag=p30AA46|tag=p3115BA' -Quiet -EA SilentlyContinue)
  $h.hit_CF5_writer = [bool](Select-String -Path $log -Pattern 'FLAG_TRANSITION\].*off=0xCF5|E8S_FLAG_WRITER\] tag=p2E7DC2|tag=p2F7F2C' -Quiet -EA SilentlyContinue)
  $h.hit_DRAW = [bool](Select-String -Path $log -Pattern '\[JJFB_DRAW\]' -Quiet -EA SilentlyContinue)
  $h.hit_SVC_AB = [bool](Select-String -Path $log -Pattern 'JJFB_FAST_SVC_AB\]' -Quiet -EA SilentlyContinue)
  $h.c44nz = [bool](Select-String -Path $log -Pattern 'C44_after=0x1|E8R_C44_UNLOCKED|off=0xC44.*to=0x1' -Quiet -EA SilentlyContinue)
  # Prefer transition to nonzero; avoid matching C9D=0x0 dumps.
  $h.c9dnz = [bool](Select-String -Path $log -Pattern 'off=0xC9D[^\r\n]*to=0x(?!0\b)[0-9A-Fa-f]|C9D=0x(?!0\b)[0-9A-Fa-f]' -Quiet -EA SilentlyContinue)
  $h.cf5nz = [bool](Select-String -Path $log -Pattern 'off=0xCF5[^\r\n]*to=0x(?!0\b)[0-9A-Fa-f]|CF5=0x(?!0\b)[0-9A-Fa-f]' -Quiet -EA SilentlyContinue)
  $h.reset = [bool](Select-String -Path $log -Pattern 'tag=p2F4E82\b|off=0xC44.*to=0x0' -Quiet -EA SilentlyContinue)
  # UI-init continued past reject early-out only if unlock BL inside UI-init fired.
  $h.ui_accepted_continue = [bool](Select-String -Path $log -Pattern 'tag=p2E4840\b|JJFB_E8I_PARENT_HIT\] tag=p2E4840' -Quiet -EA SilentlyContinue)
  $fm = Select-String -Path $log -Pattern 'UC_MEM_.*UNMAPPED|FAULT.*pc=0x([0-9A-Fa-f]+)' -EA SilentlyContinue | Select-Object -Last 1
  if ($fm) { $h.fault_pc = $fm.Line.Substring(0, [Math]::Min(80, $fm.Line.Length)) }
  return $h
}

function Case-Verdict([hashtable]$h, [string]$fallback) {
  if ($h.hit_DRAW) { return 'FAST_REACHED_DRAW' }
  if ($h.hit_new_platform) { return 'FAST_NEW_PLATFORM_API_GAP' }
  if ($h.c9dnz -and $h.cf5nz) { return 'FAST_REACHED_CF5_GATE_NEXT_GAP' }
  if ($h.c9dnz) { return 'FAST_REACHED_C9D_TRANSITION_NEXT_GAP' }
  if ($h.cf5nz -and -not $h.c9dnz) { return 'FAST_REACHED_CF5_GATE_NEXT_GAP' }
  if ($h.ui_accepted_continue -and -not $h.c9dnz) { return 'UI_INIT_ACCEPTED_STATE_DERIVED_NEXT_GAP' }
  if ($h.hit_2E4788 -and -not $h.ui_accepted_continue -and -not $h.c9dnz) {
    return 'UI_INIT_REJECTS_STATE38_NEXT_GAP'
  }
  if ($h.c44nz -and -not $h.c9dnz) { return 'POST_C44_STILL_BLOCKED_BY_C9D' }
  return $fallback
}

function Invoke-E8TCase([string]$Label, [hashtable]$ExtraEnv) {
  Write-Host "== E8T $Label =="
  [Console]::Out.Flush()
  $t0 = Get-Date
  Clear-E8Modes
  $env:JJFB_E8T_MODE = '1'
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

  $caseLog = Join-Path $logDir "stage_e8t_${Label}_stdout.txt"
  $caseErr = Join-Path $logDir "stage_e8t_${Label}_stderr.txt"
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
      Stop-E8TChildren
      Start-Sleep -Milliseconds 200
    } elseif ($p.ExitCode -ne 0) {
      $verdict = 'FATAL_ERROR'
    }
  } catch {
    $verdict = 'FATAL_ERROR'
    Write-Host "E8T case exception: $_"
    Stop-E8TChildren
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
  if ($verdict -eq 'TIMEOUT') {
    # keep
  } elseif ($hits.c44nz -or $hits.hit_2FC8CE -or $hits.hit_2E4788 -or $hits.hit_DRAW -or $hits.c9dnz -or $hits.cf5nz) {
    $verdict = Case-Verdict $hits 'POST_C44_STILL_BLOCKED_BY_C9D'
  } elseif ($verdict -eq 'OK') {
    $verdict = Case-Verdict $hits 'POST_C44_STILL_BLOCKED_BY_C9D'
  }
  Write-CaseDone -Name $Label -Verdict $verdict -Elapsed $elapsed -Hits $hits
  $script:SkipBuildNext = $true
  return @{ log = $caseLog; verdict = $verdict; hits = $hits; elapsed = $elapsed }
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}
$script:SkipBuildNext = [bool]$SkipBuild

$fastBase = @{
  JJFB_FAST_ASSIST = '1'
  JJFB_FAST_SVC_AB = 'observe'
  JJFB_FAST_STATE = '38'
  JJFB_FAST_CASE156_R1 = '20'
  JJFB_FAST_SEQUENCE = 'case156'
  JJFB_FAST_C6C22 = '1'
  JJFB_FAST_DEC30 = '1'
  JJFB_FAST_INSN_LIMIT = $insnDefault
  JJFB_FAST_UNLOCK_CALL = '1'
  JJFB_FAST_UNLOCK_WHEN = 'before'
}

function New-FastExtra([hashtable]$Overlay) {
  $h = @{}
  foreach ($k in $fastBase.Keys) { $h[$k] = $fastBase[$k] }
  foreach ($k in $Overlay.Keys) { $h[$k] = $Overlay[$k] }
  return $h
}

# Quick: post-C44 C9D watch + UI-init accepted-state counterfactual (state=20 not in reject set)
$matrix = @(
  @{ L = 'A_unlock_c9d_watch'; Extra = (New-FastExtra @{}) },
  @{ L = 'B_unlock_ui_st20'; Extra = (New-FastExtra @{
      JJFB_FAST_UI_INIT_CALL = '1'
      JJFB_FAST_UI_STATE = '20'
      # ED8 gate: CMP #0; BGT early-out — must stay 0 (do NOT poke 1).
      JJFB_FAST_UI_CA3 = '1'
    })
  }
)

if (-not $quick) {
  $matrix += @(
    @{ L = 'C_ui_st37'; Extra = (New-FastExtra @{
        JJFB_FAST_UI_INIT_CALL = '1'
        JJFB_FAST_UI_STATE = '37'
        JJFB_FAST_UI_CA3 = '1'
      })
    },
    @{ L = 'D_ui_st30'; Extra = (New-FastExtra @{
        JJFB_FAST_UI_INIT_CALL = '1'
        JJFB_FAST_UI_STATE = '30'
        JJFB_FAST_UI_CA3 = '1'
      })
    }
  )
}

Write-Host "E8T quick=$quick timeout=${CASE_TIMEOUT_SEC}s insn=$insnDefault tick~=$tickDefault cases=$($matrix.Count)"
[Console]::Out.Flush()

$results = @{}
foreach ($m in $matrix) {
  $results[$m.L] = Invoke-E8TCase -Label $m.L -ExtraEnv $m.Extra
}

# Aggregate: prefer strongest new gap; static claims layered in report.
$best = 'POST_C44_STILL_BLOCKED_BY_C9D'
foreach ($name in @('B_unlock_ui_st20','C_ui_st37','D_ui_st30','A_unlock_c9d_watch')) {
  if (-not $results.ContainsKey($name)) { continue }
  $v = $results[$name].verdict
  if ($v -eq 'FAST_REACHED_DRAW') { $best = $v; break }
  if ($v -match 'FAST_REACHED_C9D|FAST_REACHED_CF5|FAST_NEW_PLATFORM') { $best = $v; break }
  if ($v -eq 'UI_INIT_ACCEPTED_STATE_DERIVED_NEXT_GAP') { $best = $v }
  elseif ($v -eq 'UI_INIT_REJECTS_STATE38_NEXT_GAP' -and $best -eq 'POST_C44_STILL_BLOCKED_BY_C9D') { $best = $v }
  elseif ($v -eq 'POST_C44_STILL_BLOCKED_BY_C9D' -and $best -eq 'POST_C44_STILL_BLOCKED_BY_C9D') { $best = $v }
}

$xref = Join-Path $outDir 'e8t_c9d_xref.md'
$staticNote = @(
  '- Exact STRB @ C9D: ``0x30AA46`` (write_0), ``0x3115BA`` (r7=#0 clear). **No STRB #1 @ C9D in EXT.**',
  '- Historical ``0x2E3A68`` / ``0x2FB008`` write **C9C=#1**, not C9D.',
  '- UI-init ``0x2E4788`` rejects state ``{38,46,69,252,300}``; needs ED8!=0, C8E==0, CA3==1.',
  '- Lane D ``JJFB_FAST_C9D_UNLOCK_CALL`` **not enabled** — no real nonzero C9D writer fn yet.'
)

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8T-Fast Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**Co-claims (static):** `UI_INIT_REJECTS_STATE38_NEXT_GAP`, `C9D_NONZERO_WRITER_NEVER_REACHED` (no write_1 STRB in robotol.ext).')
$md.Add('')
$md.Add('**ED8 correction:** UI-init uses `CMP ED8,#0; BGT early-out` — continue only if **ED8==0** (prior E8S note ED8!=0 was wrong).')
$md.Add('')
$md.Add("**Mode:** quick=$quick timeout=${CASE_TIMEOUT_SEC}s insn=$insnDefault")
$md.Add('')
$md.Add('**NOT product success.**')
$md.Add('')
$md.Add('## Static (Lane A/C)')
$md.Add('')
foreach ($n in $staticNote) { $md.Add($n) }
$md.Add('- UI-init ED8: continue iff ``ED8<=0`` (typically 0); CA3==1; C8E==0; reject states ``{38,46,69,252,300}``.')
$md.Add('')
$md.Add('## Cases')
$md.Add('')
$md.Add('| Case | Verdict | Elapsed | C44 | C9D | CF5 | UI | UI-cont | DRAW |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- |')
foreach ($m in $matrix) {
  $r = $results[$m.L]
  $h = $r.hits
  $md.Add("| $($m.L) | $($r.verdict) | $([Math]::Round($r.elapsed,1))s | $($h.c44nz) | $($h.c9dnz) | $($h.cf5nz) | $($h.hit_2E4788) | $($h.ui_accepted_continue) | $($h.hit_DRAW) |")
}
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``reports/stage_e8t_speed_summary.jsonl``')
$md.Add('- ``logs/stage_e8t_*_stdout.txt``')
$md.Add('- ``out/e8t_tmp/e8t_c9d_xref.md``')
$md.Add('')

$reportPath = Join-Path $reportDir 'stage_e8t_fast_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8T complete (NOT product success)."
[Console]::Out.Flush()
