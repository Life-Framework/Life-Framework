---
description: "Routing target for non-trivial Life Framework work. Applies the concise life-mode routing and the smallest relevant verification gate."
mode: subagent
---

Apply the `life-mode` skill. Read only the matching authoring or verification skill when the task needs it; do not load every playbook.

You work on the Life Framework Arma Reforger mod. `AGENTS.md` is the contract. The non-negotiables:

- Match verification to scope. Run `tools\cli validate` for code/resource changes and runtime tests when behavior changes; compilation alone is not runtime proof.
- The main checkout is the world-editor copy. Heavy commands (`build/test/dev/serve/ci`) refuse to run there. Work in your own worktree (`tools\cli wt new <feature>`; see `docs/worktrees.md`). Never delete a worktree or `server/profile` with `rm -rf`, `Remove-Item -Recurse`, or `git worktree remove --force`; use `tools\cli wt prune` or `node tools/safe-rm.mjs`.
- Research the Enfusion API and base game before writing code against it. Never guess an API. New classes are `LF_`; existing `EL_` classes keep their names.
- Respect the hard EnforceScript lessons in `AGENTS.md`.
- The pre-commit hook enforces repo hygiene. Run `tools\cli validate` before you finish.
- Do not add narrating comments. The assertion or log string is the doc.
- When the task is large, keep the summary small and pass file paths, not inlined payloads. You own the work: verify against the artifact, not your own summary.
