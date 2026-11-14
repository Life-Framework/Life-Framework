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
  unregistered resource(s) ... -> refreshing`, about 10 s). `toolscli rdb` does the same on
  demand, and `validate` warns. The main checkout's copy is the human's editor file: it is
  only warned about, never rewritten. `LF_RDB_REFRESH=0` turns the refresh off. Do not commit it.
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
tools\cli wt test <slug>        # run the LFTEST suite in that worktree
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

## Base branches

Worktrees cut from and ship to `main` by default. For a long-lived branch such as `m3`:

- `tools\cli wt new <slug> --base m3` cuts from `origin/m3` and records the base in the hub state; `wt ship`, `wt sync`, `wt prune`, `wt list` and `prs heal` use it (PR base, merge-tree check, auto-merge, merged detection). `wt ship --base B` overrides. Worktrees with no recorded base stay on `main`.
- `tools\cli wt sync-base m3 [--source main]` opens a PR that merges `origin/main` into `m3` from a `ws/sync-m3-<stamp>` worktree (merge drivers resolve generated files; any other conflict stops with the file list). It never force-pushes; prune the sync worktree after the merge.
- The `validate` workflow runs on every `pull_request` (no branch filter), so PRs into `m3` get it. Making `validate` a required check on `m3` is a GitHub branch-protection setting, not done by the tooling.

## Release gates (`stable`, `promote`)

`main` is the integration branch; agents branch from it and merge to it all day, as before. `stable` is the branch a human moves, and the only one the Workshop release, the flagship's 24/7 server and `toolscli release` use. Playtests run on `main`.

- `toolscli promote [--to <sha>]` fetches, lists the PRs merged to `main` since `stable` (first-parent merge commits) and writes `tmp/promote/checklist-<sha>.md`: one item per PR with its issues and the manual steps from its body. It changes nothing.
- `toolscli promote --to <sha> --confirm` fast-forwards `origin/stable` to the commit you verified (default: `origin/main`), tags it `stable-<yyyymmdd>-<sha8>`, then marks the issues those PRs ref `verified` and closes them. An issue that a later, unverified PR on `main` also refs stays open. It refuses a target that is not a descendant of `stable` and never forces.
- `toolscli promote --fail N[,N]` marks issues `regression` and reopens them without moving `stable`. With `--confirm`, a failed issue whose PR is inside the range blocks the promotion: fix it on `main`, revert it, or pass an earlier `--to`.
- `toolscli promote --revert <PR>` opens a PR that reverts that merge on `main` (from a `ws/revert-pr<N>` worktree; auto-merge queued). `stable` is never edited directly.
- `toolscli promote --init [--to <sha>]` creates `origin/stable` once. It was created from `main` because no M1 sign-off commit is recorded; the first real promotion is #404.
- **`toolscli release <x.y.z>` (#369), human step.** Run from `main` or `stable` (or a detached HEAD at `origin/main` or `origin/stable`) on a clean tree. Without flags it is a dry run: it prints the plan, names every refusal (dirty tree, wrong branch, bad version, tag taken) and runs the cheap `release-ready --strict` check; nothing changes. `--confirm` stamps the version in both `.gproj` files and both Workshop kit READMEs, runs `validate --full`, `release-ready --strict`, the full suite, `audit placement --strict` and a `pack` of each addon, writes `tmp/release/notes-<x.y.z>.md` from the PRs merged since the previous `v*` tag, and tags `v<x.y.z>` on a release commit that no branch points at. A failed gate restores the files and tags nothing. `--push` also pushes that one tag; without it nothing leaves the machine. It never uploads to the Workshop (#370, by hand in Workbench). Agents run the dry run only, never `--confirm`.
- **Issue states.** `agent:working` (being built) to `in-dev` (merged to `main`) to `needs-playtest` (merged, and the PR has a `## Manual testing` section with real steps) to `verified`. Only `verified` closes an issue; `regression` marks a failed one. The `issue-state` workflow (`tools/issue-state.mjs`) runs when a PR merges into `main`: every issue it refs (`Refs/Closes/Fixes/Resolves #N`) gets `in-dev` or `needs-playtest` and stays open; an issue an older CLI's `Closes #N` closed is reopened, a `verified` one is left alone, and a merged `Reverts #P` PR sets `regression` on P's issues. `promote --confirm` sets `verified` and closes. The website reads `verified` (or closed) as done and `in-dev`/`needs-playtest` as "built, not verified".
- **Quorum sign-off (#575).** `docs/playtest/quorum.json` switches a rule that is off by default: when `"enabled": true`, `promote --confirm` needs `--confirmations <file>` and moves `stable` only when every issue whose PR lists manual steps has enough distinct roster testers (`docs/playtest/testers.json`) reporting pass, and none reporting fail. Rule, roster and disputes: `docs/playtest/quorum.md`. Session logistics: `docs/playtest/session-playbook.md`. Until the maintainer approves the rule, agents still never run `promote`.
- Risky changes ship behind a config flag that defaults off, so one unverified feature does not block the rest.

**Manual, maintainer (GitHub settings, not done by tooling):** protect `stable` (Settings, Branches): restrict pushes to the maintainer account, block force pushes and deletion, no pull-request requirement. `promote` pushes a fast-forward as the signed-in `gh`/git user.

**One session, many issues (#402, #403).** The human check is the bottleneck, so a pass is planned, not improvised.
`toolscli playtest checklist` collects the `## Manual testing` steps of the merged PRs of every open `needs-playtest` or
`in-dev` issue, shows a repeated step once, groups the rest by place in a walking route, marks two-player, restart,
late-joiner and dedicated-server steps, estimates minutes, and writes `tmp/playtest/checklist-<date>.md` with a pass box and
a `fail [ ]` box per issue. The default is the top hour; `--all` lists everything. The hour is ranked in tiers: issues under an open M1 or M2 chapter or epic (GitHub sub-issues, two levels; `--chapters N,N` adds more) first, then big player flows named in the title (robbery, vault, clerk, cuffs and arrest, medic and downed, dispatch, pagers and payphones, evidence and witnesses, dialogue, the item catalogue and licences), then the rest, and polish, visual-only and tooling items last (`tierOf` in `tools/lib/playtest-checklist.mjs`). Chapter work takes at most 70% of the hour, and a flow that does not fit closes the door on filler below it. Issues whose steps one walk covers share one line that lists every number; its tick or fail covers all of them. The fixed smoke list
([playtest/smoke-list.md](playtest/smoke-list.md): boot, join, save, restart, reload, late joiner) is always its first
section, and only a regression in an already-verified feature blocks promotion. After the pass,
`toolscli playtest verdicts <file>` reads the ticked file and prints the `gh` commands (verified or regression label, a
comment, close on pass). It runs none of them: the maintainer or lead reviews and runs them. Agents never do.

## Auto-merge

`wt ship <slug> [--refs N] [--body-file F]`:

1. Refuses a dirty tree or a branch with no commits beyond `origin/main`.
2. Merges `origin/main` only when `git merge-tree` reports a conflict (the
   merge drivers in `tools/merge/` then resolve the generated files). A clean
   branch ships as is.
3. Runs the gate (never incremental; see "Incremental validate" below): `validate --full` (with the Workbench compile) + headless
   build + `test` tier=all. A branch that changes nothing under `addons/`
   (docs, website, tools) runs plain `validate` instead and never waits for a
   pool slot; `--full-gate` forces the in-game gate (`wt gate` follows the same rule). After the suite,
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
tools\cli test --no-build --tier all --remote [name]       # a remote machine (any, or that one)
tools\cli test --no-build --tier all --core-only           # the core alone, pack hidden (#326); any machine
tools\cli e2e --remote                                     # the two-boot save/restart proof on the box
tools\cli pool status                                      # machines, slots and holders, queue, last 20 runs
tools\cli pool stats                                       # per machine: runs, passes, average wait and run
tools\cli pool prune [--dry-run] [--unmarked]              # remote run folders of gone worktrees
```

- **Who uses it.** `test`, `dev`, `ci`, `wt test`, `wt gate`, `wt ship` (its
  gate), `wt e2e`, and `e2e`. Every run prints where it ran and how long it
  waited (`pool: test all runs on lf-testbox ...; waited 0.7 s`) and a footer
  with the exit code, wait, and run time. `--local`, `--remote [name]`,
  `--machine <name>`, and `--any` pick; `LF_TESTBOX_AUTO=0` makes the
  default `--local`.
- **Machines.** `tools/pool.json` lists them: `name`, `via` (`local` or
  `ssh`), `host` (ssh alias), `root` (the remote main clone), `slots`, `caps`
  (`server`, `clients`, `workbench`). The gitignored `lf.local.json` overrides
  per machine with a `"pool"` key (`docs/setup.md`). `slots` is 1 everywhere
  today; slot 1 is the machine's own `server-2001` lock (the one `serve`,
  `host`, and `session` take too) and slot k is `server-2001-s<k>`, so a
  machine that can run more servers at once (a network namespace or VM per
  slot) only raises the number.
- **Queue.** Tickets live in `<main>/tmp/wt/pool/queue/` (time, pid, slug,
  machines wanted). A ticket may try a machine only when no older live ticket
  wants it; tickets of dead processes are deleted. A local lock whose owner is
  dead is taken over after any `ArmaReforgerServer` still running from that
  owner's `server/profile` is stopped.
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
  bounds each probe at 12 s and prints `DOWN`. A waiting run prints one line
  when it queues and one when it gets its slot, nothing in between. A machine whose server Steam build differs from this PC's is skipped
  with a `WARN`, unless it was named with `--remote <name>`.
- **Sync.** A remote run carries the worktree as it is: HEAD plus uncommitted
  edits plus untracked, non-ignored files, as one snapshot commit built from a
  copy of the index (your index, HEAD, and branch do not change). The run
  folder `C:\LF\Life-Framework-rt-<slug>` is a linked worktree of the box's
  clone, kept between runs. The holder reports which of the snapshot, the
  last commit this worktree sent, and `origin/main` the box has, and the git
  bundle carries only what is missing. `resourceDatabase.rdb` is gitignored:
  `test` refreshes the local copy first (headless Workbench; the box has none)
  and sends it when its SHA-1 differs from the run folder's. Measured
  2026-11-09: new run folder 5.1 s holder + 1.3 s sync, repeat 3.0 s + 0.0 s
  (no bundle, rdb unchanged); an edit sends a ~2 KiB bundle. A run folder
  whose rdb predated `LF_HideoutStash` got the fresh one by hash and passed the
  stash tests (the box's main clone rdb has no `LF_HideoutStash` entry).
- **What comes back.** The run streams live. The remote `console.log` is
  copied to `server/profile/test/logs/remote-<machine>-<logdir>/`, so
  `tools\cli logs` greps it; on a failure its `SCRIPT (E)`, VME, and `Unable
  to find` lines are printed. The summary is stamped into
  `tmp/test-results/summaries.jsonl` with `"host"`, and every run is a line
  in `<main>/tmp/wt/pool/runs.jsonl`.
- **Cleanup.** Each run folder holds `tmp/pool-owner.json` (PC, slug,
  worktree). `wt prune <slug>` removes that worktree's remote run folders;
  `pool prune` removes every folder whose worktree on this PC is gone
  (`--unmarked` also takes older folders without the marker). Both go through
  the box's own `tools/link-safety.mjs`, never a raw delete: `server/profile`
  holds junctions into the server install.
- **Cost.** A box run of the full suite takes about 22 s door to door (10 s
  suite plus ssh, checkout, CLI start, log copy), this PC about 8 s. Four
  concurrent `--tier all` runs from four worktrees (2026-11-09): 28-31 s wall,
  three on this PC and one on the box, all 224/224, against 36 s for the four
  in a row on this PC. The pool pays most when this PC's slot is held by
  something long (`host`, `session`, `e2e`, another agent's gate).
- **Limits.** `build` and `validate --full` stay on this PC. A pre-pool
  branch's snapshot (no `LF_SERVER_SLOT_HELD` in its `tools/cli.mjs`) runs
  local only until it merges main.

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
- **Disk hygiene (#675).** Every `session up` and local `test`/`e2e` start first removes
  session folders under `%LOCALAPPDATA%\LifeFramework\session` untouched for `sessionKeepHours`
  (24) hours, except one in use (live pids in `session.json`), this checkout's own, and any
  named for film (`film` in the folder name). Deletion is `safeRemoveTree`, the function behind
  `tools/safe-rm.mjs`, so junctions are unlinked, never followed. It then refuses with one line
  when free disk on that volume is under `minFreeDiskGb` (5). Both are settings: `tools/pool.json`,
  the `pool` key of `lf.local.json`, or `LF_POOL_SESSION_KEEP_H` / `LF_POOL_MIN_FREE_DISK_GB`.
  `pool status` prints the free disk.
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
  runs, which then timed out while the laptop idled). A test reserves nothing beyond that floor.
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
its RAM allows. Remote tests now try
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
discards unmerged work. A squash-merged PR counts as merged (a merged PR whose head SHA is the branch tip, or
the same files as origin/main). `cli wt prune --merged` lists every worktree that is merged, clean, idle 30 min and
holds no lock or session; `--yes` prunes them, without it a dry run. `wt ship --refs N` needs N of 1 or more; with
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
- A new check adds the header; `validate-incremental-selftest.mjs` warns about one without.
