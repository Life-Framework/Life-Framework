# Workbench and dedicated-server launches require different base addon sets.
# Sharing one generated game-addons directory lets either launch overwrite the
# other's core/data junctions and can crash Workbench during game creation.

$ErrorActionPreference = "Stop"

$root = git rev-parse --show-toplevel
if (-not $root) { Write-Host "ERROR not a git work tree"; exit 1 }

$validationScript = Get-Content -LiteralPath (Join-Path $root "tools\validation\validate-scripts.ps1") -Raw
if ($validationScript -match 'server\\profile\\test\\game-addons') {
  Write-Host "ERROR validate-scripts must not reuse the dedicated-server test game-addons directory"
  exit 1
}

$cli = Get-Content -LiteralPath (Join-Path $root "tools\cli.mjs") -Raw
$buildStart = $cli.IndexOf("function cmdBuild")
$buildEnd = $cli.IndexOf("function cmdServe")
if ($buildStart -lt 0 -or $buildEnd -le $buildStart) {
  Write-Host "ERROR unable to locate cmdBuild in tools/cli.mjs"
  exit 1
}
$buildCode = $cli.Substring($buildStart, $buildEnd - $buildStart)
if ($buildCode -notmatch 'workbenchGameAddons\(root, "build"\)') {
  Write-Host "ERROR cmdBuild must use its isolated Workbench game-addons directory"
  exit 1
}
if ($cli -notmatch 'const junction = serverGameAddons\(root\)') {
  Write-Host "ERROR dedicated-server launch must use server-compatible base addons"
  exit 1
}

$wb = $env:LF_WORKBENCH_EXE
if (-not $wb -and $env:ENFUSION_WORKBENCH_PATH) { $wb = Join-Path $env:ENFUSION_WORKBENCH_PATH "Workbench\ArmaReforgerWorkbenchSteamDiag.exe" }
if (-not $wb) { $wb = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe" }
$gameDir = $env:LF_GAME_DIR
if (-not $gameDir) { $gameDir = $env:ENFUSION_GAME_PATH }
if (-not $gameDir) { $gameDir = "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger" }

$expectedCore = Join-Path (Split-Path -Parent $wb) "addons\core"
$expectedData = Join-Path $gameDir "addons\data"
foreach ($scope in @("build", "validate")) {
  $addons = Join-Path $root "server\profile\$scope\game-addons"
  if (-not (Test-Path -LiteralPath $addons)) { continue }

  foreach ($entry in @(@("core", $expectedCore), @("data", $expectedData))) {
    $link = Join-Path $addons $entry[0]
    if (-not (Test-Path -LiteralPath $link)) {
      Write-Host "ERROR missing $scope Workbench addon junction: $link"
      exit 1
    }
    $target = (Get-Item -LiteralPath $link).Target
    if (-not $target -or [IO.Path]::GetFullPath($target) -ne [IO.Path]::GetFullPath($entry[1])) {
      Write-Host "ERROR $scope Workbench addon '$($entry[0])' targets '$target', expected '$($entry[1])'"
      exit 1
    }
  }
}

Write-Host "validate-addon-source-isolation: OK"
exit 0
