# Restores the loose Arma Reforger game manifest when the packed client data is present.
#
# Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools\utilities\repair-base-addons.ps1

[CmdletBinding(SupportsShouldProcess = $true)]
param(
  [string]$GameDir = $(if ($env:LF_GAME_DIR) { $env:LF_GAME_DIR } elseif ($env:ENFUSION_GAME_PATH) { $env:ENFUSION_GAME_PATH } else { "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger" }),
  [string]$ServerDir = $(if ($env:LF_SERVER_DIR) { $env:LF_SERVER_DIR } elseif ($env:ENFUSION_SERVER_PATH) { $env:ENFUSION_SERVER_PATH } else { "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Server" }),
  [string]$WorkbenchPath = $(if ($env:ENFUSION_WORKBENCH_PATH) { $env:ENFUSION_WORKBENCH_PATH } else { "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools" })
)

$ErrorActionPreference = "Stop"
$gameManifest = Join-Path $GameDir "addons\data\ArmaReforger.gproj"
$serverManifest = Join-Path $ServerDir "addons\data\ArmaReforger.gproj"
$expectedGuid = "58D0FB3206B6F859"

function Test-ManifestGuid([string]$Path, [string]$Guid) {
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
  $content = Get-Content -LiteralPath $Path -Raw
  return $content -match ('GUID\s+"' + [regex]::Escape($Guid) + '"')
}

function Get-WorkbenchRoot([string]$Path) {
  if ([IO.Path]::GetExtension($Path) -ieq ".exe") {
    return Split-Path -Parent (Split-Path -Parent $Path)
  }
  if ((Split-Path -Leaf $Path) -ieq "Workbench") {
    return Split-Path -Parent $Path
  }
  return $Path
}

Write-Host "Checking Arma Reforger base addon manifests..."

if (-not (Test-Path -LiteralPath $GameDir -PathType Container)) {
  throw "Game install not found: $GameDir. Set ENFUSION_GAME_PATH or pass -GameDir."
}

$workbenchRoot = Get-WorkbenchRoot $WorkbenchPath
$workbenchCore = Join-Path $workbenchRoot "Workbench\addons\core\core.gproj"
if (-not (Test-ManifestGuid $workbenchCore "5614BBCCBB55ED1C")) {
  throw "Workbench core project is missing or invalid: $workbenchCore. Verify Arma Reforger Tools in Steam."
}
Write-Host "OK  Workbench core: $workbenchCore"

if (Test-Path -LiteralPath $gameManifest -PathType Leaf) {
  if (-not (Test-ManifestGuid $gameManifest $expectedGuid)) {
    throw "Game manifest has the wrong GUID: $gameManifest. Expected $expectedGuid. Do not overwrite it manually."
  }
  Write-Host "OK  Game data project: $gameManifest"
  exit 0
}

if (-not (Test-ManifestGuid $serverManifest $expectedGuid)) {
  throw "Game data manifest is missing and no compatible Server manifest was found at $serverManifest. Verify Arma Reforger (app 1874880) and Arma Reforger Server (app 1874900) in Steam."
}

$gameDataDir = Split-Path -Parent $gameManifest
if (-not (Test-Path -LiteralPath $gameDataDir -PathType Container)) {
  throw "Game data directory not found: $gameDataDir"
}

if ($PSCmdlet.ShouldProcess($gameManifest, "restore the missing game project manifest from the installed Server package")) {
  Copy-Item -LiteralPath $serverManifest -Destination $gameManifest
  if (-not (Test-ManifestGuid $gameManifest $expectedGuid)) {
    throw "Manifest restore did not produce a valid game project: $gameManifest"
  }
  Write-Host "FIXED  Restored game data project: $gameManifest"
} else {
  Write-Host "DRY-RUN  Would restore game data project: $gameManifest"
}

Write-Host "Run tools\cli status, then relaunch Workbench if it was open."
