# tools/remote/setup-cloud-box.ps1 - first-boot setup of a rented Windows box for the test pool.
#
# Run it ON the box, in an elevated PowerShell (docs/worktrees.md "A rented box"):
#   irm https://raw.githubusercontent.com/Life-Framework/Life-Framework/main/tools/remote/setup-cloud-box.ps1 | iex
#
# It asks one question (the Tailscale auth key) and then does everything that needs no person:
#   1. power: never sleep or hibernate
#   2. OpenSSH server: key login only, PowerShell as the shell, port 22 open to Tailscale addresses only
#   3. Tailscale: installed and joined with the auth key (a tagged key; nothing is signed in by hand)
#   4. Git and Node.js 22
#   5. Steam: installed and opened for the one manual step, the sign-in
# Safe to run again: every step checks first. The box is treated as untrusted: it receives a public
# key only, and no token, password or repo credential is ever written to it.
# After it: `tools\cli testbox provision --host lf-cloud` from the main PC does the rest.

param(
  [string]$AuthKey = "",
  [string]$HostName = "lf-cloud",
  [string]$PublicKey = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIJgFOGXcdEURS5lHyt1mbd4R88uMsC4oVKup5JtOclMC lf-cloud-pool",
  [switch]$SkipSteam
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) {
  Write-Host "STOP: open PowerShell with 'Run as administrator' and run the same line again." -ForegroundColor Red
  return
}

$work = "C:\LF\setup"
New-Item -ItemType Directory -Force -Path $work | Out-Null

$results = New-Object System.Collections.ArrayList
function Step([string]$name, [scriptblock]$body) {
  Write-Host ""
  Write-Host "== $name" -ForegroundColor Cyan
  try {
    & $body
    [void]$results.Add([pscustomobject]@{ Step = $name; Result = "ok"; Note = "" })
  } catch {
    Write-Host "   FAILED: $($_.Exception.Message)" -ForegroundColor Red
    [void]$results.Add([pscustomobject]@{ Step = $name; Result = "FAILED"; Note = $_.Exception.Message })
  }
}

