param(
  [int]$N = 10,
  [switch]$Watch,
  [double]$Interval = 2
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

if ($Watch) {
  python .\relay.py watch -n $N -i $Interval
} else {
  python .\relay.py recv -n $N
}
exit $LASTEXITCODE
