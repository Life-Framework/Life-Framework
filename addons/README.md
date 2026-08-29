# Addons

`addons/` contains the Life Framework mod.

## Structure

There is a single mod, built as a Reforger GameProject:

```
addons/
└── LifeFramework/          # The mod (LifeFramework.gproj)
```

`LifeFramework/LifeFramework.gproj` defines the project, its language
configs, and its dependencies (the Enfusion Persistence Framework addon, see
the root `README.md`). Everything else — Enforce Script, prefabs, worlds,
UI layouts, localization — lives under `LifeFramework/`.

Future modules (jobs packs, vehicle packs, era-specific asset packs) may be
added later, but each one must be its own independent mod with its own mod
ID and GUID, not a folder inside `LifeFramework`.

## Development

When developing, you can symlink the mod folder into your Arma Reforger
mods directory for easier testing:

```powershell
# Example: Link to Reforger's mod directory
New-Item -ItemType SymbolicLink -Path "C:\Users\YourName\Documents\My Games\ArmaReforger\profile\addons\LifeFramework" -Target "path\to\Life-Framework\addons\LifeFramework"
```

Or copy the mod folder to your Reforger installation for testing.