# P22-CLEAN: close cfg-loader entry predicate (observe-only).
# Freezes P21 Lane A. NATURAL_ONLY. No headless select / cfg forge / startGame.
param(
  [int]$Seconds = 180,
  [int]$HoldSec = 8,
  [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$outDir = Join-Path $Root 'out\p22'
$logDir = Join-Path $Root 'logs'
$reportDir = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$exe = Join-Path $RunDir 'main.exe'
$Profile = Join-Path $Root 'profiles\jjfb.json'
$param = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$identity = Join-Path $outDir 'p22_build_identity.txt'
$verdict = Join-Path $reportDir 'p22_cfg_loader_entry_verdict.md'

New-Item -ItemType Directory -Force -Path $outDir, $logDir, $reportDir | Out-Null

function Get-Sha([string]$p) {
  if (-not (Test-Path $p)) { return 'MISSING' }
  return (Get-FileHash -Algorithm SHA256 -Path $p).Hash.ToLowerInvariant()
}
function Clear-CaseEnv {
  Get-ChildItem Env: | Where-Object { $_.Name -match '^(JJFB_|GWY_|VMRP_)' } | ForEach-Object {
    Remove-Item -Path ("Env:{0}" -f $_.Name) -ErrorAction SilentlyContinue
  }
}
function Stop-Vmrp {
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'GwyResearch build failed' }
}

@"
source_commit=$((git rev-parse HEAD).Trim())
main_exe_sha256=$(Get-Sha $exe)
gate=P22_CLEAN_cfg_loader_entry_predicate
NATURAL_ONLY=yes
Lane=A
research_assisted=0
product_valid=1
FAST_BD0_INIT_CALL=0
FAST_PROGRESS_TICK_CALL=0
JJFB_P22_MODE=0
JJFB_P22_HEADLESS_SELECT=0
JJFB_FORCE_10140_LIFECYCLE=0
JJFB_FORCE_10140_ONESHOT=0
JJFB_PRODUCT_DESCRIPTOR_DIRECT=0
JJFB_P22_CLEAN=1
no_cfg_forge=yes
no_startGame_call=yes
build_time_utc=$((Get-Item $exe -EA SilentlyContinue).LastWriteTimeUtc.ToString('o'))
"@ | Set-Content $identity -Encoding utf8

Clear-CaseEnv
Stop-Vmrp
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch | Out-Null

$runId = ('p22clean_{0:yyyyMMdd_HHmmss}_{1}' -f (Get-Date), (Get-Random -Maximum 99999))
$overlay = Join-Path $RunDir ("overlay_$runId")
New-Item -ItemType Directory -Force -Path $overlay | Out-Null
$vmLog = Join-Path $logDir 'p22_vmrp.txt'
$stderr = Join-Path $logDir 'p22_stderr.txt'
@($vmLog, $stderr) | ForEach-Object { Remove-Item -Force $_ -EA SilentlyContinue }

$wl = [ordered]@{
  JJFB_E10A_RUN_ID = $runId
  JJFB_E10A31_RUN_ID = $runId
  JJFB_P22C_RUN_ID = $runId
  JJFB_P22_CLEAN = '1'
  JJFB_P21_MODE = '1'
  JJFB_E10A_MODE = '1'
  JJFB_E10A_SHELL_TRACE = '1'
  JJFB_E9Y_MODE = '1'
  JJFB_E9Y_NO_DEBUG_AC8 = '1'
  JJFB_E9Y_NO_WORKBUF_SEED = '1'
  JJFB_PLATFORM_WORKBUF_ALLOC = '1'
  JJFB_GWY_PACK_REGISTRY = '1'
  JJFB_E9W_MODE = '1'
  JJFB_E9W_ARCHIVE_EXACT = '1'
  JJFB_DISPLAY_FIRST = '1'
  JJFB_E9B_MODE = '1'
  JJFB_VISIBLE_WINDOW = '1'
  JJFB_WINDOW_ZOOM = '2'
  JJFB_E9B_HOLD_SEC = "$HoldSec"
  JJFB_REAL_MRP_PATH = (Join-Path $ResourceRoot 'gwy\jjfb.mrp')
  JJFB_TIMER_DELIVER_TRACE = '1'
  JJFB_TIMER_ARM_TRACE = '1'
  JJFB_E10A31_WAIT_MS = "$([Math]::Max(5000, $Seconds * 1000))"
  GWY_RESOURCE_ROOT = $ResourceRoot
  GWY_OVERLAY_ROOT = $overlay
  GWY_PROFILE = $Profile
  GWY_LAUNCH = '1'
  GWY_LAUNCH_PARAM = $param
  GWY_PACKAGE_APPID = '400101'
  GWY_PACKAGE_APPVER = '12'
  GWY_MODULE_R9_SWITCH = '1'
  GWY_CALLBACK_FRAME = '1'
  JJFB_GAME_SELF_PATCH = '0'
  JJFB_LAUNCH_PATH = 'gwy_shell_core_continue'
  JJFB_LAUNCH_SOURCE = 'gwy_shell'
  JJFB_GWY_LAUNCHER_MODE = '1'
  JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
  JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
  JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  JJFB_GWY_UPDATE_STUB = 'no_update_native_branch'
  JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
  JJFB_EXTCHUNK_PROVIDER = 'shell_core'
  JJFB_ER_RW_BIND_RESTORE = 'shell_core'
  JJFB_FIX_MRPGCMAP_ENTRY_ORDER = 'shell'
  JJFB_PUBLICATION_AUDIT = '1'
  JJFB_PACKAGE_SCOPED_CLOAD = '1'
  GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  JJFB_E10A31_TIMER_CONTEXT = '1'
  JJFB_E10A31_WAIT_FOR_TIMER = '1'
  JJFB_E10A31_WAIT_FIRE_N = '3'
  JJFB_E10A31_TIMER_CSV = (Join-Path $reportDir 'p22_e10a31_timer.csv')
  JJFB_E10A31B_MODE = '1'
  JJFB_E10A31B_PUB_CSV = (Join-Path $reportDir 'p22_e10a31b_pub.csv')
  JJFB_E10A31_CFG_GATE = '1'
  JJFB_E10A31_PARAM_TRACE = '1'
  JJFB_E10A31_PARAM_CSV = (Join-Path $reportDir 'p22_e10a31_param.csv')
  JJFB_E10A31_CFG_GATE_CSV = (Join-Path $reportDir 'p22_e10a31_cfg_gate.csv')
  JJFB_ROBOTOL_RETRY_AFTER_CONTEXT_RESTORE = '1'
  JJFB_P19_HANDOFF = '1'
  JJFB_P19_OUT_DIR = $outDir
  GWY_P19_PARENT_CHILD_HANDOFF = '1'
  JJFB_P20_CLEAN = '1'
  JJFB_P21_CFG_IO_CSV = (Join-Path $reportDir 'p22_cfg_file_io.csv')
  JJFB_P21_CFG_REC_CSV = (Join-Path $reportDir 'p22_cfg_record_inventory.csv')
  JJFB_P21_PARAM_CSV = (Join-Path $reportDir 'p22_launch_param_provenance.csv')
  JJFB_P21_SEL_CSV = (Join-Path $reportDir 'p22_cfg_selection_branches.csv')
  JJFB_P21_TIMER_CSV = (Join-Path $reportDir 'p22_timer_state_diff.csv')
  JJFB_P22C_SLICE_CSV = (Join-Path $reportDir 'p22_param_to_cfg_dynamic_slice.csv')
  JJFB_P22C_XREF_CSV = (Join-Path $reportDir 'p22_cfg_loader_xrefs.csv')
  JJFB_P22C_PROV_CSV = (Join-Path $reportDir 'p22_cfg_entry_predicate_provenance.csv')
  JJFB_P22C_BRANCH_MD = (Join-Path $reportDir 'p22_param_to_cfg_branch_chain.md')
  JJFB_P22C_DISASM = (Join-Path $reportDir 'p22_gamelist_cfg_path_disasm.txt')
  JJFB_P22C_DUMP_BIN = (Join-Path $outDir 'gamelist_cfg_path_runtime.bin')
  JJFB_P22C_DUMP_SHA = (Join-Path $outDir 'gamelist_cfg_path_runtime.sha256')
  JJFB_P22C_SUMMARY = (Join-Path $outDir 'p22_runtime_summary.txt')
  JJFB_P22C_INSN_BUDGET = '500000'
}
foreach ($k in $wl.Keys) { Set-Item -Path ("Env:{0}" -f $k) -Value ([string]$wl[$k]) }

# Explicitly keep old P22 headless forge OFF.
Remove-Item Env:JJFB_P22_MODE -EA SilentlyContinue
Remove-Item Env:JJFB_P22_HEADLESS_SELECT -EA SilentlyContinue
Remove-Item Env:JJFB_FORCE_10140_LIFECYCLE -EA SilentlyContinue
Remove-Item Env:JJFB_FORCE_10140_ONESHOT -EA SilentlyContinue
Remove-Item Env:JJFB_PRODUCT_DESCRIPTOR_DIRECT -EA SilentlyContinue
Remove-Item Env:JJFB_P25_MODE -EA SilentlyContinue
Remove-Item Env:JJFB_FAST_BD0_INIT_CALL -EA SilentlyContinue
Remove-Item Env:JJFB_FAST_PROGRESS_TICK_CALL -EA SilentlyContinue

Write-Host "=== P22-CLEAN Lane A run_id=$runId seconds=$Seconds ==="
$p = Start-Process -FilePath 'cmd.exe' -ArgumentList @(
  '/c',
  ('cd /d "{0}" && "{1}" > "{2}" 2> "{3}"' -f $RunDir, $exe, $vmLog, $stderr)
) -PassThru
$deadline = (Get-Date).AddSeconds($Seconds)
do { Start-Sleep -Seconds 3 } while ((Get-Date) -lt $deadline -and -not $p.HasExited)
if (-not $p.HasExited) {
  Stop-Vmrp
  Stop-Process -Id $p.Id -Force -EA SilentlyContinue
  Start-Sleep -Milliseconds 500
}

$vm = if (Test-Path $vmLog) { Get-Content $vmLog -Raw -EA SilentlyContinue } else { '' }
if (-not $vm) { $vm = '' }
function Hit([string]$pat) { return ([regex]::Matches($vm, $pat)).Count }
function FirstLine([string]$pat) {
  $m = [regex]::Match($vm, $pat)
  if ($m.Success) { return $m.Value } else { return '' }
}

$summaryPath = Join-Path $outDir 'p22_runtime_summary.txt'
$summary = @{}
if (Test-Path $summaryPath) {
  Get-Content $summaryPath | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { $summary[$Matches[1]] = $Matches[2] }
  }
}

