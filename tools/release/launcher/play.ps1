# Life Framework playtest launcher (Windows PowerShell 5.1, no installs).
# "Play Life Framework.cmd" starts it. It finds Arma Reforger through Steam,
# asks "on my own" or "community server", and starts the game with the addon
# from this folder. Source: tools/release/launcher/play.ps1 in the repo.
#
#   -Mode solo|join   skip the choice window
#   -DryRun           print what would start, start nothing (no windows)
#   -ShotTo <png>     save a picture of the choice window and quit (website screenshot)
#   -ClickTest solo|join  click that button in the choice window, print the pick, quit
#                     (tools/validation/launcher-click-selftest.mjs)
param(
  [ValidateSet("", "solo", "join")][string]$Mode = "",
  [switch]$DryRun,
  [string]$ShotTo = "",
  [ValidateSet("", "solo", "join")][string]$ClickTest = ""
)

$ErrorActionPreference = "Stop"
$AppId = "1874880"
$Mission = "{1054CC8646357708}Missions/LF_PlaytestTown.conf"
# Solo hosts on localhost only: no firewall prompt, no clash with a server on 2001.
$SoloAddr = "127.0.0.1:2302"
$ServerInfoUrl = "https://life-framework.org/playtest-server.json"
$DiscordUrl = "https://discord.gg/MjMexJteqz"
$SiteUrl = "https://life-framework.org/playtest"
$SteamStoreUrl = "https://store.steampowered.com/app/1874880/Arma_Reforger/"

$Root = Split-Path -Parent $PSScriptRoot
$AddonsDir = Join-Path $Root "addons"
$LogDir = Join-Path $env:LOCALAPPDATA "LifeFramework"
$LogFile = Join-Path $LogDir "launcher.log"

