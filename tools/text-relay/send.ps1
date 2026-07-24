param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$Text
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

if ($Text -and $Text.Count -gt 0) {
  $joined = ($Text -join ' ')
  python .\relay.py send $joined
  exit $LASTEXITCODE
}

if (-not [Console]::IsInputRedirected) {
  Write-Host '用法:'
  Write-Host '  .\send.ps1 要发送的文本'
  Write-Host '  Get-Content note.txt -Raw | .\send.ps1'
  Write-Host '  python .\relay.py send -f note.txt'
  exit 2
}

$input | python .\relay.py send
exit $LASTEXITCODE
