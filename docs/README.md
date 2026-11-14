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

## For server owners

- [Install a server](owner/install.md) — from a server install to a player joining, then the save across restarts.
- [Add a delivery job](owner/add-job.md) — a depot and a drop on your own map, then the XP and levels in `config.json`.
- [Add a shop](owner/add-shop.md) — a counter, its stock, and its prices, or a catalogue outlet.
- [Owner config check](config/owner-config.md) — `tools\cli config-check <config.json>`: what the server would log about your config.json, before a restart.

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

- [Performance budget](architecture/performance.md) - the 128-player cost budget, the `perf/budget-128` gate, `tools\cli perf`, measured numbers.
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
- [Blender bridge](blender-bridge.md) — `tools\cli blender`: drive the user's open Blender (or a headless one), show a model in it, take pictures.
- [Asset pipeline](research/asset-pipeline.md) — agent-driven static 3D items, from Blender script to filmed in-game proof.
- [Altis Life framework config](research/altis-life-framework.md) — what its config layer tunes; ideas only, CC BY-NC-ND.
- [Integration spec](research/integration-spec.md) — ideas from a live Reforger life server cross-referenced with Overthrow and our code, with the 2026-11-11 decisions; every item is filed (M3 #746, M5 #632 and #633, M4 #231, M6 #747).
- [Reference projects](roadmap/references.md) — public Reforger projects to check for ideas, with licences.
- [Overthrow survey](overthrow-survey.md) — systems and licence notes.
- [EveronLife forks](everonlife-forks.md) — upstream context and prior ports.
- [Discord server](community/discord.md) — channels, roles, and moderation.
- [Support and donations](community/support.md) — project costs and support settings.
- [Discord onboarding](community/onboarding.md) and [the AI bot playbook](community/bot-playbook.md) — what a new member sees, and how the bot talks to people.
- [Outreach kit](community/outreach/README.md) — one file per community, the [FAQ](community/outreach/faq.md), and the [tracker](community/outreach/tracker.md).
- [Trailer](marketing/trailer/script.md) (#571): the script, the [storyboard](../video/trailer-571.json), the [shot checklist](marketing/trailer/shot-checklist.md) and the [15 s and 6 s cuts](marketing/trailer/cutdowns.md).
- [Quorum sign-off](playtest/quorum.md) and the [playtest session playbook](playtest/session-playbook.md) — who confirms a feature, and how a session runs.

The gameplay plan is maintained in `docs/roadmap/`; GitHub milestones and
issues carry current scope status.
