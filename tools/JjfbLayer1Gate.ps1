# Shared Layer-1 first-frame gate helpers for JJFB product scripts.
# Dot-source from RUN_JJFB_LAUNCHER.ps1 / RUN_PRODUCT_DIRECT_JJFB.ps1.

function Get-JjfbRuntimePid {
  param([string]$RunDir)
  $procJson = Join-Path $RunDir 'runtime_process.json'
  if (-not (Test-Path $procJson)) { return $null }
  try {
    $j = Get-Content $procJson -Raw | ConvertFrom-Json
    return [int]$j.runtime_pid
  } catch {
    # Fallback: regex if JSON has Windows-path escapes.
    $raw = Get-Content $procJson -Raw -ErrorAction SilentlyContinue
    if ($raw -match '"runtime_pid"\s*:\s*(\d+)') { return [int]$Matches[1] }
    return $null
  }
}

function Test-JjfbBmpContent {
  param(
    [Parameter(Mandatory = $true)][string]$Path,
    [int]$ExpectW = 240,
    [int]$ExpectH = 320
  )
  $result = [ordered]@{
    ok           = $false
    reason       = ''
    width        = 0
    height       = 0
    bytes        = 0
    sha256       = ''
    nonBlackPct  = 0.0
    uniqueColors = 0
    variance     = 0.0
  }
  if (-not (Test-Path $Path)) {
    $result.reason = 'missing'
    return [pscustomobject]$result
  }
  $bytes = [System.IO.File]::ReadAllBytes($Path)
  $result.bytes = $bytes.Length
  $sha = [System.Security.Cryptography.SHA256]::Create()
  try {
    $result.sha256 = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant()
  } finally { $sha.Dispose() }

  # Known blank / host-pattern hashes (extend as observed).
  $forbidden = @(
    '0000000000000000000000000000000000000000000000000000000000000000'
  )
  if ($forbidden -contains $result.sha256) {
    $result.reason = 'forbidden_sha'
    return [pscustomobject]$result
  }

  if ($bytes.Length -lt 54 -or $bytes[0] -ne 0x42 -or $bytes[1] -ne 0x4D) {
    $result.reason = 'not_bmp'
    return [pscustomobject]$result
  }
  $w = [BitConverter]::ToInt32($bytes, 18)
  $h = [Math]::Abs([BitConverter]::ToInt32($bytes, 22))
  $bpp = [BitConverter]::ToInt16($bytes, 28)
  $off = [BitConverter]::ToInt32($bytes, 10)
  $result.width = $w
  $result.height = $h
  # Accept guest 240x320 or present-scale 320x480.
  $dimOk = (($w -eq $ExpectW -and $h -eq $ExpectH) -or ($w -eq 320 -and $h -eq 480) -or ($w -eq 240 -and $h -eq 320))
  if (-not $dimOk) {
    $result.reason = "dim_${w}x${h}"
    return [pscustomobject]$result
  }
  if ($bpp -ne 24 -and $bpp -ne 16 -and $bpp -ne 32) {
    $result.reason = "bpp_$bpp"
    return [pscustomobject]$result
  }

  $rowBytes = [Math]::Ceiling(($w * $bpp / 8.0) / 4.0) * 4
  $sample = New-Object 'System.Collections.Generic.HashSet[int]'
  $nonBlack = 0
  $total = 0
  $sum = 0.0
  $sumSq = 0.0
  $stepY = [Math]::Max(1, [int]($h / 160))
  $stepX = [Math]::Max(1, [int]($w / 160))
  for ($y = 0; $y -lt $h; $y += $stepY) {
    $row = $off + ($y * $rowBytes)
    for ($x = 0; $x -lt $w; $x += $stepX) {
      if ($row + ($x + 1) * ($bpp / 8) -gt $bytes.Length) { break }
      $px = 0
      if ($bpp -eq 24) {
        $i = $row + $x * 3
        $b = $bytes[$i]; $g = $bytes[$i + 1]; $r = $bytes[$i + 2]
        $px = ($r -shl 16) -bor ($g -shl 8) -bor $b
        $lum = (0.299 * $r) + (0.587 * $g) + (0.114 * $b)
      } elseif ($bpp -eq 32) {
        $i = $row + $x * 4
        $b = $bytes[$i]; $g = $bytes[$i + 1]; $r = $bytes[$i + 2]
        $px = ($r -shl 16) -bor ($g -shl 8) -bor $b
        $lum = (0.299 * $r) + (0.587 * $g) + (0.114 * $b)
      } else {
        $i = $row + $x * 2
        $v = $bytes[$i] -bor ($bytes[$i + 1] -shl 8)
        $px = $v
        $lum = $v -band 0xFF
      }
      [void]$sample.Add($px)
      if ($lum -gt 8) { $nonBlack++ }
      $sum += $lum
      $sumSq += $lum * $lum
      $total++
    }
  }
  if ($total -le 0) {
    $result.reason = 'no_pixels'
    return [pscustomobject]$result
  }
  $result.nonBlackPct = [Math]::Round(100.0 * $nonBlack / $total, 2)
  $result.uniqueColors = $sample.Count
  $mean = $sum / $total
  $result.variance = [Math]::Round(($sumSq / $total) - ($mean * $mean), 2)

  if ($result.nonBlackPct -le 1.0) {
    $result.reason = 'mostly_black'
    return [pscustomobject]$result
  }
  if ($result.uniqueColors -le 8) {
    $result.reason = 'low_unique'
    return [pscustomobject]$result
  }
  if ($result.variance -lt 2.0) {
    $result.reason = 'low_variance'
    return [pscustomobject]$result
  }
  $result.ok = $true
  $result.reason = 'ok'
  return [pscustomobject]$result
}

