$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
python .\scripts\pc_direct_replace_v4.py restore
