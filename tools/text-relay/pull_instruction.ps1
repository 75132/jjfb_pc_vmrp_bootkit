param(
  [int]$N = 1,
  [switch]$Watch,
  [double]$Interval = 3,
  [switch]$RawJson
)
# Pull latest instruction(s) from text-relay for the agent / operator.
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root
$env:PYTHONIOENCODING = 'utf-8'

if ($Watch) {
  python .\relay.py watch -n ([Math]::Max($N, 3)) -i $Interval
  exit $LASTEXITCODE
}

if ($RawJson) {
  python .\relay.py recv -n $N --json
} else {
  python .\relay.py recv -n $N --only-new
}
exit $LASTEXITCODE
