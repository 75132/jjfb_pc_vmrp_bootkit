# Multi-game visual/boot smoke against local GWY catalog.
# Primary baseline remains jjfb (cfg36); other titles validate generic launch contract.
param(
  [int]$HoldSeconds = 28,
  [switch]$SkipBuild,
  [switch]$SkipVmrpBuild,
  [string]$Only = ''  # optional: comma list of titles, e.g. 'jjfb,sanguo,tlbb'
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

$MingwBin = if ($env:MSYS2_MINGW32) { Join-Path $env:MSYS2_MINGW32 'bin' } else { 'C:\msys64\mingw32\bin' }
$env:Path = "$MingwBin;C:\msys64\usr\bin;" + $env:Path

$ResourceRoot = Join-Path $Root 'game_files\mythroad\320x480'
$RunDir = Join-Path $Root 'out\vmrp_run'
$exe = Join-Path $RunDir 'main.exe'
$Launcher = Join-Path $Root 'build-i686\gwy_launcher.exe'
$JjfbProfile = Join-Path $Root 'profiles\jjfb.json'
$OutDir = Join-Path $Root 'out\multi_game_smoke'
$Report = Join-Path $Root 'reports\stage_multi_game_visual_smoke.md'

# cfg indices from `gwy_launcher scan` (local 320x480 catalog)
$Games = @(
  @{ Index = 36; Title = 'jjfb';      Target = 'gwy/jjfb.mrp';      UseJjfbProfile = $true }
  @{ Index = 6;  Title = 'sanguo';    Target = 'gwy/sanguo.mrp';    UseJjfbProfile = $false }
  @{ Index = 12; Title = 'spacetime'; Target = 'gwy/spacetime.mrp'; UseJjfbProfile = $false }
  @{ Index = 13; Title = 'tlbb';      Target = 'gwy/tlbb.mrp';      UseJjfbProfile = $false }
  @{ Index = 7;  Title = 'xyol';      Target = 'gwy/xyol.mrp';      UseJjfbProfile = $false }
  @{ Index = 11; Title = 'ajss';      Target = 'gwy/ajss.mrp';      UseJjfbProfile = $false }
  @{ Index = 5;  Title = 'ssjx';      Target = 'gwy/ssjx.mrp';      UseJjfbProfile = $false }
  @{ Index = 18; Title = 'sgmj';      Target = 'gwy/sgmj.mrp';      UseJjfbProfile = $false }
  @{ Index = 3;  Title = 'zsol';      Target = 'gwy/zsol.mrp';      UseJjfbProfile = $false }
)

if ($Only) {
  $want = @($Only.Split(',') | ForEach-Object { $_.Trim().ToLowerInvariant() } | Where-Object { $_ })
  $Games = @($Games | Where-Object { $want -contains $_.Title })
  if ($Games.Count -eq 0) { throw "Only= matched no games: $Only" }
}

if (-not $SkipBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD.ps1') -BuildDir build-i686
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD failed' }
}
if (-not $SkipVmrpBuild) {
  & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_BUILD_VMRP.ps1') -Mode Gwy
  if ($LASTEXITCODE -ne 0) { throw 'RUN_BUILD_VMRP failed' }
}
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'RUN_VMRP_VISUAL.ps1') -SkipBuild -NoLaunch
if ($LASTEXITCODE -ne 0) { throw 'RUN_VMRP_VISUAL -NoLaunch failed' }
if (-not (Test-Path $exe)) { throw "missing $exe" }
if (-not (Test-Path $Launcher)) { throw "missing $Launcher" }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$scanLog = Join-Path $OutDir 'catalog_scan.txt'
& $Launcher scan --root $ResourceRoot | Tee-Object -FilePath $scanLog | Out-Null

