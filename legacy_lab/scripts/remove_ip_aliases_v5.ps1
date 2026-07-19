$ErrorActionPreference = "Continue"
$ips = @(
  "111.1.17.148",
  "111.1.17.146",
  "211.155.236.226"
)
$log=@()
foreach ($ip in $ips) {
    $items = Get-NetIPAddress -IPAddress $ip -ErrorAction SilentlyContinue
    foreach ($item in $items) {
        try {
            Remove-NetIPAddress -InterfaceIndex $item.InterfaceIndex -IPAddress $ip -Confirm:$false -ErrorAction Stop
            $log += "removed $ip from ifIndex $($item.InterfaceIndex)"
        } catch {
            $log += "remove failed $ip : $($_.Exception.Message)"
            $out = netsh interface ipv4 delete address name="$($item.InterfaceAlias)" address=$ip 2>&1
            $log += $out
        }
    }
}
$log | Out-File -Encoding utf8 logs\localnet_remove_aliases.txt
$log
