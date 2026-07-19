param([int]$Seconds = 50)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Split-Path -Parent $Root
if (-not (Test-Path (Join-Path $Root 'out\vmrp_run\main.exe'))) {
  $Root = 'c:\Users\24231\Desktop\jjfb_pc_vmrp_bootkit'
}
Set-Location $Root
$MingwBin = 'C:\msys64\mingw32\bin'
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$logDir = Join-Path $Root 'logs'
$stdout = Join-Path $logDir 'fullboot_d2_host_loop_stdout.txt'
$stderr = Join-Path $logDir 'fullboot_d2_host_loop_stderr.txt'

Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 300
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
if (Test-Path $stdout) { Remove-Item -Force $stdout }
if (Test-Path $stderr) { Remove-Item -Force $stderr }

$env:GWY_LAUNCH = '1'
$env:GWY_LAUNCH_TARGET = 'gwy/gbrwcore.mrp'
$env:GWY_LAUNCH_PARAM = 'napptype=12_nextid=482_ncode=512_narg=0_narg1=1_nmrpname=gwy/jjfb.mrp_gwyblink'
$env:GWY_RESOURCE_ROOT = Join-Path $Root 'game_files\mythroad\240x320'
$env:GWY_OVERLAY_ROOT = Join-Path $RunDir 'overlay'
$env:GWY_PROFILE = Join-Path $Root 'profiles\jjfb.json'
$env:JJFB_NATIVE_BOOT_FULL = '1'
$env:JJFB_GWY_LAUNCHER_MODE = '1'
$env:JJFB_LAUNCH_PATH = 'gwy_native_full_shell'
$env:JJFB_SHELL_CHAIN_MODE = 'continue_after_gbrwcore_init'
$env:JJFB_DISABLE_JJFB_ALIAS_DIRECT = '1'
$env:JJFB_MEMBER_VIEW_PRIMARY = 'all_shell_and_game'
$env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
$env:JJFB_EXTCHUNK_PROVIDER = 'shell_and_game'
$env:JJFB_ER_RW_BIND_RESTORE = 'shell_and_game'
$env:JJFB_GWY_UPDATE_STUB = 'no_update_native_branch'
$env:JJFB_RUNAPP_NATIVE_ONLY = '1'
$env:JJFB_PRIMARY_TARGET = 'gwy/jjfb.mrp'
$env:JJFB_EXTCHUNK_SLOT_TRACE = '1'
$env:JJFB_SHELL_NATIVE_EXEC_TRACE = '1'
$env:JJFB_P_TIMELINE_TRACE = '0'
$env:GWY_CONTEXT_WRITE_WATCH = '0'
$env:GWY_P_EXTCHUNK_AUDIT = '0'
$env:JJFB_PUBLICATION_AUDIT = '0'
# Quiet noisy gates from Full Boot runner defaults
$env:GWY_CHUNK_PROVENANCE = '0'
$env:GWY_OBJECT_IDENTITY = '0'
$env:GWY_DISPATCH_TRACE = '0'
$env:GWY_HELPER_HANDOFF = '0'
$env:GWY_DSM_RECORD_CONTRACT = '0'
$env:GWY_MODULE_ENTRY_ABI = '0'
$env:GWY_ENTRY_NULL_CONTRACT = '0'
$env:GWY_MODULE_DATA_INIT = '0'
$env:GWY_MODULE_R9_SWITCH = '0'
$env:GWY_ER_RW_PRODUCER = '0'
$env:GWY_BOOTSTRAP_ABI = '0'
$env:GWY_CALLBACK_FRAME = '0'
$env:GWY_NESTED_R9_SCOPE = '0'
$env:GWY_POST_CONT_AUDIT = '0'
$env:GWY_POST_CFN_R9_AUDIT = '0'
$env:GWY_GWY_STARTGAME_AUDIT = '0'

New-Item -ItemType Directory -Force -Path $env:GWY_OVERLAY_ROOT | Out-Null
Write-Host "Starting main.exe Seconds=$Seconds"
$p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
  -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
Write-Host "pid=$($p.Id)"

$deadline = (Get-Date).AddSeconds([Math]::Max(1, $Seconds))
# Do not stop on first FIRE_DONE — need subsequent ticks toward runapp.
$stopPat = 'JJFB_RUNAPP.*source=native_shell|JJFB_SHELL_EXPORT_CALL|JJFB_MRC_INIT|UC_MEM_READ_UNMAPPED|br_mem_get failed|mythroad exit'
while (-not $p.HasExited -and (Get-Date) -lt $deadline) {
  Start-Sleep -Milliseconds 500
  if ((Test-Path $stdout) -and (Select-String -Path $stdout -Pattern $stopPat -Quiet)) { break }
}
if (-not $p.HasExited) {
  try { Stop-Process -Id $p.Id -Force } catch {}
}
Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Milliseconds 400
Write-Host '==== D2 markers ===='
if (Test-Path $stdout) {
  # PowerShell RedirectStandardOutput is UTF-16; rg/Select-String may need -Encoding unicode
  Select-String -Path $stdout -Encoding unicode -Pattern 'PLATFORM_TIMER|START_DSM_RETURN|bridge_dsm_mr_start_dsm\(|FIRE_|JJFB_RUNAPP|JJFB_SHELL_EXPORT|After app init' |
    ForEach-Object { $_.Line }
} else {
  Write-Host 'missing stdout'
}
Write-Host '==== done ===='
