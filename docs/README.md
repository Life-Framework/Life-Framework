# Life Framework Docs

Use this index to find the current project guidance. The website renders these
same documents under `/docs`.

## Start here

1. Join the [Discord](https://discord.gg/MjMexJteqz) and introduce yourself.
2. Read [CONTRIBUTING.md](../CONTRIBUTING.md), then follow the [setup guide](setup.md).
3. Open the [playtest town](playtest.md), choose an issue, and build with the team.

Reference:

- [AGENTS.md](../AGENTS.md) is the engineering contract for people and agents.
- [Agent box](../agent-box/README.md) covers rendering setup and player-equivalent checks.

## Direction and decisions

- [Direction](direction.md) — what we are building, what playable means, and the milestones.
- [Design](design.md) — the principles behind the framework and its systems.
- [Decisions](decisions.md) — recorded product and design calls.

## Current plan

- [Roadmap](roadmap/README.md) — the live gameplay plan and current milestone.
- [M1 Playtest Town](roadmap/m1-playtest-town.md) — the player journey and acceptance bar.
- [M2 outline](roadmap/m2-outline.md) — the next milestone's themes.
- [Scopes](roadmap/scopes/README.md) — how to shape a bounded piece of work.
- [Testing plan](roadmap/testing/README.md) — test gates and per-setup proof.

## Engineering reference

- [Core vs pack](architecture/core-vs-pack.md) — what goes in the core addon and what goes in the flagship pack; [inventory](architecture/core-pack-inventory.md).
- [Features](features.md) — implemented behavior and fragile paths.
- [Persistence](persistence.md) — saved data, serializers, and restart rules.
- [Foundation design](foundation-design.md) — economy, trade, processing, and ownership primitives.
- [Worktrees](worktrees.md) — parallel changes, ports, validation, and shipping.
- [Dedicated server session](dedicated-session.md) — multiplayer proof on a dedicated server.
- [Debug log triage](debug-log-triage.md) — known boot messages and what they mean.
- [Street manifest](street-manifest.md) — the DebugWorld street layout tool.

Feature design notes live beside their code under
`addons/LifeFramework/Scripts/Game/Feature/*/Feature.md` and are indexed in
[`Feature/README.md`](../addons/LifeFramework/Scripts/Game/Feature/README.md).

## Research and community

- [Ecosystem map](ecosystem.md) — upstream code, Workshop dependencies, asset sources, and licences.
- [Life/RP benchmark](research/life-rp-benchmark.md) — genre patterns and design opportunities.
- [Asset pipeline](research/asset-pipeline.md) — agent-driven static 3D items, from Blender script to filmed in-game proof.
- [Altis Life framework config](research/altis-life-framework.md) — what its config layer tunes; ideas only, CC BY-NC-ND.
- [Overthrow survey](overthrow-survey.md) — systems and licence notes.
- [EveronLife forks](everonlife-forks.md) — upstream context and prior ports.
- [Discord server](community/discord.md) — channels, roles, and moderation.
- [Support and donations](community/support.md) — project costs and support settings.

The gameplay plan is maintained in `docs/roadmap/`; GitHub milestones and
issues carry current scope status.
