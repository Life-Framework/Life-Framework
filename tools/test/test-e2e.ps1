# tools/test/test-e2e.ps1 - fast server e2e: boot DebugWorld, run the LFTEST suite, report.
#
# Booting the Reforger dedicated server takes ~20-30s before scripts run, so this
# waits up to a hard 90s cap but polls every 2s and kills the server as soon as
# the LFTEST SUMMARY marker appears (or the cap hits). Exit codes:
#   0 = SUITE PASSED (SUMMARY present, failed=0)
#   1 = SUITE FAILED (SUMMARY present, failed>0) or server crashed with errors
#   2 = NO VERDICT (timeout without SUMMARY, missing tooling, instant crash)
#
# A script that does not compile fails with exit 1 and prints the SCRIPT (E)
# lines as soon as the server logs `Can't compile`. This is why the default
# `tools\cli validate` can skip the Workbench compile.
# Red-proof (2026-11-08): a scratch Tests/LF_ZZPlant.c with `return c ? 1 : 2;`
# (and, separately, `return LF_NoSuchThing.Value();`) gives exit 1:
#   test-e2e: FAILED (scripts do not compile):
#     SCRIPT    (E): @"Scripts/Game/Tests/LF_ZZPlant.c,5": Broken expression (missing ';'?)
#     SCRIPT    (E): Can't compile "Game" script module!
# The server also crashes on it, so the older crash.log path failed too, but
# only as "server crashed". Neither compiler rejects `int x = ;`. Workbench
# validation also compiles the XBOX/PS4/PS5 configs, which this server does
# not, so `validate --full` stays in ci, wt gate, and wt ship.
#
# -Only <a,b> runs just those tests (class names or GetName() strings): it
# writes $profile:LF_TestOnly.txt and defines LF_TEST_ONLY; the suite then
# reports tier=only, so suite-stamp never mistakes it for a full tier run.
#
# Run via: tools\cli test

param(
  # Run budget (seconds), counted from the server's ready line; the pool passes the learned one. Boot has its own, shorter budget.
  [int]$TimeoutSeconds = 90,
  [int]$BootTimeoutSeconds = 30,
  [int]$PollMs = 2000,
  [string]$Tier = "all",
  [int]$BindPort = 2001,
  [int]$A2sPort = 17777,
  [string]$ListenAddr,
  [switch]$KeepProfile,
  [switch]$LoadLatestSave,
  [string]$LoadSessionSave,
  [switch]$GracefulClose,
  [int]$GracefulCloseSeconds = 30,
  [string]$ExtraDefine,
  [string]$Only
)

$ErrorActionPreference = "Stop"
# Per-run stdout/stderr capture files (two lanes run at once); drop the old ones.
Get-ChildItem -Path $env:TEMP -Filter "lf-e2e-*-*.txt" -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-1) } | Remove-Item -Force -ErrorAction SilentlyContinue
. (Join-Path $PSScriptRoot "..\lib\quiet-launch.ps1")

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 2 }
if (-not (Test-Path (Join-Path $root "addons\LifeFramework\LifeFramework.gproj"))) {
  Write-Host "ERROR addon not found at repo root: $root"
  exit 2
}

# resourceDatabase.rdb is gitignored; cli test/dev/serve copy it from the main
# checkout. Without it world tests fail as "wrong GUID", "invalid prefab" or
# GetCash returning 0, so a world tier stops here with that named cause.
$rdb = Join-Path $root "addons\LifeFramework\resourceDatabase.rdb"
if (-not (Test-Path -LiteralPath $rdb)) {
  if ($Tier -eq "fast") {
    Write-Host "WARN  resourceDatabase.rdb missing: $rdb (LOGIC tier only; world tiers refuse to run without it)"
  } else {
    Write-Host "ERROR resourceDatabase.rdb missing: $rdb"
    Write-Host "      world tests would fail with wrong GUID / invalid prefab. Run through tools\cli test (it copies the"
    Write-Host "      main checkout's copy); if the main checkout has none, open the project in Workbench once to build it."
    exit 2
  }
}

