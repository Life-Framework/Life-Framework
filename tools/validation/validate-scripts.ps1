# tools/validation/validate-scripts.ps1 - Layer 1: headless EnforceScript compile check.
#
# Runs Workbench's ScriptEditor -validate against the LifeFramework gproj. That
# is a real engine compile of the Game module for every platform config
# (WORKBENCH, PC, XBOX, PS4, PS5).
#
# The verdict comes from this run's console.log, NOT the exit code: Workbench
# exits 0 even after `Can't compile "Game" script module!` and `Script
# validation failed.`, which is how this check used to pass on broken code.
#   0 = "Script validation successful." and no SCRIPT (E) line
#   1 = any SCRIPT (E) line or "Script validation failed." (printed as file:line)
#   2 = no verdict: tool missing, timeout, no log for our gproj, addon not
#       mounted, or no success line (a compile that never ran is not a pass)
#
# Red-proof (2026-11-08): a scratch Scripts/Game/LF_ZZPlant.c with
# `return c ? 1 : 2;` on line 5 passed before this change (exit 0). Now exit 1:
#   validate-scripts: FAILED - Enforce Script does not compile (5 distinct error(s), Workbench exit 0)
#     addons/LifeFramework/Scripts/Game/LF_ZZPlant.c:5: Broken expression (missing ';'?)
# An undefined symbol (`return LF_NoSuchThing.Value();`) fails the same way.
#
# Workbench must be launched from the game install dir so that ./addons
# resolves to the game's own packed addons - the vanilla Game addon
# (58D0FB3206B6F859), the only dependency in LifeFramework.gproj. Logs land
# in Documents\My Games\ArmaReforgerWorkbench\logs.
#
# Cost: ~18-40 s and a Workbench launch. It runs in `tools\cli validate --full`,
# ci, `wt gate`, and `wt ship`. The default `validate` skips it: the test
# server compiles every script at boot and test-e2e fails on `Can't compile`.
# The pre-commit hook runs validate-repo.ps1 only.
#
# Run via: tools\cli validate --full   (or directly for verbose output)

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }

