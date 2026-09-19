# tools/validation/validate-debugworld-layer-order.ps1
# The generic world entity must be declared in default.layer. Gameplay props in
# layers loaded before it never get a physics body, so their meshes render while
# the player walks straight through. See docs/features.md -> DebugWorld coverage.

$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree" -ForegroundColor Red; exit 1 }

$layersRoot = Join-Path $root "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers"
$worldLayer = "default.layer"
$errors = @()

if (-not (Test-Path -LiteralPath $layersRoot)) {
  Write-Host "ERROR DebugWorld layers folder missing: $layersRoot" -ForegroundColor Red
  exit 1
}

$declaringLayers = @()
foreach ($layer in @(Get-ChildItem -LiteralPath $layersRoot -Filter '*.layer' -File)) {
  if (Select-String -LiteralPath $layer.FullName -Pattern '^\s*GenericWorldEntity\s+world\b' -Quiet) {
    $declaringLayers += $layer.Name
  }
}

if ($declaringLayers.Count -eq 0) {
  $errors += "no layer declares 'GenericWorldEntity world'"
} elseif ($declaringLayers -notcontains $worldLayer) {
  $errors += "'GenericWorldEntity world' is declared in $($declaringLayers -join ', ') instead of $worldLayer"
} elseif ($declaringLayers.Count -gt 1) {
  $errors += "'GenericWorldEntity world' is declared in multiple layers: $($declaringLayers -join ', ')"
}

if (Test-Path -LiteralPath (Join-Path $layersRoot "Terrain.layer")) {
  $errors += "Terrain.layer exists; the world entity belongs in $worldLayer (Terrain.layer loaded after gameplay layers)"
}

foreach ($message in $errors) { Write-Host "ERROR $message" -ForegroundColor Red }
if ($errors.Count -gt 0) { Write-Host "validate-debugworld-layer-order: FAILED ($($errors.Count) error(s))" -ForegroundColor Red; exit 1 }
Write-Host "validate-debugworld-layer-order: OK (world entity in $worldLayer, loads before gameplay layers)" -ForegroundColor Green
exit 0
