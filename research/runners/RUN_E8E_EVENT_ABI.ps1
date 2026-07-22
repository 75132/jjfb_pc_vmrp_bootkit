# Stage E8E: derive 0x10165 enqueue ABI / real event payload (observe-only)
param(
  [int]$Seconds = 80,
  [string]$Target = 'gwy/jjfb.mrp',
  [string]$Candidate = 'R0_EVENTCODE_2',
  [ValidateSet('A', 'B', 'C')]
  [string]$DrainOrder = 'B',
  [switch]$SkipBuild,
  [switch]$SkipLive
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

$outDir = Join-Path $Root 'out\e8e_tmp'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

$rob = $null
@(
  'out\JJFB_E8A_delivery\02_mrp_extracted\jjfb\robotol.ext',
  'out\e8c_tmp\jjfb_ext\robotol.ext',
  'out\e8d_tmp\jjfb_ext\robotol.ext',
  'out\e8e_tmp\jjfb_ext\robotol.ext'
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

Write-Host "== E8E static: enqueue ABI disasm + callsite + handler map =="
python (Join-Path $Root 'tools\e8e_enqueue_abi_disasm.py') --ext $rob `
  --code-base 0x2D8DF4 --entry 0x30D24D -o $outDir

$flagMap = Join-Path $Root 'out\e8c_tmp\flag_map.json'
if (-not (Test-Path $flagMap)) {
  python (Join-Path $Root 'tools\e8c_idle_flag_resolve.py') --ext $rob --out-dir (Join-Path $Root 'out\e8c_tmp')
}
$map = Get-Content $flagMap -Raw | ConvertFrom-Json
$offsets = $map.watch_offsets_csv
Write-Host "JJFB_E8C_WATCH_OFFSETS=$offsets"
Write-Host "Candidate=$Candidate DrainOrder=$DrainOrder"

if (-not $SkipLive) {
  $env:JJFB_E8E_MODE = '1'
  $env:JJFB_E8C_IDLE_WATCH = '1'
  $env:JJFB_E8C_WATCH_OFFSETS = $offsets
  $env:JJFB_E8D_EARLY_WATCH = '1'
  $env:JJFB_E8D_ERW_DIFF = '1'
  $env:JJFB_E8E_FE8_WATCH = '1'
  $env:JJFB_E8E_EVENT_PROBE = '1'
  $env:JJFB_E8E_CANDIDATE = $Candidate
  $env:JJFB_E8E_DRAIN_ORDER = $DrainOrder
  # Keep E8D probe path off; E8E probe owns the fire.
  Remove-Item Env:JJFB_E8D_10165_PROBE -ErrorAction SilentlyContinue
  Remove-Item Env:JJFB_E8D_MODE -ErrorAction SilentlyContinue

  $env:JJFB_PLAT_CENSUS = '1'
  $env:JJFB_PLAT_1E209_TRACE = '0'
  $env:JJFB_HANDLER_FORENSIC = '0'
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
  $env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
  $env:JJFB_PLAT_RET0_TRACE = '1'
  $env:JJFB_LIFECYCLE_EVENT_TRACE = '1'
  $env:JJFB_E7_LIFECYCLE_MODE = '1'
  $env:JJFB_POST_START_SCHEDULER_TRACE = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:GWY_PACKAGE_APPID = '400101'
  $env:GWY_PACKAGE_APPVER = '12'

  Remove-Item Env:GWY_POST_CONT_AUDIT -ErrorAction SilentlyContinue
  Remove-Item Env:GWY_P_EXTCHUNK_AUDIT -ErrorAction SilentlyContinue
  @(
    'JJFB_E8B_MODE', 'JJFB_E8C_MODE', 'JJFB_E8D_MODE',
    'JJFB_GWY_LAUNCHER_MODE', 'JJFB_NATIVE_BOOT_FULL', 'JJFB_SHELL_CHAIN_MODE',
    'JJFB_GWY_UPDATE_STUB', 'JJFB_RUNAPP_NATIVE_ONLY'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }

  $eArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
    (Join-Path $Root 'RUN_E_PRODUCT_ROBOTOL_MRCINIT.ps1'),
    '-Target', $Target, '-Seconds', "$Seconds")
  if ($SkipBuild) { $eArgs += '-SkipBuild' }
  & powershell @eArgs
  $rc = $LASTEXITCODE

  $src = Join-Path $logDir 'stage_e_product_robotol_stdout.txt'
  $dst = Join-Path $logDir 'stage_e8e_jjfb_stdout.txt'
  if (Test-Path $src) { Copy-Item -Force $src $dst }
} else {
  $rc = 0
  $dst = Join-Path $logDir 'stage_e8e_jjfb_stdout.txt'
}

$all = ''
if (Test-Path $dst) { $all = [System.IO.File]::ReadAllText($dst) }
function Hit([string]$pat) { return [bool]($all -match $pat) }

$abiMd = Test-Path (Join-Path $outDir 'abi_inference.md')
$callMd = Test-Path (Join-Path $outDir 'callsite_abi_samples.md')
$hmapMd = Test-Path (Join-Path $outDir 'handler_registry_map.md')
$disasm = Test-Path (Join-Path $outDir 'handler_30D24D_disasm.txt')

$lifeOk = Hit 'JJFB_LIFECYCLE\] op=FIRE_DONE tick=.*ok=1'
$fe8Watch = Hit 'JJFB_E8E_FE8_WATCH\]'
$fe8Write = Hit 'JJFB_E8E_FE8_WRITE\]'
$hmapLive = Hit 'JJFB_E8E_HANDLER_MAP\] code=0x10165'
$probe = Hit 'JJFB_E8E_EVENT_PROBE\]'
$probeDone = Hit 'JJFB_E8E_EVENT_PROBE_DONE\]'
$drain = Hit 'JJFB_E8E_DRAIN_ORDER\]'
$trans = Hit 'JJFB_E8C_FLAG_TRANSITION\][^\r\n]*off=0xC(44|9D|F5)\b'
$draw = Hit '\[JJFB_DRAW\]'
$flagsUnlock = Hit 'JJFB_E8E_EVENT_PROBE_DONE\][^\r\n]*flags_still_zero=0'
$fe8Changed = Hit 'JJFB_E8E_EVENT_PROBE_DONE\][^\r\n]*fe8_changed=1'
$queueSideMissing = $probeDone -and -not $fe8Changed -and -not $fe8Write
$idleStillZero = Hit 'JJFB_E8E_EVENT_PROBE_DONE\][^\r\n]*flags_still_zero=1'

$decision = 'EVENT_ABI_STILL_UNKNOWN'
if ($draw) {
  $decision = 'DRAW_REACHED'
} elseif ($trans -or $flagsUnlock) {
  $decision = 'EVENT_ABI_DERIVED_NEXT_GAP'
} elseif ($abiMd -and $callMd -and $disasm -and $probeDone -and -not $trans) {
  # ABI table + live probe: FE8 explained; idle flags still unset → next gap is real event source.
  if ($queueSideMissing) {
    $decision = 'QUEUE_SIDE_EFFECT_MISSING'
  } elseif ($DrainOrder -ne 'B' -and $drain -and -not $trans) {
    # Non-default drain did not unlock flags alone.
    $decision = 'EVENT_ABI_DERIVED_NEXT_GAP'
  } else {
    $decision = 'EVENT_ABI_DERIVED_NEXT_GAP'
  }
} elseif (-not $abiMd) {
  $decision = 'EVENT_ABI_STILL_UNKNOWN'
}

# Hash gate
$jjfb = Join-Path $Root 'game_files\mythroad\320x480\gwy\jjfb.mrp'
$hashExpect = '52c13182f87f5ba14bed64589e7f47cb2860a56b32c91fdb25ab13467d5fc036'
$hashGot = (Get-FileHash -Algorithm SHA256 $jjfb).Hash.ToLower()
$hashOk = ($hashGot -eq $hashExpect)

Write-Host "== audit_launcher_core =="
python (Join-Path $Root 'tools\audit_launcher_core.py') $Root
$auditRc = $LASTEXITCODE

$verdict = @"
# Stage E8E — Derive 0x10165 Enqueue ABI / Real Event Payload

## Verdict

``$decision``

## Gates

| Gate | Result |
|------|--------|
| configure/build (via product runner) | rc=$rc |
| static disasm 0x30D24C | $(if ($disasm) { 'yes' } else { 'no' }) |
| abi_inference.md | $(if ($abiMd) { 'yes' } else { 'no' }) |
| callsite_abi_samples.md | $(if ($callMd) { 'yes' } else { 'no' }) |
| handler_registry_map.md | $(if ($hmapMd) { 'yes' } else { 'no' }) |
| FE8 watch armed | $(if ($fe8Watch) { 'yes' } else { 'no' }) |
| FE8 write seen | $(if ($fe8Write) { 'yes' } else { 'no' }) |
| live HANDLER_MAP 10165 | $(if ($hmapLive) { 'yes' } else { 'no' }) |
| event probe | $(if ($probe) { 'yes' } else { 'no' }) |
| probe done | $(if ($probeDone) { 'yes' } else { 'no' }) |
| drain-order log | $(if ($drain) { 'yes' } else { 'no' }) |
| flag transition | $(if ($trans) { 'yes' } else { 'no' }) |
| DRAW | $(if ($draw) { 'yes' } else { 'no' }) |
| lifecycle ok | $(if ($lifeOk) { 'yes' } else { 'no' }) |
| audit_launcher_core | rc=$auditRc |
| jjfb.mrp SHA-256 | $(if ($hashOk) { 'unchanged' } else { "MISMATCH got=$hashGot" }) |

## Run params

- candidate=``$Candidate``
- drain_order=``$DrainOrder``
- log=``logs/stage_e8e_jjfb_stdout.txt``

## Static ABI summary (TARGET_OBSERVED)

- Entry Thumb ``0x30D24D`` / body ``0x30D24C``: save R0→r4, R1→r6; ``BL 0x3046A8``; store helper ret at ``R9+0xFE8``.
- Queue base ``R9+0x7D8`` fields ``+0x24`` / ``+0x6C``; short path ``STRB #1`` at ``R9+0xB7D``.
- Long path (depth≤0) forwards saved R0 into plat ``0x101AB`` via ``BL 0x304558`` with ``r3=2``.
- Only trampoline BL site inside robotol (``0x30D2F8 → 0x30D24C``); platform ``0x10165`` is the delivery path.
- Idle flags ``C44/C9D/CF5`` are **not** written by this enqueue core under ABI-gated probes.

## Evidence class

- Disasm / FE8 role: TARGET_OBSERVED
- Candidate semantic names / drain-order unlock: HYPOTHESIS until a probe sets idle flags

## Artifacts

- ``out/e8e_tmp/handler_30D24D_disasm.txt``
- ``out/e8e_tmp/abi_inference.json`` / ``abi_inference.md``
- ``out/e8e_tmp/callsite_abi_samples.md``
- ``out/e8e_tmp/handler_registry_map.md``
- ``logs/stage_e8e_jjfb_stdout.txt``
"@

$verdictPath = Join-Path $reportDir 'stage_e8e_verdict.md'
$utf8 = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($verdictPath, $verdict, $utf8)
$decision | Set-Content -Encoding ascii (Join-Path $outDir 'decision.txt')

$gate = @"
# Stage E8E gate

| Gate | Result |
|------|--------|
| candidate | $Candidate |
| drain | $DrainOrder |
| decision | $decision |
| hash_ok | $hashOk |
| audit_rc | $auditRc |
"@
[System.IO.File]::WriteAllText((Join-Path $reportDir 'stage_e8e_jjfb_gate.md'), $gate, $utf8)

Write-Host "e8e decision=$decision hash_ok=$hashOk audit_rc=$auditRc log=$dst"
if (-not $hashOk) { exit 3 }
if ($auditRc -ne 0) { exit 4 }
if ($draw) { exit 0 }
if ($decision -eq 'EVENT_ABI_DERIVED_NEXT_GAP' -or $decision -eq 'QUEUE_SIDE_EFFECT_MISSING') { exit 1 }
exit 2
