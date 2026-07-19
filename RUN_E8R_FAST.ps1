# Stage E8R: C44 unlock 0x2FC8C0 caller provenance — NOT product success
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

$outDir = Join-Path $Root 'out\e8r_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

$ext = Join-Path $Root 'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext'
if (Test-Path $ext) {
  python (Join-Path $Root 'tools\e8r_c44_unlock_caller.py') --ext $ext --out-dir $outDir
}

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $ext --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
if ($offsets -notmatch '(^|,)2256(,|$)') {
  $offsets = if ($offsets) { "$offsets,2256" } else { '2256' }
}

# Unlock writer + callers + UI cluster + helper + upstream sample
$bpSpec = @(
  'p:0x2FC8C0','p:0x2FC8CE','p:0x3046A8',
  'p:0x2DB948','p:0x2DB9DC',
  'p:0x2E4788','p:0x2E4840','p:0x2E48BC','p:0x2E4932','p:0x2E49A8','p:0x2E4A16','p:0x2E4A90','p:0x2E4B06',
  'p:0x30D300','p:0x30DDE2',
  'e:0x2E2F50','e:0x2E39BE','e:0x2E3BB2','e:0x2E3F7C',
  'p:0x30213E','p:0x301848','p:0x304558','p:0x2F4E82'
) -join ','