$serverInstall = $env:ENFUSION_SERVER_PATH
if (-not $serverInstall) { $serverInstall = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server" }
$exe = Join-Path $serverInstall "ArmaReforgerServerDiag.exe"
if (-not (Test-Path -LiteralPath $exe)) {
  Write-Host "ERROR dedicated server not found: $exe"
  exit 2
}

function Save-SuiteStamp([string]$ProfileDir, [string]$RepoRoot) {
  $logs = Join-Path $ProfileDir "logs"
  if (-not (Test-Path -LiteralPath $logs)) { return }
  $logDir = Get-ChildItem -LiteralPath $logs -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name | Select-Object -Last 1
  if (-not $logDir) { return }
  $consoleLog = Join-Path $logDir.FullName "console.log"
  if (-not (Test-Path -LiteralPath $consoleLog)) { return }
  $raw = [System.IO.File]::ReadAllText($consoleLog)
  $found = [regex]::Matches($raw, '\[LFTEST\] SUMMARY tier=(\w+) passed=(\d+) failed=(\d+) total=(\d+)')
  if ($found.Count -eq 0) { return }
  $match = $found[$found.Count - 1]
  $sha = (& git -C $RepoRoot rev-parse HEAD 2>$null)
  if (-not $sha) { return }
  $sha = "$sha".Trim()
  $outDir = Join-Path $RepoRoot "tmp\test-results"
  New-Item -ItemType Directory -Force -Path $outDir | Out-Null
  $outFile = Join-Path $outDir "summaries.jsonl"
  $jsonPath = $consoleLog.Replace('\', '\\').Replace('"', '\"')
  $line = '{"sha":"' + $sha + '","tier":"' + $match.Groups[1].Value + '","passed":' + $match.Groups[2].Value + ',"failed":' + $match.Groups[3].Value + ',"total":' + $match.Groups[4].Value + ',"consoleLog":"' + $jsonPath + '"}'
  $utf8 = New-Object System.Text.UTF8Encoding $false
  [System.IO.File]::AppendAllText($outFile, $line + "`n", $utf8)
}

$profile = Join-Path $root "server\profile\test"
if (-not $KeepProfile) {
  # The next boot deletes this profile, including the previous tier's console.log.
  # Record that summary first so suite-stamp can still read it.
  Save-SuiteStamp -ProfileDir $profile -RepoRoot $root
  # The profile holds game-addons junctions into Steam. Remove-Item -Recurse in
  # Windows PowerShell follows them and empties the installs; safe-rm unlinks
  # every link first.
  if (Test-Path $profile) {
    & node (Join-Path $root "tools\safe-rm.mjs") $profile
    if ($LASTEXITCODE -ne 0) { Write-Host "FAIL  could not safely remove $profile"; exit 2 }
  }
}
New-Item -ItemType Directory -Force -Path $profile | Out-Null

# Dedicated-server FileIO only reads $profile. Mirror the crafting prefabs so
# the LOGIC asset test can read the README and check prefab files without
# loading meshes.
# -profile <dir> stores script files in <dir>\profile. That inner folder is $profile:.
$craftingMirror = Join-Path $profile "profile\crafting-addon\Prefabs\Crafting"
New-Item -ItemType Directory -Force -Path $craftingMirror | Out-Null
Copy-Item -Path (Join-Path $root "addons\LifeFramework\Prefabs\Crafting\*") -Destination $craftingMirror -Recurse -Force

# tools\cli audit placement (agent-box/place-audit.mjs) hands over the town's entity list in
# LF_PLACE_AUDIT_IN; LF_Test_PlacementAuditTown reads it as $profile:place-audit/in.txt.
if ($env:LF_PLACE_AUDIT_IN -and (Test-Path -LiteralPath $env:LF_PLACE_AUDIT_IN)) {
  $auditDir = Join-Path $profile "profile\place-audit"
  New-Item -ItemType Directory -Force -Path $auditDir | Out-Null
  Copy-Item -LiteralPath $env:LF_PLACE_AUDIT_IN -Destination (Join-Path $auditDir "in.txt") -Force
}

# addonsDir = repo addon + base game data only (core + data), via a junction so
# the server does NOT auto-load other addons that live in the server install
# (LifeFramework.gproj depends only on the vanilla Game addon 58D0FB3206B6F859).
# Launch cwd = repo root (neutral) so
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
# LF_GAME_ADDONS (OS env or lf.local.json, passed in by tools\cli) names the
# core/data source outright, e.g. a SteamCMD server install on a test box.
if ($env:LF_GAME_ADDONS) {
  # mklink /J reads forward slashes as switches.
  $dataSource = $env:LF_GAME_ADDONS.Replace('/', '\').TrimEnd('\')
  if (-not (Test-Path -LiteralPath (Join-Path $dataSource "core")) -or -not (Test-Path -LiteralPath (Join-Path $dataSource "data"))) {
    Write-Host "ERROR LF_GAME_ADDONS=$dataSource has no core\ and data\ folders"
    exit 2
  }
}
Write-Host "test-e2e: base game data from $dataSource"
# game-addons holds junctions into the Steam installs. safe-rm unlinks every
# link without following it, then deletes, and exits nonzero when a link stays.
function Remove-GeneratedAddonRoot([string]$path) {
  & node (Join-Path $root "tools\safe-rm.mjs") $path
  if ($LASTEXITCODE -ne 0) { Write-Host "FAIL  could not safely remove $path"; exit 2 }
}
Remove-GeneratedAddonRoot $gameAddons
New-Item -ItemType Directory -Force -Path $gameAddons | Out-Null
cmd /c mklink /J "$gameAddons\core" "$dataSource\core" 2>&1 | Out-Null
cmd /c mklink /J "$gameAddons\data" "$dataSource\data" 2>&1 | Out-Null

# Some current installs ship core as data.pak without a core.gproj. The data
# project depends on core's GUID, so provide the minimal project manifest the
# engine needs when the install did not include one. This is generated under
# the ignored test profile, never committed to the repository.
$coreGproj = Join-Path $gameAddons "core\core.gproj"
if (-not (Test-Path -LiteralPath (Join-Path $dataSource "core\core.gproj"))) {
  Remove-GeneratedAddonRoot (Join-Path $gameAddons "core")
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
  "-scrDefine", "LF_AUTOTEST",
  "-disableCrashReporter",
  "-noBackend",
  "-bindPort", $BindPort,
  "-a2sPort", $A2sPort
)
# A test lane's server listens on its own 127.0.0.1:<port> (-server mode ignores
# -bindPort/-bindIP; -addr is what moves it off UDP 2001), so it runs beside the
# session server. tools/remote/pool.mjs lanes.
if ($ListenAddr) { $args += @("-addr", $ListenAddr) }

if ($Tier -eq "fast") {
  $args += @("-scrDefine", "LF_TEST_TIER_FAST")
} elseif ($Tier -eq "persistence") {
  $args += @("-scrDefine", "LF_TEST_TIER_PERSISTENCE")
}

$expectedTier = $Tier
$onlyFile = Join-Path $profile "profile\LF_TestOnly.txt"
Remove-Item -LiteralPath $onlyFile -Force -ErrorAction SilentlyContinue
if (-not [string]::IsNullOrEmpty($Only)) {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $onlyFile) | Out-Null
  [System.IO.File]::WriteAllText($onlyFile, $Only, (New-Object System.Text.UTF8Encoding $false))
  $args += @("-scrDefine", "LF_TEST_ONLY")
  $expectedTier = "only"
  Write-Host "test-e2e: only $Only"
}

# LF_TEST_HANG=run holds the suite in the booted server (LF_TestRunnerComponent reads the file);
# LF_TEST_HANG=boot swaps the server for a process that never gets ready. Both exist so the pool's
# runtime budgets have a real hang to kill (tools/validation/pool-budget-selftest.mjs, docs/worktrees.md).
$hangFile = Join-Path $profile "profile\LF_TestHang.txt"
Remove-Item -LiteralPath $hangFile -Force -ErrorAction SilentlyContinue
if ($env:LF_TEST_HANG -eq "run") {
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $hangFile) | Out-Null
  [System.IO.File]::WriteAllText($hangFile, "hang", (New-Object System.Text.UTF8Encoding $false))
  Write-Host "test-e2e: LF_TEST_HANG=run (the suite will not start)"
}

if (-not [string]::IsNullOrEmpty($ExtraDefine)) {
  # Comma-separated: one -scrDefine per name (wt e2e --legacy-save passes two in the reload boot).
  foreach ($define in ($ExtraDefine -split ",")) {
    if (-not [string]::IsNullOrWhiteSpace($define)) { $args += @("-scrDefine", $define.Trim()) }
  }
}

# PS 5.1 binds an unpassed [string] param to "" not $null, so "provided" must be
# detected via $PSBoundParameters. -LoadLatestSave appends a bare -loadSessionSave
# (loads the latest save point); -LoadSessionSave <uuid> targets a specific one.
if ($LoadLatestSave) {
  $args += @("-loadSessionSave")
} elseif ($PSBoundParameters.ContainsKey('LoadSessionSave')) {
  $args += @("-loadSessionSave", $LoadSessionSave)
}

if ($env:LF_TEST_HANG -eq "boot") {
  Write-Host "test-e2e: LF_TEST_HANG=boot (a stand-in process that never gets ready replaces the server)"
  $exe = (Get-Command powershell).Source
  $args = @("-NoProfile", "-Command", "Start-Sleep -Seconds 600")
}
Write-Host "test-e2e: launching $exe (tier=$Tier)"
Write-Host "test-e2e: args $($args -join ' ')"
# The server runs on the private LF-Agents desktop with no console window, so a
# test boot never opens a terminal on the user's screen
# (tools/lib/quiet-launch.ps1, dot-sourced at the top; LF_SHOW_WINDOWS=1 shows it).
$p = Start-LfQuietProcess -FilePath $exe -ArgumentList $args -WorkingDirectory $root -KillOnClose `
  -RedirectStandardOutput (Join-Path $env:TEMP "lf-e2e-out-$PID.txt") `
  -RedirectStandardError  (Join-Path $env:TEMP "lf-e2e-err-$PID.txt")

function Get-LogDir {
  Get-ChildItem (Join-Path $profile "logs") -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name | Select-Object -Last 1
}

function ConvertTo-ConsoleText($value) {
  if ($null -eq $value) { return "" }
  # Get-Content -Raw returns an array while the log is still open. -join on a
  # string walks characters, so only join a real array.
  if ($value -is [System.Array]) { return [string]::Join("`n", $value) }
  return [string]$value
}

function Read-Console([string]$dir) {
  if (-not $dir) { return "" }
  $c = Join-Path $dir "console.log"
  if (Test-Path $c) {
    return (ConvertTo-ConsoleText (Get-Content $c -Raw -ErrorAction SilentlyContinue))
  }
  return ""
}

$sw = [Diagnostics.Stopwatch]::StartNew()
$lastSummary = $null
$harnessError = $null
$compileFailed = $false
$crashLines = @()

# Print the suite lines from a console.log so a no-verdict exit still shows
# them without anyone grepping the log by hand.
function Write-SuiteLines([string]$dir) {
  if (-not $dir) { Write-Host "test-e2e: no console.log yet"; return }
  $c = Join-Path $dir "console.log"
  Write-Host "test-e2e: log $c"
  if (-not (Test-Path -LiteralPath $c)) { return }
  $hits = @(Select-String -LiteralPath $c -Pattern '\[LFTEST\] (SUMMARY|FAIL|  - )' -ErrorAction SilentlyContinue)
  if ($hits.Count -eq 0) { Write-Host "test-e2e: console.log has no [LFTEST] SUMMARY or FAIL line"; return }
  foreach ($h in ($hits | Select-Object -First 40)) { Write-Host "  $($h.Line.Trim())" }
}

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

# The most telling lines of a hung run, at most 5: script errors and VMEs first, then the
# last [LFTEST] line, then the tail of the log.
function Get-HungLines([string]$dir) {
  $lines = @()
  if ($dir) {
    $c = Join-Path $dir "console.log"
    if (Test-Path -LiteralPath $c) { $lines = @(Get-Content -LiteralPath $c -ErrorAction SilentlyContinue | Where-Object { $_ -and $_.Trim() }) }
  }
  $pick = New-Object System.Collections.ArrayList
  foreach ($l in @($lines | Where-Object { $_ -match 'SCRIPT\s+\(E\)|Virtual Machine Exception|VME' } | Select-Object -Last 2)) { [void]$pick.Add($l) }
  foreach ($l in @($lines | Where-Object { $_ -match '\[LFTEST\]|\[LF_Tests\]' } | Select-Object -Last 2)) { if (-not $pick.Contains($l)) { [void]$pick.Add($l) } }
  # Fill with the tail of the log, feature debug lines excluded (hundreds of them).
  foreach ($l in @($lines | Where-Object { $_ -notmatch '\[LFDebug:' } | Select-Object -Last 8)) { if ($pick.Count -lt 5 -and -not $pick.Contains($l)) { [void]$pick.Add($l) } }
  return @($pick | Select-Object -First 5 | ForEach-Object { $t = $_.Trim(); if ($t.Length -gt 200) { $t.Substring(0, 200) } else { $t } })
}

$ready = $false
$readySw = $null
$timedOut = $null
while ($true) {
  Start-Sleep -Milliseconds $PollMs

  $logDir = Get-LogDir
  $console = Read-Console $logDir.FullName

  try {
    $text = ConvertTo-ConsoleText $console
    $summaryMatch = [regex]::Match($text, '\[LFTEST\] SUMMARY tier=(\w+) passed=(\d+) failed=(\d+) total=(\d+)')
    if ($summaryMatch.Success) {
      $at = $summaryMatch.Index
      $lineStart = 0
      if ($at -gt 0) {
        $nl = $text.LastIndexOf("`n", $at - 1)
        if ($nl -ge 0) { $lineStart = $nl + 1 }
      }
      $lineEnd = $text.IndexOf("`n", $at)
      if ($lineEnd -lt 0) { $lineEnd = $text.Length }
      $summaryLine = $text.Substring($lineStart, $lineEnd - $lineStart).TrimEnd("`r")
      Write-Host $summaryLine
      $lastSummary = [pscustomobject]@{
        Tier = $summaryMatch.Groups[1].Value
        Passed = [int]$summaryMatch.Groups[2].Value
        Failed = [int]$summaryMatch.Groups[3].Value
        Total = [int]$summaryMatch.Groups[4].Value
        LogDir = $logDir.FullName
      }
      break
    }
  } catch {
    # The old "Cannot index into a null array" came from indexing $matches
    # after -match ran on an array; [regex]::Match above removed that source.
    # Any parse error left is a harness bug: name it and stop polling.
    $harnessError = "line $($_.InvocationInfo.ScriptLineNumber): $($_.Exception.Message)"
    break
  }

  # A compile failure does not stop the server; it runs on without the
  # addon's scripts and never prints a SUMMARY. Fail now with the errors.
  if ($console -match 'Can''t compile') {
    $crashLines = @($console -split "`n" | Where-Object { $_ -match 'SCRIPT\s+\(E\)|Can''t compile' } | Select-Object -First 12)
    $compileFailed = $true
    break
  }

  $crashes = Read-Crash $logDir.FullName
  if ($crashes.Count -gt 0) {
    $crashLines = $crashes | Select-Object -First 12
    break
  }

  if (-not $ready -and ($console -match 'Starting RPL server, listening')) {
    $ready = $true
    $readySw = [Diagnostics.Stopwatch]::StartNew()
  }
  if (-not $p.HasExited) {
    if (-not $ready -and $sw.Elapsed.TotalSeconds -gt $BootTimeoutSeconds) { $timedOut = "boot"; break }
    if ($ready -and $readySw.Elapsed.TotalSeconds -gt $TimeoutSeconds) { $timedOut = "run"; break }
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
  # The whole tree: a hung run must not leave a child holding the lane's port.
  if (-not $p.HasExited) { & taskkill /T /F /PID $p.Id 2>&1 | Out-Null }
}

$elapsed = [int]$sw.Elapsed.TotalSeconds

if ($timedOut) {
  if ($timedOut -eq "boot") {
    Write-Host "TIMEOUT after $([int]$sw.Elapsed.TotalSeconds)s (boot budget ${BootTimeoutSeconds}s, the server never reached ready); last log lines:"
  } else {
    Write-Host "TIMEOUT after $([int]$readySw.Elapsed.TotalSeconds)s (budget ${TimeoutSeconds}s); last log lines:"
  }
  $hungDir = Get-LogDir
  $hungLines = @(Get-HungLines $hungDir.FullName)
  if ($hungLines.Count -eq 0) { Write-Host "  (no server console.log yet)" }
  foreach ($l in $hungLines) { Write-Host "  $l" }
  exit 124
}

if ($harnessError) {
  Write-Host "test-e2e: HARNESS ERROR (no verdict) at $harnessError"
  Write-SuiteLines $logDir.FullName
  exit 2
}

# ---- verdict ---------------------------------------------------------------

if ($lastSummary) {
  Write-Host "test-e2e: SUMMARY tier=$($lastSummary.Tier) passed=$($lastSummary.Passed) failed=$($lastSummary.Failed) total=$($lastSummary.Total) ($elapsed s)"
  Write-Host "test-e2e: log $($lastSummary.LogDir)"

  $console = Read-Console $lastSummary.LogDir
  $fails = @($console -split "`n" | Where-Object { $_ -match '\[LFTEST\]   - ' })
  foreach ($f in $fails) { Write-Host "  $f" }

  $debugLines = @($console -split "`n" | Where-Object { $_ -match '\[LFDebug:' })
  if ($debugLines.Count -gt 0) {
    # Hundreds of lines per run: a count and a file by default; the lines inline
    # only with LF_TEST_DEBUG_LINES=1 (tools\cli dev prints its own).
    $dbgFile = Join-Path $lastSummary.LogDir "lfdebug-lines.txt"
    $debugLines | ForEach-Object { $_ -replace '.*\[LFDebug:', '[LFDebug:' } | Set-Content -LiteralPath $dbgFile -Encoding utf8
    Write-Host "test-e2e: feature debug lines ($($debugLines.Count)): $dbgFile"
    if ($env:LF_TEST_DEBUG_LINES -eq '1') {
      foreach ($d in $debugLines) { Write-Host "  $($d -replace '.*\[LFDebug:', '[LFDebug:')" }
    }
  }

  if ($lastSummary.Tier -ne $expectedTier) {
    Write-Host "test-e2e: FAILED (tier mismatch: requested $expectedTier but suite reported $($lastSummary.Tier))"
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

if ($compileFailed) {
  Write-Host "test-e2e: FAILED (scripts do not compile):"
  foreach ($l in $crashLines) { Write-Host "  $($l.Trim())" }
  Write-Host "test-e2e: log $((Get-LogDir).FullName)"
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
  Write-SuiteLines (Get-LogDir).FullName
  exit 2
}

Write-Host "test-e2e: NO VERDICT - timed out after ${TimeoutSeconds}s without an LFTEST SUMMARY"
Write-SuiteLines (Get-LogDir).FullName
exit 2