function Clear-GwyEnv {
  @(
    'GWY_LAUNCH','GWY_LAUNCH_TARGET','GWY_LAUNCH_PARAM','GWY_PROFILE','GWY_OVERLAY_ROOT',
    'GWY_PRODUCT_RUN_ID','GWY_PRODUCT_REPORTS_DIR','GWY_RUNTIME_PROGRESS_PATH',
    'JJFB_RUNTIME_PROGRESS','JJFB_PRODUCT_DESCRIPTOR_DIRECT','JJFB_DRAWFP_BINDING',
    'JJFB_PLATFORM_MRP_RESOURCE','JJFB_PLATFORM_10134_CONTRACT','JJFB_PACKAGE_SCOPED_CLOAD',
    'JJFB_MEMBER_VIEW_PRIMARY','JJFB_EXTCHUNK_PROVIDER','JJFB_ER_RW_BIND_RESTORE',
    'JJFB_E8Z_SCREENSHOT','JJFB_PRODUCT_FFP_MODE','JJFB_PRODUCT_FFP_APPLY_ABI',
    'JJFB_PRODUCT_EVENT_CONTRACT','JJFB_PATH_A_EVENT_CONTRACT','JJFB_E5_SCHEDULER_MODE',
    'GWY_MODULE_R9_SWITCH','GWY_CALLBACK_FRAME','JJFB_REFRESH_TRACE','JJFB_FB_HASH_TRACE',
    'JJFB_TIMER_TRACE','SDL_VIDEODRIVER'
  ) | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
}

function Get-CfgFields([int]$Index) {
  # Prefer live scan line: [N] napptype=.. nextid=.. ncode=.. target=..
  $line = Select-String -Path $scanLog -Pattern ("^\[{0}\]\s" -f $Index) | Select-Object -First 1
  if (-not $line) { return $null }
  $t = $line.Line
  $mN = [regex]::Match($t, 'napptype=(-?\d+)')
  $mI = [regex]::Match($t, 'nextid=(-?\d+)')
  $mC = [regex]::Match($t, 'ncode=(-?\d+)')
  $mT = [regex]::Match($t, 'target=(\S+)')
  if (-not ($mN.Success -and $mI.Success -and $mC.Success -and $mT.Success)) { return $null }
  return [pscustomobject]@{
    napptype = [int]$mN.Groups[1].Value
    nextid   = [int]$mI.Groups[1].Value
    ncode    = [int]$mC.Groups[1].Value
    target   = $mT.Groups[1].Value
    narg     = 0
    narg1    = 1
  }
}

$results = @()

