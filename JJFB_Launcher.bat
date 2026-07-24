@echo off
REM Double-click entry for JJFB product launcher.
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0RUN_JJFB_LAUNCHER.ps1" -SkipBuild -SkipVmrpBuild
if errorlevel 1 pause