function Clear-E8Modes {
  @(
    'JJFB_E8B_MODE','JJFB_E8C_MODE','JJFB_E8D_MODE','JJFB_E8E_MODE','JJFB_E8F_MODE',
    'JJFB_E8G_MODE','JJFB_E8H_MODE','JJFB_E8I_MODE','JJFB_E8J_MODE','JJFB_E8K_MODE',
    'JJFB_E8L_MODE','JJFB_E8M_MODE','JJFB_E8N_MODE','JJFB_E8O_MODE','JJFB_E8P_MODE',
    'JJFB_E8Q_MODE','JJFB_E8R_MODE',
    'JJFB_E8E_EVENT_PROBE','JJFB_E8E_FE8_WATCH','JJFB_E8D_10165_PROBE',
    'JJFB_E8F_COUNTERFACTUAL','JJFB_E8H_SVC_TRAP','JJFB_E8F_SIBLING_PROBE',
    'JJFB_E8K_10102_CASE','JJFB_E8L_10102_REGS','JJFB_E8L_10102_R1','JJFB_E8L_10102_R2',
    'JJFB_E8L_10102_R3','JJFB_E8M_SEQ','JJFB_E8M_PARENT_TRACE','JJFB_E8N_CF_STATE',
    'JJFB_FAST_ASSIST','JJFB_FAST_SVC_AB','JJFB_FAST_STATE','JJFB_FAST_CASE156_R1',
    'JJFB_FAST_SEQUENCE','JJFB_FAST_EEC7C','JJFB_FAST_DEC30','JJFB_FAST_C6C22',
    'JJFB_FAST_INSN_LIMIT','JJFB_FAST_UNLOCK_CALL','JJFB_FAST_UNLOCK_WHEN'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Invoke-E8RRun([string]$Label, [hashtable]$ExtraEnv, [int]$RunSeconds) {
  Write-Host "== E8R $Label =="
  Clear-E8Modes
  $env:JJFB_E8R_MODE = '1'
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

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$RunSeconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs
  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir "stage_e8r_${Label}_stdout.txt"
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  $vm = Join-Path $logDir 'stage_e_vmrp_stdout.txt'
  if ((Test-Path $dst) -and (Test-Path $vm) -and ((Get-Item $dst).Length -lt 10000)) {
    Copy-Item -Force $vm $dst
  }
  return $dst
}

if (-not $SkipBuild) {
  Get-ChildItem -Path (Join-Path $Root 'build-i686') -Recurse -Filter 'robotol_flag_writer_trace.c.obj' -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}

$fastBase = @{
  JJFB_FAST_ASSIST = '1'
  JJFB_FAST_SVC_AB = 'observe'
  JJFB_FAST_STATE = '38'
  JJFB_FAST_CASE156_R1 = '20'
  JJFB_FAST_SEQUENCE = 'case156'
  JJFB_FAST_C6C22 = '1'
  JJFB_FAST_DEC30 = '1'
  JJFB_FAST_INSN_LIMIT = '2000000'
}

function New-FastExtra([hashtable]$Overlay) {
  $h = @{}
  foreach ($k in $fastBase.Keys) { $h[$k] = $fastBase[$k] }
  foreach ($k in $Overlay.Keys) { $h[$k] = $Overlay[$k] }
  return $h
}

$matrix = @(
  @{ L = 'A_product'; Sec = [Math]::Max($Seconds, 120); Extra = @{} },
  @{ L = 'B_fast_success'; Sec = $Seconds; Extra = (New-FastExtra @{}) },
  @{ L = 'C_10165_success'; Sec = $Seconds; Extra = (New-FastExtra @{
      JJFB_FAST_SEQUENCE = '10165_case156'
      JJFB_E8E_MODE = '1'; JJFB_E8E_EVENT_PROBE = '1'; JJFB_E8E_FE8_WATCH = '1'
      JJFB_E8E_CANDIDATE = 'R0_EVENTCODE_2'; JJFB_E8E_DRAIN_ORDER = 'B'
    })
  },
  @{ L = 'D_310_success'; Sec = $Seconds; Extra = (New-FastExtra @{
      JJFB_FAST_SEQUENCE = 'case310_case156'
    })
  },
  @{ L = 'E_10165_310_success'; Sec = $Seconds; Extra = (New-FastExtra @{
      JJFB_FAST_SEQUENCE = '10165_case310_case156'
      JJFB_E8E_MODE = '1'; JJFB_E8E_EVENT_PROBE = '1'; JJFB_E8E_FE8_WATCH = '1'
      JJFB_E8E_CANDIDATE = 'R0_EVENTCODE_2'; JJFB_E8E_DRAIN_ORDER = 'B'
    })
  },
  @{ L = 'F_unlock_before'; Sec = $Seconds; Extra = (New-FastExtra @{
      JJFB_FAST_UNLOCK_CALL = '1'
      JJFB_FAST_UNLOCK_WHEN = 'before'
    })
  },
  @{ L = 'G_unlock_after'; Sec = $Seconds; Extra = (New-FastExtra @{
      JJFB_FAST_UNLOCK_CALL = '1'
      JJFB_FAST_UNLOCK_WHEN = 'after'
    })
  }
)

$logs = @{}
foreach ($m in $matrix) {
  $logs[$m.L] = Invoke-E8RRun -Label $m.L -ExtraEnv $m.Extra -RunSeconds $m.Sec
  $SkipBuild = $true
}

function Hit([string]$log, [string]$pat) {
  if (-not $log -or -not (Test-Path $log)) { return $false }
  return [bool](Select-String -Path $log -Pattern $pat -Quiet -ErrorAction SilentlyContinue)
}
function Grab([string]$log, [string]$pat) {
  if (-not $log -or -not (Test-Path $log)) { return '' }
  $m = Select-String -Path $log -Pattern $pat | Select-Object -Last 1
  if ($m) { return $m.Line }
  return ''
}

function Analyze-Log([string]$log) {
  # Require PARENT_HIT/ARM/PATH — WRITER_NEVER lines also contain tag=pXXXX (false positive).
  return [pscustomobject]@{
    hitCallerUi = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p2E(4840|48BC|4932|49A8|4A16|4A90|4B06|4788)\b'
    hitCaller2db = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p2DB(9DC|948)\b'
    hitCaller30d = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p30D(DE2|300)\b'
    hitUnlockFn = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p2FC8C0\b|JJFB_E8Q_ARM\] tag=p2FC8C0\b'
    hitUnlock = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p2FC8CE\b|JJFB_E8Q_C44_UNLOCK|JJFB_E8R_C44_UNLOCKED'
    hitHelper = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p3046A8\b'
    hit1848 = Hit $log 'JJFB_E8I_PARENT_HIT\] tag=p301848\b'
    c44t = Hit $log 'JJFB_E8C_FLAG_TRANSITION\].*off=0xC44'
    c44nz = Hit $log 'JJFB_E8C_FLAG_TRANSITION\].*off=0xC44.*new=0x[1-9A-Fa-f]|JJFB_E8R_C44_UNLOCKED|C44_after=0x1'
    c9d = Hit $log 'JJFB_E8C_FLAG_TRANSITION\].*off=0xC9D'
    cf5 = Hit $log 'JJFB_E8C_FLAG_TRANSITION\].*off=0xCF5'
    draw = Hit $log '\[JJFB_DRAW\]'
    svc = Hit $log 'JJFB_FAST_SVC_AB\]'
    unlockDone = Grab $log 'JJFB_FAST_UNLOCK_DONE\]'
    unlockPath = Grab $log 'JJFB_E8R_UNLOCK_PATH\] tag=p2FC8'
    snap = Grab $log 'JJFB_E8F_FLAG_SNAP\] reason=after_fast_unlock|JJFB_E8F_FLAG_SNAP\] reason=after_sibling'
    fire = Grab $log 'JJFB_FAST_FIRE_DONE\]'
  }
}

$results = @{}
foreach ($k in $logs.Keys) { $results[$k] = Analyze-Log $logs[$k] }

$best = 'C44_UNLOCK_CALLER_NEVER_REACHED'
$anyDraw = $false; $anyC44nz = $false; $anyUnlock = $false
$anyCaller = $false; $anyUi = $false; $anySvc = $false; $anyC9dCf5 = $false
foreach ($k in $results.Keys) {
  $r = $results[$k]
  if ($r.draw) { $anyDraw = $true }
  if ($r.c44nz -or ($r.c44t -and $r.hitUnlock)) { $anyC44nz = $true }
  if ($r.hitUnlock -or $r.hitUnlockFn) { $anyUnlock = $true }
  if ($r.hitCallerUi -or $r.hitCaller2db -or $r.hitCaller30d) { $anyCaller = $true }
  if ($r.hitCallerUi) { $anyUi = $true }
  if ($r.svc) { $anySvc = $true }
  if ($r.c9d -or $r.cf5) { $anyC9dCf5 = $true }
}
if ($anyDraw) { $best = 'FAST_REACHED_DRAW' }
elseif ($anyC9dCf5 -and $anyC44nz) { $best = 'FAST_REACHED_C9D_CF5_GATE_NEXT_GAP' }
elseif ($anyC44nz) { $best = 'FAST_REACHED_C44_TRANSITION_NEXT_GAP' }
elseif ($anyUnlock -and -not $anyCaller) { $best = 'C44_UNLOCK_REQUIRES_UI_INIT_CLUSTER' } # unlock via FAST call only
elseif ($anyUi) { $best = 'C44_UNLOCK_REQUIRES_UI_INIT_CLUSTER' }
elseif ($anyCaller -and -not $anyUnlock) { $best = 'C44_UNLOCK_CALLER_BRANCH_UNMET' }
elseif ($anyCaller) { $best = 'C44_UNLOCK_CALLER_FOUND_NEXT_GAP' }
elseif ($anySvc) { $best = 'FAST_REACHED_SVC_AB_NEXT_GAP' }
elseif ((Hit $logs['B_fast_success'] 'tag=p301848') -and -not $anyUnlock) {
  $best = 'PRODUCT_BACKFILL_STATE_OBJECT_UNLOCK_REQUIRED'
}

# Refine: if F/G unlocked via FAST call
$f = $results['F_unlock_before']; $g = $results['G_unlock_after']
if (($f -and $f.c44nz) -or ($g -and $g.c44nz)) {
  if ($anyDraw) { $best = 'FAST_REACHED_DRAW' }
  elseif ($anyC9dCf5) { $best = 'FAST_REACHED_C9D_CF5_GATE_NEXT_GAP' }
  else { $best = 'FAST_REACHED_C44_TRANSITION_NEXT_GAP' }
}
# Natural caller never in A-E but static says UI cluster
if (-not $anyCaller -and -not $anyC44nz) {
  $best = 'C44_UNLOCK_REQUIRES_UI_INIT_CLUSTER'
}

$md = New-Object System.Collections.Generic.List[string]
$md.Add('# Stage E8R-Fast Verdict')
$md.Add('')
$md.Add("**Verdict:** ``$best``")
$md.Add('')
$md.Add('**NOT product success.** Unlock via ``JJFB_FAST_UNLOCK_CALL`` is FAST_ASSIST only.')
$md.Add('')
$md.Add('## Static facts')
$md.Add('')
$md.Add('- Unlock fn ``0x2FC8C0``: ``BL 0x3046A8`` then ``STRB #1 @ C44``; clears C44+8/+C; stores helper ret @ C44+4.')
$md.Add('- R0-R3 unused at entry (R9-only); safe for FAST real-fn call (not C44 poke).')
$md.Add('- Callers (9): ``0x2DB9DC``; UI cluster ``0x2E4840..0x2E4B06`` (fn ``0x2E4788``, depends state ``0x8D0``); ``0x30DDE2`` inside ``0x30D300`` (10102 handler).')
$md.Add('- UI cluster class: ``APP_INIT_UI_CLUSTER``; primary target for natural unlock.')
$md.Add('')
$md.Add('## Matrix')
$md.Add('')
$md.Add('| Run | UI caller | 2DB9DC | 30DDE2 | 2FC8C0 | 2FC8CE | C44nz | C9D/CF5 | DRAW | SVC |')
$md.Add('| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |')
$order = @('A_product','B_fast_success','C_10165_success','D_310_success','E_10165_310_success','F_unlock_before','G_unlock_after')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("| $name | $($r.hitCallerUi) | $($r.hitCaller2db) | $($r.hitCaller30d) | $($r.hitUnlockFn) | $($r.hitUnlock) | $($r.c44nz) | $($r.c9d -or $r.cf5) | $($r.draw) | $($r.svc) |")
}
$md.Add('')
$md.Add('## Evidence')
$md.Add('')
foreach ($name in $order) {
  if (-not $results.ContainsKey($name)) { continue }
  $r = $results[$name]
  $md.Add("### $name")
  if ($r.unlockDone) { $md.Add("- ``$($r.unlockDone)``") }
  if ($r.unlockPath) { $md.Add("- ``$($r.unlockPath)``") }
  if ($r.snap) { $md.Add("- ``$($r.snap)``") }
  if ($r.fire) { $md.Add("- ``$($r.fire)``") }
  $md.Add('')
}
$md.Add('## Interpretation')
$md.Add('')
$md.Add('- Success arm ``0x301848/0x304558`` remains off the unlock path.')
$md.Add('- Natural next gap: enter UI-init ``0x2E4788`` / upstream ``0x2E2F50``..``0x2E3F7C`` (state ``0x8D0`` dependent).')
$md.Add('- Alternate: case arm inside ``0x30D300`` reaching ``0x30DDE2``.')
$md.Add('- Do not prioritize SVC.')
$md.Add('')
$md.Add('## Clean backfill')
$md.Add('')
$md.Add('- Naturalize case156 + state=38 + C6C22 + DEC30 + UI-init unlock caller.')
$md.Add('- Do not force C44/C9D/CF5; do not treat FAST unlock as product success.')
$md.Add('')
$md.Add('## Artifacts')
$md.Add('')
$md.Add('- ``out/e8r_tmp/e8r_deps.md``')
$md.Add('- ``logs/stage_e8r_*_stdout.txt``')
$md.Add('- ``RUN_E8R_FAST.ps1``')
$md.Add('')

$reportPath = Join-Path $reportDir 'stage_e8r_fast_verdict.md'
[System.IO.File]::WriteAllText($reportPath, (($md -join "`n") + "`n"), [System.Text.UTF8Encoding]::new($false))
Write-Host "Verdict=$best -> $reportPath"
Write-Host "E8R complete (NOT product success)."