foreach ($g in $Games) {
  Clear-GwyEnv
  Get-Process -Name main -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
  Start-Sleep -Milliseconds 300

  $cfg = Get-CfgFields -Index $g.Index
  if (-not $cfg) {
    $results += [pscustomobject]@{
      title=$g.Title; index=$g.Index; target=$g.Target; entry=$false; drawfp=$false;
      mrp_res=$false; fault=$false; shot_bytes=0; note='cfg_scan_miss'
    }
    continue
  }

  $runId = ('mg_{0}_{1:yyyyMMdd_HHmmss}' -f $g.Title, (Get-Date))
  $gameDir = Join-Path $OutDir $g.Title
  New-Item -ItemType Directory -Force -Path $gameDir | Out-Null
  $stdout = Join-Path $gameDir 'vmrp_stdout.txt'
  $stderr = Join-Path $gameDir 'vmrp_stderr.txt'
  $progress = Join-Path $gameDir 'runtime_progress.jsonl'
  $shot = Join-Path $gameDir 'first_frame.bmp'
  $overlay = Join-Path $gameDir 'overlay'
  New-Item -ItemType Directory -Force -Path $overlay | Out-Null
  Remove-Item -Force $stdout, $stderr, $progress, $shot -ErrorAction SilentlyContinue

  $param = 'napptype={0}_nextid={1}_ncode={2}_narg={3}_narg1={4}_nmrpname={5}_gwyblink' -f `
    $cfg.napptype, $cfg.nextid, $cfg.ncode, $cfg.narg, $cfg.narg1, $cfg.target

  $env:GWY_LAUNCH = '1'
  $env:GWY_LAUNCH_TARGET = $cfg.target
  $env:GWY_LAUNCH_PARAM = $param
  $env:GWY_RESOURCE_ROOT = $ResourceRoot
  $env:GWY_OVERLAY_ROOT = $overlay
  $env:GWY_PRODUCT_RUN_ID = $runId
  $env:GWY_RUNTIME_PROGRESS_PATH = $progress
  $env:JJFB_RUNTIME_PROGRESS = '1'
  $env:JJFB_DRAWFP_BINDING = '1'
  $env:JJFB_PLATFORM_MRP_RESOURCE = '1'
  $env:JJFB_PLATFORM_10134_CONTRACT = '1'
  $env:JJFB_PACKAGE_SCOPED_CLOAD = '1'
  $env:JJFB_MEMBER_VIEW_PRIMARY = 'game_package'
  $env:JJFB_EXTCHUNK_PROVIDER = 'game_package'
  $env:JJFB_ER_RW_BIND_RESTORE = 'game_package'
  $env:GWY_MODULE_R9_SWITCH = '1'
  $env:GWY_CALLBACK_FRAME = '1'
  $env:JJFB_E5_SCHEDULER_MODE = '1'
  $env:JJFB_E8Z_SCREENSHOT = $shot
  if ($g.UseJjfbProfile) {
    $env:GWY_PROFILE = $JjfbProfile
    $env:JJFB_PRODUCT_DESCRIPTOR_DIRECT = '1'
    $env:JJFB_PRODUCT_FFP_MODE = '1'
    $env:JJFB_PRODUCT_FFP_PHASE = 'event'
    $env:JJFB_PRODUCT_EVENT_CONTRACT = '1'
    $env:JJFB_PRODUCT_FFP_APPLY_ABI = '1'
    $env:JJFB_PATH_A_EVENT_CONTRACT = '1'
  }

  Write-Host ("== {0} index={1} target={2} hold={3}s ==" -f $g.Title, $g.Index, $cfg.target, $HoldSeconds)
  Write-Host "param=$param"

  $p = Start-Process -FilePath $exe -WorkingDirectory $RunDir `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
  Start-Sleep -Seconds $HoldSeconds
  if (-not $p.HasExited) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 400
  }

  $progressTxt = ''
  $stdoutTxt = ''
  if (Test-Path $progress) { $progressTxt = Get-Content $progress -Raw -ErrorAction SilentlyContinue }
  if (Test-Path $stdout) { $stdoutTxt = Get-Content $stdout -Raw -ErrorAction SilentlyContinue }
  if (Test-Path $stderr) { $stdoutTxt += "`n" + (Get-Content $stderr -Raw -ErrorAction SilentlyContinue) }

  $entry = [bool]($progressTxt -match 'guest_entry_called')
  $targetHit = [bool]($stdoutTxt -match [regex]::Escape("canonical=`"$($cfg.target)`"") -or
                       $stdoutTxt -match [regex]::Escape("target=$($cfg.target)"))
  $pkgExt = $false
  if ($g.Title -eq 'jjfb') {
    $pkgExt = [bool]($stdoutTxt -match 'package=gwy/jjfb\.mrp.*robotol\.ext|module=robotol\.ext')
  } else {
    $pkgExt = [bool]($stdoutTxt -match ("package=gwy/{0}\.mrp" -f [regex]::Escape($g.Title)))
  }
  $drawfp = [bool](($progressTxt + $stdoutTxt) -match 'drawfp_first_drawn|DRAW_FP_DRAWN|JJFB_FIRST_REAL_FRAME_REACHED')
  $mrpRes = [bool](($progressTxt + $stdoutTxt) -match 'member_loaded|entry_complete')
  $fault = [bool]($stdoutTxt -match 'UC_FAULT|UNICORN_FAULT|mem_fault|Failed on uc_emu_start')
  $exited = [bool]($stdoutTxt -match 'mr_exit\(\) called')
  $shotBytes = 0
  if (Test-Path $shot) { $shotBytes = (Get-Item $shot).Length }

  $note = if ($drawfp -or $shotBytes -gt 10000) { 'frame' }
          elseif ($entry -and $targetHit -and $exited -and -not $drawfp) { 'target_open_then_exit' }
          elseif ($entry -and $targetHit -and -not $fault) { 'boot_ok_no_frame' }
          elseif ($fault) { 'fault' }
          elseif ($entry) { 'entry' }
          else { 'no_entry' }

  $row = [pscustomobject]@{
    title = $g.Title
    index = $g.Index
    target = $cfg.target
    entry = $entry
    target_hit = $targetHit
    pkg_ext = $pkgExt
    drawfp = $drawfp
    mrp_res = $mrpRes
    fault = $fault
    exited = $exited
    shot_bytes = $shotBytes
    note = $note
  }
  $results += $row
  Write-Host ("result entry={0} target={1} pkg={2} drawfp={3} exit={4} shot={5} note={6}" -f `
    $entry, $targetHit, $pkgExt, $drawfp, $exited, $shotBytes, $note)
}

