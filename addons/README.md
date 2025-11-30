# Addons

This directory contains the Life Framework mod and any related addon mods.

## Structure

Each mod should be in its own subdirectory with a unique mod ID:

```
addons/
├── LifeFramework/          # Main Life Framework mod
├── LifeFramework_Jobs/     # Example: Jobs extension
└── LifeFramework_Vehicles/ # Example: Vehicle pack extension
```

## Development

When developing, you can symlink this directory to your Arma Reforger mods folder for easier testing:

```powershell
# Example: Link to Reforger's mod directory
New-Item -ItemType SymbolicLink -Path "C:\Users\YourName\Documents\My Games\ArmaReforger\profile\addons\LifeFramework" -Target "path\to\Life-Framework\addons\LifeFramework"
```

Or copy the mod folder to your Reforger installation for testing.
