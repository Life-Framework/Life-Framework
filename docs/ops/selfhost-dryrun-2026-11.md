# Fresh self-hosted server dry run, November 2026 (#1669)

The record of following [install a server from zero](../owner/install.md) on a fresh folder, for #379 (epic #230).
Run on 2026-11-15 on `lf-testbox`, which is the maintainer's PC (pool name `lf-testbox`, hostname `PC1`; the pool
config maps it to this machine), against `main`. The server was the installed Arma Reforger Server, build 1.9.0.37.
The run used only what the guide names: the folder `server/example/`, the server exe and the Workshop. No repo tool
was run for the server, and the log was read by hand.

Limits of this run. The person following the guide was an agent that had read the repo, so a stumble a stranger
would hit may be missing. There was no game client (RAM free was 0.6 GB), so nobody joined. The router and the
firewall were not touched. The ports in `server.json` were moved off 2001 and 17777 because the pool holds them
(the guide's step 2 allows that).

PASS means the step's command ran and its check held. GAP names what failed and the card that tracks it. NOT RUN
says why the step was out of reach.

## Times

| What | Time (AEST) |
|---|---|
| Folder made from `server/example/`, `server.json` edited | 22:51 to 22:55 |
| First `start.cmd` | 23:22:59 |
| Guide's start line from the install folder | 23:25:12 |
| Core-only variant started | 23:25:47 |
| `Entered online game state.` on the core-only variant | 23:26:23 (36 s after start) |
| Run ended, server stopped, folder removed | 23:28 |

About 37 minutes from the first file to the end, 5 minutes of it the server running.

## Result by step

| Step | Result | What was run, and what came back |
|---|---|---|
| 1 Install the server | NOT RUN | Already installed at `%ProgramFiles(x86)%\Steam\steamapps\common\Arma Reforger Server`, build 1.9.0.37. |
| 2 Get the server folder | PASS | Copied `server/example/` (the zip `LifeFramework-ExampleServer.zip` was not used) to `C:\LF\scratch\slice-1669-20261115-2255`. Edited `game.name`, `game.passwordAdmin`, `bindPort`/`publicPort` 2301 and `a2s.port` 17877 in `server.json`. `scenarioId` and `game.mods` left alone. |
| 3 Open the ports | NOT RUN | No router or firewall change on the maintainer's PC. |
| 4 Start it | GAP | See the three runs below. |
| 5 to 8 Folders, config, save, restart | NOT RUN | The 1989 server never started, so there was no save to carry across. `config.json` was copied to `profile\profile\LifeFramework\` by `start.cmd`, which is all step 6 asks. |
| 9 `server-doctor` | NOT RUN | Needs a checkout and `tools\cli`, which this run did not use. |
| Join | NOT RUN | No client. |

### Step 4, run 1: `start.cmd` as the guide says

Typed in a Command Prompt in the folder: `start.cmd`.

```
'start.cmd' is not recognized as an internal or external command
```

`start` is a Command Prompt built-in, so `start.cmd` runs it with the argument `.cmd`. Typing `.\start.cmd` runs the
file. That is stumble 1. Then:

```
ENGINE    (E): Game addon '58D0FB3206B6F859' not found
ENGINE    (E): Cannot initialize game project settings!
ENGINE    (E): Unable to initialize Enfusion
ENGINE       : Addon dirs: dir: './addons'  dir: '...\slice-1669-20261115-2255\profile\addons'
```

The script exits 0 and prints nothing else. The server reads `./addons` from the current folder, and `start.cmd` never
changes into the install folder. That is stumble 2.

### Step 4, run 2: the guide's own start line, from the install folder

`ArmaReforgerServer.exe -config <folder>\server.json -profile <folder>\profile -maxFPS 60` with the install folder as
the current folder. The base game and the core's Workshop download both resolved. Then:

```
BACKEND   (E): Addon B1E84D5687E11724 - Addon was not found on workshop.
BACKEND   (E): 1 addons are not downloadable! Cannot start until they are removed from server config.
ENGINE    (E): Unable to initialize the game
```

This is the Problems row the guide already has, and the guide says why (neither item is on the Workshop until the
first upload). The core id `79636E668EA37AC9` is on the Workshop. The 1989 town is not joinable from the guide today.

### Step 4, run 3: core only, to see a boot to the end (not in the guide)

A copy of `server.json` with only the core in `game.mods` and `scenarioId` set to
`{5A1E5A1E5A1E0002}Missions/EL_DebugTest.conf` (the core's DebugWorld header, from `server/example/README.md`).

```
23:25:56 BACKEND  : Downloading 79636E668EA37AC9 version 1.0.0
23:26:23 DEFAULT  : Entered online game state.
23:27:23 SCRIPT   : [ELDebug:Mining] spawned 0/0 ores
```

The server reached `Entered online game state.` 36 s after start. The log held 119 `[ELDebug:...]` lines and no
`[LFDebug:...]` line, and neither `fresh session: no save point exists for this mission` nor a `save loaded:` line.
The Workshop core is build 1.0.0, from before the `EL_` to `LF_` rename, so the lines step 4 tells an owner to expect
do not appear. The `WORLD (E): Unknown keyword/data 'WeaponAnimationComponent'` lines came from the base game's own
assets while the world loaded; there was no `SCRIPT (E)` or VME. That is stumble 3.

## Stumbles

| # | Stumble | Guide line | Fix |
|---|---|---|---|
| 1 | `start.cmd` typed in Command Prompt runs the built-in `start` | Step 4: "run `start.cmd` (Windows)" | Wording, done: `.\start.cmd`. |
| 2 | `start.cmd` run from the unzipped folder cannot find the base game and exits 0 | Step 4; Problems row `Game addon '58D0FB3206B6F859' not found` (blames `-addonsDir` only) | Wording, done: run the start line from the install folder, and the Problems row names the working folder. The script itself needs a `pushd`: card #1710. |
| 3 | The success lines in step 4 (`[LFDebug:...]`, `fresh session: ...`) are not what the Workshop core prints (`[ELDebug:...]`, build 1.0.0), and the 1989 pack is not on the Workshop | Step 4 and the Problems table | Needs an upload of current `main`, then this run again: card #1711. |

## Cards filed, all under #230

- #1710 `server/example` `start.cmd` and `start.sh` do not change into the server install and exit 0 on failure.
- #1711 the Workshop core is build 1.0.0 and the pack is missing, so the guide's success lines cannot be seen.

## What it says

A new owner following the guide today reaches a server that boots only if they change the pack out for the core's
DebugWorld, which the guide does not tell them; with the guide's own `server.json` the server stops at the pack id.
That is known and waits on the first Workshop upload (m3-plan D3). Run this again once both items are up: it is the
check that "joinable server from the guide alone" holds. The times above are the baseline: 36 s from start to
`Entered online game state.` with the core only.

## Re-run after the first upload, 2026-11-16 (#1711, #371)

Stumble 3 is closed. The core id in the run above, `79636E668EA37AC9`, belonged to an unrelated Workshop item (see
[workshop-publish.md](../playtest/workshop-publish.md)); the core is `4550A2B656E1B569` and the pack
`B1E84D5687E11724`, both published as 0.1.0 and Unlisted. The guide's `server.json` already lists those two ids.

This re-run used `tools\cli workshop boot-check 4550A2B656E1B569 B1E84D5687E11724 --no-client` on the pool machine
`lf-cloud` (a clean run folder, no local addons, empty cache), not the by-hand steps of the first run. It is a check of
what the Workshop serves, not of the guide's prose, so the by-hand run (steps 1 to 3, the start line) was not repeated.

```
BACKEND : Downloading 4550A2B656E1B569 version 0.1.0
BACKEND : Downloading B1E84D5687E11724 version 0.1.0
BACKEND : Required addons are ready to use.
DEFAULT : Entered online game state.
SCRIPT  : [LFDebug:Persistence] fresh session: no save point exists for this mission
SUMMARY workshop-boot-check ok stage=pass host=lf-cloud ...
```

- Both items download by id from the Workshop, core first, and the server reaches `Entered online game state.` on the
  1989 pack's scenario. The log has 0 `SCRIPT (E)` lines and no `[ELDebug:...]` line, so the core is a post-rename
  build.
- Two of step 4's success lines appear: `Entered online game state.` and `fresh session: no save point exists for this
  mission`. The `no owner config` line is absent because the run folder holds no `config.json` (step 6).
- `RESOURCES (E): Wrong GUID/name` lines for the base game's `Rally_*` worlds are the known ones in
  [debug-log-triage.md](../debug-log-triage.md). Two more name `Language/everonlife_localization.st` and
  `Language/lf_everon1989_localization.st` in the Workshop build; the boot reached
  `Entered online game state.` regardless. Whether strings resolve in game was not checked. They are a separate card if they stay in 0.1.1.
- A first run with a game client on the same box failed at `join`: the server logged `authentication timeout
  (identity=0x00000000)` for the box's client, which had no signed-in identity. That is the box, not the mod, and it
  leaves #371's "a client joins" line unproven by this tool. A player joining the live flagship on 0grind proves it.

Still to do: run `workshop boot-check` again when 0.1.1 is Public (the build the owners will get), and have one
player join from a clean install. The by-hand run of the guide, start to finish, is worth repeating once with a
person who has not read the repo.