Clear-GwyEnv

$md = @()
$md += '# Multi-game visual/boot smoke'
$md += ''
$md += "## Verdict"
$md += ''
$jjfb = $results | Where-Object { $_.title -eq 'jjfb' } | Select-Object -First 1
$others = @($results | Where-Object { $_.title -ne 'jjfb' })
$otherFrames = @($others | Where-Object { $_.drawfp -or $_.shot_bytes -gt 10000 }).Count
$otherBoot = @($others | Where-Object { $_.entry }).Count
$md += ("- jjfb baseline: **{0}**" -f $(if ($jjfb -and ($jjfb.drawfp -or $jjfb.shot_bytes -gt 10000)) { 'PASS (frame)' } elseif ($jjfb -and $jjfb.entry) { 'BOOT_ONLY' } else { 'FAIL' }))
$md += ("- other titles: entry={0}/{1}, frame={2}/{1}" -f $otherBoot, $others.Count, $otherFrames)
$md += ("- HoldSeconds={0}" -f $HoldSeconds)
$md += ''
$md += '| Title | cfg | Target | Entry | TargetHit | Pkg | DrawFP | Exit | Shot | Note |'
$md += '|---|---:|---|:---:|:---:|:---:|:---:|:---:|---:|---|'
foreach ($r in $results) {
  $md += ('| {0} | {1} | `{2}` | {3} | {4} | {5} | {6} | {7} | {8} | {9} |' -f `
    $r.title, $r.index, $r.target, $r.entry, $r.target_hit, $r.pkg_ext, $r.drawfp, $r.exited, $r.shot_bytes, $r.note)
}
$md += ''
$md += '## Notes'
$md += ''
$md += '- jjfb uses `profiles/jjfb.json` (robotol alias + Path-A) and is the only title currently expected to DrawFP.'
$md += '- Other titles launch **without** jjfb profile; `GWY_LAUNCH_TARGET` is honored (mr_open of that MRP).'
$md += '- Several non-jjfb titles currently reach target open then `mr_exit` before game EXT/DrawFP (cfg napptype/ncode often 0; need shell/gamelist or per-game profile).'
$md += '- `CROSS_TARGET` robotol/mmochat install noise may appear during VFS warmup; package-scope for the real target still applies.'
$md += ''
$md += 'Evidence dir: `out/multi_game_smoke/<title>/`'
$md -join "`n" | Set-Content -Path $Report -Encoding utf8

Write-Host ''
Write-Host "report=$Report"
$results | Format-Table title,index,entry,target_hit,pkg_ext,drawfp,exited,shot_bytes,note -AutoSize
Write-Host '[OK] multi-game smoke finished'
exit 0
