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

- **Server port** — every server boot on this machine listens on 2001. In
  `-server <world>` mode the engine ignores `-bindPort` (it only overrides a
  `-config` JSON, and `-config` cannot be combined with `-addons`; checked
  on 1.9.0.37, log line `Starting RPL server, listening on address 0.0.0.0:2001`).
  So `test` (every tier), `dev`, `wt e2e`, and `serve` take one machine-wide
  `server-2001` lock: suites queue instead of colliding with
  `Unable to start replication`. `serve` fails fast when the lock is held.
  The per-worktree `(gamePort, a2sPort)` pair from `wt list` is still passed
  and is what a2s uses.
- **Resource database** — `resourceDatabase.rdb` is gitignored. `wt new`, `test`,
  `dev`, `serve`, and `wt e2e` copy `<main-checkout>/addons/LifeFramework/resourceDatabase.rdb` into
  the worktree when that copy is missing, or smaller and older. A worktree copy newer than main's
  came from this worktree's build and is the only one listing the resources the branch adds; a
  new prefab missing from it logs `Wrong GUID/name` and replication refuses the entity, so its
  inventory operations never complete. After adding a resource, run one `test` with the build.
  Do not commit it.
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
tools\cli wt gate <slug>        # validate --full + build + test (tier=all)
tools\cli wt ship <slug>        # gate -> push -> PR -> auto-merge (merges main only on conflict)
tools\cli wt prune <slug>       # remove a merged worktree + branch
```

`wt <command> <slug>` can be run from anywhere; it always uses the latest
tooling against the named worktree.

## Workbench per worktree

Each linked checkout has its own `addons/LifeFramework/LifeFramework.gproj`, so
Workbench can be pointed at a feature branch without changing the main editor
project. Run `tools\cli wt open <slug> --workbench`; it launches that checkout's
project with a worktree-specific profile directory.

Use one Workbench instance at a time. The project files and profile are isolated,
but the Workbench shader cache is shared by the machine. Stop the current editor
before opening another worktree to avoid cache contention and stale resource
views. Do not run a headless build while an interactive Workbench is open.

## Auto-merge

`wt ship <slug> [--closes N] [--body-file F]`:

1. Refuses a dirty tree or a branch with no commits beyond `origin/main`.
2. Merges `origin/main` only when `git merge-tree` reports a conflict (the
   merge drivers in `tools/merge/` then resolve the generated files). A clean
   branch ships as is.
3. Runs the gate: `validate --full` (with the Workbench compile) + headless
   build + `test` tier=all.
4. Pushes `ws/<slug>`, opens the PR (or reuses the open one on a re-run).
   `--closes N` starts the body with `Closes #N`; `--body-file F` supplies the
   body (keep it in `<worktree>/tmp/`).
5. Queues `gh pr merge --auto --merge`: GitHub merges once the required
   `validate` check and reviews pass, and deletes the branch.
6. Prints `PR <url>, head <sha>`.

`--pr-only` stops after step 4. If the gate fails or the merge of main
conflicts, the ship stops with a message; fix, commit, and re-run the same
command. Do not merge `origin/main` by hand unless GitHub reports the PR
`CONFLICTING` or you need code from main.

## Keeping PRs moving (`prs`)

Auto-merge stalls in two ways. Main moves after the agent has left, the PR
goes `CONFLICTING` (`mergeStateStatus` `DIRTY`) or `BEHIND`, and GitHub never
merges it. Or the checks are green and auto-merge is on, but the PR sits open
with `UNKNOWN` until someone merges it by hand.

```sh
tools\cli prs status                     # read-only: every open ws/* PR
tools\cli prs heal --dry-run [<n>...]    # what heal would do, nothing touched
tools\cli prs heal [<n>...]              # do it (maintainer/lead)
```

`prs status` lists number, state, auto-merge on/off, checks, age, and the
worktree: `LIVE` when any file in it changed in the last 15 minutes
(modified or untracked files, the worktree's git index/HEAD/reflog, `tmp/`,
server logs), `idle`, or `none`. An `UNKNOWN` state is re-read through the REST
API, which makes GitHub compute it.

`prs heal`, per PR:

- **`DIRTY` or `BEHIND`, worktree not LIVE.** In its worktree (or a temporary
  detached `Life-Framework-heal-<n>` worktree at the PR head when it is gone,
  removed afterwards with the junction-safe helper) it merges `origin/main` with
  the `tools/merge/` drivers. Conflicts only in the test registry, the language
  confs, the string table, `features.md`, `debug-log-triage.md`, or the
  enforce-lint baseline are resolved: the registry and confs by `regen-tests` /
  `regen-localization`, the others by the drivers' merge logic on the index
  stages. Any other conflicted path aborts the merge; the PR, the worktree, and
  the branch stay as they were, and the PR is reported "needs a human/agent"
  with the paths. A clean merge runs `validate` and
  `test --no-build --tier fast` (`--tier all` when the PR touches
  `Scripts/`), pushes without force, and re-queues auto-merge. A failed gate
  drops the merge commit (`git reset --keep`) and reports the PR.
- **Skipped**: a LIVE worktree, a worktree with uncommitted changes, or one
  whose branch has commits the PR does not (unpushed agent work). The last two
  are reported.
- **Green, auto-merge on, mergeable (`CLEAN`/`UNKNOWN`/`UNSTABLE`), and still
  open 10 minutes after auto-merge was queued or the last check finished**:
  `gh pr merge <n> --merge`.

It prints one line per PR and exits 1 when any PR needs a human or its agent.
Agents run `prs status` after their PR is up; the maintainer or lead runs
`prs heal`. One heal runs at a time (hub lock `prs-heal`).
`tools/validation/pr-keeper-selftest.mjs` proves the merge logic on a
throwaway repo: a registry-only conflict is healed and pushed, a real code
conflict (alone or beside the registry) is aborted with HEAD and origin
untouched.

## Scratch files

Each worktree has a gitignored `tmp/` (created by `wt new`, also home of the
`tmp/wt.json` marker). Put scratch there with names unique to the task, such
as `tmp/pr-body.md`. A shared session scratch dir lets one agent publish
another's file. `validate-worktrees` warns when the main checkout has
untracked files under `addons/` or `tools/`.

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

- `tools/wt.mjs` — hub, locks, ports, resource-database copy, git helpers, gh/REST PR + merge.
- `tools/prs.mjs` — `prs status|heal`: open ws/* PRs, liveness, heal merge, stuck-merge.
- `tools/cli.mjs` — `wt` command group, main-checkout guard, worktree-aware
  build/test/dev/serve/ci.
- `tools/validation/validate-worktrees.ps1` — registry consistency (port
  uniqueness, missing dirs, stale merged worktrees).
- `.githooks/pre-commit` → `validate-repo.ps1` — blocks `main` commits from
  linked worktrees.
- `AGENTS.md` (Worktrees section) — the agent-facing rules, including the junction-safe removal rule.
