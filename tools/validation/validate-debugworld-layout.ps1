# Static DebugWorld layout gate.
# Fails on overlapping area boxes, tables outside their box, raised ground
# props, and hard intersections between configured large-prop footprints.

$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }
$layersDir = Join-Path $root "addons\LifeFramework\Worlds\DebugWorld\DebugWorld_Layers"
$errors = @()
$boxes = @()
$objects = @()

function Error([string]$message) { $script:errors += $message }
function Coord([string]$line) {
  if ($line -match '^\s*coords\s+(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)') {
    return @{ X = [double]$Matches[1]; Y = [double]$Matches[2]; Z = [double]$Matches[3] }
  }
  return $null
}
function Inside($point, $box) {
  return $point.X -ge $box.X - $box.HalfX -and $point.X -le $box.X + $box.HalfX -and $point.Z -ge $box.Z - $box.HalfZ -and $point.Z -le $box.Z + $box.HalfZ
}
function Footprint([string]$identity) {
  if ($identity -match 'M151A2|BRDM2') { return @{ X = 3.0; Z = 2.5 } }
  if ($identity -match 'Greenhouse|VegetableBed') { return @{ X = 2.5; Z = 2.5 } }
  if ($identity -match 'Furnace|Sawmill|AppleJuicer|ConcretePlant') { return @{ X = 1.25; Z = 1.25 } }
  if ($identity -match 'Safe_01|WeaponRackStand_01|Tire_M151A2') { return @{ X = 1.5; Z = 1.5 } }
  if ($identity -match 'CementPile|GravelPile|SandPile') { return @{ X = 1.25; Z = 1.25 } }
  if ($identity -match 'IronOre|GoldOre|CopperOre') { return @{ X = 0.75; Z = 0.75 } }
  return $null
}

$groundPattern = 'Furnace|Sawmill|AppleJuicer|ConcretePlant|FridgeSmall_01|MobileWaterTankUS_01|Safe_01|WeaponRackStand_01|Tire_M151A2|CementPile|GravelPile|SandPile|IronOre|GoldOre|CopperOre|WhitelistNpc'
$raisedAllowed = 'DisplayItems|TableRecreation|LampKerosene|MiningArea|VegetableBed|SeedTomato|Nugget|Bar|MoneyStack|IDCard|VehicleKey|Apple_|Tomato_|Plum_'

foreach ($layer in @(Get-ChildItem -LiteralPath $layersDir -Filter '*.layer' -File)) {
  $lines = @(Get-Content -LiteralPath $layer.FullName)
  $rootCoord = $null
  foreach ($line in $lines) { $rootCoord = Coord $line; if ($rootCoord) { break } }
  if (-not $rootCoord) { continue }

  $layerBoxes = @()
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -notmatch 'EL_DebugAreaBoundary\s*:') { continue }
    $boundaryCoord = $null; $size = $null
    for ($j = $i + 1; $j -lt [Math]::Min($i + 8, $lines.Count); $j++) {
      if (-not $boundaryCoord) { $boundaryCoord = Coord $lines[$j] }
      if ($lines[$j] -match '^\s*m_vSize\s+(-?[0-9.]+)\s+(-?[0-9.]+)\s+(-?[0-9.]+)') { $size = @{ X = [double]$Matches[1]; Z = [double]$Matches[3] }; break }
    }
    if (-not $boundaryCoord -or -not $size) { Error "$($layer.Name): malformed boundary near line $($i + 1)"; continue }
    $box = @{ Layer = $layer.Name; Line = $i + 1; X = $boundaryCoord.X; Z = $boundaryCoord.Z; HalfX = $size.X; HalfZ = $size.Z }
    $layerBoxes += $box; $boxes += $box
  }

  for ($i = 0; $i -lt $lines.Count; $i++) {
    $coord = Coord $lines[$i]
    if (-not $coord) { continue }
    $contextStart = [Math]::Max(0, $i - 16)
    $context = ($lines[$contextStart..$i] -join "`n")
    if ([Math]::Abs($coord.Y) -gt 0.05 -and $context -match $groundPattern -and $context -notmatch $raisedAllowed) {
      Error "$($layer.Name): raised ground prop at line $($i + 1), Y=$($coord.Y)"
    }
    if ((($lines[$i] -replace '^\s*','').Length -le 0) -or ($lines[$i] -notmatch '^\s{0,2}coords')) { continue }
    $footprint = Footprint $context
    if ($footprint) { $objects += @{ Layer = $layer.Name; Line = $i + 1; Identity = $context; X = $coord.X; Z = $coord.Z; HalfX = $footprint.X; HalfZ = $footprint.Z } }
  }

  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -notmatch 'TableRecreation_01\.et') { continue }
    $table = $null
    for ($j = $i + 1; $j -lt [Math]::Min($i + 8, $lines.Count); $j++) { $table = Coord $lines[$j]; if ($table) { break } }
    if (-not $table) { continue }
    $worldTable = @{ X = $rootCoord.X + $table.X; Z = $rootCoord.Z + $table.Z }
    $inside = $false
    foreach ($box in $layerBoxes) { if (Inside $worldTable $box) { $inside = $true; break } }
    if (-not $inside) { Error "$($layer.Name): table near line $($i + 1) is outside its boundary" }
  }
}

for ($i = 0; $i -lt $boxes.Count; $i++) {
  for ($j = $i + 1; $j -lt $boxes.Count; $j++) {
    $a = $boxes[$i]; $b = $boxes[$j]
    $x = [Math]::Min($a.X + $a.HalfX, $b.X + $b.HalfX) - [Math]::Max($a.X - $a.HalfX, $b.X - $b.HalfX)
    $z = [Math]::Min($a.Z + $a.HalfZ, $b.Z + $b.HalfZ) - [Math]::Max($a.Z - $a.HalfZ, $b.Z - $b.HalfZ)
    if ($x -gt 0 -and $z -gt 0) { Error "boundary overlap: $($a.Layer) line $($a.Line) with $($b.Layer) line $($b.Line)" }
  }
}

for ($i = 0; $i -lt $objects.Count; $i++) {
  for ($j = $i + 1; $j -lt $objects.Count; $j++) {
    $a = $objects[$i]; $b = $objects[$j]
    if ($a.Layer -eq $b.Layer) { continue }
    if ($a.Identity -match 'MiningArea|Boundary|DisplayItems|TableRecreation' -or $b.Identity -match 'MiningArea|Boundary|DisplayItems|TableRecreation') { continue }
    $x = [Math]::Min($a.X + $a.HalfX, $b.X + $b.HalfX) - [Math]::Max($a.X - $a.HalfX, $b.X - $b.HalfX)
    $z = [Math]::Min($a.Z + $a.HalfZ, $b.Z + $b.HalfZ) - [Math]::Max($a.Z - $a.HalfZ, $b.Z - $b.HalfZ)
    if ($x -gt 1.0 -and $z -gt 1.0) { Error "object overlap: $($a.Layer) line $($a.Line) with $($b.Layer) line $($b.Line)" }
  }
}

foreach ($error in $errors) { Write-Host "ERROR $error" -ForegroundColor Red }
if ($errors.Count -gt 0) { Write-Host "validate-debugworld-layout: FAILED ($($errors.Count) error(s))" -ForegroundColor Red; exit 1 }
Write-Host "validate-debugworld-layout: OK" -ForegroundColor Green
exit 0
