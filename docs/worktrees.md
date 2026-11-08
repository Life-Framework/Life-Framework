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

## Test box (second machine)

In-game runs on one machine queue on `server-2001`. A second Windows machine
(the "test box", ssh alias `lf-testbox`) runs the same suite with its own
`server-2001` lock, so agents stop waiting on each other.

```sh
tools\cli test --no-build --tier all --remote            # run this worktree on the box
tools\cli test --no-build --only EL_Test_Money --remote   # the red-proof loop, remotely
tools\cli test --no-build --tier fast --local             # never leave this machine
tools\cli testbox status                                  # reachable? build? the box's lock and run dirs
tools\cli testbox provision                               # set up or update the box (idempotent)
tools\cli testbox prune <slug>|--all                      # remove run dirs on the box (junction-safe)
```

- **Which machine.** `--remote [host]` runs on the box (host defaults to
  `LF_TESTBOX`); `--local` runs here. With neither, `test` (and `dev`,
  `wt test`, `wt gate`, `wt ship`, which call it) stays local while the local
  `server-2001` lock is free, and moves to the box when the lock is held by a
  live run and the box answers ssh within 5 s. If the box is down it queues
  here as before. `LF_TESTBOX_AUTO=0` turns the automatic move off.
- **What runs.** The worktree as it is now: HEAD plus uncommitted edits and
  untracked, non-ignored files. `tools/remote/testbox.mjs` builds that
  snapshot as a commit from a copy of the index (your index, HEAD, and branch
  do not change), sends it with the worktree's `resourceDatabase.rdb` over
  scp as a git bundle of the commits that are not on `origin/main`, and checks
  it out on the box. Nothing is pushed; the box fetches main from the public
  GitHub repo and holds no credentials.
- **Where it runs.** `C:\LF\Life-Framework-rt-<slug>` on the box: a linked
  worktree of the box's clone `C:\LF\Life-Framework` at a detached HEAD, kept
  between runs so the next run only checks out what changed. A plain
  per-run checkout would copy the whole tree each time; a `ws/<slug>` branch
  there would be one more thing to keep in sync. Remove run dirs with
  `testbox prune`, never by hand: their `server/profile` holds junctions into
  the server install.
- **What comes back.** The run streams live: the `test-e2e` lines, the
  `SUMMARY`, failing tests, `[ELDebug:*]` lines, and the exit code (0 pass,
  1 fail, 2 no verdict, 3 the box could not be prepared). The header prints
  both machines' server Steam build and warns when they differ; the footer
  prints the box's `Initializing engine, version` line. The box's
  `console.log` is copied to
  `server/profile/test/logs/remote-<host>-<logdir>/console.log`, so
  `tools\cli logs` greps it, and on a failure its `SCRIPT (E)`, VME, and
  `Unable to find` lines are printed. The summary is stamped into
  `tmp/test-results/summaries.jsonl` with `"host"`.
- **Limits.** `wt e2e`, `serve`, `host`, and `session` stay local. `build`
  stays local (the box has no Workbench build step; it runs `--no-build`
  against the rdb you send). Two runs of the same slug at once on the box are
  not supported.

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
`not found in replication`). `tools\cli build` in the worktree refreshes it;
the next remote run sends the fresh copy.

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
   EL_Test_AccountWantedClamp --remote` checks it out in
   `C:\LF\Life-Framework-rt-<slug>`. The server refuses a client whose scripts
   or defines differ ("script source code checksum does not match").
3. On the box's own desktop (the client must render in the signed-in user's
   session, not over ssh):

   ```bat
   "C:\Program Files (x86)\Steam\steamapps\common\Arma Reforger\ArmaReforgerSteamDiag.exe" -client Gaming-Box -addonsDir C:\LF\Life-Framework-rt-<slug>\addons -addons LifeFramework -profile C:\LF\client2 -window -screenWidth 1280 -screenHeight 720 -scrDefine LF_SESSION_IDENTITY -noBackend -disableCrashReporter
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
ssh has no desktop.

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
