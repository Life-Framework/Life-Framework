# Static material/texture chain gate.
#
# Verifies the prefab -> .emat -> .edds reference chain that textures items, and
# flags the one authoring pattern that renders white meshes: a mod .xob baking a
# base-game placeholder material (Common/Materials/Default.emat) with the real
# material left to a prefab MaterialAssignClass override. The mining bars/nuggets/
# ores shipped this shape and their overrides did not apply, so the items fell
# back to the white placeholder. Baking the real material into the .xob (the
# pattern MoneyStack/Apple use) is the reliable route.
#
# Runs as part of `tools\cli validate` and the pre-commit hook. Prefabs that
# resolve are assumed to load; this gate does not render anything.

$ErrorActionPreference = "Stop"
$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }
$addon = Join-Path $root "addons\LifeFramework"
$prefabsDir = Join-Path $addon "Prefabs"

$errors = @()
$warnings = @()
$checked = 0
$materialsChecked = 0

function Error([string]$message) { $script:errors += $message }
function Warn([string]$message) { $script:warnings += $message }

# ---------------------------------------------------------------- helpers

function GuidOf([string]$metaText) {
  if ($metaText -match 'Name "\{([0-9A-Fa-f]{16})\}') { return $Matches[1].ToUpper() }
  return $null
}

