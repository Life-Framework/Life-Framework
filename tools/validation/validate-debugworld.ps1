# tools/validation/validate-debugworld.ps1 - ensure DebugWorld keeps every
# gameplay chain available for automated and manual verification.

$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel
if (-not $root) {
  Write-Host "ERROR not a git work tree" -ForegroundColor Red
  exit 1
}

$layersRoot = Join-Path $root "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers"
$requiredLayers = @(
  "ATM.layer", "AppleChain.layer", "CementChain.layer", "CrimeChain.layer",
  "FarmingChain.layer", "GravelChain.layer", "HousesKeys.layer", "Jobs.layer",
  "LoggingChain.layer", "MiningChain.layer", "PlumChain.layer", "Police.layer",
  "SandChain.layer", "Shops.layer", "Survival.layer",
  "TraderChain.layer", "Roadmap.layer",
  # The roadmap street, one layer per site so each site scope owns one file.
  "Street_Bank.layer", "Street_Breach.layer", "Street_Contraband.layer", "Street_Fuel.layer",
  "Street_Hideout.layer", "Street_Hospital.layer", "Street_Jail.layer", "Street_Jobs.layer",
  "Street_Org.layer", "Street_Police.layer", "Street_Vehicles.layer"
)

$missing = @()
$empty = @()
foreach ($layer in $requiredLayers) {
  $path = Join-Path $layersRoot $layer
  if (-not (Test-Path -LiteralPath $path)) {
    $missing += $layer
    continue
  }
  if ((Get-Item -LiteralPath $path).Length -eq 0) { $empty += $layer }
}

if ($missing.Count -gt 0) {
  Write-Host ("ERROR DebugWorld missing gameplay layers: " + ($missing -join ", ")) -ForegroundColor Red
  exit 1
}
if ($empty.Count -gt 0) {
  Write-Host ("ERROR DebugWorld gameplay layers are empty: " + ($empty -join ", ")) -ForegroundColor Red
  exit 1
}

Write-Host ("validate-debugworld: OK ({0} gameplay layers present and non-empty)" -f $requiredLayers.Count) -ForegroundColor Green
exit 0
