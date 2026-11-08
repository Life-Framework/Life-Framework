---
name: life-mode
description: "Use for non-trivial Life Framework work: bugs, features, refactors, investigations, runtime issues, or requests to verify a change. Routes the task to a playbook, loads only the skills it needs, and finishes with scope-appropriate evidence."
---

# Life Mode

`AGENTS.md` is the contract. Do not restate its rules in plans or replies.
This skill routes the task and sets a small completion gate.

## Route by task

Pick the matching playbook in `playbooks/` and follow its steps:

- How/why question, "walk me through X": `investigation.md` (read-only).
- Bug: `bug-fix.md`, with **el-tdd** when a cheap test exists.
- Feature: `feature.md` (data shape and boundaries first).
- Refactor: `refactoring.md`.
- Live symptom with no known cause: `runtime-forensics.md`.
- Empirical design fork: `prototype.md`.
- Work spanning phases or stacked commits: `multi-phase-plan.md`.
- Resuming someone else's work: `session-pickup.md`.

Load an authoring skill only when editing that artifact:
**enfusion-script-authoring** (`.c`), **enfusion-resource-authoring** (`.et`,
`.conf`, `.layout`, input, localization, layers). Use
**enfusion-api-research** before touching an unfamiliar Enfusion API,
**enfusion-blast-radius** before a change with hidden coupling, **interrogate**
before shipping a risky diff, **show-me-your-work** for long autonomous work.
**principles** holds the six heuristics the playbooks cite.

## Working rules

- Research an unfamiliar Enfusion API, class, component, prefab, or asset before using it.
- Prefer deletion, reuse, inheritance, and the smallest change. Do not add abstractions speculatively.
- Proceed with reversible work. Ask only for product choices or irreversible actions.
- Heavy commands (`build`, `test`, `dev`, `serve`, `ci`) run only in a worktree; see `docs/worktrees.md`.
- Delegate bulk inspection when it saves context; inspect delegated diffs yourself.

## Completion

1. Inspect the diff and affected resources.
2. Run `tools\cli validate` for code/resource changes.
3. Run the cheapest relevant test. Use `tools\cli test --no-build` or manual runtime proof when behavior, networking, persistence, UI, or world state changed.
4. Report only what changed, what passed, and any unverified manual step.

Use **enfusion-verify** when the proof plan is non-obvious or the user asks
whether it works.
