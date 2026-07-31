# P21: runtime frame isolation command=0 鈫?0x10102 鈫?API builder 鈫?startGame (gate-first)
param(
  [switch]$SkipBuild,
  [int]$GateHold = 300,
  [switch]$Quick
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) { $Root = Split-Path -Parent $Root }
Set-Location $Root
$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path
$Reports = Join-Path $Root 'reports'
$RunDir = Join-Path $Root 'out\vmrp_run'
$ResearchDir = Join-Path $Root 'out\vmrp_research'
$ArchiveRoot = Join-Path $Root 'out\\p21_isolation'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot | Out-Null
if ($Quick) { $GateHold = 50 }

function Clear-Env {
  @(
    'JJFB_BOOTSTRAP_MODE','JJFB_LAUNCH_PATH','JJFB_SHELL_CHAIN_MODE','JJFB_GWY_LAUNCHER_MODE',
    'JJFB_P19_STARTGAME_CONTRACT','JJFB_P20_GBRWCORE_LIFECYCLE','JJFB_P21_RUNTIME_ISOLATION','JJFB_ORIGINAL_LOAD_GAMELIST',
    'JJFB_MEMBER_VIEW_PRIMARY','JJFB_EXTCHUNK_PROVIDER','JJFB_ER_RW_BIND_RESTORE',
    'JJFB_PACKAGE_SCOPED_CLOAD','JJFB_PRODUCT_DESCRIPTOR_DIRECT','JJFB_101AB_PROVIDER','JJFB_101AB_TRACE'
  ) | ForEach-Object { Remove-Item "Env:$_" -ErrorAction SilentlyContinue }
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1')
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'Gwy build failed' }
  Copy-Item -Force (Join-Path $RunDir 'main.exe') (Join-Path $RunDir 'main_product.exe')
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode GwyResearch
  if ($LASTEXITCODE -ne 0) { throw 'GwyResearch build failed' }
  Copy-Item -Force (Join-Path $ResearchDir 'main.exe') (Join-Path $RunDir 'main_research.exe')
  Copy-Item -Force (Join-Path $RunDir 'main_product.exe') (Join-Path $RunDir 'main.exe')
}

function Set-P20Env([string]$RunId) {
  Clear-Env
  $shotDir = Join-Path $RunDir 'screenshots'
  New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
  $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
  $env:GWY_WINDOW_TITLE = 'JJFB Launcher'
  $env:JJFB_LAUNCH_SOURCE = 'jjfb_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:JJFB_BOOTSTRAP_MODE = 'original_headless'
  $env:JJFB_P20_GBRWCORE_LIFECYCLE = '1'
  $env:JJFB_P21_RUNTIME_ISOLATION = '1'
  $env:JJFB_E10A31B_MODE = '1'
  $env:JJFB_P19_STARTGAME_CONTRACT = '1'
  $env:JJFB_ORIGINAL_LOAD_GAMELIST = '1'
  $env:JJFB_LAUNCH_PATH = 'gwy_original_headless'
  $env:JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
  $env:JJFB_GWY_LAUNCHER_MODE = '1'
  $env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
  $env:JJFB_EXTCHUNK_PROVIDER = 'gwy_shell'
  $env:JJFB_ER_RW_BIND_RESTORE = 'shell_core'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_GAME_SELF_PATCH = '0'
  $env:JJFB_DRAWFP_BINDING = '1'
  $env:JJFB_PLATFORM_MRP_RESOURCE = '1'
  $env:JJFB_304BF0_RESUME_MODE = 'direct_lr'
  $env:JJFB_MAP_LOW_GUEST_MEM = '1'
  $env:JJFB_101AB_TRACE = '1'
  $env:JJFB_101AB_PROVIDER = 'synthetic'
  $env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'launcher_first_frame.bmp')
  $env:JJFB_PRODUCT_FFP_MODE = '1'
  $env:JJFB_PRODUCT_FFP_PHASE = 'event'
  $env:JJFB_PRODUCT_EVENT_CONTRACT = '1'
  $env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
  $env:JJFB_PATH_A_EVENT_CONTRACT = '1'
  $env:JJFB_RUNTIME_PROGRESS = '1'
  $env:GWY_SHELL_OFFLINE_NO_UPDATE = '1'
  $env:JJFB_GWY_UPDATE_STUB = 'no_update'
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $Reports
  $env:JJFB_PLATFORM_TIMER_DISPATCH = '1'
  $env:JJFB_TIMER_DELIVER_TRACE = '1'
  $env:JJFB_E10A31_WAIT_FOR_TIMER = '1'
  $env:JJFB_E10A31_WAIT_FIRE_N = '4'
  $env:JJFB_E10A31_WAIT_MS = '120000'
}

