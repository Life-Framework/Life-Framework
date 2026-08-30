---
name: parallel-worktrees
description: "Use before ANY build/test/dev/serve/ci command, or when starting work on this mod as an agent. The main checkout is the world-editor copy and heavy commands refuse to run there; you work in your own git worktree and ship via an auto-merged PR. Trigger on 'start a worktree', 'where do I work', 'run the tests', 'ship this feature', 'merge to main', 'parallel agents'. Use ONLY for the workflow mechanics of this repo, not for Enfusion authoring."
---

# Parallel worktrees

Up to ~20 agents work on disparate features at once. The main checkout (the
directory you opened opencode in) is reserved for the human and the world
editor. You never touch it. You own a **linked git worktree** in a sibling
directory named `Life-Framework-ws-<slug>` (next to the main checkout) on
branch `ws/<slug>`, and your finished feature is PR-merged into `main`
automatically.

Read `AGENTS.md` "Parallel worktrees" before anything else — this skill is the
workflow on top of that contract.

## Why worktrees are enforced

The compile step (headless Workbench `build`) and the test server share one
machine. Without isolation, an agent's build breaks the human's editor and two
agents' test servers collide on ports. The worktree system solves both:

- every worktree is a full checkout, so agent builds/tests touch nothing the
  editor reads;
- every worktree gets a unique port pair, so test runs are parallel;
- one Workbench build runs at a time (lock file), so builds queue cleanly.

The CLI enforces it: `build / test / dev / serve / ci` refuse to run in the
main checkout. Do not work around that with `--force` except for a deliberate
one-off the human asked for.

## Start a worktree

```sh
tools\cli wt new <feature-slug>
tools\cli wt list
```

`wt new` fetches `origin`, creates `Life-Framework-ws-<slug>` at branch
`ws/<slug>`, allocates a port pair, and registers it. Then:

```sh
cd ../Life-Framework-ws-<slug>
```

(relative to the main checkout) and work there exactly as you would on main —
full checkout, edit, `git add`, `git commit` (the pre-commit validator runs).
Never create worktrees by hand; `wt new` is the only path that registers the
ports.

## Iterate (the fast loop)

```sh
tools\cli dev --tier fast          # from INSIDE the worktree
tools\cli wt dev <slug> --tier fast  # from anywhere
```

`dev` skips the headless build (the server compiles scripts at boot) and runs
the ELTEST suite on the worktree's ports, then dumps the `[ELDebug:*]` lines.
Use it for iteration. Only the full `test`/`gate`/`ship` path uses the scarce
Workbench build.

If the worktree's own `tools\cli` is older than main's (it branched before a
tooling change), the version-safe entry point is `tools\cli wt <command> <slug>`
run from the main checkout — it always uses the latest tooling against your
worktree. Once you `wt sync`, the worktree's own `tools\cli` matches.

## Gate before shipping

```sh
tools\cli wt gate <slug>      # validate + build + test (tier=all)
tools\cli wt gate <slug> --wait   # queue behind another worktree's build
```

The build is serialized machine-wide (lock file). `--wait` queues up to 15
minutes; without it, a busy build fails fast so you know to wait. While a
headless build runs, the human must not open the world editor — if you start a
build, say so.

## Ship (auto-merged, no babysitting)

```sh
tools\cli wt ship <slug>
```

Flow: sync `origin/main` into your branch → refuse if uncommitted changes or
conflicts → run the full gate (validate + build + test tier=all) → push →
create a PR → merge into `main`. `--pr-only` leaves the PR open for review
instead of auto-merging.

Requirements before shipping:

- commit first — ship refuses on a dirty tree;
- the gate must pass — a failing gate stops the ship;
- GitHub CLI authenticated once on this machine (`gh auth login`), or
  `GITHUB_TOKEN` set (REST fallback).

If the ship stops (conflict, gate failure, merge conflict), fix it and re-run
the same command. The PR URL is printed if the PR was created.

## Keep it on the branch

- The worktree must stay on `ws/<slug>`. `cli wt test/build/ship` refuse if it
  drifts; `cli wt sync` checks it back out.
- Never commit to `main` from a worktree — the pre-commit validator blocks it.
- `wt sync <slug>` merges `origin/main` into your branch before you ship;
  merge conflicts are yours to resolve, then re-run ship.

## Cleanup

Merged worktrees show `merged (prune me)` in `cli wt list`. Remove with:

```sh
tools\cli wt prune <slug>        # refuses unless merged + clean
tools\cli wt prune <slug> --force   # discard unmerged work deliberately
```

## Pitfalls

- **Don't run `cli build/test/dev/serve/ci` in the main checkout.** The CLI
  refuses; use the worktree or `wt <command> <slug>`.
- **Don't hand-edit the hub.** `<main-checkout>/tmp/wt/` is generated. `wt
  list` repairs drift; `validate-worktrees` checks it.
- **Ports are allocated once.** `wt new` assigns them; a removed worktree does
  not release them (fine — ~200 slots for 20 agents).
- **A stale lock** (crashed agent) is auto-stolen after its PID dies or after
  10 minutes; `--wait` handles contention.