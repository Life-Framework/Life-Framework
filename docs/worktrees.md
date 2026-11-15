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
- **Resource database** — `resourceDatabase.rdb` is gitignored and only Workbench
  writes it. `wt new`, `test`, `dev`, `serve`, `session up`, `host start`, and `wt e2e`
  copy `<main-checkout>/addons/LifeFramework/resourceDatabase.rdb` into the worktree when that
  copy is missing, or smaller and older. Before a server boot they then check that the file
  registers every `.meta` in the addon (path and GUID, `tools/lib/rdb-fresh.mjs`, ~0.2 s). A
  resource that landed on main after anyone last opened Workbench is missing from every copy;
  the server refuses it (`Wrong GUID/name`, inventory operations that never complete). When
  stale, the worktree copy is refreshed by a headless `-wbModule=ResourceManager` Workbench on
  the hidden desktop, under the `build` lock (one line: `resource database stale: N
  unregistered resource(s) ... -> refreshing`, about 10 s). `tools\cli rdb` does the same on
  demand, and `validate` warns. The main checkout's copy is the human's editor file: it is
  only warned about, never rewritten. `LF_RDB_REFRESH=0` turns the refresh off. Do not commit it.
- **Build** — the headless Workbench build is the one truly shared resource
  (shader cache, resource DB). It is serialized by a machine-wide lock file:
  one build at a time, everyone else `--wait`s. `cli dev` skips the build
  entirely, so the everyday loop never contends.
- **Hub** — coordination state lives in `<main>/tmp/wt/` (git-ignored):
  `state.json` (registry + ports + the warm pool) and `locks/`. Derived from `git worktree
  list`, so it survives worktrees created by hand.

## Fast `wt new`: short lock and warm pool

A checkout of this repo is about 1.5 GB and takes 12-30 s; that used to run
inside the `wt` lock, so a burst of agents queued behind each other and the
second one failed with `lock 'wt' is held`. Now:

- **The `wt` lock covers bookkeeping only**: reserving the slug and its ports in
  `state.json`, claiming a warm tree, and git metadata (`git branch`, `git worktree
  add --no-checkout`), each well under a second. Fetch, checkout (`git reset --hard`),
  the marker and the rdb copy run unlocked, so concurrent `wt new` calls overlap.
  `wt new` waits up to 2 min for the lock (10 min with `--wait`). While its checkout
  runs the entry has `status: "creating"` (`wt list` shows `creating`;
  `validate-worktrees` and `wt prune --merged` skip it); a `wt new` that died there is
  cleared by the next `wt new` of that slug, or `wt prune <slug> --force`. `state.json`
  is written by temp file plus rename, always under the lock.
- **Warm pool**: K checkouts on detached `origin/main` at
  `..\Life-Framework-warm-<id>`, recorded in `state.warm` (never `state.worktrees`).
  `wt new` claims the oldest ready one: it checks the tree is pristine (`git status
  --ignored` empty) and holds no junction or symlink, `git worktree move`s it to
  `Life-Framework-ws-<slug>`, and checks out `ws/<slug>` cut from `origin/<base>` (a
  fast-forward when main moved since the tree was made). It then writes `tmp/wt.json` and
  copies the rdb like a fresh one; the registry entry, marker, ports, path, branch and its
  upstream are the same, so `test`, `session up`, `look`, `wt ship`, `wt prune` and the
  pool's `Life-Framework-rt-<slug>` run folders cannot tell the difference. A claim takes
  about 2 s instead of 12-30 s; the output prints `(warm pool)` or `(fresh checkout)` and a
  `time:` line per phase. With no ready tree, or one that fails the checks (marked
  `broken`), `wt new` makes a fresh one as before.
- **Why move and not keep the pooled path**: the final path is then the one every
  consumer derives from the slug, and a move is a directory rename (about 0.1 s). A
  warm tree has never booted a server, so it has no `game-addons` junction; the claim
  refuses one that has any link, so nothing with a junction is ever moved. The git
  admin dir keeps its `Life-Framework-warm-<id>` name, which nothing reads.
