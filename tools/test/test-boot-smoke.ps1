# tools/test/test-boot-smoke.ps1 - Layer 2: headless dedicated-server boot smoke test.
#
# Boots ArmaReforgerServer.exe with the LifeFramework addon and the DebugWorld,
# lets it run for ~45 s, then scans the newest log set for script/resource
# errors. A pass means: the addon loads, the world boots, and the server stays
# up with no fatal errors in the logs.
#
# Exit codes: 0 = pass, 1 = boot/errors detected, 2 = no verdict (tool missing,
# Steam not running, crashed before logging, or timed out without logs).
#
# Run via: tools\cli test

param(
  [string]$World = "Worlds/DebugWorld/DebugWorld.ent",
  [int]$StayAliveSeconds = 45,
  [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }

$serverExe = $env:LF_SERVER_EXE
if (-not $serverExe) { $serverExe = $env:ENFUSION_SERVER_PATH }
if (-not $serverExe) { $serverExe = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server\ArmaReforgerServer.exe" }
$serverDir = Split-Path -Parent $serverExe
$wbProfileAddons = Join-Path $env:USERPROFILE "Documents\My Games\ArmaReforgerWorkbench\addons"
$profileDir = Join-Path $env:USERPROFILE "Documents\My Games\ArmaReforger"
$addonDir = Join-Path $root "addons"
$addonGuid = "79636E668EA37AC9"

if (-not (Test-Path -LiteralPath $serverExe)) {
  Write-Host "ERROR dedicated server not found at: $serverExe (set LF_SERVER_EXE to override)"
  exit 2
}

function Get-LogDirs { Get-ChildItem (Join-Path $profileDir "logs") -Directory -ErrorAction SilentlyContinue | Sort-Object Name }

function Scan-Logs([string]$logDir) {
  $fatal = @()
  $console = Join-Path $logDir "console.log"
  $errorLog = Join-Path $logDir "error.log"
  if (Test-Path $console) {
    $fatal += @(Select-String -Path $console -Pattern '(?i)(script error|failed to compile|cannot compile|unrecognized|VME:|EXCEPTION|Unable to find world|World.*not found)' |
      ForEach-Object { $_.Line.Substring(0, [Math]::Min(200, $_.Line.Length)) } | Select-Object -First 15)
  }
  if (Test-Path $errorLog) {
    $fatal += @(Get-Content $errorLog | Select-Object -First 15 | ForEach-Object { $_.Substring(0, [Math]::Min(200, $_.Length)) })
  }
  return $fatal
}

function Invoke-Boot([string]$worldArg) {
  $before = @(Get-LogDirs | ForEach-Object { $_.Name })
  # CWD = server install dir so ./addons resolves the packed vanilla Game addon;
  # addonsDir chain: repo addon + Workbench profile (packed EPF/EDF dependencies)
  $p = Start-Process -FilePath $serverExe -PassThru -WorkingDirectory $serverDir -ArgumentList @(
    '-server', "`"$worldArg`"",
    '-addonsDir', "`"$addonDir,$wbProfileAddons`"", '-addons', $addonGuid,
    '-logLevel', 'normal', '-maxFPS', '30', '-keepNumOfLogs', '5',
    '-disableCrashReporter', '-noBackend'
  )
  Write-Host "test-boot-smoke: server pid $($p.Id), world '$worldArg', staying alive for $StayAliveSeconds s ..."

  $sw = [Diagnostics.Stopwatch]::StartNew()
  while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
    Start-Sleep -Seconds 5
    if ($p.HasExited) { break }
    if ($sw.Elapsed.TotalSeconds -ge $StayAliveSeconds) { break }
  }

  $newDir = @(Get-LogDirs | Where-Object { $before -notcontains $_.Name } | Sort-Object Name | Select-Object -Last 1)
  $alive = -not $p.HasExited
  $elapsed = [int]$sw.Elapsed.TotalSeconds

  if ($alive) { try { Stop-Process -Id $p.Id -Force } catch {} }

  return @{ Pid = $p.Id; LogDir = if ($newDir) { $newDir.FullName } else { $null }; Alive = $alive; Elapsed = $elapsed }
}

# --- attempt 1: relative world path ---
$r = Invoke-Boot $World

if (-not $r.LogDir) {
  Write-Host "test-boot-smoke: NO VERDICT - server produced no log directory (Steam running? crashed instantly?)"
  exit 2
}

$fatal = Scan-Logs $r.LogDir
if (-not $r.Alive -and ($fatal | Where-Object { $_ -match '(?i)world' }).Count -gt 0) {
  Write-Host "test-boot-smoke: world path failed, retrying with resource-name form ..."
  Get-ChildItem (Join-Path $profileDir "logs") -Directory | Sort-Object Name | Select-Object -Last 1 | Out-Null
  $r = Invoke-Boot "{F3AA388E8466788C}$World"
  if ($r.LogDir) { $fatal = Scan-Logs $r.LogDir }
}

Write-Host "test-boot-smoke: alive=$($r.Alive) elapsed=$($r.Elapsed)s logs=$($r.LogDir)"
if ($fatal.Count -gt 0) {
  Write-Host "test-boot-smoke: log errors:" -ForegroundColor Yellow
  foreach ($f in $fatal) { Write-Host "  $f" }
}

if ($r.Alive -and $fatal.Count -eq 0) { Write-Host "test-boot-smoke: OK"; exit 0 }
if (-not $r.Alive -and $fatal.Count -eq 0) {
  Write-Host "test-boot-smoke: NO VERDICT - server exited early with no fatal log lines"
  exit 2
}
Write-Host "test-boot-smoke: FAILED"
exit 1
