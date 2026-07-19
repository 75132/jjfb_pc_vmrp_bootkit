$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT
python .\scripts\sdk_branch_variants_v21.py restore