- **Filling**: after every `wt new` and `wt prune`, `tools/wt-warm.mjs fill` starts
  detached and hidden at below-normal priority, makes trees one at a time until K are
  ready or preparing, and exits. One filler at a time (lock `wt-warm`). It never fills
  while free disk on the worktree volume is under `LF_WT_WARM_MIN_FREE_GB` (default 12;
  the pool's `minFreeDiskGb` floor is 5), and logs why to `<main>/tmp/wt/warm.log`.
  It also removes trees a dead filler or claimer left behind and `broken` ones.
- **Removal** of a warm tree goes through `recycleWorktree` in `tools/link-safety.mjs`
  (junctions unlinked first, `link-safety-selftest` covers it); never delete a
  `Life-Framework-warm-*` folder by hand.
- **Settings** (env or `lf.local.json`): `LF_WT_WARM` = K (default 3; 0 turns the pool
  off), `LF_WT_WARM_MIN_FREE_GB`, `LF_WT_WARM_KICK=0` (no background filler).
  `tools\cli wt warm` shows the pool, `wt warm fill` fills it in the foreground,
  `wt warm drain` removes the ready trees, `wt new <slug> --no-warm` forces a fresh one.
- Proof: `tools/validation/wt-warm-selftest.mjs` (fixture repo: fill, stale claim,
  fresh fallback, junction refusal, prune, drain).

## Daily use

```sh
tools\cli wt new <feature>      # create worktree + branch + ports
tools\cli wt list               # who's working, ports, dirty/merged state
tools\cli wt test <slug>        # run the LFTEST suite in that worktree
tools\cli wt dev <slug>         # fast loop in that worktree (no build)
tools\cli wt gate <slug>        # validate --full + build + test (tier=all)
tools\cli wt ship <slug>        # push -> PR -> ship queue (merge main, gate on the pool) -> auto-merge
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

## Base branches

Worktrees cut from and ship to `main` by default. For a long-lived branch such as `m3`:

- `tools\cli wt new <slug> --base m3` cuts from `origin/m3` and records the base in the hub state; `wt ship`, `wt sync`, `wt prune`, `wt list` and `prs heal` use it (PR base, merge-tree check, auto-merge, merged detection). `wt ship --base B` overrides. Worktrees with no recorded base stay on `main`.
- `tools\cli wt sync-base m3 [--source main]` opens a PR that merges `origin/main` into `m3` from a `ws/sync-m3-<stamp>` worktree (merge drivers resolve generated files; any other conflict stops with the file list). It never force-pushes; prune the sync worktree after the merge.
- The `validate` workflow runs on every `pull_request` (no branch filter), so PRs into `m3` get it. Making `validate` a required check on `m3` is a GitHub branch-protection setting, not done by the tooling.

### Gate tiers

`wt gate` and `wt ship` (and the ship queue, which runs `wt ship`) pick the gate from the diff
against the merge base with `origin/<base>`, uncommitted and untracked files included
(`tools/lib/gate-tier.mjs`, proven by `tools/validation/gate-tier-selftest.mjs`):

| Tier | When | Runs |
|---|---|---|
| T0 | nothing under `addons/` changed (docs, website, tools) | `validate --all` |
| T1 | data only: every changed `addons/` file is a modified `.conf`, `.st`, `.layout` or `.et` whose values changed in place | `validate` (static), the config harness static stage, the placement gate, and one test boot: every LOGIC test plus every test whose source names a touched file's stem, its GUID, its config class or a changed string key |
| T1w | a maintainer hand edit: the branch is `town-maintainer-*` and every changed `addons/` file is a modified `.layer` whose entities were only moved, or added / removed from prefabs already in use ("Maintainer world edits") | `validate --world` (hard checks block, layout checks warn), `audit placement --strict --layer <touched>` (findings warn), the tests that name a touched layer or entity |
| T2s | anything else (a normal code change), unless a risk class below hits | slim: `validate --full` (the Workbench compile on a pool machine with the `workbench` cap, no build) and the LOGIC tier (`--tier fast`). The main guard runs the rest on main after the merge |
| T2 | a risk class, or `--full-gate` | `validate --full` + build, the full suite and the core-only pass, the config harness sessions, the placement gate, the pre-move e2e |

**Risk classes keep the full T2 gate** (`tools/lib/gate-risk.mjs`, proven by `tools/validation/gate-risk-selftest.mjs`):
persistence (a `Persistence/` folder, a `*Persist*`/`*Serializ*`/`*Save*`/`*Record*.c` file, a changed line that registers a
payload or touches a save/load context); money (Shop, ATM, Money, Banking, Trader, the bank, robbery, fence, cash-van and
chop-shop scripts, vehicle lot/prices/ownership, or a changed line that moves cash); RPC (a changed line naming `RplRpc`,
`RpcAsk_` or `RpcDo_`, or one inside an RPC handler's body); GUID (any `.meta` added, changed or removed, or a hunk that
swaps one `{GUID}` reference for another); the addon boundary (a `.gproj`, a mission header, `tools/lib/addons.mjs`,
`validate-addon-boundary`); any `tmp/review-*.md` interrogate file in the worktree; a changed script whose diff cannot be
read. The gate prints which class hit (`gate tier T2 (full gate): full gate (money): ...`). `LF_SLIM_GATE=0` turns T2s back
into T2. The rules apply to a gate that starts after they merged: the queue merges main into the worktree before it gates,
and the gate runs the worktree's own `tools/cli.mjs`.

T1 is decided line by line, and anything unclear is T2. A file is data-only when its base and
head have the same line count, every line holding a brace is unchanged (same blocks, same
component and widget GUIDs), and each changed line keeps its key and swaps one scalar value with no
`{GUID}` in it. Per type: `.conf` and `.layout` any number, vector, quoted string or word
(`Text "#Key"`, `Color 1 0.8 0.8 1`, `m_iPrice 7`); `.et` numbers and vectors only; `.st`
only `Target_*` text and `Comment`; a language `.conf` never its `Ids` block. `ID`, `Id`,
`Name`, `Parent` and prefab keys are identities, never values.

Always T2: a `.c` file, a `.meta`, an added, deleted, renamed or untracked file under `addons/`,
a `.layer`/`.ent`, a binary asset, a changed resource reference, a `.conf` that **adds or removes
a key** (the key can switch on a code path its default never ran, and an equal line count is how the
classifier knows the blocks did not move), and a `.et` string value or component change.

Measured 2026-11-16 over the last 40 merges on main: 22 T0, 12 T2s, 6 kept the full gate (guid 3, persistence 2, money 2, rpc 1).

T1 skips the build because `test --no-build` loads the addons from source; the build only
packages. `ci --tiered` classifies again in the worktree and falls back to the full gate when the
diff is no longer data-only. Measured 2026-11-14 on a one-value `.conf` change (`m_iPrice 500 -> 505`), same worktree, warm rdb:
T2 (`--full-gate`) 630 s (compile + build 390 s on lf-cloud, both suite passes about 2 min, config
sessions about 2 min); T1 112 s (validate 49 s, one boot of 267 tests 57 s); T0 44 s. Both T1 runs in
that measurement failed on the related test that asserts the changed value.

### Main guard

The slim gate leaves the build, the full suite, the placement audit and the pre-move e2e to one worker on main
(`tools/main-guard.mjs`, rules in `tools/lib/main-guard.mjs`, proven by `main-guard-selftest.mjs`). It lives on one machine
(the laptop) and sends its heavy runs through the pool at `--priority high`, so it goes ahead of agent work in the queue.

```sh
toolscli main-guard start | stop | status   # status is also a line in `toolscli lead`
```

1. On a new `origin/main` head, batched to one run per 10 min, it checks the head out in its own worktree
   (`Life-Framework-ws-main-guard-run`, detached) and runs `build`, `test --no-build --tier all`, `audit placement --strict`
   and `e2e --pre-move 502ebf39 --mission playtest`. All green: that head is the last green head.
2. A red step reruns at the same head (only the failing tests). Green on the rerun is a flake: one line in
   `tmp/logs/main-guard-flakes.log`, nothing reverted.
3. Red twice: it bisects the first-parent PR merges since the last green head, rerunning only what failed at each.
4. The culprit is reverted with `git revert -m 1` on `ws/revert-pr<N>-guard`, a PR titled `Revert #N (main guard: <what
   failed>)` with auto-merge (only GitHub's `validate` check gates it), a comment on #N, its issues reopened as
   `regression`, and one line to the staff webhook when `LF_STAFF_WEBHOOK_URL` or `"mainGuard": { "webhookUrl" }` in
   `lf.local.json` names one (the URL is never printed).

Safety: it never reverts a revert, never blames a merge that was on main when it first started (`startHead` in
`<hub>/main-guard.json`), reverts a PR at most once, never pushes `main`. `LF_MAIN_GUARD=0` stops the worker and
refuses `start`. Log: `tmp/logs/main-guard.log`, one file per step beside it.

## Release gates (`stable`, `promote`)

`main` is the integration branch; agents branch from it and merge to it all day, as before. `stable` is the branch a human moves, and the only one the Workshop release, the flagship's 24/7 server and `tools\cli release` use. Playtests run on `main`.

- `tools\cli promote [--to <sha>]` fetches, lists the PRs merged to `main` since `stable` (first-parent merge commits) and writes `tmp/promote/checklist-<sha>.md`: one item per PR with its issues and the manual steps from its body. It changes nothing.
- `tools\cli promote --to <sha> --confirm` fast-forwards `origin/stable` to the commit you verified (default: `origin/main`), tags it `stable-<yyyymmdd>-<sha8>`, then marks the issues those PRs ref `verified` and closes them. An issue that a later, unverified PR on `main` also refs stays open. It refuses a target that is not a descendant of `stable` and never forces.
- `tools\cli promote --fail N[,N]` marks issues `regression` and reopens them without moving `stable`. With `--confirm`, a failed issue whose PR is inside the range blocks the promotion: fix it on `main`, revert it, or pass an earlier `--to`.
- `tools\cli promote --revert <PR>` opens a PR that reverts that merge on `main` (from a `ws/revert-pr<N>` worktree; auto-merge queued). `stable` is never edited directly.
- `tools\cli promote --init [--to <sha>]` creates `origin/stable` once. It was created from `main` because no M1 sign-off commit is recorded; the first real promotion is #404.
- **`tools\cli release <x.y.z>` (#369), human step.** Run from `main` or `stable` (or a detached HEAD at `origin/main` or `origin/stable`) on a clean tree. Without flags it is a dry run: it prints the plan, names every refusal (dirty tree, wrong branch, bad version, tag taken) and runs the cheap `release-ready --strict` check; nothing changes. `--confirm` stamps the version in both `.gproj` files and both Workshop kit READMEs, runs `validate --full`, `release-ready --strict`, the full suite, `audit placement --strict` and a `pack` of each addon, writes `tmp/release/notes-<x.y.z>.md` from the PRs merged since the previous `v*` tag, and tags `v<x.y.z>` on a release commit that no branch points at. A failed gate restores the files and tags nothing. `--push` also pushes that one tag; without it nothing leaves the machine. It never uploads to the Workshop (#370, by hand in Workbench). Agents run the dry run only, never `--confirm`.
- **Issue states.** `agent:working` (being built) to `in-dev` (merged to `main`) to `needs-playtest` (merged, and the PR has a `## Manual testing` section with real steps) to `verified`. Only `verified` closes an issue; `regression` marks a failed one. The `issue-state` workflow (`tools/issue-state.mjs`) runs when a PR merges into `main`: every issue it refs (`Refs/Closes/Fixes/Resolves #N`) gets `in-dev` or `needs-playtest` and stays open; an issue an older CLI's `Closes #N` closed is reopened, a `verified` one is left alone, and a merged `Reverts #P` PR sets `regression` on P's issues. `promote --confirm` sets `verified` and closes. The website reads `verified` (or closed) as done and `in-dev`/`needs-playtest` as "built, not verified".
- **Quorum sign-off (#575).** `docs/playtest/quorum.json` switches a rule that is off by default: when `"enabled": true`, `promote --confirm` needs `--confirmations <file>` and moves `stable` only when every issue whose PR lists manual steps has enough distinct roster testers (`docs/playtest/testers.json`) reporting pass, and none reporting fail. Rule, roster and disputes: `docs/playtest/quorum.md`. Session logistics: `docs/playtest/session-playbook.md`. Until the maintainer approves the rule, agents still never run `promote`.
- Risky changes ship behind a config flag that defaults off, so one unverified feature does not block the rest.

**Manual, maintainer (GitHub settings, not done by tooling):** protect `stable` (Settings, Branches): restrict pushes to the maintainer account, block force pushes and deletion, no pull-request requirement. `promote` pushes a fast-forward as the signed-in `gh`/git user.

**One session, many issues (#402, #403).** The human check is the bottleneck, so a pass is planned, not improvised.
`tools\cli playtest checklist` collects the `## Manual testing` steps of the merged PRs of every open `needs-playtest` or
`in-dev` issue, shows a repeated step once, groups the rest by place in a walking route, marks two-player, restart,
late-joiner and dedicated-server steps, estimates minutes, and writes `tmp/playtest/checklist-<date>.md` with a pass box and
a `fail [ ]` box per issue. The default is the top hour; `--all` lists everything. The hour is ranked in tiers: issues under an open M1 or M2 chapter or epic (GitHub sub-issues, two levels; `--chapters N,N` adds more) first, then big player flows named in the title (robbery, vault, clerk, cuffs and arrest, medic and downed, dispatch, pagers and payphones, evidence and witnesses, dialogue, the item catalogue and licences), then the rest, and polish, visual-only and tooling items last (`tierOf` in `tools/lib/playtest-checklist.mjs`). Chapter work takes at most 70% of the hour, and a flow that does not fit closes the door on filler below it. Issues whose steps one walk covers share one line that lists every number; its tick or fail covers all of them. The fixed smoke list
([playtest/smoke-list.md](playtest/smoke-list.md): boot, join, save, restart, reload, late joiner) is always its first
section, and only a regression in an already-verified feature blocks promotion. After the pass,
`tools\cli playtest verdicts <file>` reads the ticked file and prints the `gh` commands (verified or regression label, a
comment, close on pass). It runs none of them: the maintainer or lead reviews and runs them. Agents never do.

## Auto-merge

`wt ship <slug> [--refs N] [--body-file F]` goes through the ship queue (next section). The steps
below are the old local path, still used with `--no-queue` (or `LF_SHIP_QUEUE=0`, or without `gh`):

1. Refuses a dirty tree or a branch with no commits beyond `origin/main`.
2. Merges `origin/main` only when `git merge-tree` reports a conflict (the
   merge drivers in `tools/merge/` then resolve the generated files). A clean
   branch ships as is.
3. Runs the gate for the diff's tier ("Gate tiers" below). T2 is never incremental (see "Incremental validate" below): `validate --full` (with the Workbench compile) + headless
   build + `test` tier=all. T0 (nothing under `addons/`) runs plain `validate --all` and never waits for a
   pool slot. T1 (values changed in place) skips the compile, build and full suite. `--full-gate` forces T2 (`wt gate` follows the same rule); the gate prints the tier and why, and ship adds it to the PR body under `## Gate`. After the suite,
   a branch that touches a `.layer`, a `Prefabs` `.et` or LF_Utils/spawn code also runs
   `audit placement --strict` (queued like `test`); anything flagged and not in the allowlist fails the
   gate, and it prints the entities with the allowlist fix hint (`docs/placement-audit.md`).
4. Pushes `ws/<slug>`, opens the PR (or reuses the open one on a re-run).
   `--refs N` (or `--closes N`, same thing) starts the body with `Refs #N`, and any `Closes/Fixes/Resolves #N` in the body is rewritten to `Refs #N` (merging never closes an issue); `--body-file F` supplies the
   body (keep it in `<worktree>/tmp/`).
5. Queues `gh pr merge --auto --merge`: GitHub merges once the required
   `validate` check and reviews pass, and deletes the branch.
6. Prints `PR <url>, head <sha>`.

`--pr-only` stops after step 4. If the gate fails or the merge of main
conflicts, the ship stops with a message; fix, commit, and re-run the same
command. Do not merge `origin/main` by hand unless GitHub reports the PR
`CONFLICTING` or you need code from main.

## Ship queue

About fifteen agents ship at once. With a local gate each `wt ship` ran `validate --full`, a build and the
suite on this PC, and often twice: main moved during the gate, GitHub reported the PR `CONFLICTING`, the
agent merged main and gated again. The ship queue (`tools/ship-queue.mjs`, rules in
`tools/lib/ship-queue.mjs`) is one merge train for the repo:

1. `wt ship <slug>` checks the tree and the PR body as before, pushes `ws/<slug>`, opens the PR (or reuses
   it, switching off an auto-merge left from an earlier ship, so the push cannot land ungated) and adds it
   to the queue (`tmp/wt/ship-queue.json` in the main checkout). It starts the worker when none runs.
2. The worker takes queued PRs in FIFO order. For each it merges `origin/<base>` into the worktree **once**
   (`prs heal`'s `healMerge`: the `tools/merge/` drivers, then regeneration of the test registry, language
   confs and credits; a conflict outside those files fails the entry), then runs `ci --remote --tiered` there
   (`--full-gate` drops `--tiered`; a data-only diff gets the T1 gate, "Gate tiers" below, else the full gate):
   the Workbench compile and build on the pool machine with the `workbench` cap that ranks first by
   free RAM after a Workbench reserve, weighted by speed (`tools/lib/wb-gate.mjs`, "Workbench limit and
   the remote gate"; `wt ship --remote <name>` / `--machine <name>` replaces `--remote`), the suite, core-only run and placement audit on pool test lanes. A branch
   that changes nothing under `addons/` runs `validate --all` instead, as `wt gate` does. After a remote
   build, `ci` copies the run folder's fresh `resourceDatabase.rdb` home when the worktree's is stale,
   so the suite does not refresh it with a headless Workbench on this PC (that took 10 min per addon,
   one worktree at a time behind the hub build lock, in the first queue run).
3. **Re-gate only on overlap** (`tools/lib/regate-overlap.mjs`). A passed gate stands unless main moved in a way
   that can change its result. When the gate passes the worker fetches again, and when auto-merge answers
   "Base branch was modified" it merges main in and asks the same question against the main the gate saw. The new
   main commits **overlap** the PR when they touch: a file the PR changes; a file in the same feature folder
   (`Scripts/Game/Feature/<Area>`, `Prefabs/<Area>`, `Configs/<Area>`); shared Core (`Scripts/Game/Core`) while the
   PR changes a script, or the other way round; a prefab or config whose `.meta` GUID the PR's files reference
   (or main's files reference a GUID the PR changes); a class or file stem one side declares and the other
   names as a whole word; or the same generated file (test registry, language conf, string table) the PR also
   changes. Docs prose and driver-merged docs (`docs/features.md`, `docs/debug-log-triage.md`, the lint baseline)
   do not count, and a generated file main alone changed does not either. A text that cannot be read, or more
   than 400 changed files to compare, counts as overlap. Overlap: the gate runs again as before. No overlap:
   the gated head (or the head with main merged in) is pushed and auto-merge is queued with no second gate, and
   the log says `gate pass reused: main moved (N file(s)), no overlap ...`. GitHub's required `validate` check and the
   main guard cover what this misses. After three gates it pushes regardless.
4. Pass: push, `gh pr merge --auto --merge`. Fail: a PR comment with the failing lines of the log and
   its path, the entry is marked failed, and the queue moves on. Fix, commit, `wt ship` again.
   A gate the pool could not run (no machine answered ssh, a test shard placed nowhere with 0 tests)
   is not the PR's fault: the entry goes back to its place in the queue and retries after 3 min, at
   most twice, then fails like any gate.
   A queue write is bookkeeping and never fails a gate. On Windows the rename of `ship-queue.json.tmp`
   throws EPERM/EBUSY/EACCES while a `ship-queue status` or `wait` has the file open, so
   `tools/lib/atomic-file.mjs` retries it (10 tries over about 1 s); the worker's `update` then retries
   the whole read-modify-write, and if that still fails it logs `queue write ... the gate result is kept`
   in the worker log instead of failing the PR. A read that keeps failing throws rather than returning an
   empty queue. The session lease, test cache, shard history, pool tickets and look-serve state use the
   same helper. `ship-queue-eperm-selftest.mjs` holds the file open from a child process to prove it.

**Stale rdb refresh when this PC is busy (the fallback).** A stale `resourceDatabase.rdb` is registered by a
headless Workbench: ~15 s per addon, but it queued 10-50 min behind the hub `build` lock and this PC's one
Workbench slot. The rule, in order (see "Sync" in "Test pool"): a run headed for a pool machine with the `rdb`
cap refreshes in that machine's own run folder and never here; everything else refreshes on this PC, and
this fallback (`refreshOnPool` in `tools/cli.mjs`, `chooseRefresh` in `tools/lib/wb-gate.mjs`, built on
`chooseGate`) covers it: a local run (`build`, `serve`, `session up`, `rdb`, a gate), a run headed for a
machine without the `rdb` cap (lf-cloud) or pinned to this PC, and a lane given back to refresh. When this
PC's Workbench slot is busy or queued for, its `build` lock is held, or its free RAM is under a Workbench's
need (`WB_RESERVE_MB` plus `minFreeMb`), and a machine with the `rdb` and `workbench` caps has a free
Workbench slot AND the RAM, the refresh runs there: the run folder gets the snapshot, `rdb --local`
refreshes in it (taking one of that machine's Workbench slots, so the next caller sees it busy), and
`pullRemoteRdb` (the #1227 copy-back) copies the rdb home. No such machine (never one `chooseGate` would only
queue on), or any failure, and the local refresh runs, queuing for the slot and the build lock; `rdb
--budget <s>` (`LF_RDB_WAIT_S`, default 900 s, spent once per process across addons and the Workbench slot, it prints the holder: `tools/lib/lock-wait.mjs`) bounds that wait, and when it runs out the
refresh tries an `rdb`-cap machine once more. It never routes from inside a pool run folder
(`tmp/pool-owner.json`). It logs `rdb: ... refresh on <where>: <why>` and `rdb: refreshed on <machine> in N s`
or `resource database refreshed on <host> in N s`. `LF_RDB_REFRESH_WHERE=local` never goes remote, `=pool`
tries a machine first even when this PC is free; `LF_RDB_REFRESH=0` turns the refresh off.
`tools/validation/rdb-refresh-where-selftest.mjs` proves the decision.

**Parallel gates.** Up to N Workbench gates run at once, N being `"shipQueue": { "parallel": N }` in
`pool.json` or in the `pool` key of `lf.local.json` (the local value wins), else the sum of the Workbench
limits of the ssh machines with the `workbench` cap: each machine's own `"workbenchLimit"` in its
`pool.machines` entry (1 when unset; lf-cloud has 2). This PC is not counted. A queued PR starts only when
no running gate and no earlier waiting PR changes a file it changes. Driver-merged files (the string
table, language confs, test registry, lint baseline) and any `*.md` (docs, `Feature/<Area>/Feature.md`,
READMEs) do not count: two PRs that edit `docs/worktrees.md` or one `Feature.md` gate side by side, and a
text conflict shows at the merge of main, in GitHub's mergeable state, or in `prs heal`. Only a shared
code or resource file serializes. The old shared lists are per-feature files now (`Core/LF_StaticReset.c`
hooks, `docs/features/<area>.md`, harness verbs; AGENTS.md rule 10), so two feature PRs rarely share
one. PRs on disjoint files gate side by side on different machines and overlapping ones keep their order.

**Gates with no Workbench take no slot.** A PR that changes nothing under `addons/` runs `validate --all`
only (tier T0, "Gate tiers"), needs no Workbench and does not wait for a slot; it counts against
`"shipQueue": { "light": N }` instead (default 4). The entry's `tier` (0|1|2) and `builds` fields are the
hook for the data-only tier T1: `tier: 1` takes a Workbench slot only when `builds` is true. The worker
sets them once `ci --tiered` has classified the merged diff (`needsWorkbench` in `tools/lib/ship-queue.mjs`).

**Waiting.** See "Async ship" below: `wt ship` returns once the PR is queued. `wt ship --wait` blocks
until the gate passed or failed and prints `SUMMARY ship ok|fail <slug> PR #N gate <s>, in queue <s> ...
log=<path>` (exit 0 on pass); `tools\cli ship-queue wait <slug>` does the same for a queued slug (gives
up after 3 h with exit 2, the gate keeps going). `ship-queue status` and `pool status`
show what gates, what waits and why, and the last few results; `ship-queue cancel <slug>` drops a
waiting entry. A re-ship of a waiting slug replaces its entry and keeps its place; one whose gate runs
is refused until it ends. The worker logs to `tmp/logs/ship-queue-worker.log`, each gate to
`tmp/logs/ship-<slug>-<stamp>.log`; a gate is killed after 100 min, an idle worker exits after 3 min,
and a worker that died leaves its running entries to be re-queued by the next one.

### Async ship

A queued gate takes 15-90 min (PR #1250: validate --full about 15 min, the Workbench build on lf-cloud about
5, the sharded suite, then GitHub; 86 min in all). An agent's shell call is capped at 10 min, so a blocking
`wt ship` went to the background, the agent sat in wait loops, ended its turn mid-ship and went idle. Now
`wt ship <slug>` returns in about a minute or two (preflight, push, PR, enqueue) and the gate runs in the queue worker:

1. **Preflight, here, before the push** (`--no-preflight` skips it): the clean-tree and PR-body checks as
   before, `git merge-tree` against `origin/<base>` (a conflict in a driver-merged file is left to the
   worker's merge; any other conflicted file stops the ship with the merge command to run), then
   `validate` of the checks whose inputs changed (incremental, no Workbench compile). A red check costs
   seconds here instead of a queue slot and a gate.
2. **Push, PR, enqueue.** Auto-merge is off on the PR (an earlier ship's auto-merge is switched off first).
   `wt ship` prints `SUMMARY ship queued <slug> PR #N position ... log=<path>` and exits 0.
3. **The gate runs detached** in the ship-queue worker (the steps above). Its state goes to three places:
   the queue file, `<worktree>/tmp/ship-status.json` (the status line and the entry, on every change), and
   a `ship-gate` commit status on the PR head (pending while queued or gating, success on the pushed head,
   failure with the reason, error when cancelled), so `gh pr checks` and the PR page show it.
4. **Pass:** the worker pushes and enables auto-merge; GitHub merges once the required `validate` check
   is green. **Fail:** the PR gets a comment with the failing lines and the log path, `ship-gate` goes
   red with the reason, auto-merge stays off.

**Why auto-merge, not a required status, guards the merge.** Branch protection on `main` requires only
the `validate` Actions check. Making `ship-gate` required would block every PR that does not go through the
queue (`wt sync-base`, `--no-queue`, a human's PR, website-only work). The worker is the only thing that
turns auto-merge on for a queued PR, and `prs heal` skips any PR with a live ship-queue entry (it would
otherwise merge main into the same worktree and switch auto-merge on before the gate passed).

**Checking and waiting** (both read the queue in the main checkout, from any worktree):

```sh
tools\cli wt ship-status <slug>            # one line: ship-status <slug> queued|running|passed|failed PR #N: ... log=<path>
tools\cli wait ship <slug> [--budget 540]  # blocks up to the budget; one SUMMARY line; exit 0 passed, 1 failed, 2 still going
```

`wait ship` defaults to 540 s, under the 10 min tool cap: run it in the foreground and run it again on exit 2.
`wt ship-status` exits 1 only when the gate failed or the slug is unknown. `wt ship --wait` (or
`LF_SHIP_WAIT=1`) keeps the old blocking ship for a human at a terminal.

**What an agent does while the gate runs.** It may start the next stacked PR (a new worktree from
`origin/main`, or `--base` on its own branch when it needs the unmerged code). It reports
"shipped, gate running" with the PR URL, and before its final report runs `wt ship-status` (or
`wait ship`): passed means "merged, in-dev" once GitHub merges, failed means fix, commit, `wt ship` again.

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
worktree: `LIVE` when an edit, commit or session log landed in the last 15 minutes (directory and git-metadata touches from prune or fetch do not count)
(modified or untracked files, the worktree's git index/HEAD/reflog, `tmp/`,
server logs), `idle`, or `none`. An `UNKNOWN` state is re-read through the REST
API, which makes GitHub compute it.

`prs heal`, per PR:

Git runs a merge driver in the worktree being merged into, so a branch would
otherwise merge `origin/main` with its own, possibly older, `tools/merge/`
copy. `tools/merge/run-driver.mjs` runs the newest copy among HEAD, the commit
being merged and `origin/main`, and says so on stderr (`lf-merge: <driver>
from <ref>`). The string-table driver refuses a side it cannot parse whole
and writes git's line merge with exit 1 instead. `merge-content-selftest`
merges real branches and compares the result byte for byte (#1154, #1161).

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

**Safety check (every heal, before any push).** GitHub cannot run our merge
drivers, so a merge made here is checked from git objects
(`tools/prs-safety.mjs`): for every tracked `*.st` the merged key set must equal
main's keys plus the branch's own additions minus its own removals (a count
mismatch names the missing or extra keys), and every line of `docs/features.md`
and `docs/debug-log-triage.md` that main has must still be there. A failure
undoes the merge (`reset --hard` to the old head), pushes nothing, and reports
the PR (`refused`). When both sides changed `docs/credits.md`, `tools\cli
credits` and `regen-localization` run after the merge so the credits rows and
`LF_CreditsData.c` agree with it (the `LF-Credits_*` keys are then exempt from
the key check).

#### Self-healing mode: `prs heal --watch`

```sh
tools\cli prs heal --watch [--once] [--interval 10] [--max-runs 144]
tools\cli prs schedule print|install|status|remove
```

Every `--interval` minutes (default 10, at most `--max-runs` passes, default
144 = 24 h) it lists open `ws/*` PRs that GitHub reports `CONFLICTING` and, for
each one whose worktree is not LIVE, merges `origin/main` in the PR's own idle
worktree or a temporary `Life-Framework-heal-<n>` one, regenerates the
generated files, runs `validate` only (the PR's own `validate` check and
auto-merge cover the rest), runs the safety check, pushes without force, and
re-queues auto-merge. Anything else (a real content conflict, a refused merge,
a failed validate, unpushed agent work) is skipped: one log line, and one PR
comment naming the files (once per PR head and main head, so a repeat pass does
not spam). Output also appends to `tmp/logs/prs-heal-watch.log` in the main
checkout. `--once` is one pass; the scheduler uses it.

`prs schedule install` registers one Task Scheduler entry, `LF-PRs-Heal`:
`conhost.exe --headless node tools\cli.mjs prs heal --watch --once` from the
main checkout every 10 minutes as the current user (it needs that user's `gh`
login), hidden, never overlapping itself, 45 min limit. `print` shows exactly
what it would register and changes nothing; `status` and `remove` do what they
say. Nothing registers it until someone runs `install`.
`tools/validation/prs-heal-safety-selftest.mjs` proves it on throwaway repos: a
generated-only conflict heals with exact keys and rows, a real conflict is
skipped untouched, a lost key or a lost features row is refused and undone.

It prints one line per PR and exits 1 when any PR needs a human or its agent.
Agents run `prs status` after their PR is up; the maintainer or lead runs
`prs heal`. One heal runs at a time (hub lock `prs-heal`).
`tools/validation/pr-keeper-selftest.mjs` proves the merge logic on a
throwaway repo: a registry-only conflict is healed and pushed, a real code
conflict (alone or beside the registry) is aborted with HEAD and origin
untouched.

## Test pool (every machine)

Every `-server` boot binds UDP 2001 unless given `-addr` (see "Lanes" below), so a
machine without test lanes runs one test server at a time. The test pool spreads in-game runs over every machine that can boot
one: this PC and the test box (ssh alias `lf-testbox`), more when they are
added. Each run takes the first free slot on any machine; when every slot is
busy it queues, first come first served, and takes the first slot that frees
anywhere.

```sh
tools\cli test --no-build --tier all                      # first free slot anywhere (--any, the default)
tools\cli test --no-build --only LF_Test_Money --local     # this PC only
tools\cli test --no-build --red LF_Test_A,LF_Test_B        # batched red proof: one boot, every listed test must FAIL
tools\cli test --no-build --tier all --remote [name]       # a remote machine (any, or that one)
tools\cli test --no-build --tier all --core-only           # the core alone, pack hidden (#326); any machine
tools\cli test --no-build --tier all --shards auto --both  # normal + core-only passes at the same time (what ci and wt gate run)
tools\cli e2e --remote                                     # the two-boot save/restart proof on the box
tools\cli pool status                                      # machines, slots and holders, queue, last 20 runs
tools\cli pool stats                                       # per machine: runs, passes, average wait and run
tools\cli pool prune [--dry-run] [--unmarked] [--lanes]    # remote run folders of gone worktrees
tools\cli pool prune --idle <min> [--dry-run]              # every remote run folder idle that long (no lock, process or write)
```

- **Sharding.** `--shards N|auto` splits the registry into balanced slices, one pool job each (`tools/lib/test-shards.mjs`; `--only-classes` is what a slice runs; timing history in the pool dir balances them). A slice pays its own boot and run-folder sync while the tests take about 4 s, so `auto` adds a slice only per 15 s of estimated test time: 1 per pass today (4 slices per pass measured 106 s, slower). The win is `--both`: the normal and core-only passes run at the same time on different lanes (median 37 s against 70 s back to back). `ci`, `wt gate` and `wt ship` do this by default; `ci --shards 1` runs the passes one after the other.
- **Nothing in a gate waits silently.** The rdb refresh before the suite waits for the hub `build` lock through `tools/lib/lock-wait.mjs`: it prints the holder (`waiting on the build lock ...: held by pid N (rdb refresh in <worktree>), held N s; budget ...`), repeats every 30 s, and gives up at `LF_RDB_WAIT_S` (900 s) for the whole process, every addon together; a later addon fails at once instead of waiting again. Shard children run with `LF_RDB_CHECKED=1`, so the slices never refresh or wait a second time (before, the gate and each slice each waited 15 min per addon, silently: 25+ min with no output). While slices run, the parent prints `shards: <pass> i/n still running after N s: <last line>` every `LF_SHARD_HEARTBEAT_S` (120 s) and stops a slice after `LF_SHARD_TIMEOUT_S` (2700 s). `lock-wait-selftest` proves it.
- **Who uses it.** `test`, `dev`, `ci`, `wt test`, `wt gate`, `wt ship` (its
  gate), `wt e2e`, and `e2e`. Every run prints where it ran and how long it
  waited (`pool: test all runs on lf-testbox ...; waited 0.7 s`) and a footer
  with the exit code, wait, and run time. `--local`, `--remote [name]`,
  `--machine <name>`, and `--any` pick; `LF_TESTBOX_AUTO=0` makes the
  default `--local`.
- **Machines.** `tools/pool.json` lists them: `name`, `via` (`local` or
  `ssh`), `host` (ssh alias), `root` (the remote main clone), `slots`, `caps`
  (`server`, `clients`, `workbench`, `rdb`: refreshes a stale
  `resourceDatabase.rdb` in its own runs, see "Sync"). The gitignored `lf.local.json` overrides
  per machine with a `"pool"` key (`docs/setup.md`). `slots` is 1 everywhere
  today; slot 1 is the machine's own `server-2001` lock (the one `serve`,
  `host`, and `session` take too) and slot k is `server-2001-s<k>`, so a
  machine that can run more servers at once (a network namespace or VM per
  slot) only raises the number.
- **Queue.** Tickets live in `<main>/tmp/wt/pool/queue/` (time, pid, slug,
  machines wanted). A ticket served before another (priority, then arrival)
  wants the machine first; tickets of dead processes are deleted. A local lock whose owner is
  dead is taken over after any `ArmaReforgerServer` still running from that
  owner's `server/profile` is stopped.
- **Burst starts (#1195).** Runs that fit start together. A ticket may start
  beside the tickets ahead of it on a machine when that machine has a free lane
  of its kind for each of them and one more for itself, none of them waits on
  RAM, and live free RAM, less what still boots there, covers all their
  reserves and its own above `minFreeMb`. The reserves of the starts still in
  flight ahead (`testReserveMb`, 800, per test; a session's reserve) go into
  this start's RAM gate, here or in the box's holder, and a test lane locked
  less than `testBootS` (60) seconds ago counts with `testReserveMb` too, so a
  burst cannot overshoot the floor. A ticket whose holder has taken its lock is
  marked `granted` and no longer holds anyone back while its run folder is
  prepared. Unknown lanes or RAM (an unreadable machine) fall back to one start
  at a time. Each start still takes its lock with an atomic create (the
  holder's `CreateNew`, this PC's `openSync "wx"`, unchanged since the pool
  began), so starts in flight together, and pool code on older branches that
  still starts one at a time, can race for a lane but never share one; a burst
  start tries the k-th free lane first so they rarely race. Runs asked together
  from one worktree share one snapshot (taken under `tmp/remote/snapshot.lock`,
  reused for 10 s at the same HEAD and index). Measured 2026-11-14, 14
  `test --tier fast --machine lf-cloud` at once (16 lanes): before, one start
  every ~3 s (holder ssh, gate and lock 2-3 s, then a 1 s poll; a new run folder
  held the queue head 22 s), peak 7, starts spread over 83 s; after, peak 14,
  spread 3.4 s.
- **Remote slots.** A holder (one ssh session running PowerShell on the
  remote) creates the remote `server-2001` lock with its own pid, prepares the
  run folder, and watches its ancestor processes. When the run ends, or this
  process is killed, ctrl-c'd, or crashes, the local ssh dies, sshd drops the
  session, and within about 2 s the holder stops anything still running out of
  the run folder and deletes the lock. A remote lock whose pid is dead is
  reclaimed by the next acquirer there. The remote CLI runs under the held
  slot (`LF_SERVER_SLOT_HELD` names the lock and a token it must still carry).
- **Stale and hung slots.** A holder can outlive its owner (observed
  2026-11-08: the local ssh was gone, the remote PowerShell holder and its lock
  stayed). Each queued run that finds a remote slot busy checks the lock: when
  its `origin` is this PC and its `originPid` is dead, the run kills that
  holder by its recorded pid (only if the lock still carries that pid and token
  and the pid is a PowerShell), stops what runs out of its run folder (by
  command line; nothing is deleted), removes the lock, and retries. The hang
  guard takes any pool slot, local or remote, held longer than max(5 min, 10x
  the median run of that purpose) while no `ArmaReforgerServer` runs for it;
  `serve`/`host`/`session` locks name no purpose and are never taken. A
  reclaim prints `pool: reclaimed <machine> <lock> from <slug>: <reason>`, the
  owner prints `WARN  pool: the scheduler reclaimed this run's slot ...` when
  it releases or loses it, and `pool stats` counts reclaims per machine.
  `pool status` marks a remote lock whose owner is dead here as `STALE`.
- **Config changes while queued.** Every poll re-reads `tools/pool.json` and
  `lf.local.json`: a machine enabled while a ticket waits counts at once, and
  the ticket's "wants" follow (same file, same place in the queue). Only
  `--machine <name>` / `--remote <name>` pin a ticket to one machine;
  `--local` and `--remote` stay within their kind.
- **Health.** An unreachable machine (ssh connect timeout 4 s) is skipped for
  a minute (`<main>/tmp/wt/pool/health.json`); the holder attempt itself is the
  probe. A holder that connects but sends nothing for 40 s, or never reports
  within 180 s, counts as down too: the run prints one line (`pool: <machine>:
  <reason>; <machine> marked down for this run, falling back to <next>`), does
  not retry that machine, and takes the next machine or this PC. `pool status`
  bounds each probe at 12 s (a machine that answered in the last 5 min gets a
  second, 30 s probe). One missed answer is not DOWN: a machine that answered
  within 5 min stays in use and `pool status` prints `slow (last answer N s ago,
  M misses)`; DOWN needs 3 misses in a row over at least 90 s, or a miss with no
  answer in the last 5 min, or a run that lost its machine mid-run. A waiting run prints one line
  when it queues and one when it gets its slot, nothing in between. A machine whose server Steam build differs from this PC's is skipped
  with a `WARN`, unless it was named with `--remote <name>`.
- **Sync.** A remote run carries the worktree as it is: HEAD plus uncommitted
  edits plus untracked, non-ignored files, as one snapshot commit built from a
  copy of the index (your index, HEAD, and branch do not change). A test,
  e2e or audit run uses its lane's folder `C:\LF\Life-Framework-lane-<lock>`
  (`-lane-server-test-3`), a linked worktree of the box's clone shared by
  every worktree; sessions, `-gate` syncs and a machine without lanes keep
  the per-worktree `C:\LF\Life-Framework-rt-<slug>` folders. The holder reports which of the snapshot, the
  last commit this worktree sent, and `origin/main` the box has, and the git
  bundle carries only what is missing. `resourceDatabase.rdb` is gitignored
  and sent when its SHA-1 differs from the run folder's. A stale one (a
  resource the rdb does not register) is refreshed by a headless Workbench
  where the run lands: this PC for a run here or on a machine without the
  `rdb` cap; a machine with the cap (lf-testbox) refreshes the run folder's
  copy in its own `test --local`, `e2e --local` or `session up`, is not sent
  the stale copy, and keeps the refreshed one for the next run. Before
  2026-11-14 every run refreshed here first, behind the build lock all
  worktrees on this PC share: runs headed for a pool machine waited up to
  10 min behind other worktrees' refreshes. lf-cloud has no `rdb` cap: its
  Workbench exits 0 without registering anything (`ResourceDB: could not
  open cache file`); add the cap in its `lf.local.json` entry once a refresh
  there registers resources. A run that gets this PC's lane with a stale rdb
  gives the lane back, refreshes, and queues again. Measured
  2026-11-09: new run folder 5.1 s holder + 1.3 s sync, repeat 3.0 s + 0.0 s
  (no bundle, rdb unchanged); an edit sends a ~2 KiB bundle. A run folder
  whose rdb predated `LF_HideoutStash` got the fresh one by hash and passed the
  stash tests (the box's main clone rdb has no `LF_HideoutStash` entry).
- **Lane folders (2026-11-14).** Whoever holds a lane's lock owns its folder,
  so two runs never share one, and a worktree's first run on a lane no longer
  creates a folder (`git worktree add` of ~5000 files: holder + prep 17-24 s).
  Each run checks its snapshot out with `git checkout -f`, which resets every
  tracked file and removes the ones the snapshot does not have, then drops
  what git does not track under `addons/` (untracked and ignored: a stray
  script, an addon `log`, `.gproj.user`), keeping each addon's
  `resourceDatabase.rdb`, which is sent by hash. git only lists those paths;
  each delete goes through the box clone's `safeRemoveTree`, which unlinks a
  junction instead of following it (`tools/link-safety.mjs` `cleanRunFolder`,
  shipped to the box with the run). `server/` (the test profile, which
  `test-e2e.ps1` deletes and rebuilds every boot, and its `game-addons`
  junctions) and `tmp/` stay. A clean that fails stops the run (exit 3).
  Measured on lf-cloud: a lane's first use 19-20 s holder + prep, a reused
  lane 2-5 s; a compile-breaking `LF_ZZStray.c` planted in a lane folder was
  removed by the next run there (`cleaned 1 untracked path(s)`), which passed.
  A worktree on an older branch still uses its `rt-<slug>` folders; both kinds
  take the same lane locks.
- **What comes back.** The run streams live. The remote `console.log` is
  copied to `server/profile/test/logs/remote-<machine>-<logdir>/`, so
  `tools\cli logs` greps it; on a failure its `SCRIPT (E)`, VME, and `Unable
  to find` lines are printed. The summary is stamped into
  `tmp/test-results/summaries.jsonl` with `"host"`, and every run is a line
  in `<main>/tmp/wt/pool/runs.jsonl`.
- **Cleanup.** Each run folder holds `tmp/pool-owner.json` (PC, slug,
  worktree). `wt prune <slug>` removes that worktree's remote run folders;
  `pool prune` removes every folder whose worktree on this PC is gone
  (`--unmarked` also takes older folders without the marker). Lane folders
  belong to no worktree and stay; `pool prune --lanes` removes the idle ones
  of lanes a machine no longer has (its lane count went down). All of it goes through
  the box's own `tools/link-safety.mjs`, never a raw delete: `server/profile`
  holds junctions into the server install.
- **Remote disk GC.** Run folders (`Life-Framework-rt-<slug>[-session][-sN]`, about 1.4 GB each)
  pile up while their worktree still exists (2026-11-14: lf-cloud hit 0 GB free and every run
  sent there failed the disk floor). Before the pool starts a run on an ssh machine it reads the
  box's free disk (one ssh call); under `rtGcFreeFactor` x `minFreeDiskGb` (3 x 5 = 15 GB) it
  removes that box's idle run folders, oldest first, until it is back above that, and logs one
  `pool: gc <machine>: removed <dir> (idle ..., freed .. GB, .. GB free)` line per folder. A folder is
  kept when a live lock in `<box>/tmp/wt/locks` names it (`rDir`), a process's command line names
  it, anything in it was written in the last `rtIdleMin` (60) minutes, or it is one the run is about
  to use. Deletion is the box's own `tools/safe-rm.mjs`, then `git worktree prune`. Settings:
  `rtIdleMin`, `rtGcFreeFactor` (`LF_POOL_RT_IDLE_MIN`, `LF_POOL_RT_GC_FREE_FACTOR`;
  `LF_POOL_RT_GC=0` turns it off). `pool prune --idle <min>` does the same by hand on every
  reachable ssh machine whatever the free disk. A box with nothing idle left to remove is said
  so once and not asked again for five minutes.
- **Cost.** A box run of the full suite takes about 22 s door to door (10 s
  suite plus ssh, checkout, CLI start, log copy), this PC about 8 s. Four
  concurrent `--tier all` runs from four worktrees (2026-11-09): 28-31 s wall,
  three on this PC and one on the box, all 224/224, against 36 s for the four
  in a row on this PC. The pool pays most when this PC's slot is held by
  something long (`host`, `session`, `e2e`, another agent's gate).
- **Limits.** A plain `build` and `validate --full` stay on this PC while it has
  the `workbench` cap; without it they go to a pool machine ("No Workbench on this PC"). A pre-pool
  branch's snapshot (no `LF_SERVER_SLOT_HELD` in its `tools/cli.mjs`) runs
  local only until it merges main.

### Pass cache

A suite that passed on byte-identical inputs is not booted again. Before it asks
the pool for a slot, `test` hashes its inputs (`tools/lib/test-cache.mjs`) and
looks the key up in `<main>/tmp/test-cache/<key>.json`, shared by every worktree.
On a hit it prints `test: CACHED pass, same inputs as the run at <when> on
<machine> in <worktree> (<passed>/<total> passed ...)`, appends that summary to
`tmp/test-results/summaries.jsonl` with `"cached": true`, and exits 0 in about a
second. `wt gate` and `wt ship` run `ci`, whose suite steps go through the same
check, so a gate right after a green `--tier all` reuses it.

- **The key**: git tree hashes of `addons/`, `tools/test/` and
  `tools/lib/addons.mjs` from a throwaway copy of the index with every untracked
  non-ignored file added; the hash of each `addons/*/resourceDatabase.rdb`; the
  tier, the resolved `--only` list (order kept), `--world` (the routed one too,
  "Test world" below), `--core-only`; the
  launch addon list; this PC's server build id (the pool sends a run only to a
  machine with that build). The machine is not in it: a gate already accepts a
  pass from any pool machine, and `LF_Test_PerfBudget` is calibrated per
  machine. Docs and the rest of `tools/` are not in it.
- **The gate trusts the agent's own run.** The gate's test step asks the cache for the pass its own run would
  produce, so an agent that ran `toolscli test --no-build --tier all` on the exact tree being gated
  (same addons incl. untracked, rdb, `tools/test`, addon list and server build; main not moved over them) is not run
  again: the gate prints `test: reused agent pass <key>` and the usual `CACHED pass` line. `ci`'s sharded suite
  checks the cache per pass (normal and core-only; it did not before) and stores a merged pass under the
  whole-run key. The slim gate's LOGIC step (`--tier fast`) is also answered by an `all` pass of the same tree
  (`lookupCovering` in `tools/lib/test-cache.mjs`); never the other way round, never an `--only` or core-only
  pass for a different selection, never a failure, a timeout or a `--red` run. A stored record whose own inputs
  differ from the ones asked for is not trusted. A pass a queue gate recorded is called a `gate pass` in the line.
- **What is stored**: passes only (exit 0, `failed=0`), written to a temp file
  and renamed. A failure, a timeout or a no-verdict run is never stored; a
  corrupt or unreadable file is a miss.
- **Never cached**: `--no-cache`, `LF_TEST_CACHE=0`, `--tier persistence`,
  red-proof runs (`--red`), `dev` (it prints that run's debug lines), `e2e`, and
  the placement audit's test run. A remote leg runs with `--no-cache`; the
  caller owns the cache.
- `runs.jsonl` rows of a cacheable run carry `cacheKey` (16 hex), so the hit rate
  can be counted later.

### Test world (LOGIC runs)

A run whose every selected test is LOGIC boots the minimal world
`Worlds/TestLogic/TestLogic.ent` instead of DebugWorld (`tools/lib/test-world.mjs`):
a world entity, the time manager, `GameMode_Roleplay_Debug`, the destruction
manager and an `LF_TestRunner` flagged `m_bLogicWorld`. No terrain, village,
annex, zones or spawn points, so the engine skips the entity load and preload a
LOGIC test never reads (about 2.4 s on lf-cloud, 1.5 s on the laptop).

- **Routed to TestLogic**: `--tier fast` (fast runs LOGIC only), and `--only` or
  `--red` when every name resolves to a LOGIC test (class name or `GetName()`,
  read from the `// tier:` lines). `test` prints `test: world
  Worlds/TestLogic/TestLogic.ent (<reason>)`.
- **DebugWorld as before**: `--tier all`, `--tier persistence`, an `--only` list
  with any WORLD, PERSISTENCE or unknown name, `e2e`, `ci`, `wt gate`.
- **Override**: `--world <path>` always wins (`--world
  Worlds/DebugWorld/DebugWorld.ent` for the old behaviour); `LF_TEST_LOGIC_WORLD=0`
  turns the routing off.
- **Mis-tiered tests fail loudly.** In TestLogic a WORLD or PERSISTENCE test fails
  as `FAIL <name> (wrong world)`; a failing LOGIC test prints `hint: <name> ran in
  the minimal LOGIC world ...; if it needs world content, its tier is WORLD`. A
  LOGIC test that asserts nothing there already fails (`no assertions executed`).
- **Pass cache**: the chosen world is the key's `--world`, so a TestLogic pass is
  never reused for a DebugWorld request.

### Workbench limit and the remote gate

Every heavy headless Workbench step on a machine (the `validate-scripts.ps1`
compile, `build`, `look`) first takes one of that machine's Workbench slots
(`tools/lib/wb-lock.mjs`): lock files `tmp/wt/locks/workbench-<k>.lock` in the
main checkout. The limit is `workbenchLimit` in `tools/pool.json` (default 1),
overridden by the `pool` key of `lf.local.json`, then `LF_WB_LIMIT`. A waiter
queues FIFO (tickets in `tmp/wt/wb-queue`) and prints one
`workbench: queued ...` line; a holder whose process is gone is reclaimed.
`pool status` shows `workbench (this PC): <busy>/<limit>`. On 2026-11-14 seven
parallel ships ran seven compiles and builds on this 15.6 GB PC at once.

`ci` (so `wt gate` and `wt ship`) runs the cheap validate checks here and the
Workbench compile and build where there is room (`tools/lib/wb-gate.mjs`).
Every machine with the `workbench` cap (this PC, and each pool machine that
answers ssh) is ranked like the pool ranks machines for tests: live free RAM
less a Workbench reserve (2.25 GB), weighted by speed (`budgetMultiplier`).
A machine whose headroom after the reserve is under the floor (`minFreeMb`,
so about 3.7 GB free in all) is never chosen while another has room: on
2026-11-14 this PC, at 1.5 GB free, started a Workbench that died after 6 s
three gates in a row while `lf-cloud` had 18 GB free. The first ranked
machine with RAM and a free Workbench slot runs the steps; if every roomy
machine's Workbench limit is full, they queue on the best of those. If no
machine has the RAM, the gate prints why (each machine's free RAM) and
re-checks every 30 s for up to an hour (`LF_WB_RAM_WAIT_S`), then fails as a
pool failure, which the ship queue requeues. A pool machine that does not
answer, or answers and exits 3, leaves the choice to the rest (this PC when
nothing else answers).

A Workbench compile that ends within a minute with `NO VERDICT` on a machine
under the reserve is an environment failure, not a compile failure: it
prints `ENV  ... GB free`, skips the build, exits 4, and the gate retries the
steps once on the best other machine. A second early death fails the gate as
a pool failure.

The remote run is `ci --wb-only --local` in a
`Life-Framework-rt-<slug>-gate` run folder, synced like a remote test
(uncommitted state included). ssh is session 0, which has no GPU device, so the box reruns it in its signed-in desktop session (`run-interactive.ps1`); with nobody signed in it exits 3 and the gate chooses again. `ci --local` keeps everything here;
`ci --remote` uses pool machines only; `ci --remote <name>` / `--machine <name>`
pin one machine, even a short one. `wt gate` and `wt ship` pass `--local`,
`--remote [name]` and `--machine <name>` through to that `ci` (the ship queue
gate included). Agents do not pin; a human or the lead can.
The placement audit takes a test lane through the pool like `test` (any
machine) and on a pool machine runs `audit placement` in the synced run folder.

### No Workbench on this PC (the `workbench` cap)

The maintainer's world editor lives on this PC (Gaming-Box) and agents never
run a headless Workbench on it. That is `workbench` missing from this machine's
caps: `lf.local.json` `pool.machines` entry `{ "name": "local", "caps":
["server"] }`. A pool machine's own entry (`lf-cloud`, `lf-testbox`) keeps the
cap and runs what is sent to it. `tools/lib/wb-route.mjs` reads the cap; every
path that would start a Workbench here asks it first:

| Path | Without the cap on this PC |
| --- | --- |
| `ci`, `ci --remote`, `ci --tiered`, `wt gate`, `wt ship` (`chooseGate`) | this PC is never a candidate and never the fallback; no pool machine answering is "wait" (re-check every 30 s, `LF_WB_RAM_WAIT_S`, 1 h), then a pool failure. The retry after an early death never lands here. `LF_TESTBOX_AUTO=0` no longer keeps the gate here |
| the rdb refresh before `test`, `session up`, `serve`, `e2e` and a gate; `rdb`, `rdb --wait` (`chooseRefresh`) | goes to a machine with the `rdb` cap even when it can only queue there; none yet: waits (`LF_RDB_WAIT_S`, 900 s, `rdb --budget <s>`), then warns and leaves the rdb stale. It never falls back to a local refresh |
| `validate --full` (the `validate-scripts.ps1` compile) | `ci --wb-only --compile-only` on a pool machine, its transcript streamed here |
| `build`, and `test` without `--no-build` | `ci --wb-only --build-only [--addon X]` on a pool machine; the exit code is the result |
| `look <area>`, `look item` | runs on a pool machine (`look ... --local --fresh --clean` in its desktop session); the pictures are copied back to `tmp/look` (or `--out`) with the paths in each `index.json` made local. `look serve` (start) refuses: its Workbench is long-lived and local |
| `pack`, `asset` plugin runs, `ui-image`, `wt open --workbench` | refuse with the reason: their output is files in this worktree |
| `cli call wb_*` (and so `agent-box/harness.mjs launch`, `play`) | refuse: every `wb_*` tool needs a Workbench and the MCP starts one itself. `api_search`, `game_read`, `asset_search` and `api class` need none |
| `validate-scripts.ps1` run by hand | refuses unless the CLI launched it (`LF_WB_GUARDED=1`) or `LF_WB_LOCAL=1` |

The backstop is the Workbench slot itself: `wb-route.acquireWorkbench` throws
`LocalWorkbenchRefused` before `wb-lock` takes a lock, so a caller that forgot to
route fails loudly instead of starting Workbench. `validate-wb-policy.mjs`
fails the repo checks when a file takes a slot from `wb-lock` directly, or names
the Workbench executable without consulting the cap. A new launcher goes through
`wb-route` (`routeWorkbench` for a step that can run elsewhere,
`requireLocalWorkbench` for one that cannot).

An explicit `--local` (or `--machine local`) on `build`, `ci`, `rdb`, `look`,
`pack`, `asset`, `validate` or `wt` from a human sets `LF_WB_LOCAL=1` and runs
here anyway; so does exporting `LF_WB_LOCAL=1`. `test`, `session`, `e2e` and
`serve` take `--local` for the server only and never unlock the Workbench.
Agents do not pass it. Proven by `tools/validation/wb-policy-selftest.mjs`
(choice, wait budget, routed run, slot backstop, `--local`).

### Lanes, priority, leases, budgets (this PC)

You change nothing: run `test`, `e2e`, `session up` as before. The pool queues
it, picks a lane and a budget, and prints one line per outcome.

- **Lanes.** `tools/pool.json` gives this PC `"lanes": {"test": 2, "session": 2}`.
  Session lane 1 is the old `server-2001` lock (UDP 2001: `session`, `host`,
  `serve`; clients join `127.0.0.1:2001`; the only one friends reach over the LAN).
  Session lane 2 is `server-2011` on `127.0.0.1:2011` (`-addr`, A2S `-a2sPort 17877`;
  its clients join `127.0.0.1:2011`); `session up` takes the first free lane and
  records it in `session.json` (`lane`), and `down`, `restart` and `join` use that
  one. Harness commands find their session by the checkout's own session dir
  (`%LOCALAPPDATA%LifeFrameworksession<checkout>`) and its session id, so a
  checkout never reaches the other lane's clients; a session client's command no longer
  takes the machine-wide `agent-box-play` claim (that guards the shared default profile).
  `session up` refuses (or queues) only when every lane is taken. Each test lane is its own lock,
  `server-test-1`, `server-test-2`, with its server on `127.0.0.1:2002`,
  `127.0.0.1:2003`. The engine ignores `-bindPort` and `-bindIP` in `-server`
  mode and listens on 0.0.0.0:2001; `-addr <ip>:<port>` is what moves it
  (measured 2026-11-10: a test boot on 127.0.0.1:2002 ran beside a live session
  server on 2001). So a 10 s test never waits behind a 30 min session. A
  machine without `lanes` (the box) keeps one shared lane. Old CLIs keep using
  `server-2001` for everything, so the old lock is still honoured during the
  switch.
- **How many lanes.** The code sets no lane count: a machine's `lanes` decides, and what starts
  is decided live by the RAM gate, the thermal hold and `maxClients`. The one ceiling is the port
  range, 64 test and 32 session lanes per machine (`tools/lib/pool-queue.mjs` "Lane ports";
  `pool status` prints it beside each machine's lanes). Lanes 1-4 (session) and 1-9 (test) keep
  the names and ports above. Later lanes have locks named after their port, in blocks nothing else
  uses: test lanes 10+ on `server-2403`, `-2408`, ... (127.0.0.1, every 5 ports), session lanes 5+
  on `server-2803`, `-2808`, ... (A2S on the port + 1). A lane port ends in 3 or 8, so it is never
  a worktree game port (ends in 1) or A2S port (ends in 7); `pool-selftest` enumerates every lane and
  2000 worktrees and fails on any shared port. Pool code before 2026-11-14 caps session lanes at 4
  and puts a 10th test lane on 2011, the second session lane's port, and it reads only `lanes`.
  So a machine with more than 9 test or 4 session lanes keeps `"lanes"` at most `{"test": 9,
  "session": 4}` (what older branches use) and gives the full count in `"lanesV2"` (what this code
  uses); `pool status` warns about `lanes.test` over 9. Every lane but session lane 1 binds
  127.0.0.1 only, so a new lane needs no firewall rule. A `-server` boot without `-config` binds
  only its game port (seen on lf-cloud 2026-11-14), so the A2S numbers matter only if that
  changes.
- **Disk hygiene (#675).** Every `session up` and local `test`/`e2e` start first removes
  session folders under `%LOCALAPPDATA%\LifeFramework\session` untouched for `sessionKeepHours`
  (24) hours, except one in use (live pids in `session.json`), this checkout's own, and any
  named for film (`film` in the folder name). Deletion is `safeRemoveTree`, the function behind
  `tools/safe-rm.mjs`, so junctions are unlinked, never followed. It then refuses with one line
  when free disk on that volume is under `minFreeDiskGb` (5). Both are settings: `tools/pool.json`,
  the `pool` key of `lf.local.json`, or `LF_POOL_SESSION_KEEP_H` / `LF_POOL_MIN_FREE_DISK_GB`.
  `pool status` prints the free disk. A test that may run anywhere is not refused: this PC drops out
  (`pool: this PC refuses tests for now (...); the run goes to lf-testbox or lf-cloud`) and the pool
  machines take it, each checking its own disk first (`diskRoute` in `tools/remote/pool.mjs`). A run
  pinned here (`--local`), or with no pool machine, keeps the refusal.
- **One request per worktree (#542).** A new `session up` from a worktree whose earlier request is
  still queued drops the earlier queue entry (the older process exits with "a newer session request
  replaced this one") and keeps the first request's age. `pool status` marks any leftover
  duplicate. A waiter never rewrites a ticket that is gone: the older request used to write its
  queue entry back after the drop (its ssh read of the laptop takes seconds), so one worktree
  waited twice.
- **Machine choice (any run that is not pinned).** A test, e2e or session request tries machines in
  order of RAM headroom, read live (this PC's `freemem`, a pool machine's ssh read cached for 8 s,
  30 s when it did not answer, so a decision never costs an ssh):
  `headroom = free RAM - sessions still booting - the job's reserve`; a machine with
  `headroom >= minFreeMb` has room and goes before one without; among machines with room the one
  with the higher `min(headroom, roomyMb) / budgetMultiplier` goes first (`roomyMb` 6144 caps what
  more RAM is worth; `budgetMultiplier` is 1 here and 2 for the laptop, so two roomy machines go to
  the faster one, and the laptop wins over this PC only when this PC's headroom is under half of
  the laptop's, up to the cap). Ties keep the faster machine, then registry order. A machine whose
  RAM could not be read goes after those with room and before those without. A machine below floor
  plus reserve takes no job while another machine has room; with no room anywhere the job waits as
  before. The rule is `q.rankMachines` in `tools/lib/pool-queue.mjs`, proven by
  `tools/validation/pool-selftest.mjs`.
- **Pinning.** `--local`, `--remote [name]` and `--machine <name>` keep their pin, RAM or not. When
  another machine has room and the pool would have chosen it, the run prints one `WARN  pool: pinned
  to local (--local); lf-testbox has room ...; drop --local ...` at queue time, and `pool status`
  shows `waits: ram: needs 1.5 GB free, local has 0.4 (pinned; lf-testbox has room)` while it
  waits. Do not pin to save a thought: the pool's choice is the one that finishes. `ci`,
  `wt gate` and `wt ship` pin nothing; their suite and the placement audit go through the same
  choice (the audit's input list goes up with the run and `out.txt` comes back; only the
  interactive `place` loop stays on this PC).
- **Sessions flow like a river.** A lane is held for the window it is used. A session idle
  `sessionIdleMin` (3) minutes while a job waits for its lane, or `sessionIdleMaxMin` (5) with
  nobody waiting, is closed (the check runs whenever the pool is touched: a queued request polls it
  every 10 s, and so does `pool status`); any harness or session command resets the clock, and
  `session lease [min]` holds one. The hold cap with a session request waiting is `sessionHoldMin`
  (10) minutes, then `sessionGraceMin` (2) to finish, at most `sessionHoldMaxMin` (20) for
  `session up --hold`. `wt gate` and `wt ship` hold a test lane only for the suite run itself,
  never while `validate --full` or the build runs (`cmdCi` takes no lane; the selftest asserts
  that no gate function acquires one).
- **RAM gate (every machine, read live).** A job starts on a machine only when that machine's
  free RAM, read at that moment (`freemem` here, one ssh read on a pool machine, the same check in
  the box's slot holder), covers it. A session reserves its server (`sessionReserveServerMb`, 600)
  plus `sessionReserveClientMb` (850; lowspec 800) per client; sessions still booting count with
  their reserve too, since free RAM does not show them yet. Every start, the first server on a
  machine included, must leave `minFreeMb` (1500) free after its reserve (2026-11-14: the floor
  gated only a second server, so this PC at 0.4 GB free kept taking its first test and session
  runs, which then timed out while the laptop idled). A test reserves nothing beyond that floor
  for itself, but tests still booting (locked under `testBootS`, 60 s) and starts in flight
  ahead of it count with `testReserveMb` (800) each ("Burst starts").
  A job that does not fit waits with one line ("free RAM 2.2 GB minus 1.4 GB for the session
  would be under 1.5 GB"). Lane counts in `tools/pool.json` are ceilings (ports, heat); the
  gate decides how many run, so a RAM change (the laptop lost 32 GB on 2026-11-14) needs no config
  edit. Measured 2026-11-10 on this 15.6 GB PC: server plus one client 1.3 GB, the second client
  +0.8 GB; with ~3.8 GB free and nothing running a 2-client full session (2.3 GB) fits, a second
  one does not.
- **Backfill without starvation.** A job that tried and cannot start for RAM or a lane is passed
  by the jobs behind it that can start now (a lowspec 1-client session goes while a full 2-client
  one waits for RAM). Once the waiting job has waited `starveMin` (20) minutes it holds its place:
  a RAM-stuck one drains its machine (no new job of either lane kind starts there, since tests eat
  the same RAM) until it fits, as long as draining can help it (its need fits in free RAM plus what
  the pool's running jobs there hold). A job that could not fit even on an empty machine never
  blocks anyone.
- **Status.** `pool status` prints each machine's live free RAM (and the laptop's temperature),
  and per waiting job where it may run, the reason (`ram 2-client full: needs 3.8 GB free, local
  has 1.1, lf-testbox has 2.8`; a pinned job adds `(pinned; lf-testbox has room)`; `lane`, `behind N`, `starved, so it holds its place`) and an
  estimated start: the (ahead+1)-th projected lane release on its machines, a session at the median
  hold of the last 50 (never past its hold cap plus grace), a test at its median run plus boot.
- **Priority.** `--priority high|normal|low` on `test`, `e2e`, `dev`, `ci`,
  `session up`, `wt gate`, `wt ship`; `LF_PRIORITY` or `wt new <slug> --priority
  low` sets a worktree's default (recorded in its `tmp/wt.json`). Within a lane
  the order is priority, then arrival. A job moves up one level per
  `promoteMin` (20) minutes waited, so low work cannot starve. Running jobs are
  never preempted. High: blocking bugfixes and the orchestrator's checks.
  Normal: milestone work (the default). Low: spikes, research,
  Development-track experiments, filming retries.
- **Self-deadlock.** If this worktree's own session holds the only slot a test
  could use (a machine without lanes), `test` stops at once with one line and the
  `session.mjs down` hint instead of waiting for itself.
- **Session hold cap.** One session at a time fits this PC, so a session that
  outlives its job starves the queue. A session started after the cap existed
  (`session.json` carries `holdCap`; older sessions are never capped) is judged
  only while another `session up` request is queued: past `sessionHoldMin` (10
  min, `tools/pool.json`) it is warned, and `sessionGraceMin` (2) later it is
  closed (`session down`, logged as a `hold-cap` reclaim, the waiter starts at
  once). The waiting `up` runs the check every 10 s. The warning is one line,
  printed once on the holder's next harness or session command: "session: N waiting;
  finish and run node agent-box/session.mjs down within 2 min or it is closed"
  (a harness prints it on stderr). With nobody queued there is no cap,
  and a warning whose waiter left is void. `session up --hold <min>` raises this
  session's cap (20 at most, the number it was asked for is logged). A test job
  waiting on RAM does not start the cap; the idle rule below still applies.
- **Session records.** Every session writes one line to `runs.jsonl` when it ends
  (`"kind":"session"`: slug, priority, `waitS`, `heldS`, `boots`, `bootS`,
  `closedBy` owner | lease | hold-cap). `pool status` prints the last 8 and the
  average wait and hold; `pool stats` the last 5. Sessions from before this
  change are not recorded (no `heldSince`).
- **Queue age.** A session request that times out or is interrupted (an agent's
  10 min tool call ended) and is retried keeps its first `at` for 15 min
  (`<main>/tmp/wt/pool/queued.json`), so aging (low to normal at 20 min, to high
  at 40, first in line there since it arrived first) is not restarted by every
  retry. `session up --wait` queues up to 90 min (it was 30, which ended before
  a low job reached high). The resource-database refresh runs before the queue.
- **Idle sessions without a waiter, and on the laptop.** A session idle `sessionIdleMaxMin` (5) minutes is
  closed even with nobody waiting (it holds RAM and two client windows), on the next `pool status` or queue
  poll. A pool machine's sessions are judged from this PC too (one ssh read of their lease and the clients'
  command/result mtimes, at most once a minute): idle `sessionIdleMin` while a request here waits for that
  machine, or `sessionIdleMaxMin` regardless; the session is closed there with its own `session.mjs down`
  and `reaped.json` says why. An explicit lease still protects a session.
- **Session lease.** Every `session.mjs` verb and every harness command
  to a session client stamps `lease.json` in the session dir; `node
  agent-box/session.mjs lease [min]` holds the session for that long. A session
  with no command for `sessionIdleMin` (3) minutes while a job waits for its
  slot or for RAM is closed by the next waiting run (`pool: closed the session of
  X: idle N min ...`), and the agent's next harness command says why. The
  mtimes of the clients' `command.txt` and `result.txt` count too, so a session
  driven by an older harness is not mistaken for idle. A session being driven is
  never touched.
- **Runtime budget.** A test run is killed when it exceeds its budget (clock
  starts at the server's ready line; boot has its own `bootBudgetS`, 30 s):
  3 times the p95 of that machine's last 30 passing runs of the kind, never under
  30 s; until it has 5 passes, seeded: `--only` 30 s, `fast` 45 s, `all` 90 s,
  `persistence` 120 s, `e2e` 120 s per pass, times the machine's
  `budgetMultiplier` (the box has 2). Measured here: `--only` 9 s, `all` 10 s,
  `e2e` 22 s. A kind whose recent runs timed out more than 2 % of the time gets
  double. On a timeout the server tree is killed, the slot is released, and the
  caller gets `TIMEOUT after Ns (budget Ns); last log lines:` with 5 console lines,
  exit code 124; the run is logged as `timeout` in `runs.jsonl` and counted in
  `pool status`. `--budget <s>` raises it for a known-long run. To prove it,
  `LF_TEST_HANG=run` holds the suite in the booted server and `LF_TEST_HANG=boot`
  swaps the server for a process that never gets ready.
- **If the bookkeeping breaks** (a corrupt lock, an unreadable history) the run
  prints one `WARN  pool: ...` line and falls back to the single `server-2001`
  slot, plain first come first served, no budget. It never blocks on the queue's
  own state.
- **Two tests from one worktree at once** still share `server/profile/test`; run
  them in order.

### The laptop's lanes (lf-testbox)

`lf-testbox` (ASUS ROG Zephyrus G15: Ryzen 9 6900HS 8c/16t, RTX 3070 Ti, ssh as `lf-testbox`, hostname `PC1`) has
15.6 GB of RAM since 2026-11-14 (it had 47 GB; a stick was removed). With nothing of the pool running it had 3.8 to
4.2 GB free that day (the user's desktop apps, Steam, a Workbench). It also powered itself off several times: ASUS
thermal protection, 0 to 5 minutes after bursts of pool load, at 93 °C with 2 servers and 4 clients (the CPU limit is
about 95 °C; CPU boost is now off). It now stands outside (53 to 60 °C idle), so `tools/pool.json` gives it 2 session
lanes (`server-2001`, `-2011`) of at most 2 clients each (`maxClients`) and 3 test lanes (`server-test-1..3`), as
its RAM allows. Those numbers are the laptop's heat and RAM, not a limit in code ("How many lanes" above). Remote tests now try
every test lane (they used only `server-test-1`: 54 of 54 runs). Its slot holder applies the RAM gate on the laptop's
own free RAM before it takes a lock, and holds new jobs while the newest reading of its thermal logger
(`thermalLog`, `C:\LF\thermal-log\*.csv`, column `thrm_c`, written in the last 2 minutes) is at or over
`thermalHoldC` (90). A session boots there in 18 s (36 s door to door with the sync); a full suite is 10 s (44 s door
to door). The `hostname` key makes the entry the local machine when the CLI runs on the laptop, so the remote
`session up` uses the laptop's lanes and RAM gate, not this PC's. A stale lock is cleared by any pinned run
(`test --machine lf-testbox`).

**Sessions pick the machine.** `session up` (`tools\cli session up` goes through the same router) without
`--machine` considers every machine that can run it, any client count and profile, this PC first on a tie. With
`--wait` the request holds one queue ticket that wants all of them and starts on the first machine with a free lane
and the RAM (and, on the laptop, below its heat hold). A machine that is down when the request arrives stays in it and
is probed again every 10 s while it waits (2026-11-14: two 2-client requests arrived while the laptop was off, went to
this PC and queued there pinned for 58 and 11 minutes while the laptop came back with free lanes). The chosen machine's
own `up` runs without `--wait`: a lost race there exits 75 (`LANE_BUSY_CODE`) and the request queues again for
any machine, keeping its age. `--machine local` (or `--machine <name>`) pins it. The remote session's clients start
on the laptop's desktop. Everything that drives them follows the session there: `harness.mjs`, `session.mjs
state`, `reset`, `join`, `roster`, `film` and every proof verb run in the run folder over ssh (`--local` keeps
one on this PC), and a path of this worktree among a proof's arguments (an out dir) is mapped into the run folder
and copied home afterwards. `asset states <slug> --shots` follows the session the same way; `--local` pins it here.
`session pull` copies other evidence back. Filming there is #482.

**A machine that goes away.** A remote test or e2e run whose slot holder drops, or whose ssh fails while the machine
stops answering, is not a verdict: the machine is marked down for the health window and the run queues again
(`pool: lf-testbox went away during the run ...; queuing the run again`, up to 3 tries). A session `up` whose
machine goes away queues again for any machine; `session down` with an unreachable machine clears the record, and the
next `up` forgets a record whose machine does not answer.

### Session profiles (lowspec and full)

`session up --profile lowspec|full` (`agent-box/session-profile.mjs`). **`lowspec` is the
default**: most proofs read logs and the harness's structured results, never pixels. A session
that takes pictures (`shots`, `shots area`, filming, `journey`, any proof whose images go in a PR)
runs `--profile full`; `--res`, `--visible` and `--sound` without `--profile` also mean full. The
profile is recorded in `session.json`, kept by `join` and `restart`, and shown by `status`. A
`capture` on a lowspec session prints one WARN (the shot still goes out; the proofs capture to
move the camera, so they run unchanged); `shots`, `journey` and `film` refuse a lowspec session.

A profile is a list of levers, each one measured below; `--levers a,b` runs any subset.
`lowspec` = `res fps tex geom post shadow view`:

| Lever | What it sets (source) |
|---|---|
| `res` | 640x360 window (`-screenWidth/-screenHeight`) |
| `fps` | `-maxFPS 15` (30 in full) |
| `tex` | `ResourceManagerUserSettings.TextureDetail 4`: the 768 MB texture-budget tier of `MemoryBudgetTextures` (`ArmaReforger.gproj`; the game's LOW preset is 3) |
| `geom` | `ResourceManagerUserSettings.GeometricDetail 0`: the 204 MB mesh tier |
| `post` | `Postprocess 0`, `PPQuality` HBAO/SSDO/SSR/Rain off, `Fsaa NONE`, `Atoc NONE` (the PS4 platform config's values) |
| `shadow` | `ShadowQuality Low`, `DistantShadowsQuality LOW`, `EnvironmentQuality LOW` |
| `view` | `ObjectDrawDistanceScale Low`, grass `Distance 60 Lod 1`, terrain `Detail 0`, `TextureFilter 1`, `MaxAniso 1` |
| `nds`, `stream`, `sfps`, `st` | server `-nds 1`, `-streamingBudget 200`, `-maxFPS 20`; both sides `-singleThreadedUpdate`. Measured no change; not in lowspec |

Sound is already off (`-noSound`). The engine settings are written into each client profile's
`.save/app1874880_user<steamid>/settings/ReforgerEngineSettings.conf` before every launch (the
Steam id comes from the profile or `loginusers.vdf`); a full launch deletes the file, so a
lowspec run never leaks into the next full one. The server has no per-profile settings: it is a
headless process and none of its flags moved its memory.

**Measured 2026-11-10** on this 15.6 GB PC (`tools\cli session measure --variants ... --proof
1:quarry-proof`: boots each variant on a held lane, runs the proof, reads each process's working
set and private bytes; JSON in the PR). Working set (MB, physical) / private (MB, committed), one client:

| Variant | Client at the registration form | Client after `quarry-proof` |
|---|---|---|
| full | 988 / 2022 | 1150-1184 / 3085-3089 |
| lowspec | 1031 / 1962 | 1074-1087 / 2488 |
| `tex`+`geom` | 1025 / 2078 | 1163 / 2798 |
| `view` | 1035 / 2083 | 1114 / 2656 |
| `res` | 947 / 1976 | 1068 / 2832 |
| `post`+`shadow` | 981 / 2014 | 1162 / 3053 |
| `st` | n/a | 1135 / 3036 |

The server is 699-709 MB working set in every variant. Two clients (at the form): full
949+935, lowspec 914+899. **The graphics levers save little physical RAM**: about 7% of a
client's working set after a proof (80-100 MB) and about 19% of its committed bytes (600 MB);
at idle they save nothing measurable (the client has not streamed the world yet). The engine,
the world, the script heap and the physics are the 1 GB, and no flag shrinks them. So the pool's
reserve per lowspec client is 800 MB instead of 850 (`pool.json` `sessionReserve`); the server's is
unchanged at 600.

**What does free RAM is trimming.** A session waiting for the next harness command touches a
fraction of its resident pages. `EmptyWorkingSet` (`tools/lib/ws-trim.mjs`) moves the rest to the
standby list: still cached, counted as available by Windows and by the gate, faulted back on use.
Measured on the same sessions (working set before, 3 s after, 18 s after; server first):

| Session | Before | 3 s after | 18 s after |
|---|---|---|---|
| full, 1 client, after `quarry-proof` | 707 / 1184 | 26 / 96 | 26 / 106 |
| lowspec, 1 client, after `quarry-proof` | 708 / 1087 | 25 / 85 | 26 / 94 |
| full, 2 clients | 655 / 949 / 935 | 22 / 81 / 81 | 23 / 92 / 93 |
| lowspec, 2 clients | 655 / 914 / 899 | 23 / 73 / 74 | 23 / 84 / 83 |

So an idle session holds about 0.12 GB, not 1.8 GB. The pool does it for you: when `tryLocal`
would refuse a session or test for RAM, it first trims every live session's recorded pids (never
by process name; at most once per 20 s), looks again, and prints `pool: RAM is short; trimmed the
working sets of N session process(es)`. `tools\cli session measure` also prints the trim table.
The 1.5 GB floor still gates: a session is admitted only while free RAM minus its reserve stays at
least `minFreeMb`, and a client that joins late (`session join`) is gated the same way (its client reserve).

**Two lowspec sessions at once (2026-11-10, this PC, other agents' sessions and a Workbench
running beside).** Session A (1 client, then client 2 joins late) ran `siren-late-proof` on lane
2001 while session B (2 clients, then a late third) ran `downed-proof` on lane 2011, both
`--profile lowspec`, overlapping for nine minutes: both PASS, reading only the harness results and
logs. With both up that was 2 servers and 5 clients, about 4.7 GB of working set, and free RAM
bottomed at 165 MB (median 1.1 GB) because the late joins are not gated and the rest of the PC held
about 10 GB. So on this PC **two lowspec sessions fit, and a third does not while two are active**; the
RAM left once they sit idle (trimmed, about 0.12 GB each) is what lets a queued third start. The
late-join path (`session join`) trims first when free RAM is under the floor plus a client's
reserve, then refuses with a one-line reason if it still would breach the floor (#563). The lane count (`lanes.session`) stays 2 here: raise it only after a run with three
concurrent proofs holds the floor.

### Session boot time

Where `session up` spends its time (lf-cloud, lowspec, 1 client, 2026-11-14, from the box's `up.log` and
the client `console.log`; this PC could not run it that day: free disk under the 5 GB floor, and since
the same day client sessions go to lf-cloud or the laptop only):

| Phase | Time |
|---|---|
| Fresh worktree only: resource database refresh here (about 15 s) and on the box, queue, sync | up to 220 s, once per worktree; later `up` is 0.1 s of sync |
| Server launch to `Entered online game state.` | 10-15 s |
| Client launch to its connect attempt: render device 3 s, shader reload 3.4 s, compiling Game scripts 16 s, rest | 28-32 s |
| Connect to the server's registration-form line (world load, join) | 1-3 s, 20-60 s when the box is busy |

The client was the critical path: it used to start only after the server was ready, so the server's
10-15 s were pure waiting. **Client 1 now launches with the server** (`tools/lib/session-overlap.mjs`,
`sessionLaunch` in `tools/cli.mjs`); clients 2 and up still start one after another, each after the
previous one joined, so player ids follow client numbers. Nothing is shared between sessions: every `up`
still boots its own server on this worktree's scripts (a warm, reusable server would add nothing, since
the client's own start-up is longer than a server boot, and it would need a per-build validity key), the lane
lock moves to the server's pid before the client starts, and the RAM reserve and idle reaping are
unchanged.

The risk is a client that reaches its connect attempt before the server listens. The client's own log is
watched (`Starting RPL client`, `Client L2 connection established`): a client that tried and is not
connected 20 s after the server was ready, or died, or never tried within 150 s, is relaunched once (the
line `session: client 1 raced the server; relaunching it once` and `up.log` say so). The worst case is the
old serial time plus the grace period, never a failed session. `LF_SESSION_SERIAL=1` restores the old
order. Polls of the server log and the join check are 0.5 s (were 2 s).

Measured on lf-cloud, ready-in (`up` start to the join line), 10 runs each, one session after another
(the box is shared, so the spread is wide; run it again if a number matters):

| | median | min | max |
|---|---|---|---|
| serial (before; 5 runs before, 5 as a control after) | 61.4 s | 31.9 s | 168.8 s |
| overlapped (after, two batches of 5) | 34.8 s | 25.2 s | 141.1 s |

`selftest`: `tools/validation/session-overlap-selftest.mjs` (the plan, the retry and give-up rules, the log patterns).

### Waiting on a session

`node agent-box/session.mjs up` blocks until the server and every client are
ready (up to about 10 min) and prints about three lines: `session: ready; N
client(s) joined ...` and `session: full log <path>` (the launch command
lines and progress go to that file), or one `FAIL  <reason>` (port 2001 held
by whom, a client or the server exited, a client was refused, Steam refused).
Exit code 0 or 1. `--wait` queues for port 2001 behind another run and prints
one line saying who holds it.

`node agent-box/session.mjs wait [--timeout 600]` does the same for a session
that is already starting (an `up` started in the background or by someone
else): one blocking call, exit 0 when ready, 1 with a one-line reason when the
server or a client exited, the session was closed, nothing is starting after
30 s, or the timeout passes. Never poll `status` in a loop.

`node agent-box/session.mjs join <n>` starts client n (1 to 3) on a running session that
was brought up without it, so a proof can show a late joiner: one blocking call that returns
when the server log shows the extra join. `downed-proof` uses it for client 3.

### Sessions on a pool machine

`node agent-box/session.mjs up --machine <name|any|local> --clients N`. A machine with the `clients` cap keeps a
user signed in; its `up` starts each client on that desktop through an interactive scheduled task
(`tools/lib/interactive-launch.mjs`), and its server through WMI (`Win32_Process.Create`), because ssh kills every
child of the session when the connection closes. Without the cap a remote machine runs server-only sessions
(`--clients 0`). The machine is remembered in `<worktree>/tmp/session-remote.json`, so `down`, `status`, `state`,
`restart`, the harness and the proof verbs from that worktree go there (above). `journey` drives clients in-process,
so it runs on this PC; it refuses `--machine lf-testbox` and a recorded remote session. Clients on this PC can join a
box server over the LAN (manual steps below).

### Box setup

Machine settings live in the gitignored `lf.local.json` of each machine's
main checkout (`docs/setup.md` "Machine-local settings"). On this PC:

```json
{ "LF_TESTBOX": "lf-testbox" }
```

`tools\cli testbox provision` copies `tools/remote/provision-testbox.ps1` to
the box and runs it: Git and Node.js 22+ through winget only when missing,
the dedicated server (app 1874900), a clone in `C:\LF\Life-Framework`, and
the box's `lf.local.json` (`ENFUSION_SERVER_PATH`, `LF_GAME_ADDONS` = that
server's own `addons`, whose `core` and `data` the test server loads). For
the server it uses a complete install in one of the box's Steam libraries
(the box's Steam client keeps it on the current build, like the main PC's);
without one it installs SteamCMD in `C:\steamcmd` and the server
**anonymously** into `C:\LF\ArmaReforgerServer` (`-UseSteamCmd` forces
that). It then sends this checkout's `resourceDatabase.rdb`. Results compare
across machines only on the same server build: every remote run prints both
builds and warns on a mismatch. Prerequisite: an `ssh lf-testbox` key login as
a local administrator whose default shell is Windows PowerShell.

A stale `resourceDatabase.rdb` fails world tests the same way on both
machines (on 2026-11-08 the main checkout's copy predated the jail and stash
prefabs: `hideout/stash`, `jail/slip` and two others failed with
`not found in replication`). `test` refreshes a stale copy on this PC
(headless Workbench) before it picks the machine, so the box always gets a
fresh one.

### Game clients and filming on the box (manual)

The box's Steam client (signed into a different account than the main PC's,
so both can run Reforger at once) has Arma Reforger, Arma Reforger Tools, and
Arma Reforger Server in its library. The test runs use only the server. The
provisioning never starts, signs into, or changes Steam. Workbench is not
used on the box; `build` and `validate --full` stay on the main PC.

**A client on the box joining a session on the main PC.** Not wired into
`session up`: the harness talks to a client through files in its local
profile, so a box client is for watching and filming, not harness verbs.
Not yet exercised end to end.

1. Main PC (hostname `Gaming-Box`, LAN 192.168.1.130), in your worktree:
   `tools\cli session up --clients 1` (server on UDP 2001 plus client 1). Its
   Windows network is on the Public profile; the Steam-created "Arma Reforger
   dedicated server" rules already allow `ArmaReforgerServerDiag.exe` inbound
   there.
2. Send the same commit to the box: `tools\cli test --no-build --only
   LF_Test_AccountWantedClamp --remote` checks it out in
   `C:\LF\Life-Framework-rt-<slug>`. The server refuses a client whose scripts
   or defines differ ("script source code checksum does not match").
3. On the box's own desktop (the client must render in the signed-in user's
   session, not over ssh):

   ```bat
   "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\ArmaReforgerSteamDiag.exe" -client Gaming-Box -addonsDir C:\LF\Life-Framework-rt-<slug>\addons -addons LifeFramework,LF_Everon1989 -profile C:\LF\client2 -window -screenWidth 1280 -screenHeight 720 -scrDefine LF_SESSION_IDENTITY -noBackend -disableCrashReporter
   ```

   Use `192.168.1.130` if `Gaming-Box` does not resolve. For `--mission
   playtest` add `-scrDefine LF_HARNESS`.

**The box hosting the server.** The box's network is Private. An inbound rule
"LF Arma Reforger Server (LAN)" (UDP, Private profile, the Steam library's
`ArmaReforgerServerDiag.exe`) was added on 2026-11-08 so main-PC clients can
join a server there (`-client PC1`, LAN 192.168.1.128).

**Filming on the box.** A client started on the box's desktop is a normal
foreground window there, so screen capture sees it (on the main PC the
session clients render on the hidden `LF-Agents` desktop, which capture
cannot see). From the box's desktop, in its clone: `node tools/film/film.mjs
fetch` once (downloads ffmpeg to `tools/film/.bin`), then
`node tools/film/film.mjs record --window "Arma Reforger" --seconds 20`;
the clip lands in `agent-box/reports/film/`. Over ssh capture gets no frames:
ssh has no desktop; use the `testbox` commands below, which capture in the
signed-in session.

### Desktop on the box (virtual display, recording, clients from ssh)

ssh runs in session 0. The box's signed-in desktop is session 1, so
`tools/remote/desktop/run-interactive.ps1` runs a program there through a
one-shot scheduled task (LogonType Interactive, the ssh user) and removes the
task afterwards. The scripts in `tools/remote/desktop/` are copied to
`C:\LF\desktop` on every call; output comes back to `tmp/testbox/`.

```sh
tools\cli testbox screenshot                 # png of every active display; prints each screen's bounds
tools\cli testbox record 10 [--gdigrab]      # 10 s mp4, screen + system audio, plus its audio levels
tools\cli testbox start "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\ArmaReforgerSteam.exe" -window -screenWidth 1920 -screenHeight 1080
tools\cli testbox vdd                        # (re)install the virtual display driver, no reboot
```

- **Virtual display.** With the lid closed the box has no active display, so
  nothing renders and there is nothing to capture. The Virtual Display Driver
  (github.com/VirtualDrivers/Virtual-Display-Driver, MIT, 25.7.23; the
  catalog is signed by SignPath Foundation) was installed on 2026-11-08 by
  `install-vdd.ps1`: a `Root\MttVDD` display adapter with one monitor,
  "Generic Monitor (VDD by MTT)", whose modes come from
  `C:\VirtualDisplayDriver\vdd_settings.xml` (1920x1080 and 1280x720 at
  60 Hz). The install added the SignPath certificate to the machine's
  TrustedPublisher store so it runs without a prompt; it needed no reboot.
  With the lid open the internal panel stays the only desktop (the virtual
  monitor is enumerated but not extended onto); with the lid closed it is the
  only display, so Windows makes it the desktop. The lid action on AC is "do
  nothing", so the box stays awake. Remove it with Device Manager, "Virtual
  Display Driver", Uninstall device (tick delete driver).
- **Audio.** `lf-loopback.ps1` is a WASAPI loopback of the default output,
  written to stdout as raw PCM with silence padded in so it stays aligned with
  the video; `record.ps1` pipes it into ffmpeg next to `ddagrab` (DXGI output
  0, `-Output N` for another) or `gdigrab`. It compiles its C# in memory with
  `Add-Type`, because Smart App Control blocks unsigned exes on the box (a
  `csc`-built exe ran once and was blocked after the next rebuild). Loopback
  records after the endpoint's volume and mute: the laptop speakers were muted
  at 0 %, so every clip was silent (-91 dB). The default output is now
  "Steam Streaming Speakers" at 100 % (`lf-loopback.ps1 --use <endpoint id>`),
  which plays to no speaker, so the room stays quiet and the loopback hears the
  game. `lf-loopback.ps1 --diag` prints the default endpoint, volume, and mute.
  ffmpeg is `Gyan.FFmpeg` 9.0.2 from winget (GPL-3.0, full build: `ddagrab`,
  `libx264`).
- **Clients.** Start the game with its working directory set to the game
  folder (`run-interactive.ps1` uses the exe's folder); from `C:\Windows\System32`
  the client fails with `Game addon '58D0FB3206B6F859' not found`. Close it
  with `run-interactive.ps1 -Exe taskkill.exe -Arguments "/IM ArmaReforgerSteam.exe"`
  (a window close, so it shuts down cleanly).
- **Proof, 2026-11-08 (lid open, internal panel 2560x1440 at 150 %).** The
  release client drew its menu background in a 1920x1080 window 50 s after
  `start`; a 10 s `record` of it was 2560x1440 at 28 fps with mean audio
  -25.9 dB, max -12.9 dB (the menu music). A Windows chime looped through the
  loopback alone measured -30.9 dB mean. Not yet measured: a capture with the
  lid closed, on the virtual display.

### A rented box

A rented Windows box joins the pool with one script on the box and one command here. `lf-cloud` (AirGPU,
Windows Server 2025, 8 vCPU, 31.6 GB RAM, Tesla T4, billed by the hour, never idle-stopped) joined this way on
2026-11-14. Treat a rented box as untrusted: it gets a public key and an anonymous HTTPS clone, never a token,
password or repo credential.

1. **Tailscale key.** In the Tailscale admin console, make a one-off auth key with a tag (for example
   `tag:pool`). A tagged key joins the box without anyone signing in to Tailscale on it.
2. **On the box**, in PowerShell run as administrator:

   ```powershell
   irm https://raw.githubusercontent.com/Life-Framework/Life-Framework/main/tools/remote/setup-cloud-box.ps1 | iex
   ```

   It asks for the auth key and then does the rest: power never sleeps, OpenSSH with key login only and port 22
   open to Tailscale addresses only, Tailscale, Git, Node 22 and Steam. Steam opens with the Arma Reforger and Tools
   installs queued. The one manual step is the Steam sign-in.
   After a reboot Steam can come back signed out (`HKCU\Software\Valve\Steam\ActiveProcess\ActiveUser` is 0, `Clearing in-memory token ... cached creds not available` in Steam's `logs\connection_log.txt`); sign in again by hand. Workbench then still writes the rdb in ~3 s but freezes instead of exiting; the refresh stops it once the rdb is complete, and gives up within ~65 s with "Steam is not signed in" if a freeze dump appears with no complete rdb.
3. **Steam window.** On a cloud GPU box Steam's window stays blank unless its web views run without the GPU.
   The setup script starts it with `-cef-disable-gpu -cef-disable-gpu-compositing`; restart it the same way.
4. **ssh.** Add a `Host lf-cloud` entry to `~/.ssh/config` on this PC (the box's Tailscale address, user, key).
   The address stays out of the repo.
5. **Provision** from the main checkout: `tools\cli testbox provision --host lf-cloud`. It uses a complete
   dedicated server (app 1874900) in the box's Steam library; Steam keeps that copy on the current build, as on
   this PC. Without one it installs the server through SteamCMD anonymously into `C:\LF\ArmaReforgerServer`,
   which does not update itself, so run `provision` again after a server update. It then clones the repo to
   `C:\LF\Life-Framework` and sends the rdb (above). `pool status` must show the same server build as this PC.
   A provision stopped part-way leaves SteamCMD running on the box; the next run waits for it.
   It also gives `Users` modify on `C:\LF` (inherited, so new and existing files). ssh runs elevated, so the
   clone, the run folders and the rdb `scp` sends belong to Administrators, while Workbench in the desktop session
   runs with a standard token. A stock Server image's `C:\` gives Users no modify, so that Workbench could not save
   `resourceDatabase.rdb` (`ResourceDB: could not open cache file`, exit 0, nothing registered). A client image
   inherits `Authenticated Users:Modify` from `C:\` and never shows it.
6. **Join the pool** with the gitignored `lf.local.json`. A rented box stays out of `tools/pool.json`.
   Give the same entry to both sides: this PC's `lf.local.json`, and the box's own
   `C:\LF\Life-Framework\lf.local.json` with `hostname` set to the box's hostname. On the box, the box-side
   copy makes the entry the local machine, so `session up` uses its own lanes there and not `tools/pool.json`'s
   `local` lanes. `provision` keeps that key.

   ```json
   "pool": { "machines": [ { "name": "lf-cloud", "via": "ssh", "host": "lf-cloud", "root": "C:/LF/Life-Framework",
     "hostname": "airgpu", "slots": 1, "lanes": { "test": 8, "session": 4 },
     "caps": ["server", "clients", "workbench"], "budgetMultiplier": 2 } ] }
   ```

   Start with `"caps": ["server"]`. Add `clients` once a pinned `session up --machine lf-cloud --clients 1`
   joins. Add `workbench` once the box's Workbench has run a gate: `ci --remote lf-cloud` must print
   `Workbench compile + build on lf-cloud: exit 0`. `--wb-only` runs here and proves nothing about the box.
   Add `rdb` once a run there with a stale rdb prints `resource database refreshed in Ns (N resources
   registered)`; `workbench` alone does not prove it (lf-cloud's Workbench compiles but registered nothing
   on 2026-11-14).
   `{"name": "lf-cloud", "disabled": true}` takes it out. Session lanes stop at 4 (`pool.mjs`). Test lane k
   uses port 2001+k, so more than 9 test lanes would collide with session port 2011. A box-side
   `"workbenchLimit": 2` under `pool` lets two headless Workbenches run at once there.

**Measured on lf-cloud, 2026-11-14.** Its free RAM was 25 GB with Steam running; a server is about 0.7 GB and a
client about 1 GB. The box runs about 2x slower per thread than this PC (the perf test's calibration loop took 33 ms
against the 16 ms reference). It starts with generous lanes (8 test, 4 session) and loses some only on real
timeouts, or when runs under load take more than about 2x a single run. With several suites running together,
`LF_Test_PerfBudget` `money-event-at-cap-us` once came in just over its load-scaled budget (333 against 300 µs). A
lone `test all` took about 50 s there; under load it took 50 to 99 s. This PC's median is 18 s and the laptop's
42 s, so lf-cloud's `budgetMultiplier` is 2. A server-only session was ready in 21 s. A 1-client session joined in
40 s (101 s on the first client launch). The Workbench compile took 94 s.
On the first gate that went there, the wait for the box's one Workbench slot (505 s, behind another gate) was most
of the time.

**Shared Steam account.** A Steam account runs one game at a time, so two machines signed in to the same account
cannot both run game clients. `lf-cloud` and `lf-testbox` share an account. The machine with clients keeps the
`clients` cap. The other machine gets a `caps` override in this PC's `lf.local.json` without it, for example
`{"name": "lf-testbox", "caps": ["server", "workbench"]}`, and keeps taking tests and server-only sessions. Steam
also pauses its downloads while a client runs, so a box still downloading Tools gets no further during sessions.

## Live reload (#970)

Scripts and the string table are compiled in and cannot be read again in a running game, and the engine
caches a `.conf` once loaded (`Resource.Load` "gets it from cache"). What can change without a restart is
text read from the profile. `reload <what>` (harness verb; the server decides) re-reads it and sends it to
every client through the config stream:

| What | Reads | Shows |
|---|---|---|
| `config` | `$profile:LifeFramework/config.json` (what `/configreload` does) | shop prices, vehicle prices, police, organizations, ... |
| `dialogue` | `$profile:LifeFramework/dev/dialogue.json`: `{"captions": {"#LF-Dialogue_Clerk_Greet": "new text"}}` | the NPC's caption, the replies and the speaker name in the next scene |

```
node agent-box/harness.mjs --client 1 reload                  config and dialogue
node agent-box/harness.mjs --client 1 reload dialogue         one of them (a comma list is fine)
node agent-box/harness.mjs --client 1 dialogue caption '#LF-Dialogue_Clerk_Greet'
                                                              what this client would show, and the override count
node agent-box/session.mjs live-reload-proof                  the proof (up --clients 1)
```

- Gate: `config` needs the dev gate (`LF_DevGate.CheckPlayerRequest`: dev tools or an administrator; a harness build opens it). `dialogue`
  also needs dev tools open in the game mode (`LF_ContentReload.DevToolsOpen`: a dev world or a harness
  build); an administrator alone does not read it, so a live server never reads a developer's file.
- Dialogue: the text is a caption only. The voice is the recording and does not change; node structure,
  replies and hooks live in the `.conf` and need a restart. The overrides live in memory: a restart, or a
  reload with no file, drops them. A client that joins later gets them through the join snapshot.
- The log names what changed: `[LFDebug:Dialogue] reload: <key> changed [old] -> [new]`, then
  `[LFDebug:Config] reload dialogue by player N: 1 added, 0 changed, 0 removed, 1 override(s) now`.
- Terms (#958) are not built yet. They plug in with one file: `class LF_ContentReloader_Terms :
  LF_ContentReloader` (`Core/Config/LF_ContentReload.c`), found by name, no list to append to.

## Scratch files

Each worktree has a gitignored `tmp/` (created by `wt new`, also home of the
`tmp/wt.json` marker). Put scratch there with names unique to the task, such
as `tmp/pr-body.md`. A shared session scratch dir lets one agent publish
another's file. `validate-worktrees` warns when the main checkout has
untracked files under `addons/` or `tools/`.

**One-time setup**: GitHub CLI must be authenticated once per machine
(`gh auth login`). The REST API fallback works with a `GITHUB_TOKEN` env var
instead.

## Maintainer world edits

The maintainer edits DebugWorld by hand in Workbench in his own worktree (`Life-Framework-ws-town-maintainer-edits`,
branch `ws/town-maintainer-edits`). When he says done, the lead runs, from the main checkout on that PC:

```sh
toolscli world diff --wt town-maintainer-edits          # what changed, one line per entity, NOISE apart (under 3 s)
toolscli world ship --wt town-maintainer-edits --dry-run
toolscli world ship --wt town-maintainer-edits          # one SUMMARY line; then toolscli wt ship-status town-maintainer-edits
```

- **diff** (`tools/lib/world-diff.mjs`) matches entities by name, or by prefab plus parent path when unnamed, children
  included, and prints MOVED (old -> new coords and angles, metres), ADDED, REMOVED or PROP CHANGED (the property).
  NOISE is what a Workbench save rewrites without changing anything: the BSP line in `default.layer`, a block at
  another file position, `angleY 180` written as `angles 0 180 0`, an explicit `GenericEntity` prefix dropped,
  whitespace. `--json` for tools.
- **ship** refuses while Workbench's window title shows unsaved changes (`*`; Workbench keeps no lock file per
  unsaved edit) and warns while it runs. It refuses `addons/` changes other than layers, restores noise-only
  layers to the merge base, commits the rest (the worktree's git identity, or `--author`), runs `validate`,
  `audit placement --layer <touched>` and the tests that name a touched layer or entity, adds each audit finding
  on an entity the maintainer touched to `agent-box/place-audit-allow.json` with the reason "maintainer hand
  placement (world ship)" (listed in the PR body), then runs `wt ship` with a body generated from the diff.
- **Gate tier T1w** ("Gate tiers"): the queue gates such a branch with static validate, the placement audit on the
  touched layers and the layer-named tests (a failing test warns, it does not block). A property or component change, a new layer, a prefab no layer used
  before or any other `addons/` file makes it T2.
- `toolscli world sync` is the other half (`tools/lib/world-sync.mjs`, a separate change).
- Agents never touch the maintainer's worktree; the proofs run on fixtures (`tools/test/fixtures/world-diff/`).

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

## Maintainer world edits

The maintainer hand-edits DebugWorld in Workbench, in his own worktree (`ws/town-maintainer-edits`) on the lead's PC, with
uncommitted `.layer` edits at any time. `toolscli world-sync` keeps that worktree level with `origin/main` without losing
or mangling them. (Other tools for this workflow add their own `###` subsections under this heading.)

### What blocks a world ship, what warns

`world ship` and the T1w gate run `validate --world`: only the checks that read `.layer` files (`tools/lib/world-checks.mjs`, about a dozen, a few seconds), not all of them. **Blocks:** a layer parse error, a missing or unknown prefab GUID, a duplicate GUID, `validate-world-zones` (an edit outside town layers), a script compile error (or a test run that printed no SUMMARY and so judged nothing). **Warns** (listed under `## Layout warnings` in the PR body, never blocking): a failing layer-named test (each failing assertion and its test, under `## Test warnings (maintainer layout)`; a maintainer's edit is never blocked by a test, `tools/lib/world-test-judge.mjs`), the town manifest's sign distances and neighbour rule, the village-name and fixture checks, the player-props and road-id checks, and the placement audit's findings. On a `town-maintainer-*` branch a plain `validate` (CI's included) demotes the same layout checks to `WARN`. Selftests: `tools/validation/world-checks-selftest.mjs`, `tools/validation/world-test-judge-selftest.mjs`.

Every check that reads `.layer` files is classified in `tools/lib/world-checks.mjs` (`HARD_CHECKS`, `LAYOUT_CHECKS`, or `LAYER_READERS_KEPT_HARD` with its reason); `world-checks-selftest` fails on a new layer-reading check that is none of them. The placement-audit part of `agent-box-selftest` (every vanilla prefab a layer places has a row in `tools/place/prefab-bounds.json`) counts only prefabs an entity is declared from (`Class Name : "{GUID}path.et" {`), never a `Prefab` property such as an NPC's loadout-slot clothing, and on a `town-maintainer-*` branch a missing row prints `WARN` instead of failing (`tools/lib/layer-prefabs.mjs`).

### Warn-only gate and the tier of a synced branch

On any `town-maintainer-*` branch the `ci` placement audit (full gate, T1 and T1w) and the layout checks of `validate` are warn-only: they print `WARN`, record their findings in `tmp/gate-warnings.json`, and the ship queue replaces a `## Gate warnings (maintainer layout)` section in the PR body after each passing gate (`tools/lib/gate-warnings.mjs`). The ci test step (full, sharded, core-only, T2s slim and T1) is warn-only too: a failing test prints `WARN`, is recorded under the `tests` check in `tmp/gate-warnings.json` and listed in the PR body (`demoteTests`, judged by `tools/lib/world-test-judge.mjs`); a script compile error or a run with no `SUMMARY` line still blocks. A hard check (parse, GUIDs, world zones, compile) still blocks. The tier is judged on the branch's own change: `ownBase` (`tools/lib/gate-tier.mjs`) is the merge base with `origin/main`, moved up to the newest main the branch merged (`world-sync`, `wt ship`), so main's files never count as the maintainer's, even with a stale local ref. When a hand edit is refused as T1w (a property change, a prefab no layer used), the tier line says so ("not a T1w hand edit: ..."), and that edit takes the full gate with the audit still warn-only. A `world ship` body is cut at 60 KB with a "+N more" line (the whole list is `world diff`); `gh pr create` and `gh pr edit` always read the body from `--body-file`. Proof: `tools/validation/maintainer-gate-selftest.mjs`.

### Keeping it level with main (`world-sync`)

```sh
toolscli world-sync [--wt <slug>] [--dry-run] [--force]     # default --wt town-maintainer-edits; run on the PC that has it
toolscli world-sync --watch-hint                            # prints the line the lead runs at each check-in (no daemon)
```

Engine: `tools/lib/world-sync.mjs` (`syncWorktree(dir, { dryRun, force })`); the CLI entry is `tools/commands/world-sync.mjs`.
Selftest: `tools/validation/world-sync-selftest.mjs` (temp repos). Never run it against his worktree from another machine.

1. Fetches `origin main`, lists what main changed since the merge base and what he changed locally (staged, unstaged,
   untracked, and committed but not on main).
2. **No overlap:** fast-forward, or a merge commit when he has commits main lacks. The working tree and index move with
   `git read-tree -m -u` (the engine under a fast-forward), so every local edit is carried across; afterwards the unstaged
   and staged diffs and every dirty file must be byte-identical or the run counts as failed.
3. **Both sides changed a `.layer`:** that file is *held* (`held: you and main both changed X`) and stays exactly as it is.
   If main renamed it, the other half of the rename is held too, so the world never gets two copies. Everything else main
   changed is pulled into the working tree and **staged, with HEAD unmoved** (history is never told the held file was
   merged; a rerun is stable and reports `pulled=0` until he commits the layer and a normal merge settles it).
4. **Both sides changed any other file:** *blocked*, nothing changes, exit 2. Commit or set that file aside, then run again.
5. **Workbench open on that worktree** (a process whose command line holds the worktree path): still syncs, then prints one
   line to reload the world (File > Reload, or reopen it). `reload-workbench=yes` in the summary.
6. Never `reset --hard`, `--force`, `checkout --` on a modified file, or a stash. Before the first write the dirty files are
   copied under the git dir; any error after that puts HEAD, the index and the files back, deletes the copy and exits 1
   (`state-restored=yes`). If the restore itself cannot be proven, the copy is kept and the message names it.

```
SUMMARY world-sync ok pulled=12 files held=1 (Street_Jobs.layer) reload-workbench=yes
SUMMARY world-sync blocked pulled=0 files held=0 blocked=1 (notes.md) reload-workbench=no
```

Exit 0 ok (held files still count as ok), 2 blocked, 1 error. `--force` only lets it sync a branch that is not `ws/*`.

## Cleanup

Merged worktrees show `merged (prune me)` in `cli wt list`. `cli wt prune
<slug>` verifies the branch is merged and clean before removing it; `--force`
discards unmerged work. A squash-merged PR counts as merged (a merged PR whose head SHA is the branch tip, or
the same files as origin/main). `cli wt prune --merged` lists every worktree whose branch has a MERGED PR, is clean, idle 30 min and
holds no lock or session; `--yes` prunes them, without it a dry run. The merged PR comes from one batched
`gh pr list --state merged` (newest 500, 45 s timeout), matched on `ws/<slug>` and the worktree's base, and its
head SHA must equal the branch tip, so a branch with a commit the PR did not carry is kept. **Ancestry of
`origin/main` never qualifies a worktree**: a brand-new worktree is an ancestor too. The dry run says why each
one stays (`keep <slug>: no merged PR (ancestor only)`, `PR #N merged, but the branch has commits after it`,
`uncommitted changes`, `touched N min ago`, a lock). When `gh` is missing, fails or times out, nothing is pruned
and the command exits 1 saying so; there is no fallback to ancestry. Warm-pool trees are never listed. `wt prune <slug>`
and `--force` keep their checks: you named the worktree on purpose. `wt ship --refs N` needs N of 1 or more; with
no issue, leave the flag out. `cli validate` warns you about worktrees that are ready
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

## Incremental validate

Plain `tools\cli validate` (and `dev`) runs a check only when something it reads changed since its last green run in
this worktree, and says `incremental: ran N, skipped M`. `validate --all`, `validate --full`, `ci`, `wt gate`,
`wt ship` and CI (`CI` or `GITHUB_ACTIONS` set, or `LF_VALIDATE_ALL=1`) always run every check.

- A check declares its inputs in a header line in its first 30 lines: `// inputs: addons/**/*.layer, docs/features.md`
  (`# inputs:` in a .ps1). `*` stays in one folder, `**` crosses folders; the check's own file, `tools/validation/*.json`
  and `tools/validation/fixtures/**` are always inputs. `inputs: always` runs every time (a check that reads machine or
  git state: the worktree registry, `origin/main`); `inputs: ttl=300` trusts a green run for 300 s. No header means always.
- State is `tmp/validate-state.json`: a fingerprint (path, size, mtime of every matching file from
  `git ls-files --cached --others --exclude-standard`) written only when the check passed, so a failing check is never
  skipped and an edit, add, delete or rename re-runs it. Delete the file to start cold.
- Checks start longest first (from the recorded times) and every run ends with `slowest: ...`.
- Each check has a budget: 600 s (`LF_VALIDATE_CHECK_BUDGET_S`), 1200 s for the Workbench compile. A check over it
  prints `TIMEOUT validate/<check>`, is killed with its children and fails. Check times leave out any time the
  validate process itself was frozen (a synchronous call beside the checks, as `ci` runs validate beside the build);
  that freeze is printed once as `WARN  validate: this process was frozen N s`.
- A new check adds the header; `validate-incremental-selftest.mjs` warns about one without.

## Lead view

`tools\cli lead` is the dispatch screen for the lead agent (about 30 lines, about 10 s, no session or lock taken). The sections run in
parallel, each with its own timeout; one that fails prints one line and the rest still print.

- MILESTONES: open M2 and M3 issues split by label: needs code (`agent:working` counted separately), `in-dev`, `needs-playtest`,
  `needs-human`. A human gate beats a playtest, a playtest beats in-dev. M2.5 is not M2.
- PRS: every open PR, conflicting first, then BLOCKED/UNSTABLE/BEHIND, then CLEAN; checks and age. A conflicting PR names whether only
  generated or driver-merged files conflict (`prs heal` fixes those) or a source file needs a human (`git merge-tree`, no checkout).
  A PR GitHub has not graded yet (UNKNOWN) is asked through the REST endpoint.
- WORKTREES: active (a file, commit or log touched in 30 min) vs idle; merged-prunable; orphans (in the registry but not a git worktree
  or missing, or a `Life-Framework-ws-*` folder nobody registered); disk. A worktree is merged only through a merged GitHub PR whose head
  SHA is the branch tip, never through git ancestry: a fresh worktree is an ancestor of main and is never listed (`lead-selftest`).
  Disk sizes are cached per folder for 24 h in `tmp/wt/lead-disk.json` and refreshed in a 9 s budget, so the first runs say
  `~N GB (k/n measured)`; the walk never follows a junction.
- POOL: one line per machine (free RAM, lanes busy/total, temperature, `up`, `slow` or `DOWN`) and the queue length. Each machine is
  read in its own process (`lead --pool-worker <name>`), all at once, with a per-machine timeout (9 s, `LF_LEAD_POOL_MACHINE_MS`) inside
  an overall budget (12 s, `LF_LEAD_POOL_BUDGET_MS`). A machine that has not finished shows what it had: `slow` when it answered within
  5 min (a busy box, lf-cloud's 7 s ssh), `DOWN` otherwise, with `** slow: no answer in 9 s **`; a slow machine never drops the section.
  When the budget itself cuts a machine the section ends with `pool budget N s exceeded ... partial rows for ...`. A machine whose
  free RAM is under the pool floor (`minFreeMb`, 1.5 GB) adds `WARN RAM: <machine> has 0.5 GB free, under its 1.5 GB floor`: it takes no new job.
- SHIP QUEUE: `SHIP QUEUE 1 gating, 2 queued, oldest wait 14 min (<slug> #<pr>)`, from the same file `ship-queue status` prints.
- ACCOUNTS and SLICES appear when `usage --accounts` and `slice list --ready` exist; until then each prints one skip line.

`tools\cli lead prune-safe [--dry-run]` prunes the merged-prunable worktrees through `wt prune` (never `--force`) and prints one line each:
a merged PR on GitHub, no uncommitted change, idle 30 min, no live lock or session, and re-checked clean just before the delete. When
GitHub cannot list the merged PRs it prunes nothing.
