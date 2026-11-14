# tools/remote/provision-testbox.ps1 - make a Windows machine a Life Framework test box.
#
# Idempotent: every step checks first and only installs what is missing, so a
# re-run updates the dedicated server and the clone and changes nothing else.
#
#   1. Git and Node.js (22+) via winget, only when missing.
#   2. The dedicated server (Arma Reforger Server, app 1874900):
#      - a complete install in one of the machine's Steam libraries
#        (appmanifest StateFlags 4) is used as is; the Steam client keeps it
#        on the same build as the main PC's Steam install;
#      - otherwise SteamCMD in $SteamCmdDir installs it ANONYMOUSLY into
#        $ServerDir. No Steam client, no Steam account.
#      This script never starts, signs into, or changes the Steam client.
#   3. A read-only clone of the public repo in $RepoDir (fetch from GitHub).
#   4. lf.local.json in the clone: ENFUSION_SERVER_PATH and LF_GAME_ADDONS
#      (the server's own core/data) for this machine.
#
# Run from the main PC (copies this file over and runs it, then sends the
# resource database): tools\cli testbox provision
# Or on the box itself: powershell -ExecutionPolicy Bypass -File provision-testbox.ps1

param(
  [string]$SteamCmdDir = "C:\steamcmd",
  [string]$ServerDir = "C:\LF\ArmaReforgerServer",
  [string]$RepoDir = "C:\LF\Life-Framework",
  [string]$RepoUrl = "https://github.com/Life-Framework/Life-Framework.git",
  [switch]$UseSteamCmd,
  [switch]$SkipServerUpdate
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Step([string]$m) { Write-Host "provision: $m" }

function Refresh-Path {
  $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
}

function Have([string]$cmd) { [bool](Get-Command $cmd -ErrorAction SilentlyContinue) }

function Winget-Install([string]$id) {
  Step "winget install $id"
  & winget install --id $id -e --silent --accept-package-agreements --accept-source-agreements --disable-interactivity
  if ($LASTEXITCODE -ne 0) { throw "winget install $id failed (exit $LASTEXITCODE)" }
  Refresh-Path
}

function Read-Acf([string]$path, [string]$key) {
  $m = [regex]::Match((Get-Content -LiteralPath $path -Raw), '"' + $key + '"\s+"([^"]*)"')
  if ($m.Success) { return $m.Groups[1].Value }
  return $null
}

# A fully installed app in any Steam library on this machine, read from the
# Steam client's files only: { Dir; BuildId; State } or $null.
function Find-SteamApp([string]$appId) {
  $steam = $null
  try { $steam = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -Name SteamPath -ErrorAction Stop).SteamPath } catch {}
  if (-not $steam) { $steam = "C:\Program Files (x86)\Steam" }
  $libs = @($steam)
  $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
  if (Test-Path -LiteralPath $vdf) {
    foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) { $libs += $m.Groups[1].Value.Replace("\\", "\") }
  }
  foreach ($lib in ($libs | Select-Object -Unique)) {
    $acf = Join-Path $lib "steamapps\appmanifest_$appId.acf"
    if (-not (Test-Path -LiteralPath $acf)) { continue }
    $dir = Join-Path $lib ("steamapps\common\" + (Read-Acf $acf "installdir"))
    return [pscustomobject]@{ Dir = $dir; BuildId = (Read-Acf $acf "buildid"); State = (Read-Acf $acf "StateFlags") }
  }
  return $null
}

# ---- 1. Git + Node -----------------------------------------------------------
if (Have git) { Step "git present: $(& git --version)" } else { Winget-Install "Git.Git" }
$nodeOk = $false
if (Have node) {
  $v = (& node --version).Trim()
  $major = [int]($v.TrimStart("v").Split(".")[0])
  $nodeOk = $major -ge 22
  Step "node present: $v$(if (-not $nodeOk) { ' (too old)' })"
}
if (-not $nodeOk) { Winget-Install "OpenJS.NodeJS.LTS" }

# ---- 2. Dedicated server ---------------------------------------------------------
$steamServer = $null
if (-not $UseSteamCmd) { $steamServer = Find-SteamApp "1874900" }
if ($steamServer -and $steamServer.State -eq "4" -and (Test-Path -LiteralPath (Join-Path $steamServer.Dir "addons\data\ArmaReforger.gproj"))) {
  $ServerDir = $steamServer.Dir
  Step "using the Steam library's server: $ServerDir (buildid $($steamServer.BuildId); the Steam client updates it)"
} else {
  if ($steamServer) { Step "Steam library server not ready (StateFlags $($steamServer.State)); using SteamCMD" }
  $steamcmd = Join-Path $SteamCmdDir "steamcmd.exe"
  if (-not (Test-Path -LiteralPath $steamcmd)) {
    Step "installing SteamCMD to $SteamCmdDir"
    New-Item -ItemType Directory -Force -Path $SteamCmdDir | Out-Null
    $zip = Join-Path $env:TEMP "steamcmd.zip"
    Invoke-WebRequest -UseBasicParsing -Uri "https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip" -OutFile $zip
    Expand-Archive -LiteralPath $zip -DestinationPath $SteamCmdDir -Force
    Remove-Item -LiteralPath $zip -Force
  } else { Step "steamcmd present: $steamcmd" }

  $exe = Join-Path $ServerDir "ArmaReforgerServerDiag.exe"
  if ($SkipServerUpdate -and (Test-Path -LiteralPath $exe)) {
    Step "server present, update skipped: $ServerDir"
  } else {
    New-Item -ItemType Directory -Force -Path $ServerDir | Out-Null
    # A provision stopped part-way (ssh dropped, ctrl-c) leaves its SteamCMD
    # running in session 0; a second one on the same folder fails on the
    # lock. Let the first finish (it was seen completing after the ssh died).
    $running = @(Get-Process steamcmd -ErrorAction SilentlyContinue)
    if ($running.Count -gt 0) {
      Step "steamcmd already running (pid $($running.Id -join ', ')); waiting up to 30 min for it to finish"
      $running | Wait-Process -Timeout 1800 -ErrorAction SilentlyContinue
      if (Get-Process steamcmd -ErrorAction SilentlyContinue) { throw "steamcmd still running after 30 min; stop it or wait, then provision again" }
    }
    # SteamCMD updates itself on first start and sometimes exits nonzero (7/8)
    # after that self-update; a retry covers it. Anonymous login only.
    for ($try = 1; $try -le 3; $try++) {
      Step "steamcmd app_update 1874900 validate (try $try) -> $ServerDir"
      & $steamcmd +force_install_dir $ServerDir +login anonymous +app_update 1874900 validate +quit | Out-Host
      $code = $LASTEXITCODE
      if ((Test-Path -LiteralPath $exe) -and (Test-Path -LiteralPath (Join-Path $ServerDir "addons\data\ArmaReforger.gproj"))) { break }
      Step "steamcmd exit $code, server not complete yet"
      if ($try -eq 3) { throw "Arma Reforger Server install failed: $exe or addons\data missing" }
    }
  }
  $manifest = Join-Path $ServerDir "steamapps\appmanifest_1874900.acf"
  if (Test-Path -LiteralPath $manifest) { Step "server buildid $(Read-Acf $manifest 'buildid')" }
}
$serverExe = Join-Path $ServerDir "ArmaReforgerServerDiag.exe"
foreach ($need in @("ArmaReforgerServerDiag.exe", "addons\core\core.gproj", "addons\data\ArmaReforger.gproj")) {
  if (-not (Test-Path -LiteralPath (Join-Path $ServerDir $need))) { throw "server install incomplete: $need missing under $ServerDir" }
}

# ---- 3. Repo clone (read-only; fetches from GitHub) ----------------------------
if (-not (Test-Path -LiteralPath (Join-Path $RepoDir ".git"))) {
  Step "cloning $RepoUrl -> $RepoDir"
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $RepoDir) | Out-Null
  # A clone competing with a Steam download dropped once with "curl 18
  # transfer closed"; git removes the partial clone, so a retry starts clean.
  for ($try = 1; $try -le 3; $try++) {
    & git clone -q $RepoUrl $RepoDir
    if ($LASTEXITCODE -eq 0) { break }
    if ($try -eq 3) { throw "git clone failed 3 times" }
    Step "git clone failed (exit $LASTEXITCODE), retrying"
  }
} else {
  Step "updating clone $RepoDir"
  & git -C $RepoDir fetch -q origin
  if ($LASTEXITCODE -ne 0) { throw "git fetch failed" }
}
# The clone's main checkout stays on main at origin/main. Remote runs use
# linked worktrees beside it (tools\cli test --remote), never this checkout.
$dirty = (& git -C $RepoDir status --porcelain --untracked-files=no)
if (-not $dirty) {
  & git -C $RepoDir checkout -q main
  & git -C $RepoDir merge -q --ff-only origin/main
}
& git -C $RepoDir config core.hooksPath .githooks
& git -C $RepoDir config core.longpaths true

# ---- 4. Machine-local config ----------------------------------------------------
# lf.local.json (gitignored) carries this machine's paths; the CLI reads it
# from the main checkout. Process env still wins over it.
$local = Join-Path $RepoDir "lf.local.json"
$cfg = [ordered]@{}
if (Test-Path -LiteralPath $local) {
  $old = Get-Content -LiteralPath $local -Raw | ConvertFrom-Json
  foreach ($p in $old.PSObject.Properties) { $cfg[$p.Name] = $p.Value }
}
$cfg["ENFUSION_SERVER_PATH"] = $ServerDir.Replace("\", "/")
$cfg["LF_GAME_ADDONS"] = (Join-Path $ServerDir "addons").Replace("\", "/")
# Depth: a box that is a pool member keeps its own "pool" entry here (nested
# objects); the default depth of 2 would flatten it to strings.
$json = ($cfg | ConvertTo-Json -Depth 10)
[System.IO.File]::WriteAllText($local, $json + "`n", (New-Object System.Text.UTF8Encoding $false))
Step "wrote $local"

Step "OK  git $((& git --version) -replace 'git version ', '')  node $(& node --version)  server $serverExe  repo $RepoDir"
