$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
python .\scripts\skip_loader_variants_v24.py restore
