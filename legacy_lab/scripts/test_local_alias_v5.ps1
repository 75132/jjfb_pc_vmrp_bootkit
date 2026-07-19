$ErrorActionPreference = "Continue"
$log=@()
$targets = @(
  @{ip="111.1.17.148";port=21002},
  @{ip="211.155.236.226";port=20000}
)
foreach ($t in $targets) {
    $ip=$t.ip; $port=$t.port
    try {
        $r = Test-NetConnection -ComputerName $ip -Port $port -InformationLevel Detailed
        $log += "TEST $ip:$port TcpTestSucceeded=$($r.TcpTestSucceeded) RemoteAddress=$($r.RemoteAddress)"
    } catch {
        $log += "TEST $ip:$port failed: $($_.Exception.Message)"
    }
}
$log | Out-File -Encoding utf8 logs\localnet_route_test.txt
$log