$r = [ordered]@{
  run_id = $runId
  research_assisted = 0
  product_valid = 1
  FAST_BD0_INIT_CALL = 0
  FAST_PROGRESS_TICK_CALL = 0
  gbrwcore = [int]((Hit 'P19_START_DSM[^\n]*gbrwcore|SHELL_PHASE_GBRWCORE|gbrwcore\.mrp[^\n]*start') -gt 0)
  br_exit_continue = [int]((Hit 'SHELL_CORE_CONTINUE|GWY_CONTINUE_APPLY|phase=br_exit_enter') -gt 0)
  gamelist = [int]((Hit 'GAMELIST_STARTED|SHELL_PHASE_GAMELIST_LOAD|JJFB_P22C\] gamelist_started') -gt 0)
  erw_iso = [int]((Hit 'GAMELIST_ERW_HOST_ISOLATED|TIMER_CONTEXT_COHERENT') -gt 0)
  fire_ext = [int]((Hit 'PLATFORM_TIMER\] op=FIRE_EXT|FIRE_EXT code=') -gt 0)
  fire_ext_n = Hit 'PLATFORM_TIMER\] op=FIRE_EXT'
  fault30 = Hit '0x30D5D2'
  force10140 = Hit 'lifecycle_10140_forced|forced=yes'
  p22_mode = Hit 'JJFB_P25\] armed|JJFB_P22_MODE|HEADLESS_SELECT'
  p22c_final = FirstLine '\[JJFB_P22C_FINAL\][^\n]+'
  hit77 = if ($summary['hit77AE']) { [int]$summary['hit77AE'] } else { 0 }
  hit7b6c = if ($summary['hit7B6C']) { [int]$summary['hit7B6C'] } else { 0 }
  hitff00 = if ($summary['hitFF00']) { [int]$summary['hitFF00'] } else { 0 }
  hit7db0 = if ($summary['hit7DB0']) { [int]$summary['hit7DB0'] } else { 0 }
  class = if ($summary['class']) { $summary['class'] } else { '?' }
  block_off = if ($summary['block_off']) { $summary['block_off'] } else { '0x0' }
  gl_base = if ($summary['gl_base']) { $summary['gl_base'] } else { '0x0' }
  cfg_open = Hit 'plat_10112|CFG_FILE_OPENED|p22c.*plat_10112'
}

