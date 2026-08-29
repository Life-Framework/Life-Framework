# Addons

`addons/` contains the Life Framework mod.

## Structure

Single mod, built as a Reforger GameProject:

```
addons/
└── LifeFramework/          # LifeFramework.gproj — scripts, prefabs, worlds, UI
```

`LifeFramework/LifeFramework.gproj` declares the project, language configs, and
dependencies (base game only — no third-party addons). EnforceScript, prefabs,
worlds, UI layouts, and localization all live under `LifeFramework/`.

Future packs (jobs, vehicles, era assets) must be **separate mods** with their
own mod ID and GUID — not folders inside `LifeFramework`.

## Key paths

| Path | Purpose |
| --- | --- |
| `Scripts/Game/` | EnforceScript (`EL_*` classes) |
| `Scripts/Game/Tests/` | In-game `EL_Test*` suite |
| `Prefabs/` | Entities, items, NPCs |
| `Worlds/MainWorld` | Playable world |
| `Worlds/DebugWorld` | Dev/test layers + autotest |
| `Language/` | Localization (`everonlife_localization.st`) |
| `Configs/` | Systems, persistence, game mode |

## Development

**Setup:** [docs/setup.md](../docs/setup.md) — Workbench, CLI, symlinks.

**Symlink for faster iteration** (Windows example):

```powershell
New-Item -ItemType SymbolicLink `
  -Path "$env:USERPROFILE\Documents\My Games\ArmaReforger\profile\addons\LifeFramework" `
  -Target "C:\path\to\Life-Framework\addons\LifeFramework"
```

Or copy `LifeFramework/` into your Reforger profile `addons/` folder.

**Authoring guides** (in this directory):

- [GUIDE_DEBUG_WORLD_SCRIPTS.md](LifeFramework/GUIDE_DEBUG_WORLD_SCRIPTS.md)
- [GUIDE_COMPONENT_REFERENCE.md](LifeFramework/GUIDE_COMPONENT_REFERENCE.md)
- [GUIDE_CREATING_DEBUG_WORLD.md](LifeFramework/GUIDE_CREATING_DEBUG_WORLD.md)
