# Build clean gwy_launcher as PE32 (i686) and optionally smoke-build upstream vmrp.
param(
  [string]$BuildDir = "build-i686",
  [switch]$UpstreamBaseline,
  [switch]$SkipConfigure
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
if (-not (Test-Path (Join-Path $MingwBin 'cmake.exe'))) {
  throw "MinGW32 cmake not found under $MingwBin. Install mingw-w64-i686-cmake / ninja / gcc."
}

$env:Path = "$MingwBin;" + $env:Path
$Toolchain = Join-Path $Root 'cmake\toolchains\mingw-i686.cmake'

if (-not $SkipConfigure) {
  & cmake.exe -S $Root -B (Join-Path $Root $BuildDir) -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
  if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
}

& cmake.exe --build (Join-Path $Root $BuildDir)
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

$exe = Join-Path $Root "$BuildDir\gwy_launcher.exe"
if (-not (Test-Path $exe)) { throw "missing $exe" }

$dump = & objdump.exe -f $exe 2>&1 | Out-String
Write-Host $dump
if ($dump -notmatch 'pei-i386|elf32-i386|i386') {
  throw "gwy_launcher.exe is not PE32/i386"
}
Write-Host "[OK] gwy_launcher.exe is PE32/i386"

$info = Join-Path $Root "$BuildDir\build-info.json"
if (-not (Test-Path $info)) { throw "missing build-info.json" }
Write-Host "[OK] build-info.json written"

if ($UpstreamBaseline) {
  Write-Host '== upstream plain baseline (weak unbound) =='
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Plain -LauncherBuildDir $BuildDir
  if ($LASTEXITCODE -ne 0) { throw 'upstream plain build failed' }
  $upExe = Join-Path $Root 'out\vmrp_plain\main.exe'
  if (-not (Test-Path $upExe)) { throw "missing $upExe" }
  $upDump = & objdump.exe -f $upExe 2>&1 | Out-String
  Write-Host $upDump
  if ($upDump -notmatch 'pei-i386|elf32-i386|i386') {
    throw "upstream plain main.exe is not PE32/i386"
  }
  Write-Host "[OK] upstream plain baseline main.exe is PE32/i386"
}

Write-Host "[OK] RUN_BUILD complete"
