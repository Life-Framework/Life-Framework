# Life Framework — rigor pack (opencode skills)

A collection of opencode skills and subagents that encode the engineering discipline
this mod is built with. Modeled on Cursor's `pstack` (poteto-mode) philosophy: less
output, higher quality, and every change proven in-game. Adapted to opencode's skill
format and to the reality of building an Arma Reforger / Enfusion "Life" mod.

## The portable layer

The same knowledge is available to tools that cannot load opencode skills (Codex,
Cursor, Claude Code). The canonical, tool-agnostic copy of the factual rules lives in
`AGENTS.md` at the repo root: golden rules, hard EnforceScript lessons, Enfusion data
rules (prefabs, layouts, input, localization), and the verification ladder. Every
tool that reads `AGENTS.md` inherits it. The `.opencode/skills/` files are plain
markdown, so any agent can also read the playbooks and principles directly; opencode
is the only one that loads them as skills. **When a skill and `AGENTS.md` disagree,
`AGENTS.md` wins** — it is the portable contract.

## Install

Nothing to install. opencode scans `.opencode/skills/`, `.opencode/agent/`, and
`.opencode/command/` automatically. Restart opencode after pulling this directory if
you added it to an existing checkout.

## Get started

Use `/life-mode` at the start of any task that needs rigor:

```
/life-mode a bug where woodcutting doesn't drop logs when the tree is destroyed. repro first.
/life-mode add a job system tier. build it data-first, verify in the DebugWorld.
/life-mode review this refactor of the inventory quantity component.
```

Life-mode reads your request, matches it to a playbook, and runs the other skills as
the steps need them. It also applies the engineering principles (leaf `principle-*`
skills) and gates completion on the verification ladder (`enfusion-verify`).

Two more direct commands:

- `/prove` runs the verification ladder on the current change and reports evidence.
- `/interrogate` adversarially reviews the current diff before you ship.

## What is here

| Piece | Purpose |
|---|---|
| `skill/life-mode/` | The routing mode: playbooks + inline principles index. |
| `skill/life-mode/playbooks/` | Investigation, bug fix, feature, refactoring, prototype, runtime forensics, multi-phase plan, session pickup. |
| `skill/principle-*` | One engineering principle per leaf skill, each grounded in this repo (13 total). |
| `skill/enfusion-*` | The domain layer pstack lacks: API research, script authoring, prefab authoring, config/layout/keybind authoring, verification. |
| `skill/el-tdd`, `skill/enfusion-how`, `skill/enfusion-architect`, `skill/enfusion-blast-radius`, `skill/interrogate`, `skill/show-me-your-work`, `skill/unslop` | The situational rigor skills life-mode routes to. |
| `agent/life-agent.md` | Subagent that runs life-mode's style end to end. Spawn with `subagent_type: "life-agent"`. |
| `agent/el-reviewer.md` | Read-only adversarial reviewer. Spawn via `/interrogate`. |
| `command/life-mode.md`, `command/prove.md`, `command/interrogate.md` | The three slash commands. |

## What was deliberately left out

The pstack pack ships far more than this mod needs. Dropped as irrelevant here:
PR orchestration (babysit/shipping/autopilot), eval, perf/hillclimb, trace forensics,
visual parity, worktree cleanup, bot UIs, Cursor-specific model routing, TypeScript
conventions, and the recall/why evidence-mining skills. The skills here are the ones a
solo contributor or small team building an Enfusion Life mod actually reaches for.

## The principles

Core: laziness-protocol, foundational-thinking, subtract-before-you-add, model-the-domain,
boundary-discipline, type-system-discipline, make-operations-idempotent.
Verification: prove-it-works, fix-root-causes, sequence-verifiable-units.
Delegation: guard-the-context-window, never-block-on-the-human.
Meta: encode-lessons-in-structure.

## Hard lessons borrowed from the Overthrow mod

Several rules in `enfusion-script-authoring` and `enfusion-config-authoring` are
hard-won lessons from a sibling Reforger mod (`Overthrow/.claude/skills`): no ternary
operators, `ref` on both collections and elements, `EntityID` vs `RplId`, `BumpMe`
after replicated-value changes, RPC direction and host checks, persistence serializers
that are never called when unregistered, layout GUID rules, the WASD / VON / `a` /
d-pad / `shoulder_left` input traps, and the restart / dedicated-server / late-joiner
proof classes. The engine is the same, so the lessons transfer.

## Contributing

Add a skill by dropping `SKILL.md` in a new folder under `.opencode/skills/` with
frontmatter `name` and `description` (third person, front-loaded trigger keywords).
Add a playbook by extending `skill/life-mode/playbooks/` and naming it in the
life-mode playbook list. New checks belong in `tools/validation/` or `tools/test/`,
wired by `tools\cli` — that is the repo's way of encoding lessons in structure.

**Factual rules go in `AGENTS.md` first.** A hard-won lesson that is a fact (an
engine constraint, a GUID rule, an input trap) belongs in the portable contract so
every tool inherits it; the skill then references it. A lesson that is a workflow (a
step sequence, a cadence) belongs in a playbook or skill. Keep the two homes in sync;
AGENTS.md is canonical.