function Log([string]$msg) {
  try {
    if (-not (Test-Path $LogDir)) { New-Item -ItemType Directory -Path $LogDir -Force | Out-Null }
    Add-Content -Path $LogFile -Value ("{0}  {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $msg) -Encoding UTF8
  } catch {}
  if ($DryRun) { Write-Host $msg }
}

if (-not $DryRun) {
  Add-Type -AssemblyName System.Windows.Forms
  Add-Type -AssemblyName System.Drawing
  [System.Windows.Forms.Application]::EnableVisualStyles()
}

# ---------------------------------------------------------------- messages
function Say([string]$title, [string]$text, [string]$linkLabel = "", [string]$linkUrl = "") {
  Log "$title | $text"
  if ($DryRun) { return }
  if ($linkUrl) {
    $r = [System.Windows.Forms.MessageBox]::Show("$text`n`nOpen $linkLabel now?", "Life Framework - $title", "YesNo", "Information")
    if ($r -eq "Yes") { Start-Process $linkUrl }
  } else {
    [System.Windows.Forms.MessageBox]::Show($text, "Life Framework - $title", "OK", "Information") | Out-Null
  }
}

# ---------------------------------------------------------------- find the game
function Get-SteamRoot {
  foreach ($k in @("HKCU:\Software\Valve\Steam", "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam", "HKLM:\SOFTWARE\Valve\Steam")) {
    try {
      $p = Get-ItemProperty -Path $k -ErrorAction Stop
      foreach ($name in @("SteamPath", "InstallPath")) {
        $v = $p.$name
        if ($v -and (Test-Path $v)) { return ($v -replace "/", "\") }
      }
    } catch {}
  }
  foreach ($d in @("${env:ProgramFiles(x86)}\Steam", "$env:ProgramFiles\Steam")) { if (Test-Path $d) { return $d } }
  return $null
}

function Get-SteamLibraries([string]$steam) {
  $libs = New-Object System.Collections.Generic.List[string]
  if ($steam) { $libs.Add($steam) }
  $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
  if ($steam -and (Test-Path $vdf)) {
    foreach ($line in Get-Content $vdf) {
      if ($line -match '^\s*"path"\s+"(.+)"\s*$') {
        $lib = $Matches[1] -replace "\\\\", "\"
        if (-not $libs.Contains($lib)) { $libs.Add($lib) }
      }
    }
  }
  return $libs
}

function Find-Reforger {
  if ($env:LF_REFORGER_DIR -and (Test-Path (Join-Path $env:LF_REFORGER_DIR "ArmaReforgerSteam.exe"))) {
    return (Join-Path $env:LF_REFORGER_DIR "ArmaReforgerSteam.exe")
  }
  $steam = Get-SteamRoot
  Log "steam: $steam"
  foreach ($lib in (Get-SteamLibraries $steam)) {
    $dir = "Arma Reforger"
    $acf = Join-Path $lib "steamapps\appmanifest_$AppId.acf"
    if (Test-Path $acf) {
      foreach ($line in Get-Content $acf) {
        if ($line -match '^\s*"installdir"\s+"(.+)"\s*$') { $dir = $Matches[1] }
      }
    }
    $exe = Join-Path $lib "steamapps\common\$dir\ArmaReforgerSteam.exe"
    if (Test-Path $exe) { return $exe }
  }
  return $null
}

# ---------------------------------------------------------------- steam login
function Test-SteamReady {
  try {
    $u = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam\ActiveProcess" -ErrorAction Stop).ActiveUser
    return ([int]$u -ne 0)
  } catch { return $false }
}

function Wait-Steam([string]$steamRoot) {
  if (Test-SteamReady) { return $true }
  $steamExe = Join-Path $steamRoot "steam.exe"
  if (-not (Get-Process -Name steam -ErrorAction SilentlyContinue) -and (Test-Path $steamExe)) {
    Log "starting Steam"
    Start-Process -FilePath $steamExe
  }
  for ($i = 0; $i -lt 90; $i++) {
    if (Test-SteamReady) { Start-Sleep -Seconds 3; return $true }
    Start-Sleep -Seconds 1
  }
  return (Test-SteamReady)
}

# ---------------------------------------------------------------- choice window
function Show-Choice {
  $form = New-Object System.Windows.Forms.Form
  $form.Text = "Life Framework playtest"
  $form.StartPosition = "CenterScreen"
  $form.FormBorderStyle = "FixedDialog"
  $form.MaximizeBox = $false
  $form.MinimizeBox = $false
  $form.ClientSize = New-Object System.Drawing.Size(520, 330)
  $form.BackColor = [System.Drawing.Color]::FromArgb(240, 235, 225)
  $form.Font = New-Object System.Drawing.Font("Segoe UI", 10)

  $title = New-Object System.Windows.Forms.Label
  $title.Text = "LIFE FRAMEWORK PLAYTEST"
  $title.Font = New-Object System.Drawing.Font("Segoe UI", 18, [System.Drawing.FontStyle]::Bold)
  $title.ForeColor = [System.Drawing.Color]::FromArgb(27, 29, 25)
  $title.AutoSize = $true
  $title.Location = New-Object System.Drawing.Point(28, 22)
  $form.Controls.Add($title)

  $sub = New-Object System.Windows.Forms.Label
  $sub.Text = "How do you want to play?"
  $sub.AutoSize = $true
  $sub.ForeColor = [System.Drawing.Color]::FromArgb(84, 86, 77)
  $sub.Location = New-Object System.Drawing.Point(30, 64)
  $form.Controls.Add($sub)

  $make = {
    param($text, $hint, $y, $back, $fore, $result)
    $b = New-Object System.Windows.Forms.Button
    $b.Text = $text
    $b.Font = New-Object System.Drawing.Font("Segoe UI", 14, [System.Drawing.FontStyle]::Bold)
    $b.Size = New-Object System.Drawing.Size(460, 64)
    $b.Location = New-Object System.Drawing.Point(30, $y)
    $b.FlatStyle = "Flat"
    $b.FlatAppearance.BorderSize = 2
    $b.FlatAppearance.BorderColor = [System.Drawing.Color]::FromArgb(27, 29, 25)
    $b.BackColor = $back
    $b.ForeColor = $fore
    $b.Cursor = [System.Windows.Forms.Cursors]::Hand
    # GetNewClosure captures only this block's locals, so $form is not visible
    # here; the button finds its own window (playtest-m2-2 crashed on the click).
    $b.Add_Click({ $f = $this.FindForm(); $f.Tag = $result; $f.Close() }.GetNewClosure())
    $form.Controls.Add($b)
    $h = New-Object System.Windows.Forms.Label
    $h.Text = $hint
    $h.AutoSize = $true
    $h.ForeColor = [System.Drawing.Color]::FromArgb(84, 86, 77)
    $h.Location = New-Object System.Drawing.Point(32, ($y + 68))
    $form.Controls.Add($h)
    return $b
  }
  $solo = & $make "Play on my own" "The playtest town, just you. Works any time." 100 ([System.Drawing.Color]::FromArgb(227, 178, 60)) ([System.Drawing.Color]::FromArgb(27, 29, 25)) "solo"
  $join = & $make "Join the community server" "Play with others when someone is hosting." 200 ([System.Drawing.Color]::FromArgb(45, 74, 51)) ([System.Drawing.Color]::FromArgb(251, 248, 242)) "join"
  $form.AcceptButton = $solo
  $form.Topmost = $true
  if ($ShotTo) {
    $form.Add_Shown({
      $form.Refresh()
      Start-Sleep -Milliseconds 400
      $b = $form.Bounds
      $bmp = New-Object System.Drawing.Bitmap($b.Width, $b.Height)
      $g = [System.Drawing.Graphics]::FromImage($bmp)
      $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
      $bmp.Save($ShotTo, [System.Drawing.Imaging.ImageFormat]::Png)
      $form.Close()
    })
  }
  if ($ClickTest) {
    $pick = $solo
    if ($ClickTest -eq "join") { $pick = $join }
    # Invisible, and a failing click prints instead of raising the WinForms error dialog.
    $form.Opacity = 0
    $form.ShowInTaskbar = $false
    $form.Add_Shown({
      try { $pick.PerformClick() } catch { Write-Output ("click failed: " + $_.Exception.Message) }
      if ($this.Visible) { $this.Close() }
    }.GetNewClosure())
  }
  [void]$form.ShowDialog()
  return [string]$form.Tag
}

# ---------------------------------------------------------------- community server
function Get-ServerInfo {
  [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
  try {
    $info = Invoke-RestMethod -Uri $ServerInfoUrl -UseBasicParsing -TimeoutSec 15 -Headers @{ "Cache-Control" = "no-cache" }
    Log ("server info: " + ($info | ConvertTo-Json -Compress))
    return $info
  } catch {
    Log "server info: $($_.Exception.Message)"
    return $null
  }
}

# ---------------------------------------------------------------- main
Log "---- launcher start (folder $Root, mode '$Mode'$(if ($DryRun) { ', dry run' }))"
$version = "unknown"
$versionFile = Join-Path $Root "version.txt"
if (Test-Path $versionFile) { $version = (Get-Content $versionFile | Select-Object -First 1).Trim() }
Log "playtest build: $version"
if ($ShotTo) { [void](Show-Choice); exit 0 }
if ($ClickTest) { Write-Output ("picked=" + (Show-Choice)); exit 0 }

if (-not (Test-Path (Join-Path $AddonsDir "LifeFramework\LifeFramework.gproj"))) {
  Say "Files missing" "The playtest files are not next to this launcher.`n`nUnzip the whole download first (right-click the zip, Extract All), then double-click 'Play Life Framework' in the unzipped folder." "the download page" $SiteUrl
  exit 1
}

$exe = Find-Reforger
if (-not $exe) {
  Say "Arma Reforger not found" "We could not find Arma Reforger on this PC.`n`nThe playtest runs inside Arma Reforger from Steam. Install it, start it once, then double-click 'Play Life Framework' again." "the Steam store page" $SteamStoreUrl
  exit 1
}
Log "game: $exe"

$running = Get-Process -Name "ArmaReforgerSteam", "ArmaReforger_BE", "ArmaReforger" -ErrorAction SilentlyContinue
if ($running -and -not $DryRun) {
  Say "Game already open" "Arma Reforger is already running.`n`nClose it first, then double-click 'Play Life Framework' again."
  exit 1
}

if (-not $Mode) {
  if ($DryRun) { $Mode = "solo" } else { $Mode = Show-Choice }
}
if (-not $Mode) { Log "closed without a choice"; exit 0 }
Log "mode: $Mode"

$gameArgs = @("-addonsDir", "`"$AddonsDir`"", "-addons", "LifeFramework")
if ($Mode -eq "solo") {
  $gameArgs += @("-server", "`"$Mission`"", "-addr", $SoloAddr)
} else {
  $info = Get-ServerInfo
  if (-not $info) {
    Say "Could not check the server" "We could not reach life-framework.org to find the community server. Check your internet connection and try again.`n`nYou can still pick 'Play on my own'." "Discord" $DiscordUrl
    exit 1
  }
  if (-not $info.address) {
    $msg = "Nobody is hosting the community server right now.`n`nPlaytest sessions are announced on our Discord. Until then, pick 'Play on my own'."
    if ($info.message) { $msg = "$($info.message)`n`n$msg" }
    Say "No server running" $msg "Discord" $DiscordUrl
    exit 0
  }
  if ($info.release -and $version -ne "unknown" -and $info.release -ne $version) {
    Say "New download needed" "The community server runs playtest build $($info.release), and you have $version.`n`nDownload the playtest again from life-framework.org, unzip it, and play from the new folder." "the download page" $SiteUrl
    exit 0
  }
  $gameArgs += @("-client", [string]$info.address)
}

if ($DryRun) {
  Log "would start: `"$exe`" $($gameArgs -join ' ')"
  exit 0
}

$steamRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $exe)))
$steamHome = Get-SteamRoot
if ($steamHome) { $steamRoot = $steamHome }
if (-not (Wait-Steam $steamRoot)) {
  Say "Steam is not signed in" "Open Steam and sign in, then double-click 'Play Life Framework' again."
  exit 1
}

Log "start: `"$exe`" $($gameArgs -join ' ')"
try {
  $p = Start-Process -FilePath $exe -ArgumentList $gameArgs -WorkingDirectory (Split-Path -Parent $exe) -PassThru
  Log "game pid $($p.Id)"
} catch {
  Say "The game did not start" "Arma Reforger did not start: $($_.Exception.Message)`n`nAsk for help on Discord and attach $LogFile." "Discord" $DiscordUrl
  exit 1
}
