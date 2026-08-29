# Life Framework

Open-source life/RP server framework for Arma Reforger. A single mod — built
as the Reforger GameProject `addons/LifeFramework/LifeFramework.gproj` — with
economy, jobs, survival, crime/police, trading, and persistence, set in 1989
to use Reforger's native assets.

> **Status: Pre-alpha.** A working single-player/debug skeleton exists: two
> worlds, most core systems implemented in Enforce Script, and an in-game test
> suite. This is **not yet a shippable server release** — expect breaking
> changes and incomplete rough edges.

## Features

What's implemented today (each backed by working code and DebugWorld prefabs):

- **Economy & banking** — bank accounts, ATMs (deposit/withdraw), money
  handling and cash stacks.
- **Trading** — shops with buy/sell, trader NPCs.
- **Jobs & levels** — a job system gated by player level and skill-point
  licenses.
- **Character creation & accounts** — persistent player accounts, character
  and faction selection, ID display.
- **Survival** — hunger and thirst with a HUD and consumable effects.
- **Gathering & processing** — a chain of gather/process actions: mining,
  logging/woodcutting, gravel, sand, apples, plums, tomatoes, cement, and
  more (mining/logging resources are destructible in the world).
- **Crime & police** — robbery (people and vehicles), police fines, duty,
  confiscation, and whitelisted police/license roles.
- **Vehicle license plates** — generated plates assigned to vehicles.
- **Inventory** — quantity/stacking system with split dialog and inventory
  UI patches.
- **Notifications** — in-game toast/notification system.
- **NPCs** — base NPC entity for traders, police, and shops.
- **Era** — 1989, using only Arma Reforger's native assets and setting.

The framework is time-period agnostic at its core: additional asset mods can
shift the setting to any era without touching the gameplay systems.

## Requirements / Dependencies

- **Arma Reforger** (base game) and **Arma Reforger Tools (Workbench)** to
  build and run.
- **No third-party addons.** The mod uses Reforger's first-party persistence
  system (`SCR_PersistenceSystem` + serializers bound in
  `Configs/Systems/Persistence/LifeFramework.conf`). The EPF (Enfusion
  Persistence Framework) dependency was removed; the gproj declares only the
  base game.

## Getting started / Development

The mod lives in `addons/LifeFramework`:

- `Missions/EveronLifeGameMode.conf` — the playable scenario (loads
  `MainWorld`).
- `Worlds/MainWorld` — the main playable world.
- `Worlds/DebugWorld` — the development/test bed, with a layer per feature
  and an in-game `EL_Test*` suite (`Scripts/Game/Tests/`) that runs under the
  `EL_AUTOTEST` define and writes a report.

Workflow:

1. Open `addons/LifeFramework/LifeFramework.gproj` in Workbench.
2. Build the project (PC).
3. Run `EveronLifeGameMode.conf` or the DebugWorld world.

The repo ships a small dev CLI (`tools\cli`, a zero-dependency Node.js
script) for tooling and checks:

```sh
tools\cli status        # toolchain + MCP server state
tools\cli validate      # repo consistency checks (artifacts, GUIDs, metas)
tools\cli lint          # run tools/lint/* checks
tools\cli test          # run tools/test/* checks
tools\cli mcp ...       # install/update/verify/enable/disable MCP servers
```

`tools\cli validate` is also wired as a git pre-commit hook.

## Repository layout

```
addons/               The mod (LifeFramework.gproj + scripts, prefabs, worlds, UI, language)
server/               Placeholder for dedicated-server configs and launch scripts (see server/README.md)
tools/                Dev CLI (cli.mjs), MCP server tooling, validation/lint/test scripts
assets-source/        Source asset licenses/attribution (LICENSES.md)
```

## Credits / Provenance

Life Framework is a fork/continuation of the **Everon Life RPG Framework** by
Arkensor (https://github.com/EveronLife/EveronLife), released under the MIT
license. The `EL_` class prefix used throughout the code reflects that
heritage. The upstream project and its community remain the source of much of
this codebase's design; please respect that project and its contributors.

## License

MIT — see [LICENSE](LICENSE). By contributing, you agree your contributions
are licensed under the MIT License. See [CONTRIBUTING.md](CONTRIBUTING.md).