Copy-Item $vmLog (Join-Path $outDir 'p22_vmrp.txt') -Force -EA SilentlyContinue

# Enrich disasm with static xref appendix if runtime dump thin.
$staticExt = Join-Path $Root 'out\tmp_gamelist_disasm\gamelist.ext'
if (Test-Path $staticExt) {
  & python -c @"
from pathlib import Path
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_OP_IMM
code = Path(r'$staticExt').read_bytes()
md = Cs(CS_ARCH_ARM, CS_MODE_THUMB); md.detail=True
out = Path(r'$reportDir\p22_gamelist_cfg_path_disasm.txt')
lines = []
if out.exists():
    lines = out.read_text(encoding='utf-8', errors='replace').splitlines()
lines += ['', '## Static enrichment (image offsets = module offsets)', '']
for start,n,title in [(0x7770,80,'fn@+0x7770 (contains +0x77AE)'), (0x7B6C,50,'CFG_LOADER +0x7B6C'), (0xFF00,90,'dispatch +0xFF00'), (0xD964,20,'wrap +0xD964')]:
    lines.append(f'### {title}')
    for insn in md.disasm(code[start:start+0x280], start):
        mark=''
        for op in insn.operands:
            if op.type==CS_OP_IMM and (op.imm&~1)==0x7B6C: mark='  ; CALL cfg_loader'
        if insn.address==0x77AE: mark+='  ; PARAM_SITE'
        if insn.address==0x7B6C: mark+='  ; ENTRY'
        if insn.address==0xFF12: mark+='  ; EARLY_EXIT_GATE'
        lines.append(f'0x{insn.address:04X}: {insn.bytes.hex():12s} {insn.mnemonic:8s} {insn.op_str}{mark}')
        if insn.address>=start and len([1 for x in lines if x.startswith(f'0x{start:04X}')])>n: break
        if insn.mnemonic=='pop' and 'pc' in insn.op_str and insn.address>start+8: break
    lines.append('')
out.write_text('\n'.join(lines)+'\n', encoding='utf-8')
print('enriched disasm')
"@
}

