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
  [int]$PollMs = 2000,
  [string]$Tier = "all",
  [int]$BindPort = 2001,
  [int]$A2sPort = 17777,
  [switch]$KeepProfile,
  [switch]$LoadLatestSave,
  [string]$LoadSessionSave,
  [switch]$GracefulClose,
  [int]$GracefulCloseSeconds = 30,
  [string]$ExtraDefine
)

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 2 }
if (-not (Test-Path (Join-Path $root "addons\LifeFramework\LifeFramework.gproj"))) {
  Write-Host "ERROR addon not found at repo root: $root"
  exit 2
}

$serverInstall = $env:ENFUSION_SERVER_PATH
if (-not $serverInstall) { $serverInstall = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server" }
$exe = Join-Path $serverInstall "ArmaReforgerServerDiag.exe"
if (-not (Test-Path -LiteralPath $exe)) {
  Write-Host "ERROR dedicated server not found: $exe"
  exit 2
}

$profile = Join-Path $root "server\profile\test"
if (-not $KeepProfile) {
  if (Test-Path $profile) { Remove-Item $profile -Recurse -Force }
}
New-Item -ItemType Directory -Force -Path $profile | Out-Null

# addonsDir = repo addon + base game data only (core + data), via a junction so
# the server does NOT auto-load the EPF/EDF paks that live in the server install
# (they fail to compile on Reforger 1.8). Launch cwd = repo root (neutral) so
# ./addons does not resolve to the server install's addons folder.
# The junction sources core+data from the SERVER install, not the game install:
# the server binary is only guaranteed to parse data shipped with it (a game-side
# data hotfix newer than the server binary breaks prefab parsing). When the server
# install is a workshop-only deployment without base game data (empty core/data),
# fall back to the game client install where the data lives (same engine version).
$gameAddons = Join-Path $root "server\profile\test\game-addons"
$gameInstall = $env:ENFUSION_GAME_PATH
if (-not $gameInstall) { $gameInstall = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger" }
$dataSource = Join-Path $serverInstall "addons"
$serverHasData = (Get-ChildItem (Join-Path $dataSource "core") -ErrorAction SilentlyContinue).Count -gt 0 -and
                 (Get-ChildItem (Join-Path $dataSource "data") -ErrorAction SilentlyContinue).Count -gt 0
if (-not $serverHasData) { $dataSource = Join-Path $gameInstall "addons" }
if (-not (Test-Path (Join-Path $gameAddons "data"))) {
  New-Item -ItemType Directory -Force -Path $gameAddons | Out-Null
  cmd /c mklink /J "$gameAddons\core" "$dataSource\core" 2>&1 | Out-Null
  cmd /c mklink /J "$gameAddons\data" "$dataSource\data" 2>&1 | Out-Null
}

# Some current installs ship core as data.pak without a core.gproj. The data
# project depends on core's GUID, so provide the minimal project manifest the
# engine needs when the install did not include one. This is generated under
# the ignored test profile, never committed to the repository.
$coreGproj = Join-Path $gameAddons "core\core.gproj"
if (-not (Test-Path -LiteralPath (Join-Path $dataSource "core\core.gproj"))) {
  if (Test-Path -LiteralPath (Join-Path $gameAddons "core")) { Remove-Item -LiteralPath (Join-Path $gameAddons "core") -Recurse -Force }
  $coreDir = Split-Path -Parent $coreGproj
  New-Item -ItemType Directory -Force -Path $coreDir | Out-Null
  Copy-Item -Path (Join-Path $dataSource "core\*") -Destination $coreDir -Recurse -Force
  @'
GameProject {
 ID "core"
 GUID "5614BBCCBB55ED1C"
 TITLE "core"
 Configurations {
  GameProjectConfig PC {
  }
  GameProjectConfig HEADLESS : PC {
  }
 }
}
'@ | Set-Content -LiteralPath $coreGproj -Encoding ASCII
}

$addonsDir = (Join-Path $root "addons") + "," + $gameAddons
$args = @(
  "-server", "Worlds/DebugWorld/DebugWorld.ent",
  "-worldSystemsConfig", "{2104C177A245B6D1}Configs/Systems/LifeFrameworkSystems.conf",
  "-addonsDir", $addonsDir,
  "-addons", "LifeFramework",
  "-profile", $profile,
  "-maxFPS", "30",
  "-logLevel", "normal",
  "-scrDefine", "EL_AUTOTEST",
  "-disableCrashReporter",
  "-noBackend",
  "-bindPort", $BindPort,
  "-a2sPort", $A2sPort
)

if ($Tier -eq "fast") {
  $args += @("-scrDefine", "EL_TEST_TIER_FAST")
} elseif ($Tier -eq "persistence") {
  $args += @("-scrDefine", "EL_TEST_TIER_PERSISTENCE")
}

if (-not [string]::IsNullOrEmpty($ExtraDefine)) {
  $args += @("-scrDefine", $ExtraDefine)
}

# PS 5.1 binds an unpassed [string] param to "" not $null, so "provided" must be
# detected via $PSBoundParameters. -LoadLatestSave appends a bare -loadSessionSave
# (loads the latest save point); -LoadSessionSave <uuid> targets a specific one.
if ($LoadLatestSave) {
  $args += @("-loadSessionSave")
} elseif ($PSBoundParameters.ContainsKey('LoadSessionSave')) {
  $args += @("-loadSessionSave", $LoadSessionSave)
}

Write-Host "test-e2e: launching $exe (tier=$Tier)"
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

function Read-Crash([string]$dir) {
  if (-not $dir) { return @() }
  $out = @()
  $cl = Join-Path $dir "crash.log"
  if (Test-Path $cl) {
    $out += Get-Content $cl -ErrorAction SilentlyContinue
  }
  $c = Join-Path $dir "console.log"
  if (Test-Path $c) {
    $out += @(Get-Content $c -ErrorAction SilentlyContinue | Where-Object { $_ -match 'Virtual Machine Exception|NULL pointer|Exception|VME|\( E \)' })
  }
  return $out
}

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

  $crashes = Read-Crash $logDir.FullName
  if ($crashes.Count -gt 0) {
    $crashLines = $crashes | Select-Object -First 12
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
if ($alive) {
  if ($GracefulClose) {
    # Let the server close on its own so the blocking shutdown save (OnGameEnd)
    # completes before process exit; fall back to a force kill on timeout.
    $gcSw = [Diagnostics.Stopwatch]::StartNew()
    while (-not $p.HasExited -and $gcSw.Elapsed.TotalSeconds -lt $GracefulCloseSeconds) {
      Start-Sleep -Milliseconds 1000
    }
  }
  if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
}

$elapsed = [int]$sw.Elapsed.TotalSeconds

# ---- verdict ---------------------------------------------------------------

if ($lastSummary) {
  Write-Host "test-e2e: SUMMARY tier=$($lastSummary.Tier) passed=$($lastSummary.Passed) failed=$($lastSummary.Failed) total=$($lastSummary.Total) ($elapsed s)"
  Write-Host "test-e2e: log $($lastSummary.LogDir)"

  $console = Read-Console $lastSummary.LogDir
  $fails = @($console -split "`n" | Where-Object { $_ -match '\[ELTEST\]   - ' })
  foreach ($f in $fails) { Write-Host "  $f" }

  $debugLines = @($console -split "`n" | Where-Object { $_ -match '\[ELDebug:' })
  if ($debugLines.Count -gt 0) {
    Write-Host "test-e2e: feature debug lines ($($debugLines.Count)):"
    foreach ($d in $debugLines) { Write-Host "  $($d -replace '.*\[ELDebug:', '[ELDebug:')" }
  }

  if ($lastSummary.Tier -ne $Tier) {
    Write-Host "test-e2e: FAILED (tier mismatch: requested $Tier but suite reported $($lastSummary.Tier))"
    exit 1
  }

  $errors = @($console -split "`n" | Where-Object { $_ -match '\( E \)' })
  if ($errors.Count -gt 0) {
    Write-Host "test-e2e: FAILED ($($errors.Count) engine/script error(s) during the run):"
    foreach ($e in ($errors | Select-Object -First 10)) { Write-Host "  $($e.Trim())" }
    exit 1
  }

  if ($lastSummary.Failed -eq 0) { Write-Host "test-e2e: OK"; exit 0 }
  Write-Host "test-e2e: FAILED ($($lastSummary.Failed) failing test(s))"
  exit 1
}

if ($crashLines.Count -gt 0) {
  Write-Host "test-e2e: server crashed (crash.log / VM exception present):"
  foreach ($l in $crashLines) { Write-Host "  $($l.Trim())" }
  Write-Host "test-e2e: FAILED (crash) - log $((Get-LogDir).FullName)"
  exit 1
}

if (-not $alive) {
  Write-Host "test-e2e: server exited early without a SUMMARY verdict"
  exit 2
}

Write-Host "test-e2e: NO VERDICT - timed out after ${TimeoutSeconds}s without an ELTEST SUMMARY (log $((Get-LogDir).FullName))"
exit 2
