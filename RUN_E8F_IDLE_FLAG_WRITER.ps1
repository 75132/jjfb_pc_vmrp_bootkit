# Stage E8F: idle flag writer callpath / upstream condition tracing
param(
  [int]$Seconds = 80,
  [string]$Target = 'gwy/jjfb.mrp',
  [ValidateSet('NONE', 'C44', 'C9D', 'CF5', 'ALL')]
  [string]$Counterfactual = 'NONE',
  [ValidateSet('10162', '10102', 'BOTH', 'NONE')]
  [string]$Sibling = 'BOTH',
  [switch]$SkipBuild,
  [switch]$SkipLive
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\e8f_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

$rob = $null
@(
  'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext',
  'out\e8e_tmp\jjfb_ext\robotol.ext',
  'out\e8f_tmp\jjfb_ext\robotol.ext'
) | ForEach-Object {
  $p = Join-Path $Root $_
  if (-not $rob -and (Test-Path $p)) { $rob = $p }
}
if (-not $rob) {
  python (Join-Path $Root 'tools\mrp_inspect.py') `
    (Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp') `
    --extract (Join-Path $outDir 'jjfb_ext') | Out-Null
  $rob = Join-Path $outDir 'jjfb_ext\robotol.ext'
}

Write-Host "== E8F static writer callpath =="
python (Join-Path $Root 'tools\e8f_writer_callpath.py') --ext $rob -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$offsets = (Get-Content $flagMap -Raw | ConvertFrom-Json).watch_offsets_csv
$bpCsv = Get-Content (Join-Path $outDir 'writer_bp_csv.txt') -Raw
$bpCsv = $bpCsv.Trim()

Write-Host "Sibling=$Sibling Counterfactual=$Counterfactual"

if (-not $SkipLive) {
  $env:JJFB_E8F_MODE = '1'
  $env:JJFB_E8C_IDLE_WATCH = '1'
  $env:JJFB_E8C_WATCH_OFFSETS = $offsets
  $env:JJFB_E8D_EARLY_WATCH = '1'
  $env:JJFB_E8D_ERW_DIFF = '1'
  $env:JJFB_E8E_FE8_WATCH = '1'
  $env:JJFB_E8F_WRITER_BP = '1'
  $env:JJFB_E8F_WRITER_PCS = $bpCsv
  $env:JJFB_E8F_LONGPATH_WATCH = '1'
  if ($Sibling -ne 'NONE') {
    $env:JJFB_E8F_SIBLING_PROBE = '1'
    $env:JJFB_E8F_SIBLING = $Sibling
  } else {
    Remove-Item Env:JJFB_E8F_SIBLING_PROBE -ErrorAction SilentlyContinue
  }
  if ($Counterfactual -ne 'NONE') {
    $env:JJFB_E8F_COUNTERFACTUAL = $Counterfactual
  } else {
    Remove-Item Env:JJFB_E8F_COUNTERFACTUAL -ErrorAction SilentlyContinue
  }
  # No 10165 event spray this stage.
  Remove-Item Env:JJFB_E8E_EVENT_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8D_10165_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8E_MODE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8D_MODE -ErrorAction SilentlyContinue

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

  @(
    'JJFB_E8B_MODE', 'JJFB_E8C_MODE', 'JJFB_E8D_MODE', 'JJFB_E8E_MODE',
    'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs
  $rc = $LASTEXITCODE

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir 'stage_e8f_jjfb_stdout.txt'
  if ($Counterfactual -ne 'NONE') {
    $dst = Join-Path $logDir ("stage_e8f_cf_{0}_stdout.txt" -f $Counterfactual.ToLower())
  }
  if (Test-Path $src) { Copy-Item -Force $src $dst }
  # Always keep primary name for NONE run
  if ($Counterfactual -eq 'NONE' -and (Test-Path $src)) {
    Copy-Item -Force $src (Join-Path $logDir 'stage_e8f_jjfb_stdout.txt')
  }
} else {
  $rc = 0
  $dst = Join-Path $logDir 'stage_e8f_jjfb_stdout.txt'
}

$all = ''
if (Test-Path $dst) { $all = [System.IO.File]::ReadAllText($dst) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$xref = Test-Path (Join-Path $outDir 'writer_callpath.md')
$armed = Hit 'JJFB_E8F_WRITER_BP\] armed=1'
$never = Hit 'JJFB_E8F_WRITER_NEVER\]'
$reached = Hit 'JJFB_E8F_WRITER_REACHED\]'
$writerHit = Hit 'JJFB_E8F_WRITER_HIT\]'
$longHit = Hit 'JJFB_E8F_LONGPATH_HIT\]'
$sib62 = Hit 'JJFB_E8F_SIBLING_FIRE\] code=0x10162'
$sib02 = Hit 'JJFB_E8F_SIBLING_FIRE\] code=0x10102'
$sibDone = Hit 'JJFB_E8F_SIBLING_FIRE_DONE\]'
$cf = Hit 'JJFB_E8F_COUNTERFACTUAL\]'
$draw = Hit '\[JJFB_DRAW\]'
$idleTrans = Hit 'JJFB_E8C_FLAG_TRANSITION\][^\r\n]*off=0xC(44|9D|F5)\b'
$lifeOk = Hit 'JJFB_LIFECYCLE\] op=FIRE_DONE tick=.*ok=1'
$summary = Hit 'JJFB_E8F_WRITER_SUMMARY\]'
$flagsStill = Hit 'JJFB_E8F_FLAG_SNAP\][^\r\n]*C44=0x0 C9D=0x0 CF5=0x0'
$cfAllSet = Hit 'JJFB_E8F_FLAG_SNAP\] reason=cf_after_poke[^\r\n]*C44=0x1 C9D=0x1 CF5=0x1'
$queueDepth = if ($all -match 'JJFB_E8F_QUEUE_DEPTH\][^\r\n]*depth=0x([0-9A-Fa-f]+)') { $Matches[1] } else { '?' }

$decision = 'EVENT_CHAIN_STILL_UNKNOWN'
if ($draw) {
  $decision = 'DRAW_REACHED'
} elseif ($cf -and $cfAllSet -and -not $draw) {
  $decision = 'FLAG_GATE_CONFIRMED_NEXT_GAP'
} elseif ($armed -and -not $writerHit -and ($never -or $summary -or $lifeOk)) {
  # Primary: top writers never execute during long run (+ sibling ZERO_ARGS).
  $decision = 'WRITER_FUNCTION_NEVER_ENTERED'
} elseif ($writerHit -and -not $idleTrans -and -not $draw) {
  $decision = 'WRITER_BRANCH_CONDITION_UNMET'
} elseif ($longHit) {
  $decision = 'QUEUE_LONG_PATH_REQUIRED'
} elseif ($sib62 -and $sibDone -and $flagsStill -and $writerHit) {
  $decision = 'MISSING_10162_DELIVERY'
} elseif ($sib02 -and $sibDone -and $flagsStill -and $writerHit) {
  $decision = 'MISSING_10102_INIT'
} elseif ($xref -and $armed -and $lifeOk) {
  $decision = 'WRITER_FUNCTION_NEVER_ENTERED'
}

$hashExpect = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
$jjfb = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'
$hashGot = (Get-FileHash -Algorithm SHA256 $jjfb).Hash.ToLower()
$hashOk = ($hashGot -eq $hashExpect)

Write-Host "== audit_launcher_core =="
python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
$auditRc = $LASTEXITCODE

$verdict = @"
# Stage E8F — Idle Flag Writer Callpath / Upstream Conditions

## Verdict

``$decision``

## Gates

| Gate | Result |
|------|--------|
| static writer_callpath.md | $(if ($xref) { 'yes' } else { 'no' }) |
| writer BP armed | $(if ($armed) { 'yes' } else { 'no' }) |
| writer HIT live | $(if ($writerHit) { 'yes' } else { 'no' }) |
| writer REACHED summary | $(if ($reached) { 'yes' } else { 'no' }) |
| writer NEVER | $(if ($never) { 'yes' } else { 'no' }) |
| longpath HIT | $(if ($longHit) { 'yes' } else { 'no' }) |
| queue_depth (0x844) | 0x$queueDepth |
| sibling 10162 | $(if ($sib62) { 'yes' } else { 'no' }) |
| sibling 10102 | $(if ($sib02) { 'yes' } else { 'no' }) |
| counterfactual | $(if ($cf) { $Counterfactual } else { 'off' }) |
| idle flag transition | $(if ($idleTrans) { 'yes' } else { 'no' }) |
| DRAW | $(if ($draw) { 'yes' } else { 'no' }) |
| lifecycle ok | $(if ($lifeOk) { 'yes' } else { 'no' }) |
| summary dump | $(if ($summary) { 'yes' } else { 'no' }) |
| audit | rc=$auditRc |
| jjfb hash | $(if ($hashOk) { 'unchanged' } else { "MISMATCH $hashGot" }) |

## Run params

- sibling=``$Sibling``
- counterfactual=``$Counterfactual``
- log=``$dst``

## Interpretation notes

- E8E proved 10165 is enqueue/token (FE8/B7D), not C44/C9D/CF5 unlock.
- E8F asks whether top writers ever execute; NEVER_ENTERED means upstream callpath missing.
- Counterfactual is COUNTERFACTUAL_ONLY — not product success.

## Artifacts

- ``out/e8f_tmp/writer_callpath.md`` / ``.json``
- ``out/e8f_tmp/writer_breakpoints.json``
- ``out/e8f_tmp/sibling_handler_disasm.md``
- ``out/e8f_tmp/long_path_101ab.md``
- ``logs/stage_e8f_jjfb_stdout.txt``
"@

$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e8f_verdict.md'), $verdict, $utf8)
$decision | Set-Content -Encoding ascii (Join-Path $outDir 'decision.txt')
Write-Host "e8f decision=$decision hash_ok=$hashOk audit_rc=$auditRc log=$dst"
if (-not $hashOk) { exit 3 }
if ($auditRc -ne 0) { exit 4 }
if ($draw) { exit 0 }
exit 1
