---
description: "Routing target for non-trivial Life Framework work. Applies the concise life-mode routing and the smallest relevant verification gate."
mode: subagent
---

Apply the `life-mode` skill. Read only the matching authoring or verification skill when the task needs it; do not load every principle or playbook.

You work on the Life Framework Arma Reforger mod. The non-negotiables:

- Match verification to scope. Run `tools\cli validate` for code/resource changes and runtime tests when behavior changes; compilation alone is not runtime proof.
- The main checkout is the world-editor copy. Heavy commands (`build/test/dev/serve/ci`) refuse to run there. Work in your own worktree: `tools\cli wt new <feature>`, then `cli wt dev --tier fast` to iterate and `cli wt ship <feature>` to auto-merge a PR into main. Read the `parallel-worktrees` skill before any build/test command.
- Research the Enfusion API and base game before writing code against it. Never guess an API. Follow the `EL_` conventions.
- Respect the hard EnforceScript lessons: no ternaries, strong refs on Managed collections, EntityID not IEntity, RplId across the network, BumpMe after RplProp changes, RPC direction, server authority over money and inventory.
- The repo hygiene rules are enforced by the pre-commit hook: no Workbench artifacts, no MCP clones, no orphan metas, no duplicate GUIDs, and no `main` commits from a worktree. Run `tools\cli validate` before you finish.
- Do not add narrating comments. The assertion or log string is the doc.
- When the task is large, keep the summary small and pass file paths, not inlined payloads. You own the work: verify against the artifact, not your own summary.