function Test-JjfbLayer1Gate {
  param(
    [Parameter(Mandatory = $true)][string]$RunDir,
    [Parameter(Mandatory = $true)][string]$ArchiveDir,
    [switch]$RequireCatalog,
    [switch]$Require240
  )
  New-Item -ItemType Directory -Force -Path $ArchiveDir | Out-Null
  $progress = Join-Path $RunDir 'runtime_progress.jsonl'
  $shot = Join-Path $RunDir 'screenshots\launcher_first_frame.bmp'
  $manifest = Join-Path $RunDir 'launch_manifest.json'
  $procJson = Join-Path $RunDir 'runtime_process.json'
  $fail = @()

  foreach ($f in @($progress, $shot, $manifest, $procJson)) {
    if (Test-Path $f) {
      Copy-Item $f (Join-Path $ArchiveDir (Split-Path $f -Leaf)) -Force
    }
  }
  if (Test-Path (Join-Path $RunDir 'screenshots')) {
    New-Item -ItemType Directory -Force -Path (Join-Path $ArchiveDir 'screenshots') | Out-Null
    Copy-Item (Join-Path $RunDir 'screenshots\*') (Join-Path $ArchiveDir 'screenshots') -Force -ErrorAction SilentlyContinue
  }

  $progTxt = if (Test-Path $progress) { Get-Content $progress -Raw } else { '' }
  $manifestTxt = if (Test-Path $manifest) { Get-Content $manifest -Raw } else { '' }

  if ($Require240) {
    if ($manifestTxt -notmatch '240x320' -and $progTxt -notmatch '240x320') {
      $fail += 'resource_root_not_240x320'
    }
  }
  if ($RequireCatalog) {
    if ($progTxt -notmatch 'JJFBOL_CATALOG_READY|jjfbol_catalog_ready') {
      $fail += 'catalog_not_ready'
    }
    if ($progTxt -notmatch 'downVersion=1006') {
      $fail += 'downVersion_not_1006'
    }
  }

  $drawOk = [bool]($progTxt -match 'drawfp_first_drawn|DRAW_FP_DRAWN|drawfp_call')
  $frameReached = [bool]($progTxt -match 'FIRST_REAL_FRAME|first_frame|drawfp_first_drawn')
  $guestSrc = [bool]($progTxt -match 'guest_drawfp|drawfp_first_drawn|loadingbar')
  if (-not $drawOk) { $fail += 'no_drawfp' }
  if (-not $frameReached) { $fail += 'no_first_frame_milestone' }
  if (-not $guestSrc) { $fail += 'no_guest_drawfp_source' }

  $bmp = Test-JjfbBmpContent -Path $shot
  if (-not $bmp.ok) { $fail += "bmp_$($bmp.reason)" }

  $pid_ = Get-JjfbRuntimePid -RunDir $RunDir
  $alive = $false
  if ($null -ne $pid_) {
    $alive = [bool](Get-Process -Id $pid_ -ErrorAction SilentlyContinue)
  } else {
    $fail += 'missing_runtime_process_json'
  }
  if (-not $alive) { $fail += 'runtime_pid_not_alive' }

  if ($progTxt -match 'mr_free invalid|MR_FREE_INVALID') { $fail += 'mr_free_invalid' }
  if ($progTxt -match 'P3_FAULT|ALLOC_STORM') { $fail += 'fault_or_storm_flag' }

  $members = @()
  if ($progTxt) {
    [regex]::Matches($progTxt, '"details":"([^"]+\.bmp)"') | ForEach-Object { $members += $_.Groups[1].Value }
  }
  $members = $members | Select-Object -Unique

  $report = [ordered]@{
    pass           = ($fail.Count -eq 0)
    fail_reasons   = $fail
    runtime_pid    = $pid_
    runtime_alive  = $alive
    bmp            = $bmp
    members        = @($members)
    drawfp         = $drawOk
    first_frame    = $frameReached
  }
  ($report | ConvertTo-Json -Depth 6) | Set-Content (Join-Path $ArchiveDir 'layer1_gate.json') -Encoding utf8
  Write-Host "LAYER1 pass=$($report.pass) fails=$($fail -join ',') pid=$pid_ alive=$alive bmp=$($bmp.reason) sha=$($bmp.sha256)"
  return [pscustomobject]$report
}
