# Restore hosts: removes JJFB PC Bootkit block. Run as Administrator.
$hosts = "$env:windir\System32\drivers\etc\hosts"
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (!$isAdmin) { Write-Host "Run as Administrator"; exit 1 }
$text = Get-Content $hosts -Raw
$text = [regex]::Replace($text, "`r?`n# JJFB PC Bootkit begin[\s\S]*?# JJFB PC Bootkit end`r?`n", "`r`n")
Set-Content -Path $hosts -Value $text -Encoding ascii
Write-Host "Restored hosts."
