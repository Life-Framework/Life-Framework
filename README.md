# Life Framework

Open-source life/RP server framework for [Arma Reforger](https://www.bohemia.net/arma-reforger).
One mod — `addons/LifeFramework/LifeFramework.gproj` — with economy, jobs,
survival, crime/police, trading, and persistence, set in 1989 using Reforger's
native assets.

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Discord](https://img.shields.io/badge/Discord-join-5865F2?logo=discord&logoColor=white)](https://discord.com/invite/MjMexJteqz)
[![Contributing](https://img.shields.io/badge/Contributing-welcome-green)](CONTRIBUTING.md)

> **Status: Pre-alpha.** A working debug skeleton exists — two worlds, core
> systems in EnforceScript, and an in-game test suite. **Not yet a shippable
> server release.** Expect breaking changes.

## Quick links

| | |
| --- | --- |
| **Get started** | [Development setup](docs/setup.md) |
| **Contribute** | [CONTRIBUTING.md](CONTRIBUTING.md) · [Code of Conduct](CODE_OF_CONDUCT.md) |
| **Engineering** | [AGENTS.md](AGENTS.md) · [Docs index](docs/README.md) |
| **Community** | [Discord](https://discord.com/invite/MjMexJteqz) |
| **Roadmap** | [docs/roadmap.md](docs/roadmap.md) |

## Features

What's implemented today (each backed by code and DebugWorld prefabs):

- **Economy & banking** — accounts, ATMs, cash stacks
- **Trading** — shops, trader NPCs, buy/sell
- **Jobs & levels** — jobs gated by level and skill-point licenses
- **Character creation & accounts** — persistence, faction selection, ID display
- **Survival** — hunger/thirst HUD and consumables
- **Gathering & processing** — mining, logging, farming chains, destructible resources
- **Crime & police** — robbery, fines, duty, confiscation, role whitelists
- **Vehicle license plates** — generated plates
- **Inventory** — quantity/stacking, split dialog, UI patches
- **Notifications** — in-game toasts
- **NPCs** — base entity for traders, police, shops

The core is era-agnostic: asset packs can shift the setting without rewriting
gameplay systems.

## Requirements

- **Arma Reforger** and **Arma Reforger Tools (Workbench)** — build and play
- **Arma Reforger Server** (Steam app 1874900) — automated test pipeline
- **Node.js 18+** — dev CLI and MCP tooling
- **No third-party addons** — persistence uses Reforger's first-party
  `SCR_PersistenceSystem` (see `Configs/Systems/Persistence/LifeFramework.conf`)

Full install steps: **[docs/setup.md](docs/setup.md)**.

## Development

The mod lives under `addons/LifeFramework/`:

| Path | Purpose |
| --- | --- |
| `Missions/EveronLifeGameMode.conf` | Playable scenario → MainWorld |
| `Missions/EL_DebugTest.conf` | Test scenario → DebugWorld + `EL_Test*` suite |
| `Worlds/MainWorld` | Main playable world |
| `Worlds/DebugWorld` | Dev/test bed — one layer per feature |
| `Scripts/Game/Tests/` | In-game automated tests (`EL_AUTOTEST` define) |

**Workbench:** open `LifeFramework.gproj` → Build (PC) → play a scenario.

**CLI** (cross-platform; Windows shim `tools\cli`):

```sh
node tools/cli.mjs status              # toolchain + MCP state
node tools/cli.mjs validate            # repo hygiene (GUIDs, metas, artifacts)
node tools/cli.mjs build               # headless Workbench build
node tools/cli.mjs test --tier fast    # logic-tier in-game tests
node tools/cli.mjs test                # full test suite on DebugWorld
node tools/cli.mjs ci                  # validate + build + test
```

AI-assisted development is encouraged — MCP servers, skills, and expectations
are documented in [CONTRIBUTING.md](CONTRIBUTING.md).

## Repository layout

```
addons/LifeFramework/   The mod (gproj, scripts, prefabs, worlds, UI, language)
docs/                   Roadmap, features, test plan, setup guide
server/                 Dedicated-server configs and launch scripts
tools/                  Dev CLI, validation, MCP tooling
assets-source/          Source asset licenses (LICENSES.md)
.opencode/              AI skills and playbooks (opencode)
AGENTS.md               Engineering contract for humans and AI agents
```

## Credits

Fork/continuation of the **Everon Life RPG Framework** by
[Arkensor](https://github.com/EveronLife/EveronLife) (MIT). The `EL_` class
prefix reflects that heritage.

## License

MIT — see [LICENSE](LICENSE). Contributions are licensed under MIT;
see [CONTRIBUTING.md](CONTRIBUTING.md).
