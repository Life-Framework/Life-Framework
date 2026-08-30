# Parallel worktrees for agents

How up to ~20 agents work on the Life Framework at once without ever breaking
your world-editor session on `main`, and how their finished work auto-merges.

## The model

| Checkout | Branch | Purpose |
|---|---|---|
| `Life-Framework` (main) | `main` | **Yours.** The world editor. Never touched by agents. |
| `Life-Framework-ws-<slug>` | `ws/<slug>` | One per feature, one per agent. Full git worktree. |

The CLI enforces the boundary: `build / test / dev / serve / ci` refuse to run
in the main checkout. Agents work in worktrees; heavy commands there are
harmless to your editor because the worktree is a separate directory.

## Per-worktree isolation

- **Ports** — every worktree gets a unique `(gamePort, a2sPort)` pair,
  allocated at creation (2001+N·10 / 17777+N·10). `wt test` boots the dedicated
  server with `-bindPort`/`-a2sPort`, so many test runs happen at once.
- **Build** — the headless Workbench build is the one truly shared resource
  (shader cache, resource DB). It is serialized by a machine-wide lock file:
  one build at a time, everyone else `--wait`s. `cli dev` skips the build
  entirely, so the everyday loop never contends.
- **Hub** — coordination state lives in `<main>/tmp/wt/` (git-ignored):
  `state.json` (registry + ports) and `locks/`. Derived from `git worktree
  list`, so it survives worktrees created by hand.

## Daily use

```sh
tools\cli wt new <feature>      # create worktree + branch + ports
tools\cli wt list               # who's working, ports, dirty/merged state
tools\cli wt test <slug>        # run the ELTEST suite in that worktree
tools\cli wt dev <slug>         # fast loop in that worktree (no build)
tools\cli wt gate <slug>        # validate + build + test (tier=all)
tools\cli wt ship <slug>        # sync -> gate -> push -> PR -> auto-merge
tools\cli wt prune <slug>       # remove a merged worktree + branch
```

`wt <command> <slug>` can be run from anywhere; it always uses the latest
tooling against the named worktree.

## Auto-merge

`wt ship` runs the full gate in the worktree (validate + build + test tier=all),
pushes `ws/<slug>`, opens a PR into `main`, and merges it — no babysitting.
`--pr-only` leaves the PR open if you want to review. If the gate fails or
`origin/main` conflicts, the ship stops with a message; the agent fixes and
re-runs the same command.

**One-time setup**: GitHub CLI must be authenticated once per machine
(`gh auth login`). The REST API fallback works with a `GITHUB_TOKEN` env var
instead.

## Keeping your world editor safe

- The main checkout stays clean; agents never run heavy commands there (CLI
  enforces it, `--force` overrides it).
- One caveat: do not open the world editor while an agent's headless build is
  running — concurrent Workbench instances can collide on the shader cache.
  `cli wt list` does not show running builds; the build lock file at
  `<main>/tmp/wt/locks/build.lock` indicates one in flight.
- Merge your own local `main` work first: agent worktrees branch from
  `origin/main`, so uncommitted or unpushed changes in your checkout are not
  visible to agents until you push.

## Cleanup

Merged worktrees show `merged (prune me)` in `cli wt list`. `cli wt prune
<slug>` verifies the branch is merged and clean before removing it; `--force`
discards unmerged work. `cli validate` warns you about worktrees that are ready
to prune and about any registry inconsistency.

## Under the hood (files)

- `tools/wt.mjs` — hub, locks, ports, git helpers, gh/REST PR + merge.
- `tools/cli.mjs` — `wt` command group, main-checkout guard, worktree-aware
  build/test/dev/serve/ci.
- `tools/validation/validate-worktrees.ps1` — registry consistency (port
  uniqueness, missing dirs, stale merged worktrees).
- `.githooks/pre-commit` → `validate-repo.ps1` — blocks `main` commits from
  linked worktrees.
- `.opencode/skills/parallel-worktrees/SKILL.md` — the agent-facing workflow.