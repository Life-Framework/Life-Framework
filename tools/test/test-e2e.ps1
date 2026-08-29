# tools/test/test-e2e.ps1 - fast server e2e: boot DebugWorld, run the ELTEST suite, report.
#
# Booting the Reforger dedicated server takes ~20-30s before scripts run, so this
# waits up to a hard 90s cap but polls every 2s and kills the server as soon as
# the ELTEST SUMMARY marker appears (or the cap hits). Exit codes:
#   0 = SUITE PASSED (SUMMARY present, failed=0)
#   1 = SUITE FAILED (SUMMARY present, failed>0) or server crashed with errors
#   2 = NO VERDICT (timeout without SUMMARY, missing tooling, instant crash)
#
# Run via: tools\cli test

param(
  [int]$TimeoutSeconds = 90,
  [int]$PollMs = 2000
)

$ErrorActionPreference = "Stop"

$root = "C:\Users\jaspe\Documents\Reforger\Life-Framework-wt"
if (-not (Test-Path (Join-Path $root "addons\LifeFramework\LifeFramework.gproj"))) {
  Write-Host "ERROR addon not found at worktree root: $root"
  exit 2
}

$serverDir = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server"
$exe = Join-Path $serverDir "ArmaReforgerServerDiag.exe"
if (-not (Test-Path -LiteralPath $exe)) {
  Write-Host "ERROR dedicated server not found: $exe"
  exit 2
}

$profile = Join-Path $root "server\profile\test"
if (Test-Path $profile) { Remove-Item $profile -Recurse -Force }
New-Item -ItemType Directory -Force -Path $profile | Out-Null

# addonsDir = repo addon + base game data only (core + data), via a junction so
# the server does NOT auto-load the EPF/EDF paks that live in the server install
# (they fail to compile on Reforger 1.8). Launch cwd = repo root (neutral) so
# ./addons does not resolve to the server install's addons folder.
$gameAddons = Join-Path $root "server\profile\test\game-addons"
if (-not (Test-Path (Join-Path $gameAddons "data"))) {
  New-Item -ItemType Directory -Force -Path $gameAddons | Out-Null
  cmd /c mklink /J "$gameAddons\core" "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\addons\core" 2>&1 | Out-Null
  cmd /c mklink /J "$gameAddons\data" "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\addons\data" 2>&1 | Out-Null
}

$addonsDir = (Join-Path $root "addons") + "," + $gameAddons
$args = @(
  "-server", "Worlds/DebugWorld/DebugWorld.ent",
  "-addonsDir", $addonsDir,
  "-addons", "LifeFramework",
  "-profile", $profile,
  "-maxFPS", "30",
  "-logLevel", "normal",
  "-disableCrashReporter",
  "-noBackend"
)

Write-Host "test-e2e: launching $exe"
Write-Host "test-e2e: args $($args -join ' ')"
$p = Start-Process -FilePath $exe -ArgumentList $args -PassThru -WorkingDirectory $root `
  -RedirectStandardOutput (Join-Path $env:TEMP "lf-e2e-out.txt") `
  -RedirectStandardError  (Join-Path $env:TEMP "lf-e2e-err.txt")

function Get-LogDir {
  Get-ChildItem (Join-Path $profile "logs") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name | Select-Object -Last 1
}

function Read-Console([string]$dir) {
  if (-not $dir) { return "" }
  $c = Join-Path $dir "console.log"
  if (Test-Path $c) { return (Get-Content $c -Raw -ErrorAction SilentlyContinue) }
  return ""
}

$sw = [Diagnostics.Stopwatch]::StartNew()
$lastSummary = $null
$crashLines = @()

while ($sw.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
  Start-Sleep -Milliseconds $PollMs

  $logDir = Get-LogDir
  $console = Read-Console $logDir.FullName

  if ($console -match '\[ELTEST\] SUMMARY tier=(\w+) passed=(\d+) failed=(\d+) total=(\d+)') {
    $lastSummary = [pscustomobject]@{
      Tier = $matches[1]; Passed = [int]$matches[2]; Failed = [int]$matches[3]; Total = [int]$matches[4]
      LogDir = $logDir.FullName
    }
    break
  }

  if ($p.HasExited) {
    if ($console -match 'SCRIPT\s+\(E\)|Can''t compile|Unable to initialize|fatal|EXCEPTION') {
      $crashLines = @($console -split "`n" | Where-Object { $_ -match 'SCRIPT\s+\(E\)|Can''t compile|Unable to initialize|fatal|EXCEPTION' } | Select-Object -First 10)
    }
    break
  }
}

$alive = -not $p.HasExited
if ($alive) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }

$elapsed = [int]$sw.Elapsed.TotalSeconds

# ---- verdict ---------------------------------------------------------------

if ($lastSummary) {
  Write-Host "test-e2e: SUMMARY tier=$($lastSummary.Tier) passed=$($lastSummary.Passed) failed=$($lastSummary.Failed) total=$($lastSummary.Total) ($elapsed s)"
  Write-Host "test-e2e: log $($lastSummary.LogDir)"

  $console = Read-Console $lastSummary.LogDir
  $fails = @($console -split "`n" | Where-Object { $_ -match '\[ELTEST\]   - ' })
  foreach ($f in $fails) { Write-Host "  $f" }

  if ($lastSummary.Failed -eq 0) { Write-Host "test-e2e: OK"; exit 0 }
  Write-Host "test-e2e: FAILED ($($lastSummary.Failed) failing test(s))"
  exit 1
}

if ($crashLines.Count -gt 0) {
  Write-Host "test-e2e: server crashed with script errors:"
  foreach ($l in $crashLines) { Write-Host "  $l" }
  Write-Host "test-e2e: FAILED (crash) - log $((Get-LogDir).FullName)"
  exit 1
}

if (-not $alive) {
  Write-Host "test-e2e: server exited early without a SUMMARY verdict"
  exit 2
}

Write-Host "test-e2e: NO VERDICT - timed out after ${TimeoutSeconds}s without an ELTEST SUMMARY (log $((Get-LogDir).FullName))"
exit 2