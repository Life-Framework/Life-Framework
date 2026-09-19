---
name: life-mode
description: "Use for non-trivial Life Framework work: bugs, features, refactors, runtime issues, or requests to verify a change. Route only the skills the task needs and finish with scope-appropriate evidence."
---

# Life Mode

Use `AGENTS.md` as the canonical contract. Do not copy its rules into plans or
restate them in replies. This skill supplies routing and a small completion gate.

## Route by task

- How/why question: `enfusion-how` or `enfusion-api-research`; do not edit.
- Bug: reproduce, isolate the cause, fix it, then add `el-tdd` when a cheap test exists.
- Feature: choose the owner/data shape, implement the smallest vertical slice, verify it.
- Refactor: preserve behavior, check callers/references, run the relevant validation.
- Script, prefab, or config: load the matching authoring skill only when editing that artifact.
- Cross-boundary or risky change: use `enfusion-architect`, `enfusion-blast-radius`, or `interrogate` only as needed.
- Long autonomous work: use `show-me-your-work`; otherwise skip the decision trail.

## Working rules

- Research an unfamiliar Enfusion API, class, component, prefab, or asset before using it.
- Prefer deletion, reuse, inheritance, and the smallest change. Do not add abstractions speculatively.
- Proceed with reversible work. Ask only for product choices or irreversible actions.
- Use `parallel-worktrees` before `build`, `test`, `dev`, `serve`, or `ci`.
- Delegate bulk inspection when it saves context; inspect delegated diffs yourself.

## Completion

1. Inspect the diff and affected resources.
2. Run `tools\cli validate` for code/resource changes.
3. Run the cheapest relevant test. Use `tools\cli test` or manual runtime proof when behavior, networking, persistence, UI, or world state changed.
4. Report only what changed, what passed, and any unverified manual step.

Use `enfusion-verify` when the proof plan is non-obvious or the user explicitly
asks whether it works. Use `interrogate` before shipping a contested or risky diff.