$gproj = Join-Path $root "addons\LifeFramework\LifeFramework.gproj"
$wb = $env:LF_WORKBENCH_EXE
if (-not $wb -and $env:ENFUSION_WORKBENCH_PATH) { $wb = Join-Path $env:ENFUSION_WORKBENCH_PATH "Workbench\ArmaReforgerWorkbenchSteamDiag.exe" }
if (-not $wb) { $wb = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe" }
$gameDir = $env:LF_GAME_DIR
if (-not $gameDir) { $gameDir = $env:ENFUSION_GAME_PATH }
if (-not $gameDir) { $gameDir = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger" }
$wbProfileLogs = Join-Path $env:USERPROFILE "Documents\My Games\ArmaReforgerWorkbench\logs"

if (-not (Test-Path -LiteralPath $wb)) {
  Write-Host "ERROR Workbench not found at: $wb (set ENFUSION_WORKBENCH_PATH to override)"
  exit 2
}
if (-not (Test-Path -LiteralPath $gameDir)) {
  Write-Host "ERROR game install not found at: $gameDir (set ENFUSION_GAME_PATH to override)"
  exit 2
}
if (-not (Test-Path -LiteralPath $gproj)) {
  Write-Host "ERROR gproj not found at: $gproj"
  exit 2
}

$before = @(Get-ChildItem $wbProfileLogs -Directory -ErrorAction SilentlyContinue | ForEach-Object { $_.Name })

Write-Host "validate-scripts: launching Workbench from game dir (first run may rebuild the resource database and take a while) ..."

# game-addons holds junctions into the Steam installs. safe-rm unlinks every
# link without following it, then deletes, and exits nonzero when a link stays.
function Remove-GeneratedAddonRoot([string]$path) {
  & node (Join-Path $root "tools\safe-rm.mjs") $path
  if ($LASTEXITCODE -ne 0) { Write-Host "ERROR could not safely remove $path"; exit 2 }
}

# Workbench validation owns this generated addon root. Dedicated-server tests
# use server/profile/test/game-addons with a smaller server data set; sharing
# that directory produced a hybrid install and crashed Workbench at game init.
$workbenchDir = Split-Path -Parent $wb
$coreSource = Join-Path $workbenchDir "addons\core"
$dataSource = Join-Path $gameDir "addons\data"
if (-not (Test-Path -LiteralPath (Join-Path $coreSource "core.gproj"))) {
  Write-Host "ERROR Workbench core project not found: $coreSource"
  Write-Host "FIX  Steam > Arma Reforger Tools > Properties > Installed Files > Verify integrity of game files"
  exit 2
}
if (-not (Test-Path -LiteralPath (Join-Path $dataSource "ArmaReforger.gproj"))) {
  Write-Host "ERROR game data project not found: $dataSource"
  Write-Host "FIX  Steam > Arma Reforger (the GAME, app 1874880) > Properties > Installed Files > Verify integrity of game files"
  Write-Host "     Verifying only 'Arma Reforger Tools' does NOT restore the game data project."
  exit 2
}

$gameAddonsJunction = Join-Path $root "server\profile\validate\game-addons"
Remove-GeneratedAddonRoot $gameAddonsJunction
New-Item -ItemType Directory -Force -Path $gameAddonsJunction | Out-Null
cmd /c mklink /J "$gameAddonsJunction\core" "$coreSource" 2>$null | Out-Null
cmd /c mklink /J "$gameAddonsJunction\data" "$dataSource" 2>$null | Out-Null
$wbArgs = @(
  '-gproj', "`"$gproj`"",
  '-addonsDir', "`"$gameAddonsJunction`"",
  '-wbModule=ScriptEditor', '-run',
  '-validate', 'PC',
  '-exitAfterInit', '-noSplash', '-noThrow'
)

$started = Get-Date
$p = Start-Process -FilePath $wb -ArgumentList $wbArgs -PassThru -WorkingDirectory $gameDir

if (-not $p.WaitForExit(900000)) {
  Write-Host "validate-scripts: TIMEOUT after 15 min - killing Workbench (pid $($p.Id))"
  try { $p.Kill() } catch {}
  exit 2
}
$code = $p.ExitCode

# Find THIS run's console.log. Other worktrees validate concurrently into the
# same Workbench logs folder, so "newest new folder" can be another run; match
# the CLI Params line on our own gproj path instead.
$gprojFull = [IO.Path]::GetFullPath($gproj)
$console = $null
$candidates = Get-ChildItem $wbProfileLogs -Directory -ErrorAction SilentlyContinue |
  Where-Object { $before -notcontains $_.Name -and $_.CreationTime -ge $started.AddSeconds(-5) } |
  Sort-Object Name -Descending
foreach ($dir in $candidates) {
  $log = Join-Path $dir.FullName "console.log"
  if (-not (Test-Path -LiteralPath $log)) { continue }
  $params = Select-String -LiteralPath $log -Pattern 'CLI Params:' -SimpleMatch | Select-Object -First 1
  if ($params -and $params.Line.IndexOf($gprojFull, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
    $console = $log
    break
  }
}
if (-not $console) {
  Write-Host "validate-scripts: NO VERDICT - Workbench exited $code but wrote no console.log for $gprojFull"
  exit 2
}
$lines = @(Get-Content -LiteralPath $console)

# The addon must have been mounted, or a "successful" validation only compiled
# vanilla code.
$addonDir = (Split-Path -Parent $gprojFull).TrimEnd('\')
$mounted = @($lines | Where-Object { $_.IndexOf("Adding relative directory '$addonDir'", [StringComparison]::OrdinalIgnoreCase) -ge 0 })
if ($mounted.Count -eq 0) {
  Write-Host "validate-scripts: NO VERDICT - LifeFramework addon was never mounted (log: $console)"
  exit 2
}

# Engine error formats seen in console.log:
#   SCRIPT    (E): @"Scripts/Game/X.c,5": Broken expression (missing ';'?)
#   SCRIPT    (E): Scripts/Game/X.c(5): error: Syntax error
#   SCRIPT    (E): Can't compile "Game" script module!
# The same error repeats once per validated platform; report each once.
$addonRoot = Join-Path $root "addons\LifeFramework"
$errors = New-Object System.Collections.Generic.List[string]
$seen = @{}
foreach ($line in $lines) {
  if ($line -notmatch 'SCRIPT\s+\(E\):\s*(.*)$') { continue }
  $msg = $Matches[1].Trim()
  $file = $null
  $ln = $null
  if ($msg -match '^@"([^"]+),(\d+)":\s*(.*)$') {
    $file = $Matches[1]; $ln = $Matches[2]; $msg = $Matches[3]
  } elseif ($msg -match '^(\S+\.c)\((\d+)\):\s*(?:error:\s*)?(.*)$') {
    $file = $Matches[1]; $ln = $Matches[2]; $msg = $Matches[3]
  }
  if ($file) {
    if (Test-Path -LiteralPath (Join-Path $addonRoot ($file -replace '/', '\'))) { $file = "addons/LifeFramework/$file" }
    $entry = "${file}:${ln}: $msg"
  } else {
    $entry = $msg
  }
  if (-not $seen.ContainsKey($entry)) {
    $seen[$entry] = $true
    $errors.Add($entry)
  }
}

$succeeded = @($lines | Where-Object { $_ -match 'Script validation successful\.' }).Count -gt 0
$failed = @($lines | Where-Object { $_ -match 'Script validation failed\.' }).Count -gt 0

if ($errors.Count -gt 0 -or $failed) {
  Write-Host "validate-scripts: FAILED - Enforce Script does not compile ($($errors.Count) distinct error(s), Workbench exit $code)"
  $errors | Select-Object -First 25 | ForEach-Object { Write-Host "  $_" }
  if ($errors.Count -gt 25) { Write-Host "  ... $($errors.Count - 25) more" }
  Write-Host "  log: $console"
  exit 1
}
if (-not $succeeded) {
  Write-Host "validate-scripts: NO VERDICT - log has no 'Script validation successful.' line (Workbench exit $code, log: $console)"
  exit 2
}
if ($code -ne 0) {
  Write-Host "validate-scripts: FAILED - validation reported success but Workbench exited $code (log: $console)"
  exit 1
}
Write-Host "validate-scripts: OK - LifeFramework scripts compile (WORKBENCH, PC, XBOX, PS4, PS5)"
exit 0