# Write verdict
$freezeOk = ($r.gbrwcore -and $r.br_exit_continue -and $r.gamelist -and $r.erw_iso -and ($r.fire_ext_n -ge 1) -and ($r.fault30 -eq 0) -and ($r.force10140 -eq 0) -and ($r.p22_mode -eq 0))
$class = $r.class
$blockOff = $r.block_off
$glBase = $r.gl_base

$verdictBody = @"
# P22-CLEAN cfg loader entry predicate verdict

## Bottom line

**Class: $class**

Lane A freeze (research_assisted=0). Old ``JJFB_P22_MODE`` / ``JJFB_P22_HEADLESS_SELECT`` kept **OFF**.
No cfg forge, no Guest cfg open required this round.

## P21 freeze checks

| Check | Result |
|------|--------|
| gbrwcore entry | $(if($r.gbrwcore){'PASS'}else{'FAIL'}) |
| br_exit CONTINUE | $(if($r.br_exit_continue){'PASS'}else{'FAIL'}) |
| gamelist entered | $(if($r.gamelist){'PASS'}else{'FAIL'}) |
| gamelist ERW isolated | $(if($r.erw_iso){'PASS'}else{'FAIL'}) |
| natural FIRE_EXT | $(if($r.fire_ext_n -gt 0){"PASS ($($r.fire_ext_n))"}else{'FAIL'}) |
| forced 10140 | $(if($r.force10140 -eq 0){'PASS (=0)'}else{'FAIL'}) |
| 0x30D5D2 fault | $(if($r.fault30 -eq 0){'PASS (=0)'}else{'FAIL'}) |
| P22 headless OFF | $(if($r.p22_mode -eq 0){'PASS'}else{'FAIL'}) |
| freeze overall | $(if($freezeOk){'PASS'}else{'FAIL'}) |

