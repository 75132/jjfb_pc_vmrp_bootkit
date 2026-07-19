$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
python .\scripts\direct_ext_boot_v26.py restore
