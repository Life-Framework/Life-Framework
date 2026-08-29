# server/scripts/launch-test.ps1 - boot the headless test server.
#
# Loads the local addon (addons/LifeFramework) and runs the EL_DebugTest
# scenario. The EL_TestGameMode prints [ELTEST] markers to the log; the CLI
# (tools\cli test) orchestrates build + launch + result parsing.
#
# Usage:
#   .\server\scripts\launch-test.ps1
#   .\server\scripts\launch-test.ps1 -Diag
#   .\server\scripts\launch-test.ps1 -ServerExe "D:\reforger\ArmaReforgerServer.exe"
param(
  [string]$ServerExe = "",
  [string]$Profile = "",
  [switch]$Diag
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$defaultServerDir = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server"

if ($ServerExe) {
  $exe = $ServerExe
} elseif ($Diag) {
  $exe = Join-Path $defaultServerDir "ArmaReforgerServerDiag.exe"
} else {
  $exe = Join-Path $defaultServerDir "ArmaReforgerServer.exe"
}

if (-not (Test-Path -LiteralPath $exe)) {
  Write-Host "ERROR dedicated server not found: $exe" -ForegroundColor Red
  Write-Host "Install Arma Reforger Server (Steam app 1874900) or pass -ServerExe." -ForegroundColor Yellow
  exit 1
}

$profile = if ($Profile) { $Profile } else { Join-Path $root "server\profile\test" }
$config = Join-Path $root "server\configs\test-server.json"
$addonsDir = Join-Path $root "addons"

$args = @(
  "-config", $config,
  "-profile", $profile,
  "-addonsDir", $addonsDir,
  "-addons", "LifeFramework",
  "-maxFPS", "60"
)

Write-Host "Launching: $exe $($args -join ' ')"
& $exe @args
exit $LASTEXITCODE