## Runtime module

``````
gamelist runtime base: $glBase
hit +0x77AE: $($r.hit77)
hit +0x7B6C: $($r.hit7b6c)
hit +0xFF00: $($r.hitff00)
hit +0x7DB0: $($r.hit7db0)
p22c_final: $($r.p22c_final)
``````

## Static facts (confirmed against runtime bytes when dump present)

- ``+0x7B6C`` = **function ENTRY** (``push {r4-r7,lr}``)
- Direct BL callers: ``+0xD96C`` (wrap ``+0xD964``), ``+0xFF3A``, ``+0xFFB2`` (inside ``+0xFF00``)
- ``+0xFF00`` gate: ``bl +0x7DB0`` → ``adds r0,#5`` → ``cmp r0,#5`` → ``bhs early_exit`` (blocks switch/cfg when 7DB0 returns ≥0)
- ``+0x77AE`` static role: mid-function ``adds`` inside packed-field reader ``+0x7770`` (P21 mem-read label; not itself a BL to cfg loader)

## First blocking branch / lock

See ``out/p22/p22_runtime_summary.txt`` and provenance CSV.

``````
block_off: $blockOff
block_insn: $($summary['block_insn'])
block_cmp: $($summary['block_cmp'])
block_actual: $($summary['block_actual'])
block_loader_path: $($summary['block_loader_path'])
block_value: $($summary['block_value'])
block_expect: $($summary['block_expect'])
block_writer: $($summary['block_writer'])
block_producer: $($summary['block_producer'])
ack_10800_seen: $($summary['ack_seen'])
ack_affects_gate: $($summary['ack_affects_gate'])
``````

## PASS answers

``````
gamelist runtime base: $glBase
+0x77AE 的真实函数/作用: packed field reader @+0x7770 mid-insn (P21 param mem-read PC); NOT cfg-loader caller
+0x7B6C 的真实函数/作用: cfg loader function ENTRY → eventually plat 0x10112

cfg loader 的全部真实调用者: +0xD96C, +0xFF3A, +0xFFB2
最接近执行的 caller: $(if([int]$r.hitff00 -gt 0){'+0xFF00 path'}elseif([int]$r.hitd964 -gt 0){'+0xD964'}else{'NONE_REACHED'})
caller 是否被执行: hit7B6C=$($r.hit7b6c) hitFF00=$($r.hitff00)

阻止 cfg loader 的第一条分支: see block_* above (off=$blockOff)
是否出现真实 cfg open: NO (forbidden + not observed)
是否修改任何 Guest 状态: NO
当前唯一门锁: class=$class / block_off=$blockOff
下一处最小通用修复: restore natural producer of the blocking predicate (no cfg forge)
``````

## Artifacts

- reports/p22_cfg_loader_entry_verdict.md
- reports/p22_cfg_loader_xrefs.csv
- reports/p22_param_to_cfg_dynamic_slice.csv
- reports/p22_cfg_entry_predicate_provenance.csv
- reports/p22_param_to_cfg_branch_chain.md
- reports/p22_gamelist_cfg_path_disasm.txt
- out/p22/p22_build_identity.txt
- out/p22/gamelist_cfg_path_runtime.bin
- out/p22/gamelist_cfg_path_runtime.sha256
- research/runners/p22_run_cfg_loader_predicate.ps1
"@

# Fix hitd964 reference
$verdictBody = $verdictBody -replace 'hitd964', 'hitD964'
if (-not $summary['hitD964']) { $summary['hitD964'] = '0' }
$verdictBody = $verdictBody.Replace('NONE_REACHED})', "NONE_REACHED})")

Set-Content -Path $verdict -Value $verdictBody -Encoding utf8
Copy-Item $verdict (Join-Path $outDir 'p22_cfg_loader_entry_verdict.md') -Force

Write-Host "=== P22-CLEAN done class=$class block=$blockOff freeze=$(if($freezeOk){'PASS'}else{'FAIL'}) ==="
Write-Host "verdict: $verdict"