function Refresh-Path {
  $env:Path = [Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [Environment]::GetEnvironmentVariable("Path", "User")
}

function Have([string]$cmd) { [bool](Get-Command $cmd -ErrorAction SilentlyContinue) }

# Download with three tries; a half-written file is never run.
function Fetch([string]$url, [string]$name) {
  $out = Join-Path $work $name
  for ($i = 1; $i -le 3; $i++) {
    try {
      Invoke-WebRequest -Uri $url -OutFile "$out.part" -UseBasicParsing
      Move-Item -Force "$out.part" $out
      return $out
    } catch {
      if ($i -eq 3) { throw "download failed: $url ($($_.Exception.Message))" }
      Start-Sleep -Seconds 3
    }
  }
}

function Run-Installer([string]$file, [string[]]$arguments) {
  $p = Start-Process -FilePath $file -ArgumentList $arguments -Wait -PassThru
  if ($p.ExitCode -ne 0 -and $p.ExitCode -ne 3010) { throw "$file exited with $($p.ExitCode)" }
}

# The newest release asset of a GitHub project whose name matches the pattern.
function Latest-Asset([string]$repo, [string]$pattern) {
  $rel = Invoke-RestMethod -Uri "https://api.github.com/repos/$repo/releases/latest" -UseBasicParsing -Headers @{ "User-Agent" = "lf-setup" }
  $asset = $rel.assets | Where-Object { $_.name -match $pattern } | Select-Object -First 1
  if (-not $asset) { throw "no asset matching $pattern in $repo" }
  return $asset
}

# ---- the one question ---------------------------------------------------------
if (-not $AuthKey) {
  Write-Host "Paste the Tailscale auth key (tskey-auth-...) and press Enter." -ForegroundColor Yellow
  Write-Host "Press Enter with nothing to skip Tailscale (SSH then stays closed to everything)."
  $AuthKey = (Read-Host "Auth key").Trim()
}
# The log starts after the question, so the key is never written to it.
try { Start-Transcript -Path (Join-Path $work "setup.log") -Append | Out-Null } catch {}

# ---- 1. power -----------------------------------------------------------------
Step "Power: never sleep" {
  foreach ($k in "standby-timeout-ac", "standby-timeout-dc", "hibernate-timeout-ac", "hibernate-timeout-dc", "monitor-timeout-ac", "disk-timeout-ac") {
    & powercfg /change $k 0 | Out-Null
  }
  & powercfg /hibernate off 2>$null | Out-Null
}

# ---- 2. OpenSSH ---------------------------------------------------------------
Step "OpenSSH server" {
  if (-not (Get-Service sshd -ErrorAction SilentlyContinue)) {
    try {
      Add-WindowsCapability -Online -Name "OpenSSH.Server~~~~0.0.1.0" | Out-Null
    } catch {
      Write-Host "   Windows feature install failed; using the OpenSSH release from GitHub"
    }
  }
  if (-not (Get-Service sshd -ErrorAction SilentlyContinue)) {
    $asset = Latest-Asset "PowerShell/Win32-OpenSSH" "Win64.*\.msi$"
    $msi = Fetch $asset.browser_download_url $asset.name
    Run-Installer "msiexec.exe" @("/i", "`"$msi`"", "/qn", "/norestart")
  }
  if (-not (Get-Service sshd -ErrorAction SilentlyContinue)) { throw "the sshd service is still missing" }

  Set-Service sshd -StartupType Automatic
  Start-Service sshd   # the first start writes C:\ProgramData\ssh\sshd_config

  # Windows PowerShell as the shell: the pool's remote commands are PowerShell.
  New-Item -Path "HKLM:\SOFTWARE\OpenSSH" -Force | Out-Null
  New-ItemProperty -Path "HKLM:\SOFTWARE\OpenSSH" -Name DefaultShell -Value "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" -PropertyType String -Force | Out-Null

  # The key of the main PC, for administrators.
  $keys = "C:\ProgramData\ssh\administrators_authorized_keys"
  $have = @()
  if (Test-Path $keys) { $have = @(Get-Content $keys) }
  if ($have -notcontains $PublicKey) { Add-Content -Path $keys -Value $PublicKey -Encoding ascii }
  & icacls $keys /inheritance:r /grant "*S-1-5-32-544:F" /grant "*S-1-5-18:F" | Out-Null

  # Keys only, never a password.
  $cfg = "C:\ProgramData\ssh\sshd_config"
  $text = Get-Content $cfg -Raw
  $text = [regex]::Replace($text, "(?m)^\s*#?\s*PasswordAuthentication\s+\S+.*$", "PasswordAuthentication no")
  if ($text -notmatch "(?m)^PasswordAuthentication no") { $text = "PasswordAuthentication no`r`n" + $text }
  $text = [regex]::Replace($text, "(?m)^\s*#?\s*PubkeyAuthentication\s+\S+.*$", "PubkeyAuthentication yes")
  Set-Content -Path $cfg -Value $text -Encoding ascii

  # Port 22 answers Tailscale addresses only (100.64.0.0/10), not the internet.
  Get-NetFirewallRule -Name "OpenSSH-Server-In-TCP", "sshd", "LF-SSH-Tailscale" -ErrorAction SilentlyContinue | Remove-NetFirewallRule
  New-NetFirewallRule -Name "LF-SSH-Tailscale" -DisplayName "LF SSH (Tailscale only)" -Direction Inbound -Protocol TCP -LocalPort 22 -RemoteAddress "100.64.0.0/10" -Action Allow | Out-Null

  Restart-Service sshd
  if ((Get-Service sshd).Status -ne "Running") { throw "sshd did not start" }
}

# ---- 3. Tailscale -------------------------------------------------------------
$tsExe = "C:\Program Files\Tailscale\tailscale.exe"
Step "Tailscale" {
  if (-not $AuthKey) { throw "skipped: no auth key was given (run the line again with a key)" }
  if (-not (Test-Path $tsExe)) {
    $msi = Fetch "https://pkgs.tailscale.com/stable/tailscale-setup-latest-amd64.msi" "tailscale.msi"
    Run-Installer "msiexec.exe" @("/i", "`"$msi`"", "/qn", "/norestart", "TS_NOLAUNCH=true")
  }
  if (-not (Test-Path $tsExe)) { throw "tailscale.exe is missing after the install" }
  Start-Service Tailscale -ErrorAction SilentlyContinue
  $joined = $false
  for ($i = 1; $i -le 5 -and -not $joined; $i++) {
    & $tsExe up --authkey $AuthKey --hostname $HostName --unattended --reset 2>&1 | ForEach-Object { Write-Host "   $_" }
    if ($LASTEXITCODE -eq 0) { $joined = $true } else { Start-Sleep -Seconds 4 }
  }
  if (-not $joined) { throw "tailscale up failed (is the key valid and unused?)" }
}

# ---- 4. Git + Node ------------------------------------------------------------
Step "Git" {
  Refresh-Path
  if (Have git) { Write-Host "   already installed"; return }
  $asset = Latest-Asset "git-for-windows/git" "^Git-.*-64-bit\.exe$"
  $exe = Fetch $asset.browser_download_url $asset.name
  Run-Installer $exe @("/VERYSILENT", "/NORESTART", "/NOCANCEL", "/SP-", "/SUPPRESSMSGBOXES")
  Refresh-Path
  if (-not (Have git)) { throw "git is not on PATH after the install" }
}

Step "Node.js 22" {
  Refresh-Path
  if (Have node) {
    $major = [int]((& node --version) -replace "^v(\d+)\..*$", '$1')
    if ($major -ge 22) { Write-Host "   already installed"; return }
  }
  $index = Invoke-RestMethod -Uri "https://nodejs.org/dist/index.json" -UseBasicParsing
  $ver = ($index | Where-Object { $_.version -like "v22.*" } | Select-Object -First 1).version
  if (-not $ver) { throw "no Node 22 release found" }
  $msi = Fetch "https://nodejs.org/dist/$ver/node-$ver-x64.msi" "node.msi"
  Run-Installer "msiexec.exe" @("/i", "`"$msi`"", "/qn", "/norestart")
  Refresh-Path
  if (-not (Have node)) { throw "node is not on PATH after the install" }
}

# ---- 5. Steam -----------------------------------------------------------------
$steamExe = "C:\Program Files (x86)\Steam\steam.exe"
if (-not $SkipSteam) {
  Step "Steam" {
    if (-not (Test-Path $steamExe)) {
      $exe = Fetch "https://cdn.cloudflare.steamstatic.com/client/installer/SteamSetup.exe" "SteamSetup.exe"
      Run-Installer $exe @("/S")
    }
    if (-not (Test-Path $steamExe)) { throw "steam.exe is missing after the install" }
    # Opens Steam and queues both installs; they start once someone has signed in.
    Start-Process $steamExe
    Start-Sleep -Seconds 5
    Start-Process "steam://install/1874880"   # Arma Reforger
    Start-Process "steam://install/1874910"   # Arma Reforger Tools
  }
}

# ---- summary ------------------------------------------------------------------
Write-Host ""
Write-Host "================ RESULT ================" -ForegroundColor Cyan
$results | ForEach-Object {
  $colour = "Green"
  if ($_.Result -ne "ok") { $colour = "Red" }
  Write-Host ("{0,-22} {1}  {2}" -f $_.Step, $_.Result, $_.Note) -ForegroundColor $colour
}
$tsIp = ""
if (Test-Path $tsExe) { $tsIp = (& $tsExe ip -4 2>$null | Select-Object -First 1) }
Write-Host ""
Write-Host "Send these two lines back:" -ForegroundColor Yellow
Write-Host "  box:  $HostName  $tsIp"
Write-Host "  user: $(whoami)"
Write-Host ""
if (-not $SkipSteam) {
  Write-Host "One thing left for you: sign in to the Steam window (QR code) and accept the two" -ForegroundColor Yellow
  Write-Host "install prompts (Arma Reforger, Arma Reforger Tools). Then close the stream WITHOUT" -ForegroundColor Yellow
  Write-Host "signing out of Windows." -ForegroundColor Yellow
}
$failed = @($results | Where-Object { $_.Result -ne "ok" })
if ($failed.Count -gt 0) {
  Write-Host ""
  Write-Host "$($failed.Count) step(s) failed. Running the same line again is safe. Log: $work\setup.log" -ForegroundColor Red
}
try { Stop-Transcript | Out-Null } catch {}
