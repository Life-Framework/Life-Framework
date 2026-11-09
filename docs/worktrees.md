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

## Auto-merge

`wt ship <slug> [--closes N] [--body-file F]`:

1. Refuses a dirty tree or a branch with no commits beyond `origin/main`.
2. Merges `origin/main` only when `git merge-tree` reports a conflict (the
   merge drivers in `tools/merge/` then resolve the generated files). A clean
   branch ships as is.
3. Runs the gate: `validate --full` (with the Workbench compile) + headless
   build + `test` tier=all. A branch that changes nothing under `addons/`
   (docs, website, tools) runs plain `validate` instead and never waits for a
   pool slot; `--full-gate` forces the in-game gate (`wt gate` follows the same rule).
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

## Test pool (every machine)

Every `-server` boot binds UDP 2001, so one machine runs one test server at a
time. The test pool spreads in-game runs over every machine that can boot
one: this PC and the test box (ssh alias `lf-testbox`), more when they are
added. Each run takes the first free slot on any machine; when every slot is
busy it queues, first come first served, and takes the first slot that frees
anywhere.

```sh
tools\cli test --no-build --tier all                      # first free slot anywhere (--any, the default)
tools\cli test --no-build --only LF_Test_Money --local     # this PC only
tools\cli test --no-build --tier all --remote [name]       # a remote machine (any, or that one)
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

### Sessions on a pool machine

`node agent-box/session.mjs up --machine <name|any|local> --clients N`.
Game clients need a desktop session. Over ssh there is none: on 2026-11-09
the box showed the logon screen (`LogonUI` in session 1, no `explorer`) and
ssh commands run in session 0, so a remote machine runs **server-only**
sessions (`--clients 0`). Its server is started through WMI
(`Win32_Process.Create`) because ssh kills every child of the session when
the connection closes (a detached spawn was gone seconds later). The machine
is remembered in `<worktree>/tmp/session-remote.json`, so `down`, `status`,
and `restart` from that worktree go there. `--machine any` picks the first
machine with a free slot that can run the requested clients (clients: this
PC only). `journey` drives clients, so it runs on this PC; it refuses
`--machine lf-testbox` and a recorded remote session. Clients on this PC can
join a box server over the LAN (manual steps below).

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
