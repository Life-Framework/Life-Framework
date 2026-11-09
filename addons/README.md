# Addons

`addons/` contains the Life Framework core and the flagship content pack.

## Structure

Two Reforger GameProjects:

```
addons/
├── LifeFramework/          # the core: LifeFramework.gproj, scripts, prefabs, worlds, UI
└── LF_Everon1989/          # the 1989 Everon pack: skeleton, not loaded yet
```

`LifeFramework/LifeFramework.gproj` declares the project, language configs, and
dependencies (base game only — no third-party addons). EnforceScript, prefabs,
worlds, UI layouts, and localization all live under `LifeFramework/` today.

`LF_Everon1989/LF_Everon1989.gproj` depends on the base game and on the core by
its GUID. The core never references the pack. What goes where:
[`docs/architecture/core-vs-pack.md`](../docs/architecture/core-vs-pack.md).
Another setting is another pack with its own ID and GUID, never a folder
inside `LifeFramework`.

## Key paths

| Path | Purpose |
| --- | --- |
| `Scripts/Game/` | EnforceScript (`LF_*` classes) |
| `Scripts/Game/Tests/` | In-game `LF_Test*` suite |
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

**Authoring references:**

- Per-feature design and component notes: [`Feature/README.md`](LifeFramework/Scripts/Game/Feature/README.md)
  and each feature's `Feature.md`.
- Implemented behavior contracts: [`docs/features.md`](../docs/features.md).
- DebugWorld: `Worlds/DebugWorld/DebugWorld.ent` is 0 bytes on purpose; each
  feature is its own layer in `Worlds/DebugWorld/DebugWorld_Layers/*.layer`.
