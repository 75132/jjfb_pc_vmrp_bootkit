$ErrorActionPreference = "Continue"
$ROOT = Split-Path -Parent $PSScriptRoot
Set-Location $ROOT

$ips = @(
  "111.1.17.148",
  "111.1.17.146",
  "211.155.236.226"
)

$log = @()
$cfg = Get-NetIPConfiguration | Where-Object { $_.IPv4DefaultGateway -ne $null } | Select-Object -First 1
if ($null -eq $cfg) {
    $cfg = Get-NetIPConfiguration | Where-Object { $_.NetAdapter.Status -eq "Up" } | Select-Object -First 1
}
if ($null -eq $cfg) {
    $log += "ERROR: no active interface found"
    $log | Out-File -Encoding utf8 logs\localnet_add_aliases.txt
    exit 1
}

$alias = $cfg.InterfaceAlias
$log += "Using interface: $alias"

foreach ($ip in $ips) {
    $exists = Get-NetIPAddress -IPAddress $ip -ErrorAction SilentlyContinue
    if ($exists) {
        $log += "exists: $ip on $($exists.InterfaceAlias)"
        continue
    }
    try {
        New-NetIPAddress -InterfaceAlias $alias -IPAddress $ip -PrefixLength 32 -SkipAsSource $true -ErrorAction Stop | Out-Null
        $log += "New-NetIPAddress added: $ip/32 on $alias"
    } catch {
        $log += "New-NetIPAddress failed for $ip : $($_.Exception.Message)"
        $cmd = "interface ipv4 add address name=`"$alias`" address=$ip mask=255.255.255.255 skipassource=true"
        $log += "Trying netsh $cmd"
        $out = netsh $cmd 2>&1
        $log += $out
    }
}

Get-NetIPAddress | Where-Object { $ips -contains $_.IPAddress } | Format-Table -AutoSize | Out-String | ForEach-Object { $log += $_ }

# Firewall allow current python
try {
    $py = (Get-Command python).Source
    if ($py) {
        $ruleName = "JJFB Local Mock Python"
        if (-not (Get-NetFirewallRule -DisplayName $ruleName -ErrorAction SilentlyContinue)) {
            New-NetFirewallRule -DisplayName $ruleName -Direction Inbound -Program $py -Action Allow | Out-Null
            $log += "firewall rule added for $py"
        } else {
            $log += "firewall rule exists"
        }
    }
} catch {
    $log += "firewall rule failed: $($_.Exception.Message)"
}

$log | Out-File -Encoding utf8 logs\localnet_add_aliases.txt
$log
