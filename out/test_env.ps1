$ErrorActionPreference='Stop'
$env:JJFB_304BF0_RESUME_MODE='epilogue'
$env:JJFB_304BF0_EPILOGUE_R0='0'
$r = 'C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\out\test_env_result.txt'
$seen = & powershell.exe -NoProfile -Command { param() Write-Output ("CHILD_PWSH_SEES=$env:JJFB_304BF0_RESUME_MODE") }
$native = & 'C:\Users\Administrator\Desktop\jjfb_pc_vmrp_bootkit\build-i686\gwy_launcher.exe' --print-env-test 2>&1
"PARENT_SEES=$env:JJFB_304BF0_RESUME_MODE" | Out-File -FilePath $r -Encoding utf8
"$seen" | Add-Content -FilePath $r
"NATIVE_OUT=$native" | Add-Content -FilePath $r
Write-Host "wrote $r"
