$ErrorActionPreference = "Stop"
$ROOT = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $ROOT

$py = ".\scripts\binary_patch_vmrp_param_v12.py"
if (!(Test-Path $py)) {
    Write-Host "ERROR: not found $py"
    Write-Host "Please run this inside jjfb_pc_vmrp_bootkit root."
    exit 1
}

$text = Get-Content $py -Raw -Encoding UTF8

$old = 'vs, va, rawsz, rawptr, _, _, _, _, _, chars = struct.unpack_from("<IIIIIIHHI", data, o+8)'
$new = 'vs, va, rawsz, rawptr, _, _, _, _, chars = struct.unpack_from("<IIIIIIHHI", data, o+8)'

if ($text.Contains($old)) {
    $text = $text.Replace($old, $new)
    Set-Content $py -Value $text -Encoding UTF8
    Write-Host "Fixed parse_pe unpack bug in $py"
} elseif ($text.Contains($new)) {
    Write-Host "Already fixed."
} else {
    Write-Host "WARNING: target line not found. Showing matching unpack lines:"
    Select-String -Path $py -Pattern "struct.unpack_from" | ForEach-Object { Write-Host $_.Line }
}

Write-Host ""
Write-Host "Now rerun:"
Write-Host "powershell -ExecutionPolicy Bypass -File .\RUN_PC_BINARY_PATCH_AND_BOOT_V12.ps1"
