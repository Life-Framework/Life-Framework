# RAM budget: where the memory goes, and what each job needs

Measured 2026-11-17 on both pool machines (#2268 follow-up: a cold `look` spent 132 s waiting for RAM, 100 s for
the Workbench slot and 43 s working). Numbers are working set (WS, the physical RAM a process holds) and private
bytes (committed memory; a working-set trim does not shrink it, the pagefile backs it). The pool's admission
(`tools/lib/pool-ram.mjs`, `docs/worktrees.md` "RAM admission") reasons in working set.

How it was measured: a full `Win32_Process` snapshot (pid, parent, name, WS, private, command line) on each machine,
then a 5 s sampler of every ArmaReforger process and of free RAM for 15 minutes during normal agent load (8-16 agents,
looks, tests, a session, the main guard). Re-measure with the same snapshot; the role rules are below.

## The two machines

| | this PC (GAMING-BOX) | laptop (lf-testbox, PC1) |
|---|---|---|
| RAM | 16.0 GB | 15.6 GB |
| commit limit (RAM + pagefile) | 36.4 GB, **32.4 GB committed** | 40.2 GB, 17.0 GB committed |
| free during the window | min 0.08 GB, median 2.1 GB, max 3.8 GB | min 2.7 GB, median 4.9 GB, max 8.4 GB |
| Workbench | none (no `workbench` cap; the maintainer's editor lives here) | look viewer, rdb, compile |

### By role, this PC (snapshot 08:14, no game process running)

| role | processes | WS GB | private GB |
|---|---|---|---|
| claude agents (T3 threads) | 16 | 2.94 | 9.14 |
| other user apps (OneDrive 0.46, Superhuman, Slack, ChatGPT, Chrome, Wispr, StreamDeck, SteelSeries, ...) | 159 | 2.32 | 8.74 |
| tools/cli node (agents' commands: validate fan-out, look/test/session waiters) | 32 | 1.79 | 1.15 |
| git (a warm-pool `git reset --hard` held 1.07 GB for seconds) | 8 | 1.11 | 0.29 |
| Defender (MsMpEng) | 3 | 1.10 | 1.80 |
| Windows | 121 | 1.03 | 1.46 |
| shells (69 conhost, 19 bash, 10 cmd, 9 PowerShell) | 114 | 0.96 | 0.88 |
| T3 Code | 6 | 0.70 | 1.06 |
| NVIDIA (Broadcast 0.11, containers) | 11 | 0.21 | 1.24 |
| Steam webhelper | 9 | 0.17 | 0.89 |
| Windows Search | 4 | 0.14 | 0.25 |
| MCP servers (node children of claude) | **0** | 0 | 0 |

- **An agent costs about 0.2 GB of RAM and 0.6 GB of commit.** Each claude.exe held 125-266 MB WS and 550-700 MB
  private; all 15 were active (0.5-2.9 s CPU per minute). No claude process starts an MCP server child: T3 serves its
  tools in-process (`--allowedTools=mcp__t3-code__*`) and the repo's MCP tools run on demand through `tools\cli call`.
  There is nothing to trim per agent.
- **The commit charge is the hard limit here, not the working set.** 32.4 of 36.4 GB is committed with no game
  running; a game server commits 0.9-2.5 GB and a client 1.5-4.4 GB. Agents and user apps each commit about 9 GB.
- **A `validate` run fans out 8-12 node selftests at about 52 MB each** (0.5-0.6 GB per run, for seconds); several
  agents validate at once.
- **Every heavy `tools\cli` command is two node processes**: the compact-output wrapper (`tools/lib/quiet-run.mjs`,
  about 50 MB WS with the whole CLI module graph loaded) waits for the child. A queued `look`, `test` or `session up`
  holds both for the whole wait.
- Defender's 1.1 GB WS is scanning the worktrees and the git objects: the laptop excludes `C:\LF`, the game folders and
  the session folder already; this PC excludes nothing of ours (a suggestion below, not changed: it is the user's PC).

### By role, the laptop (snapshot 17:14 local)

| role | processes | WS GB | private GB |
|---|---|---|---|
| Windows (92 svchost, dwm 0.4, explorer) | 127 | 3.06 | 1.69 |
| Workbench (the look viewer) | 1 | 2.50 | 4.98 |
| other user apps (ASUS Armoury, AMD, Phone Link, Cross Device, Terminal, ...) | 59 | 1.73 | 0.89 |
| shells/ssh (12 sshd sessions, pool holders' PowerShell) | 35 | 0.87 | 0.63 |
| NVIDIA (App overlay 0.36, display containers) | 12 | 0.75 | 0.50 |
| Defender | 3 | 0.41 | 0.41 |
| Steam (webhelper 0.34) | 8 | 0.40 | 1.05 |
| T3 serve | 2 | 0.20 | 0.33 |
| claude | 1 | 0.18 | 0.52 |
| Windows Search | 3 | 0.17 | 0.07 |
| main guard worker | 1 | 0.03 | 0.02 |

### Orphans

No ArmaReforger process without an owner was seen in either snapshot or during the 15-minute window: every game
server, client and Workbench belonged to a live lane lock, session record, look viewer or the main guard. The one node
"orphan" (parent gone) was `tools/wt-warm.mjs fill`, which `wt new` starts detached on purpose.

## Per job: real peak against the admission reserve

Peaks are the highest WS a process reached (5 s samples), with its private bytes at that time.

| job | world / profile | measured peak WS | private | reserve before 2026-11-17 | verdict |
|---|---|---|---|---|---|
| test server | TestLogic (this PC) | 0.70 GB | 0.89 GB | 0.8 GB (`test:small` seed; never measured, see below) | right |
| test server | GridWorld (this PC) | 0.76 GB | 0.95 GB | 0.8 GB | right |
| test server | PlaytestTown, persistence (main guard, laptop) | 0.76 GB | - | 0.8 GB | right |
| session server | Everon island (laptop, full) | 2.20 GB | 2.54 GB | 2.5 GB | 0.3 GB over |
| client | Everon island, full 1280x720 (laptop) | 1.70 GB | 4.46 GB | counted as a *small* client (1.2 GB seed, laptop p90 2.0 GB) | **under** (wrong class), see below |
| client | small world, full (laptop samples) | 1.37-1.48 GB | - | 1.2 GB seed / laptop p90 2.0 GB inflated by island samples | over on the laptop |
| Workbench | look viewer (`LF_Everon1989.gproj`, laptop) | 2.52-2.65 GB (p90 of 40 samples 2.54) | 4.9-5.2 GB | **3.7 GB** seed on this PC's view | **1.1 GB over** |
| Workbench | rdb refresh / compile (laptop) | 0.99-1.18 GB | 1.45-1.71 GB | 3.7 GB (this PC) / 2.54 GB (laptop): same key as the viewer | **1.4-2.5 GB over** |

Why the reserves were off, and the fix (`pool-ram.mjs`, `pool.mjs`, docs/worktrees.md "What each process is"):

1. **The scan could not tell a test server from a session server**, so every test peak was recorded under `server` and
   `test:small` stayed at its seed forever. A server whose `-profile` is `...\server\profile\test` is now a `test`.
2. **An island client was a small client.** A client joins by address; only its server names `CoreOnEveron`. Its
   growth was computed against 1.2 GB instead of 2.9 (under-reserve: the crash side), and its 2.0 GB samples raised the
   laptop's small-client p90 to 2.0 GB (over-reserve for every small session there). A client is island now when its
   session folder (`<dir>\client1`) is the one of an island server (`<dir>\server`).
3. **A client's peak was keyed by the profile of the job being admitted**, not its own: a full client seen while a
   lowspec job asked was recorded as lowspec. The scan reads it (`-maxFPS 15` = lowspec).
4. **Every Workbench counted as the 2.5-3.7 GB look viewer**; an rdb refresh or compile peaks at 1.2 GB. Tool
   Workbenches have their own key (`workbench:small:tool`, seed 1.3 GB).
5. **This PC judged the laptop by its own peak file.** This PC's `pool-peaks.json` held only 8 `server:small` samples, so
   the laptop's admission (ranking, remote session verdicts and the holder gate's table) used the seeds: Workbench 3.7 GB
   where the laptop's own file had 40 samples at p90 2.54 GB. The ssh state read now carries the box's own peak file and
   its measured keys win.

Effect on the laptop's admission (RAM a running job counts for, or a new one reserves):

| case | before | after | freed |
|---|---|---|---|
| a look start (Workbench reserve) judged from this PC | 3.7 GB | 2.54 GB | 1.16 GB |
| an rdb/compile Workbench at 0.3 GB, still loading (growth) | 3.4 GB (this PC) / 2.24 GB (laptop) | 1.0 GB | 1.2-2.4 GB |
| a full small client (reserve) | 2.0 GB (laptop p90, island-contaminated) | 1.2-1.5 GB once 3 clean samples exist | 0.5-0.8 GB |
| an island full client at 1.7 GB WS (growth) | 0 (counted small, already over 1.2) | 0 while private >= peak; else up to 1.2 GB | correctness, not room |

## Suggestions for this PC (not changed; the user's machine)

- Defender exclusions for `C:\Users\jaspe\Documents\Reforger` (the worktrees), the three Arma Reforger Steam folders and
  `%LOCALAPPDATA%\LifeFramework`, as the laptop has: MsMpEng held 1.1 GB WS and scans every checkout.
- OneDrive (0.46 GB WS): pause it, or keep the worktrees outside synced folders (they are).
- The 9 GB of user-app commit (Superhuman 1.5 GB private across 9 processes, ChatGPT 0.95, Wispr 0.61, NVIDIA Broadcast
  0.96, Slack 0.56) is what keeps the commit charge at 32 of 36 GB: a larger pagefile (for example 32 GB) removes the
  commit ceiling without touching any app.

## Laptop trims (done 2026-11-17, reversible)

| change | freed WS | undo |
|---|---|---|
| Windows Search (`WSearch`) stopped and disabled: SearchIndexer and its protocol hosts | ~0.06 GB | `Set-Service WSearch -StartupType Automatic; Start-Service WSearch` |
| Telemetry (`DiagTrack`) stopped and disabled | ~0.02 GB | `Set-Service DiagTrack -StartupType Automatic; Start-Service DiagTrack` |
| Phone Link and Cross Device background access off (`BackgroundAccessApplications`, `Disabled=1`) | ~0.24 GB | delete the two keys under `HKCU:\Software\Microsoft\Windows\CurrentVersion\BackgroundAccessApplications` |

Left on, with a reason: the NVIDIA App overlay (0.36-0.42 GB; it respawns from its container, so it needs the NVIDIA
App's own "Overlay off" setting, a GUI step for the maintainer), Steam (a client needs it), Defender (already excludes
our folders).

## What each change freed (2026-11-17)

"Freed" is RAM the admission no longer reserves for a job that does not need it, or RAM a process no longer holds.

| change | PR | freed | where |
|---|---|---|---|
| Workbench reserve from the laptop's own peaks (3.7 to 2.54 GB) | #2344 | 1.16 GB per look start | laptop |
| rdb/compile Workbench as a `tool` (1.3 GB instead of 2.54-3.7) | #2344 | 1.2-2.4 GB while one loads | laptop |
| island clients and client profiles classified; small client p90 no longer island-inflated | #2344 | 0.5-0.8 GB per small session client | laptop |
| island client growth counted at the island peak | #2344 | none (closes an under-reserve: crash risk) | both |
| test servers record `test:*` peaks (the reserve can now follow the measured 0.70-0.76 GB) | #2344 | up to 0.05-0.1 GB per test, once samples exist | both |
| a test or audit run on an island world (MainWorld, CoreOnEveron, the island mission; `town fit`'s `audit placement --world MainWorld`) reserves the island server peak (2.5 GB seed, measured 2.2 GB working set) and gets an island run budget (300 s seed, 180 s floor, its own history) instead of the small test's 0.8 GB and 30 s; the lock, ticket and holder carry the world | #2394 | none (closes an under-reserve: `Out of memory. Free system 355 MB` on lf-testbox, no `out.txt`) | both |
| orphan game/Workbench reaping | #2350 | 0 today (no orphans seen); 0.7-2.6 GB per hung server or Workbench it catches | both |
| validate fan-out fits over the floor | #2352 | 0.16-0.8 GB at a validate's peak when free RAM is short | this PC |
| Windows Search, telemetry, Phone Link off | (box config) | ~0.3 GB | laptop |
| MCP servers per agent | (none needed) | 0: no claude process starts one | - |

### Effective lanes

What fits at each machine's median free RAM in the 15-minute window, over the 1.5 GB floor, with the pool's lane caps
(laptop 3 test, 2 session; this PC 2 and 2):

| machine | median free | before | after |
|---|---|---|---|
| laptop | 4.9 GB (look viewer resident) | 3 tests, **or** 1 small full session (0.7 + 2.0), **no look** (needs 5.2 GB: the 132 s wait in #2268) | 3 tests, **or** 1 small full session (0.7 + 1.2) plus 1 test, **or** 1 look (needs 4.0 GB) |
| laptop, look viewer closed | ~7.4 GB | 3 tests + 1 small session | 3 tests + 1 small session + room for a look |
| this PC | 2.1 GB | 0 (a test needs 2.3 GB free) | 0; with the Defender and OneDrive suggestions above (~1.5 GB): 1-2 tests or 1 lowspec session |

This PC stays the bottleneck: agents (0.2 GB each) and the user's apps hold it, and nothing in the pool's control
frees enough for a game server there. The pool already sends work to the machine with the most headroom (never pin).

## Not done, and why

- **Trimming idle processes' working sets.** Windows trims under pressure already, and the admission counts a trimmed
  process at the peak it can regrow to (`unrealisedMb`), so a trim admits no extra job; trimming a game process also
  stalls it when it pages back in.
- **Lower Workbench texture settings for non-PR looks.** The look viewer loads `LF_Everon1989.gproj` (core plus pack)
  at 2.5-2.65 GB. Whether a Workbench profile setting lowers that is unmeasured; it is slice card 3 below rather than a
  guessed engine setting.
- **The smallest world a test needs** was already the default (`docs/worktrees.md` "Test world"): LOGIC runs boot
  TestLogic (0.70 GB), WORLD and PERSISTENCE runs boot GridWorld (0.76 GB), only pins boot DebugWorld.

## Bigger work: slice cards

1. **Shared warm test server per machine** (lf-hard, 45 min). Every test run boots its own server (0.7-0.76 GB, 10-30 s).
   A resident TestLogic server per machine that takes the next worktree's LOGIC selection (scripts compile at boot, so this
   needs a restart whenever the scripts differ; see docs/worktrees.md "Live reload") removes the boot and lets
   LOGIC runs from several worktrees share one 0.7 GB. Acceptance: two worktrees' `--tier fast` runs back to back on
   one server process, results identical to separate boots.
2. **Test batching across worktrees** (lf-builder, 45 min). Queued `--only` runs with the same pack, world and build
   could run in one boot with a per-worktree result split. Needs the same addon-per-run question as card 1; start with
   runs from the same base commit and an identical `addons/` tree hash (the pass-cache key already computes it).
3. **Look viewer memory** (lf-builder, 30 min). Measure the viewer with a profile whose engine settings lower texture
   quality and view distance, for looks without `--pr`; also measure a core-only `.gproj` for grid looks that use no
   pack resource. Acceptance: peak WS before and after in this file; switch only if pictures stay usable.
4. **Thin CLI wrapper** (lf-builder, 30 min). The compact-output parent (`tools/lib/quiet-run.mjs`) loads the whole CLI
   module graph (about 50 MB) and then only waits for its child. An entry that decides quiet-or-not before importing
   the CLI saves about 25-30 MB per waiting `look`, `test` or `session up` (8-10 wait at once on this PC: 0.2-0.3 GB).
5. **Look restart churn** (lf-hard, 45 min). In the window the laptop's look viewer restarted 7 times in 15 minutes
   (each look comes from another worktree with added or deleted addon files: `mirrorMode` restart). Each restart is a
   1.2 GB to 2.6 GB climb and the 100 s Workbench slot wait in #2268. A mirror that adds new files without a restart
   when Workbench never loaded them would keep one viewer warm.