# Split a .et text file into top-level `Name "{GUID}" { ... }` component blocks.
function ComponentBlocks([string[]]$lines) {
  $blocks = @()
  $i = 0
  while ($i -lt $lines.Count) {
    if ($lines[$i] -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s+"\{([0-9A-Fa-f]{16})\}"\s*\{\s*$') {
      $name = $Matches[1]
      $open = 1
      $depth = 1
      $j = $i + 1
      while ($j -lt $lines.Count -and $depth -gt 0) {
        $line = $lines[$j]
        $openCount = ([regex]::Matches($line, '\{')).Count
        $closeCount = ([regex]::Matches($line, '\}')).Count
        $depth += $openCount - $closeCount
        $j++
      }
      $body = $lines[($i + 1)..($j - 2)]
      $blocks += [pscustomobject]@{ Name = $name; Body = ($body -join "`n") }
      $i = $j
    } else {
      $i++
    }
  }
  return $blocks
}

# Extract `{GUID}path` string literals matching an extension.
function ResourceRefs([string]$text, [string]$ext) {
  $pattern = '\{([0-9A-Fa-f]{16})\}([A-Za-z0-9_./\\\-]+\.' + $ext + ')'
  $refs = @()
  foreach ($m in [regex]::Matches($text, $pattern)) {
    $refs += [pscustomobject]@{ Guid = $m.Groups[1].Value.ToUpper(); Path = $m.Groups[2].Value }
  }
  return $refs
}

function ModPathExists([string]$relPath) {
  return Test-Path -LiteralPath (Join-Path $addon ($relPath -replace '/', '\'))
}

# Verify a `{GUID}path` resource reference. Only mod-owned resources (those with
# a .meta in this addon) are verified; base-game and dependency assets have no
# .meta here and are skipped - they cannot be checked against this addon and
# flagging them would cry wolf on every commit.
function CheckResourceRef([string]$label, [object]$ref) {
  $metaPath = Join-Path $addon (($ref.Path -replace '/', '\') + ".meta")
  if (-not (Test-Path -LiteralPath $metaPath)) {
    return $true
  }
  if (-not (ModPathExists $ref.Path)) {
    Error "$label references missing file: $($ref.Path)"
    return $false
  }
  $metaGuid = GuidOf ((Get-Content -LiteralPath $metaPath -Raw))
  if (-not $metaGuid) {
    Error "$label .meta has no GUID: $($ref.Path).meta"
    return $false
  }
  if ($metaGuid -ne $ref.Guid) {
    Error "$label GUID mismatch: reference $($ref.Guid) vs meta $metaGuid for $($ref.Path)"
    return $false
  }
  return $true
}

# Read the visual (.emat) materials baked into a mod .xob binary.
function BakedVisualMaterials([string]$xobPath) {
  try {
    $bytes = [System.IO.File]::ReadAllBytes($xobPath)
  } catch {
    return @()
  }
  $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
  $mats = @()
  foreach ($m in [regex]::Matches($ascii, '\{([0-9A-Fa-f]{16})\}([A-Za-z0-9_./\\\-]+\.emat)')) {
    $mats += [pscustomobject]@{ Guid = $m.Groups[1].Value.ToUpper(); Path = $m.Groups[2].Value }
  }
  return $mats
}

# ---------------------------------------------------------------- walk

foreach ($et in @(Get-ChildItem -LiteralPath $prefabsDir -Recurse -Filter '*.et' -File)) {
  $rel = $et.FullName.Substring($addon.Length).TrimStart('\').Replace('\', '/')
  $text = Get-Content -LiteralPath $et.FullName -Raw
  $blocks = ComponentBlocks ($text -split "`r?`n")

  foreach ($block in $blocks) {
    if ($block.Name -ne "MeshObject") { continue }

    $xobs = ResourceRefs $block.Body "xob"
    $assigns = @()
    foreach ($am in [regex]::Matches($block.Body, 'AssignedMaterial\s+"\{([0-9A-Fa-f]{16})\}([A-Za-z0-9_./\\\-]+\.emat)"')) {
      $assigns += [pscustomobject]@{ Guid = $am.Groups[1].Value.ToUpper(); Path = $am.Groups[2].Value }
    }

    $checked++

    foreach ($xob in $xobs) {
      # Only mod-authored .xobs (a .meta in this addon) are audited. Base game
      # models bake their own real material and cannot be checked here.
      $xobAbs = Join-Path $addon ($xob.Path -replace '/', '\')
      if (-not (Test-Path -LiteralPath ($xobAbs + ".meta"))) { continue }
      if (-not (Test-Path -LiteralPath $xobAbs)) {
        Error "$rel MeshObject references missing .xob: $($xob.Path)"
        continue
      }
      $baked = BakedVisualMaterials $xobAbs
      $bakedBase = @($baked | Where-Object { $_.Path -match '^Common/' })
      $hasOverride = $assigns.Count -gt 0

      foreach ($b in $bakedBase) {
        if ($hasOverride) {
          Warn "$rel .xob $($xob.Path) bakes base placeholder $($b.Path) and relies on a MaterialAssignClass override. If the override does not apply at runtime the mesh renders the white placeholder - bake the real material into the .xob instead."
        } else {
          Warn "$rel .xob $($xob.Path) bakes base placeholder $($b.Path) with no material override - the mesh has no real texture and renders white. Assign a material or bake one into the .xob."
        }
      }
    }

    foreach ($assign in $assigns) {
      $materialsChecked++
      if (-not (CheckResourceRef "$rel AssignedMaterial" $assign)) { continue }

      $ematAbs = Join-Path $addon ($assign.Path -replace '/', '\')
      if (-not (Test-Path -LiteralPath ($ematAbs + ".meta"))) { continue }
      $ematText = Get-Content -LiteralPath $ematAbs -Raw
      $textures = ResourceRefs $ematText "edds"
      if ($textures.Count -eq 0) {
        # .emat with no texture refs still loads (e.g. a flat material); only the
        # ones that reference textures need the texture chain checked.
        continue
      }
      foreach ($tex in $textures) {
        if (-not (CheckResourceRef "$($assign.Path) texture" $tex)) { continue }
      }
    }
  }
}

foreach ($message in $warnings) { Write-Host "WARN $message" -ForegroundColor Yellow }
foreach ($message in $errors) { Write-Host "ERROR $message" -ForegroundColor Red }
if ($errors.Count -gt 0) {
  Write-Host "validate-material-chains: FAILED ($($errors.Count) error(s), $($warnings.Count) warning(s), $checked prefab(s) audited)" -ForegroundColor Red
  exit 1
}
Write-Host "validate-material-chains: OK ($checked MeshObject(s) audited, $materialsChecked material ref(s), $($warnings.Count) warning(s))" -ForegroundColor Green
if ($warnings.Count -gt 0) { exit 0 }
exit 0