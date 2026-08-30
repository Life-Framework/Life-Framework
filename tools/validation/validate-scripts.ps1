# tools/validation/validate-scripts.ps1 - Layer 1: headless EnforceScript compile check.
#
# Runs Workbench's ScriptEditor -validate against the LifeFramework gproj and
# maps its exit code onto the CLI contract: 0 = compiles, 1 = compile errors,
# 2 = no verdict (tool missing or timed out).
#
# Workbench must be launched from the game install dir so that ./addons
# resolves to the game's own packed addons - the vanilla Game addon
# (58D0FB3206B6F859) that our dependency chain (EPF -> EDF -> Game) resolves
# against. Logs land in Documents\My Games\ArmaReforgerWorkbench\logs.
#
# Run via: tools\cli validate   (or directly for verbose output)

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
if (Test-Path -LiteralPath $gameAddonsJunction) { Remove-Item -LiteralPath $gameAddonsJunction -Recurse -Force }
New-Item -ItemType Directory -Force -Path $gameAddonsJunction | Out-Null
cmd /c mklink /J "$gameAddonsJunction\core" "$coreSource" 2>$null | Out-Null
cmd /c mklink /J "$gameAddonsJunction\data" "$dataSource" 2>$null | Out-Null
$args = @(
  '-gproj', "`"$gproj`"",
  '-addonsDir', "`"$gameAddonsJunction`"",
  '-wbModule=ScriptEditor', '-run',
  '-validate', 'PC',
  '-exitAfterInit', '-noSplash', '-noThrow'
)

$p = Start-Process -FilePath $wb -ArgumentList $args -PassThru -WorkingDirectory $gameDir

if (-not $p.WaitForExit(900000)) {
  Write-Host "validate-scripts: TIMEOUT after 15 min - killing Workbench (pid $($p.Id))"
  try { $p.Kill() } catch {}
  exit 2
}
$code = $p.ExitCode

# pull the run's console log for diagnostics
$logDir = Get-ChildItem $wbProfileLogs -Directory -ErrorAction SilentlyContinue |
  Where-Object { $before -notcontains $_.Name } | Sort-Object Name | Select-Object -Last 1
if ($code -ne 0 -and $logDir) {
  $console = Join-Path $logDir.FullName "console.log"
  if (Test-Path $console) {
    Write-Host "validate-scripts: workbench log $($logDir.Name), errors:"
    Select-String -Path $console -Pattern '\(E\)|SCRIPT.*error|failed|not found' |
      Select-Object -First 12 | ForEach-Object { Write-Host "  $($_.Line)" }
  }
}

switch ($code) {
  0  { Write-Host "validate-scripts: OK (exit $code)"; exit 0 }
  -1 { Write-Host "validate-scripts: FAILED - project init or script compilation errors (exit -1)"
       exit 1 }
  default { Write-Host "validate-scripts: FAILED - unexpected Workbench exit code $code"
       exit 1 }
}
