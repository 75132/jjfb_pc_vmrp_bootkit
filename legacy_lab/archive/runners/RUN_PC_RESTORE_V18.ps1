$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
python .\scripts\sdk_bypass_live_v18.py restore
