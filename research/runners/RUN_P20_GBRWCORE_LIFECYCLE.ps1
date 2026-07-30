# P20: gbrwcore lifecycle command=0 → 0x10102 → API builder → startGame (gate-first)
param(
  [switch]$SkipBuild,
  [int]$GateHold = 180,
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
$ArchiveRoot = Join-Path $Root 'out\p20_lifecycle'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot | Out-Null
if ($Quick) { $GateHold = 50 }

function Clear-Env {
  @(
    'JJFB_BOOTSTRAP_MODE','JJFB_LAUNCH_PATH','JJFB_SHELL_CHAIN_MODE','JJFB_GWY_LAUNCHER_MODE',
    'JJFB_P19_STARTGAME_CONTRACT','JJFB_P20_GBRWCORE_LIFECYCLE','JJFB_ORIGINAL_LOAD_GAMELIST',
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
    image    = $img
    sg_fn    = $sg
    log      = $Log
  }
}

function Write-P20Reports($g) {
  $yn = { param($b) if ($b) { 'YES' } else { 'NO' } }
  @"
# P20 gbrwcore Module Lifecycle

## Gates

| Gate | Hit |
|---|---|
| 1 command=0 | $(& $yn $g.cmd0) |
| 2 0x10102 register | $(& $yn $g.reg10102) |
| 3 callback enter | $(& $yn $g.callback) |
| 4 lazy init | $(& $yn $g.lazy) |
| 5 API builder | $(& $yn $g.builder) |
| 6 startGame ptr | $(& $yn $g.sg_ptr) ($($g.sg_fn)) |
| 7 startGame entry | $(& $yn $g.sg_enter) |
| 8 opcode 300 | $(& $yn $g.op300) |
| 9 nested jjfb | $(& $yn $g.nested) |

## Notes

- image_base (pad-refined): $($g.image)
- timer FIRE observed: $(& $yn $g.fire_ext)
- Policy: no forced PC/R9, no host callback write, no forged events, no code15/E6C
- cfg36 napptype=12 (live cfg.bin)
- Shortest path: command=0 → 0x10102/0x11100/callback → natural event → lazy init → API builder → startGame

"@ | Set-Content (Join-Path $Reports 'P20_GBRWCORE_LIFECYCLE.md') -Encoding UTF8

  "platform_code,family,callback,owner,ok`n0x10102,0x11100,0x30B7E1,gbrwcore.ext,$($g.reg10102)" |
    Set-Content (Join-Path $Reports 'P20_CALLBACK_REGISTRATION.csv') -Encoding UTF8

  $hits = New-Object System.Collections.Generic.List[string]
  [void]$hits.Add('tag,pc,note')
  if (Test-Path $g.log) {
    Select-String -Path $g.log -Pattern '\[P20_HIT\] tag=(\S+) pc=(0x[0-9A-Fa-f]+)' | ForEach-Object {
      [void]$hits.Add("$($_.Matches[0].Groups[1].Value),$($_.Matches[0].Groups[2].Value),observed")
    }
  }
  $hits -join "`n" | Set-Content (Join-Path $Reports 'P20_API_BUILDER_TRACE.csv') -Encoding UTF8

  @"
# P20 Nested JJFB Result

- nested_jjfb: **$(& $yn $g.nested)**
- startGame fn: ``$($g.sg_fn)``
- opcode300: $(& $yn $g.op300)
- parent code15: not closed (gate9 incomplete)
- first screen vs direct_boot: N/A until gate9
- timer_fire: $(& $yn $g.fire_ext)
"@ | Set-Content (Join-Path $Reports 'P20_NESTED_JJFB_RESULT.md') -Encoding UTF8
}

Get-Process main_research,main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400

$ts = Get-Date -Format 'yyyyMMdd_HHmmss'
$runId = "p20_gate_$ts"
$cell = Join-Path $ArchiveRoot "gate_$ts"
New-Item -ItemType Directory -Force -Path $cell | Out-Null
Set-P20Env $runId
$exe = Join-Path $RunDir 'main_research.exe'
if (-not (Test-Path $exe)) { $exe = Join-Path $RunDir 'main.exe' }
$log = Join-Path $cell 'vm_stdout.txt'
Write-Host "P20 gate hold=${GateHold}s run_id=$runId"
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir -RedirectStandardOutput $log -RedirectStandardError (Join-Path $cell 'vm_stderr.txt') -PassThru
Start-Sleep -Seconds $GateHold
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 800
$g = Parse-Gates $log
$g | Format-List | Out-String | Write-Host
$g | ConvertTo-Json | Set-Content (Join-Path $cell 'gates.json')
Write-P20Reports $g
Write-Host "P20 reports written under $Reports"
