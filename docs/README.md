# Life Framework Docs

Project documentation for the Life Framework Arma Reforger mod.

## Start here

| Doc | Audience |
| --- | --- |
| [Setup guide](setup.md) | New contributors — install, Workbench, CLI, MCP, troubleshooting |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | How to contribute, PR checklist, AI harness |
| [AGENTS.md](../AGENTS.md) | Engineering contract — GUID rules, EnforceScript traps, verification ladder |

## Project plan

- [Roadmap](roadmap.md) — phased development plan. Phase 0 in progress; later
  phases open for contributors.
- [Vision](vision.md) — philosophy and goals (MIT-licensed, extensible, 1989-era base).
- [Design philosophy](design-philosophy.md) — in-world-first interaction ladder and
  shippable-V1 approach. The "why" behind every feature's `Feature.md`.

Feature design context files live beside the code in
`addons/LifeFramework/Scripts/Game/Feature/*/Feature.md` (indexed by
[`Feature/README.md`](../addons/LifeFramework/Scripts/Game/Feature/README.md)).

## Engineering

- [Features](features.md) — implemented features, behavior contracts, fragile paths.
- [Test plan](test-plan.md) — test format and per-feature cases.
- [Foundation design](foundation-design.md) — Economy, Trade, Processing, Real Estate primitives.

## Mod authoring guides

In `addons/LifeFramework/`:

- [Debug World scripts](GUIDE_DEBUG_WORLD_SCRIPTS.md) — attach and configure test prefabs.
- [Component reference](GUIDE_COMPONENT_REFERENCE.md) — component inventory.
- [Creating Debug World](GUIDE_CREATING_DEBUG_WORLD.md) — build a debug layer.

## Agent guidance

When picking up roadmap work, prefer in-progress Phase 0 items, then Phase 1
core systems. Coordinate on [Discord](https://discord.com/invite/MjMexJteqz)
before large efforts.

All factual engineering rules live in [AGENTS.md](../AGENTS.md). Workflow
skills for opencode live in [`.opencode/`](../.opencode/README.md).
