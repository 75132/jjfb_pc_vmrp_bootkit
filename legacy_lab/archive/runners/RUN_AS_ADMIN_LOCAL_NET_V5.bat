@echo off
setlocal
cd /d "%~dp0"
echo JJFB PC v5 localnet launcher
echo This will request Administrator permission through UAC.
echo.

powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile -ExecutionPolicy Bypass -NoExit -Command cd ''%CD%''; powershell -ExecutionPolicy Bypass -File .\RUN_PC_GWY_LOCAL_NET_V5_ADMIN.ps1'"

echo.
echo If a UAC window appeared, click Yes.
echo The elevated PowerShell window will continue the test.
pause
