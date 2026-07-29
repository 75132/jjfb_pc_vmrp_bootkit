# P10 — compile + run standalone Unicorn epilogue stack-contract proof.
# Builds research/runners/p10_verify_epilogue_stack.c, copies unicorn.dll,
# executes the test, and saves reports/p10_epilogue_stack_verify.txt.
# Non-zero test exit code => FAIL (this script exits non-zero).
param(
  [string]$OutExe = ""
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
  $Root = Split-Path -Parent $Root
}
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$Gcc = Join-Path $MingwBin 'gcc.exe'
if (-not (Test-Path $Gcc)) { throw "missing i686 gcc: $Gcc" }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

function Find-UnicornRoot {
  $candidates = @(
    (Join-Path $Root 'third_party\vmrp_upstream\windows\unicorn-1.0.2-win32'),
    (Join-Path $Root 'third_party\deps\unicorn-1.0.2-win32')
  )
  if ($env:GWY_UNICORN_ROOT) { $candidates = @($env:GWY_UNICORN_ROOT) + $candidates }
  foreach ($c in $candidates) {
    $h = Join-Path $c 'include\unicorn\unicorn.h'
    $lib = Join-Path $c 'unicorn.lib'
    $dll = Join-Path $c 'unicorn.dll'
    if ((Test-Path $h) -and (Test-Path $lib) -and (Test-Path $dll)) {
      return [pscustomobject]@{ Root = $c; Include = (Join-Path $c 'include'); Lib = $lib; Dll = $dll }
    }
  }
  throw 'Unicorn not found (expected unicorn-1.0.2-win32 with include/unicorn/unicorn.h + unicorn.lib + unicorn.dll)'
}

$uc = Find-UnicornRoot
$Src = Join-Path $Root 'research\runners\p10_verify_epilogue_stack.c'
if (-not (Test-Path $Src)) { throw "missing $Src" }

$OutDir = Join-Path $Root 'out\p10_epilogue_verify'
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
if (-not $OutExe) {
  $OutExe = Join-Path $OutDir 'p10_verify_epilogue_stack.exe'
}

$Reports = Join-Path $Root 'reports'
New-Item -ItemType Directory -Force -Path $Reports | Out-Null
$ReportTxt = Join-Path $Reports 'p10_epilogue_stack_verify.txt'

Write-Host "== P10 verify epilogue stack =="
Write-Host "gcc=$Gcc"
Write-Host "unicorn=$($uc.Root)"
Write-Host "src=$Src"
Write-Host "out=$OutExe"

& $Gcc -m32 -O0 -g `
  "-I$($uc.Include)" `
  $Src `
  "-L$($uc.Root)" `
  -lunicorn `
  -o $OutExe
if ($LASTEXITCODE -ne 0) { throw "gcc failed exit=$LASTEXITCODE" }

Copy-Item -Force $uc.Dll (Join-Path (Split-Path -Parent $OutExe) 'unicorn.dll')
# Also drop next to cwd for LoadLibrary fallbacks.
Copy-Item -Force $uc.Dll (Join-Path $OutDir 'unicorn.dll')

Write-Host "== run $OutExe =="
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $OutExe
$psi.WorkingDirectory = (Split-Path -Parent $OutExe)
$psi.RedirectStandardOutput = $true
$psi.RedirectStandardError = $true
$psi.UseShellExecute = $false
$psi.CreateNoWindow = $true
$proc = [System.Diagnostics.Process]::Start($psi)
$stdout = $proc.StandardOutput.ReadToEnd()
$stderr = $proc.StandardError.ReadToEnd()
$proc.WaitForExit()
$exit = $proc.ExitCode

$stamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
$header = @"
# P10 epilogue stack verify
timestamp=$stamp
exe=$OutExe
unicorn_root=$($uc.Root)
exit_code=$exit

"@
($header + $stdout + "`n--- stderr ---`n" + $stderr) | Set-Content -Path $ReportTxt -Encoding utf8
Write-Host $stdout
if ($stderr) { Write-Host $stderr }

if ($exit -ne 0) {
  Write-Host "[FAIL] p10_verify_epilogue_stack exit=$exit — see $ReportTxt"
  exit $exit
}
Write-Host "[PASS] report=$ReportTxt"
exit 0
