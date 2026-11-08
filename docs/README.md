# Life Framework Docs

Project documentation for the Life Framework Arma Reforger mod.

## Start here

| Doc | Audience |
| --- | --- |
| [Setup guide](setup.md) | New contributors — install, toolchain paths, Workbench, CLI, MCP, troubleshooting |
| [Playtest](playtest.md) | Play DebugWorld on your own machine and send feedback |
| [Agent box](../agent-box/README.md) | GPU or remote box — setup script, player-equivalent checks, screenshots, roadmap street pass, DebugWorld chains |
| [CONTRIBUTING.md](../CONTRIBUTING.md) | How to contribute, PR checklist, AI harness |
| [AGENTS.md](../AGENTS.md) | Engineering contract — GUID rules, EnforceScript traps, verification ladder |

## Product and plan

- [Design](design.md) — vision, values, MIT commitment, in-world-first
  interaction ladder, economy/profession/ownership principles, base mod vs
  extension, feature checklist. The "why" behind every feature's `Feature.md`.
- [Roadmap](roadmap/README.md) — the live plan: phases and scopes for the
  playable mod. Testing gates in [roadmap/testing](roadmap/testing/README.md).
- [Community roadmap scrape](roadmap.md) — copy of the life-framework.org
  roadmap, kept for reference.

Feature design context files live beside the code in
`addons/LifeFramework/Scripts/Game/Feature/*/Feature.md` (indexed by
[`Feature/README.md`](../addons/LifeFramework/Scripts/Game/Feature/README.md)).

## Engineering

- [Features](features.md) — implemented features, behavior contracts, fragile paths.
- [Testing plan](roadmap/testing/README.md) — test gates, file rules, and per-setup cases.
- [Persistence](persistence.md) — save data, serializers, autosave/shutdown orchestration, rules.
- [Foundation design](foundation-design.md) — Economy, Trade, Processing, Real Estate primitives.
- [Worktrees](worktrees.md) — parallel agent worktrees, ports, gate, ship.
- [Debug log triage](debug-log-triage.md) — what every known DebugWorld boot line means. Read before chasing a log message.
- [Dedicated server session](dedicated-session.md) — the dedicated-server proof pass.
- [Street manifest](street-manifest.md) — DebugWorld street layout tool.

## Surveys

- [Overthrow survey](overthrow-survey.md) — what Overthrow.Arma4 offers and its license terms.
- [EveronLife forks](everonlife-forks.md) — what the EveronLife forks hold and what was already ported.
- [Ecosystem leverage map](ecosystem.md) — community repos, Workshop dependencies, free asset sources, licenses, and the import policy.

## Agent guidance

When picking up roadmap work, start from [roadmap/README.md](roadmap/README.md).
Coordinate on [Discord](https://discord.com/invite/MjMexJteqz) before large
efforts.

All factual engineering rules live in [AGENTS.md](../AGENTS.md); agent
workflows (playbooks, authoring guides) live in `.claude/skills/`.
