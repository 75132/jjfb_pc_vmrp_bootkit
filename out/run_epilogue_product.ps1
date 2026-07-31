$ErrorActionPreference='Stop'
# P10 A/B: epilogue resume mode, R0=0 (direct_lr-equivalent). Env hardcoded so child main.exe inherits it.
$env:JJFB_304BF0_RESUME_MODE='epilogue'
$env:JJFB_304BF0_EPILOGUE_R0='0'
$Root='C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit'
& (Join-Path $Root 'RUN_PRODUCT_DIRECT_JJFB.ps1') -SkipBuild -SkipVmrpBuild -Seconds 150
Write-Host ('[EXIT] {0}' -f $LASTEXITCODE)