function Parse-Gates([string]$Log) {
  $t = if (Test-Path $Log) { Get-Content $Log -Raw } else { '' }
  $sg = '0x0'
  if ($t -match 'P20_SG_PTR\][^\n]*fn=(0x[0-9A-Fa-f]+)') { $sg = $Matches[1] }
  $img = '0x0'
  if ($t -match 'P20_IMAGE_BASE\] image_base=(0x[0-9A-Fa-f]+) map_base=0x2EB7E0 pad=0x1C') { $img = $Matches[1] }
  elseif ($t -match 'image_base=(0x2EB7FC)') { $img = '0x2EB7FC' }
  elseif ($t -match 'P20_IMAGE_BASE\] image_base=(0x[0-9A-Fa-f]+)') { $img = $Matches[1] }
  [pscustomobject]@{
    cmd0     = [int]([bool]($t -match 'MODULE_COMMAND_0_ENTER|P20_HIT\] tag=cmd0_branch'))
    reg10102 = [int]([bool]($t -match 'P20_REGISTER_10102_OK|ACCEPTED_MODULE'))
    callback = [int]([bool]($t -match 'P20_HIT\] tag=event_callback|P20_FIRST_NATURAL_EVENT|GAMELIST_HANDLER_ENTER.*0x30B7E'))
    lazy     = [int]([bool]($t -match 'P20_HIT\] tag=lazy_init'))
    builder  = [int]([bool]($t -match 'P20_HIT\] tag=api_builder'))
    sg_ptr   = [int]([bool]($t -match 'P20_SG_PTR.*fn=0x[1-9A-Fa-f]'))
    sg_enter = [int]([bool]($t -match 'P20_HIT\] tag=sg_entry'))
    op300    = [int]([bool]($t -match 'P20_HIT\] tag=opcode300'))
    nested   = [int]([bool]($t -match 'nested_jjfb=1|P20_FINALIZE.*nested=1'))
    fire_ext = [int]([bool]($t -match 'FIRE_EXT|FIRE_DUE'))
    cont_ok  = [int]([bool]($t -match 'CONTINUE_APPLY.*timer_fire_ext_init_ok|gbrwcore_init_ok'))
    p_iso    = [int]([bool]($t -match 'isolate_p parent_p=|parent_snap=|snapshot_parent_keep_child_va'))
    parent_ok= [int]([bool]($t -match 'PARENT_P_SHA_AFTER_CHILD.*match=yes'))
    cb_r9    = [int]([bool]($t -match 'FAULT_R9_OWNER=GBRWCORE|CALLBACK_R9.*0x2B0D18'))
    no_fault = [int]([bool]($t -notmatch 'fault_pc=0x30D5D2|invalid_address=0xD09C6C91') -and [bool]($t -match 'FIRE_EXT'))
    sg_live  = [int]([bool]($t -match 'P21_STARTGAME_ENTER|P20_HIT\] tag=sg_entry'))
    no_400   = [int]([bool]($t -notmatch 'FETCH_PROT at 0x400|fault_pc=0x400|invalid_address=0x400'))
    gl_erw   = [int]([bool]($t -match 'GAMELIST_ERW_HOST_ISOLATED'))
    image    = $img
    sg_fn    = $sg
    log      = $Log
  }
}

function Write-P21Reports($g) {
  $yn = { param($b) if ($b) { 'YES' } else { 'NO' } }
  # Runtime finalize may already write P21_*. Prefer merging runner gate view.
  $isoMd = Join-Path $Reports 'P21_RUNTIME_FRAME_ISOLATION.md'
  @"
# P21 Runtime Frame Isolation (runner)

## Isolation gates

| Gate | Hit |
|---|---|
| 1 P isolated (log) | $(& $yn $g.p_iso) |
| 2 parent P SHA intact | $(& $yn $g.parent_ok) |
| 3 callback R9 parent | $(& $yn $g.cb_r9) |
| 4 no 0x30D5D2 fault | $(& $yn $g.no_fault) |
| 5/6 startGame live | $(& $yn $g.sg_live) ($($g.sg_fn)) |

## P20 lifecycle (prerequisite)

| Gate | Hit |
|---|---|
| command=0 | $(& $yn $g.cmd0) |
| 0x10102 | $(& $yn $g.reg10102) |
| callback | $(& $yn $g.callback) |
| lazy | $(& $yn $g.lazy) |
| builder | $(& $yn $g.builder) |
| sg_ptr | $(& $yn $g.sg_ptr) |
| fire_ext | $(& $yn $g.fire_ext) |
| continue | $(& $yn $g.cont_ok) |
| opcode300 | $(& $yn $g.op300) |
| nested | $(& $yn $g.nested) |

- image_base: $($g.image)
- log: $($g.log)
- Policy: no forced R9/slot, no shared-P overwrite, no find_by_p(latest) for exec

"@ | Set-Content $isoMd -Encoding UTF8
}

Get-Process main_research,main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$ts = Get-Date -Format 'yyyyMMdd_HHmmss'
$runId = "p21_gate_$ts"
$cell = Join-Path $ArchiveRoot "gate_$ts"
New-Item -ItemType Directory -Force -Path $cell | Out-Null
Set-P20Env $runId
$exe = Join-Path $RunDir 'main_research.exe'
if (-not (Test-Path $exe)) { $exe = Join-Path $RunDir 'main.exe' }
$log = Join-Path $cell 'vm_stdout.txt'
Write-Host "P21 gate hold=${GateHold}s run_id=$runId"
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -RedirectStandardOutput $log -RedirectStandardError (Join-Path $cell 'vm_stderr.txt') -PassThru
Start-Sleep -Seconds $GateHold
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 800
$g = Parse-Gates $log
$g | Format-List | Out-String | Write-Host
$g | ConvertTo-Json | Set-Content (Join-Path $cell 'gates.json')
Write-P21Reports $g
Write-Host "P21 reports written under $Reports"

