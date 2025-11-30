# Server Setup

> [!CAUTION]
> **⚠️ Not Ready Yet - v0.0.1**
> 
> This directory is prepared for future server setup files, but there's **no working mod to run yet**. We're still in the early development phase building the core framework.
> 
> This documentation describes what will be available once we have a working release.

---

## Quick Start (Coming Soon)

Once the mod is ready, this is how you'll set up a server:

1. **Install Arma Reforger Dedicated Server** via Steam or SteamCMD
2. **Copy the mod** from `../addons/LifeFramework` to your server's mod directory
3. **Configure your server** using the example configs provided
4. **Launch the server** using the provided scripts

## Contents

- `configs/` - Example server configuration files
- `scripts/` - Server launch scripts for different platforms
- `missions/` - Pre-configured mission files
- `setup-guide.md` - Detailed setup instructions

## Server Requirements

- **Arma Reforger Dedicated Server** (Steam App ID: 1874900)
- **Operating System:** Windows Server or Linux
- **RAM:** Minimum 4GB, recommended 8GB+
- **Network:** Stable internet connection with open ports

## Configuration Files

### `server.json`
Main server configuration including:
- Server name and description
- Player slots
- Network settings
- Admin configuration

### `launch.bat` / `launch.sh`
Launch scripts that:
- Start the server with correct parameters
- Load the Life Framework mod
- Apply your configuration

## Need Help?

Join our Discord server (link in main README) for support and community help!
