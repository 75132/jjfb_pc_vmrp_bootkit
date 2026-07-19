$ErrorActionPreference = "Continue"
$ports = @(21002,21003,20000,6009)
$log=@()
foreach ($p in $ports) {
    $conns = Get-NetTCPConnection -LocalPort $p -State Listen -ErrorAction SilentlyContinue
    foreach ($c in $conns) {
        $pid = $c.OwningProcess
        try {
            $proc = Get-Process -Id $pid -ErrorAction Stop
            $log += "port $p owned by PID $pid $($proc.ProcessName)"
            if ($proc.ProcessName -match "python|python3|py") {
                Stop-Process -Id $pid -Force
                $log += "stopped PID $pid"
            } else {
                $log += "skip non-python PID $pid"
            }
        } catch {
            $log += "failed checking/stopping PID $pid : $($_.Exception.Message)"
        }
    }
}
$log | Out-File -Encoding utf8 logs\localnet_stop_ports.txt
$log
