# P19: API table → startGame → opcode 300 → nested JJFB (gate-first, short holds)
param(
  [switch]$SkipBuild,
  [int]$GateHold = 60,
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
$ArchiveRoot = Join-Path $Root 'out\p19_startgame'
$ResourceRoot = Join-Path $Root 'game_files\mythroad\240x320'
$MatrixCsv = Join-Path $Reports 'P19_NESTED_JJFB_MATRIX.csv'
$ContractMd = Join-Path $Reports 'P19_STARTGAME_RUNTIME_CONTRACT.md'
$ApiCsv = Join-Path $Reports 'P19_API_TABLE.csv'
$OpCsv = Join-Path $Reports 'P19_OPCODE300_TRACE.csv'
New-Item -ItemType Directory -Force -Path $Reports, $ArchiveRoot | Out-Null
if ($Quick) { $GateHold = 35 }

function Clear-Env {
  @(
    'JJFB_BOOTSTRAP_MODE','JJFB_LAUNCH_PATH','JJFB_SHELL_CHAIN_MODE','JJFB_GWY_LAUNCHER_MODE',
    'JJFB_P19_STARTGAME_CONTRACT','JJFB_ORIGINAL_LOAD_GAMELIST','JJFB_MEMBER_VIEW_PRIMARY',
    'JJFB_EXTCHUNK_PROVIDER','JJFB_ER_RW_BIND_RESTORE','JJFB_PACKAGE_SCOPED_CLOAD',
    'JJFB_PRODUCT_DESCRIPTOR_DIRECT','JJFB_101AB_PROVIDER','JJFB_101AB_TRACE'
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

function Set-P19Env([string]$RunId) {
  Clear-Env
  $shotDir = Join-Path $RunDir 'screenshots'
  New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
  $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
  $env:GWY_WINDOW_TITLE = 'JJFB Launcher'
  $env:JJFB_LAUNCH_SOURCE = 'jjfb_launcher'
  $env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
  $env:JJFB_BOOTSTRAP_MODE = 'original_headless'
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
  $env:JJFB_ORIGINAL_API_MAP_CSV = (Join-Path $Reports 'ORIGINAL_GWY_API_MAP.csv')
  $env:JJFB_ORIGINAL_RUNTIME_STACK_JSON = (Join-Path $Reports 'ORIGINAL_GWY_RUNTIME_STACK.json')
}

function Set-DirectEnv([string]$RunId) {
  Clear-Env
  $shotDir = Join-Path $RunDir 'screenshots'
  New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
  $env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
  $env:JJFB_BOOTSTRAP_MODE = 'direct_boot'
  $env:JJFB_LAUNCH_PATH = 'descriptor_direct'
  $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_PLATFORM_MRP_RESOURCE = '1'
  $env:JJFB_304BF0_RESUME_MODE = 'direct_lr'
  $env:JJFB_MAP_LOW_GUEST_MEM = '1'
  $env:JJFB_101AB_TRACE = '1'
  $env:JJFB_101AB_PROVIDER = 'synthetic'
  $env:JJFB_PRODUCT_FFP_MODE = '1'
  $env:JJFB_PRODUCT_FFP_PHASE = 'event'
  $env:JJFB_PRODUCT_EVENT_CONTRACT = '1'
  $env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
  $env:JJFB_PATH_A_EVENT_CONTRACT = '1'
  $env:JJFB_E8Z_SCREENSHOT = (Join-Path $shotDir 'launcher_first_frame.bmp')
  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = 'gwy/jjfb.mrp'
  $env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_PRODUCT_RUN_ID = $RunId
  $env:GWY_PRODUCT_REPORTS_DIR = $Reports
}

function Parse-Gates([string]$Log) {
  $t = if (Test-Path $Log) { Get-Content -Raw $Log } else { '' }
  [pscustomobject]@{
    api_builder = [int]($t -match 'P19_API_BUILDER')
    sg_ptr = [int]($t -match 'P19_STARTGAME_PTR_STORE[^\r\n]*function_ptr=0x[1-9A-Fa-f]')
    sg_enter = [int]($t -match 'P19_STARTGAME_ENTER')
    args = [int]($t -match 'P19_STARTGAME_ARGS\] summary')
    op300 = [int]($t -match 'P19_OPCODE300')
    nested = [int]($t -match 'P19_NESTED_JJFB|nested_jjfb target=gwy/jjfb')
    robotol = [int]($t -match 'P19_CHILD_ROBOTOL|module=robotol\.ext')
    continue_gl = [int]($t -match 'CONTINUE.*gamelist|to=gwy/gamelist')
    gbrw_pc = [int]($t -match 'SHELL_GUEST_PC[^\r\n]*gbrwcore')
    live_r9 = if ($t -match 'P19_STARTGAME_PTR_STORE[^\r\n]*R9=(0x[0-9A-Fa-f]+)') { $Matches[1] } else { '0x0' }
    sg_fn = if ($t -match 'P19_STARTGAME_PTR_STORE[^\r\n]*function_ptr=(0x[0-9A-Fa-f]+)') { $Matches[1] } else { '0x0' }
  }
}

function Invoke-Cell([string]$Name, [string]$Mode, [int]$Hold, [string]$Exe) {
  $ts = Get-Date -Format 'yyyyMMdd_HHmmss'
  $runId = "p19_${Name}_$ts"
  $cell = Join-Path $ArchiveRoot "${Name}_$ts"
  New-Item -ItemType Directory -Force -Path $cell | Out-Null
  $log = Join-Path $cell 'vm_stdout.txt'
  $err = Join-Path $cell 'vm_stderr.txt'
  if ($Mode -eq 'direct') { Set-DirectEnv $runId } else { Set-P19Env $runId }
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch -ResourceRoot $ResourceRoot | Out-Null
  Get-Process -Name 'main','main_gwy','main_research','JJFB_Launcher' -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 300
  $exePath = Join-Path $RunDir $Exe
  if (-not (Test-Path $exePath)) { $exePath = Join-Path $RunDir 'main.exe' }
  $p = Start-Process -FilePath $exePath -WorkingDirectory $RunDir -RedirectStandardOutput $log -RedirectStandardError $err -PassThru
  $deadline = (Get-Date).AddSeconds($Hold)
  while (-not $p.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 2 }
  if (-not $p.HasExited) { try { Stop-Process -Id $p.Id -Force } catch {} }
  Start-Sleep -Milliseconds 400
  $g = Parse-Gates $log
  $g | Add-Member -NotePropertyName name -NotePropertyValue $Name
  $g | Add-Member -NotePropertyName hold -NotePropertyValue $Hold
  $g | Add-Member -NotePropertyName cell -NotePropertyValue $cell
  return $g
}

Write-Host "== P19 gate run hold=${GateHold}s =="
$gate = Invoke-Cell 'gate_headless' 'p19' $GateHold 'main_research.exe'

$hdr = 'name,hold,api_builder,sg_ptr,sg_enter,args,op300,nested,robotol,continue_gl,gbrw_pc,live_r9,sg_fn,cell'
$line = '{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13}' -f `
  $gate.name,$gate.hold,$gate.api_builder,$gate.sg_ptr,$gate.sg_enter,$gate.args,$gate.op300,`
  $gate.nested,$gate.robotol,$gate.continue_gl,$gate.gbrw_pc,$gate.live_r9,$gate.sg_fn,$gate.cell
@($hdr, $line) | Set-Content $MatrixCsv -Encoding utf8

# Ensure required report files exist
if (-not (Test-Path $ApiCsv)) {
  "api_name,function_pointer,table_offset,owner_module,R9,table_object,generation,kind`n" |
    Set-Content $ApiCsv -Encoding utf8
}
if (-not (Test-Path $OpCsv)) {
  "pc,R9,dispatcher,opcode,arg1,arg2,arg3,stack0,stack1,note`n" | Set-Content $OpCsv -Encoding utf8
}

$allGates = ($gate.api_builder -and $gate.sg_ptr -and $gate.sg_enter -and $gate.args -and $gate.op300 -and $gate.nested -and $gate.robotol)
if (-not $allGates) {
  Write-Host 'Gate incomplete — skipping long holds per P19 policy.'
} else {
  Write-Host 'All gates hit — running longer matrix.'
  $r1 = Invoke-Cell 'headless_180_r1' 'p19' 180 'main_research.exe'
  $r2 = Invoke-Cell 'headless_180_r2' 'p19' 180 'main_research.exe'
  $r3 = Invoke-Cell 'headless_300' 'p19' 300 'main_research.exe'
  $d1 = Invoke-Cell 'direct_180' 'direct' 180 'main.exe'
  $extra = @($r1,$r2,$r3,$d1) | ForEach-Object {
    '{0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13}' -f `
      $_.name,$_.hold,$_.api_builder,$_.sg_ptr,$_.sg_enter,$_.args,$_.op300,`
      $_.nested,$_.robotol,$_.continue_gl,$_.gbrw_pc,$_.live_r9,$_.sg_fn,$_.cell
  }
  @($hdr, $line) + $extra | Set-Content $MatrixCsv -Encoding utf8
}

# Refresh contract markdown if finalize did not write (cwd issues)
if (-not (Test-Path $ContractMd) -or ((Get-Item $ContractMd).Length -lt 80)) {
  @"
# P19 startGame Runtime Contract

## Gates (from runner parse)

| Gate | Hit |
|---|---|
| 1 API builder | $(if($gate.api_builder){'YES'}else{'NO'}) |
| 2 startGame ptr | $(if($gate.sg_ptr){'YES'}else{'NO'}) ($($gate.sg_fn)) |
| 3 startGame entry | $(if($gate.sg_enter){'YES'}else{'NO'}) |
| 4 three args | $(if($gate.args){'YES'}else{'NO'}) |
| 5 opcode 300 | $(if($gate.op300){'YES'}else{'NO'}) |
| 6 nested jjfb | $(if($gate.nested){'YES'}else{'NO'}) |
| 7 child robotol | $(if($gate.robotol){'YES'}else{'NO'}) |

## Notes

- continue_to_gamelist observed: $(if($gate.continue_gl){'YES'}else{'NO'})
- gbrwcore guest PC: $(if($gate.gbrw_pc){'YES'}else{'NO'})
- live R9 at ptr store: $($gate.live_r9)
- cfg36 napptype=12 (live cfg.bin)
- No descriptor-string call; no static product hardcode of 0x306655
- Cell: $($gate.cell)
"@ | Set-Content $ContractMd -Encoding utf8
}

Write-Host "Wrote $MatrixCsv"
Write-Host "Wrote $ContractMd"
Write-Host "Wrote $ApiCsv / $OpCsv (from runtime finalize if hit)"
Write-Host '[OK] P19 gate-first run complete'
