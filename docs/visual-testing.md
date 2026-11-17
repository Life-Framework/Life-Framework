# Visual testing

What can look wrong, the cheapest evidence that catches each kind, and which tool gives it.
Use the cheapest tool that can see the problem; a game session is the last step, not the first.

```
tools\cli audit placement [--area a]     numbers, no pixels, test server, about 10 s      (docs/placement-audit.md)
tools\cli audit placement --grid         the same on GridWorld at the grid bar: on the ground and reachable, nothing else
tools\cli look <area | x,z> [more]       pictures, hidden Workbench, no session, about 25 s
tools\cli look grid:<slug>              a debug-grid cell, or grid:annex-gate: opens GridWorld; town:<area> is an area that must lie in the town (docs/architecture/world-zones.md)
tools\cli look grid:all                 every claimed grid cell in one run on GridWorld, plus overview.jpg
tools\cli look all                       every area of agent-box/areas.json in one run, about 4 min, plus overview.jpg
tools\cli look item <prefab|slug> ...   a dropped item and its inventory camera view, no session, about 30 s
tools\cli look <x,z[,r]> --world Worlds/CoreOnEveron/CoreOnEveron.ent [--dump m]   vanilla Everon at a coordinate; `--dump` also writes every entity, road and ground height near it as dump.txt (`tools\cli scout`, docs/everon/scout.md)
tools\cli look serve                     one Workbench that stays up and takes every look in turn: a few seconds a look
tools\cli call wb_preview '{"resource":"{GUID}path"}'   one resource in an open Workbench, about 2 s, PNG path returned
tools\cli shots area <name>              pictures from a game client on a `--profile full` session (docs/agent-area-polish.md)
tools\cli evidence <issue> before|after <area|png>   keep a look or wb_preview picture for the PR; `evidence <issue> table` prints the embed
```

What right looks like is `docs/style-guide.md`; the checklist to walk against the pictures is the
`polish` skill. `wt ship` refuses a player-visible change whose PR body embeds no picture.

**PR pictures come from `look`, `look item` or `wb_preview`, kept with `tools\cli evidence`.** A session is
for what Workbench play cannot capture: HUD and other UI widgets, a second player or late joiner, motion,
animation, sound, and filming. A session lane held for a picture is the slowest way to get it (the pool's
session lanes had the longest waits on 2026-11-14: p90 about 6 min, worst 61 min).

## Use `look` unless you need motion, UI, input or a second player

Ask in this order and stop at the first yes.

1. **Numbers or files are enough?** (a placement, a GUID, a prefab value, a scale, a shelf height) `validate`, `measure <prefab> --surfaces`, `audit placement`, `asset states <slug>`. No picture.
2. **A still of a place, a placed prop, a sign, an NPC standing?** `look <area>`. Close-ups of every placed prop are in the sheet.
3. **A still of one item: lying on the ground, or as the inventory sees it?** `look item <prefab|slug>` (`--states ground,inventory`, `--at area|x,z`).
4. **Two or three variants side by side?** `look item a b c`: one sheet each, 3 m apart.
5. **Does it move, play a sound, or react to input?** A session (animation, the hold pose, a door opening, a hit, a siren).
6. **Is it a UI sheet, HUD, or the inventory slot frame?** A session (Workbench play captures no UI).
7. **Does a second player or a late joiner see it?** A session with `--clients 2`.
8. **Is the picture for a PR?** Same tools: `tools\cli evidence <issue> before|after <area|png>` keeps a `look`, `look item` or `wb_preview` picture, so a world, prop, sign, item, prefab, material or layer change needs no session for its PR pair. Only a picture of something steps 5 to 7 send to a session (a sheet, a held item, motion) comes from the session. Media (trailer, website gallery, graded look) is shot on a `full` session.

What an agent session used to be needed for, and what does it now:

| Job | Before | Now |
|---|---|---|
| A dropped item at rest | `asset states --shots` or `itemdrop` and a free camera, 2 clients | `look item` (ground) |
| The inventory preview camera of an item | `asset states --shots`, preview sheet | `look item` (inventory): the 3D view, not the slot UI |
| A prop, a placed object, a sign | `shots area` | `look <area>` |
| The PR picture of a prop, sign, item, prefab, material or layer | `shots area` on a session | `look`, `look item` or `wb_preview`, kept with `evidence` |
| The item in a hand | session, 2 clients | still a session (`asset states --shots` does hand and use) |
| Dialogue shot, a door, a siren, a held pose | session | still a session |

## Profile fixtures and rejoin (agent-box sessions)

A proof that needs a police roster (or any file the server reads from `$profile:LifeFramework/`) seeds it
in `up`: `session.mjs up --clients 2 --mission playtest --roster 2` or `--seed <file>`. The leg that boots
the server writes the fixture just before the server starts, on the machine the session runs on; a file
written on this PC before `up` never reaches a pool machine. `session.mjs roster <id>` on a live session
writes the roster and the server reloads it. A client that quits and rejoins keeps its identity (a session
slot, not the player id): `session.mjs rejoin <n>`, worked example `rejoin-proof`. Details:
`agent-box/README.md` "Stable client identity and rejoin".

## The matrix

Evidence levels: **(a)** data only (layer, prefab and config files), **(b)** a world query with no
renderer (the test server), **(c)** a render, **(d)** a judgment by a human or a model reading the
picture. Each row names the lowest level that catches the problem, and what is left over.

| Property | Lowest level | Tool | What it leaves for the next level |
|---|---|---|---|
| Placement: floating, sunk, in a wall, blocking a door, in the road, unreachable | (b) | `audit placement` (in `wt gate`) | (d) on a (c) picture: on a surface, but the wrong one (a till beside its counter). `look` close-ups |
| Scale: the entity's scale factor | (a) | `audit placement` (scale outside 0.5 to 2) | |
| Scale: the real size against its neighbours (a 1.4 m sack) | (b) for the size, (d) for "too big" | `look` prints the bounds in metres under each close-up | A table of expected sizes per prefab would move this to (b); not built |
| Missing mesh: no `.xob`, or a broken `{GUID}path` | (a) | `tools\cli validate` | |
| Invisible entity: the mesh did not load | (b) | `audit placement` lists it under "not found"; `look` flags `not-found` | |
| White mesh: a material override over a placeholder bake | (c) | `look` close-up; the `white` flag is a hint | (d) a white sign is meant to be white |
| Textures: missing (magenta), wrong, stretched | (c) | `look`; the `magenta` flag catches missing | (d) the right texture. Final check on a client |
| Lighting and glare: too dark, blown out, sun on a sign | (c) | `look --hour h`; `dark`, `blown` and `flat` flags | (d) glare and mood. Workbench renders with its own graphics settings, so the trailer look is judged on a `full` client |
| UI at several resolutions | (c) on a game client | `session up --profile full --res WxH`, `harness shot ui` | Workbench play captures no UI (`agent-box/README.md` "Screenshots"). Layout GUIDs and anchors are (a): `validate` |
| Poses: NPC idle poses | (c) | `look` (NPCs that spawn at play start are in the frame) | (d). Player poses, held items and animation in motion need a client: `shots`, `film` |
| NPC clothing | (a) for the loadout a prefab names, (c) for how it looks | `look` close-up of the NPC | (d) |
| Composition and clutter | (c) + (d) | `look` contact sheet, read by the agent | Nothing automatic judges taste; a human signs off launch media (`docs/agent-area-polish.md`) |
| Second client and JIP parity | (b) for state, (c) for looks, on a real server | `session up --clients 2`, the harness state verbs, `journey` | No shortcut: Workbench play is one local player with no replication |

Reading the matrix: `validate` and `audit placement` run on every change for free. `look` is the
default picture while you iterate. A session is for the four things only a client shows: UI, the
final graded look, motion, and a second player.

## `tools\cli look`

One hidden Workbench (the `LF-Agents` desktop) opens DebugWorld, enters play mode, spawns a free
camera, and for each area takes a wide shot from the side players approach, three obliques, a top
view and up to 12 close-ups (interactables first, then named props, then the rest, nearest first).
Then it exits. No server, no client, no session lane, nothing shared with another agent.

**Which world.** A run opens one world (`docs/architecture/three-worlds.md`): `grid:<slug>`, `grid:annex-gate`
and `grid:all` open `Worlds/GridWorld/GridWorld.ent`; `<area>`, `town:<area>`, `x,z`, `all` and `look item`
open the playtest world (DebugWorld, with the pack town on top when the pack is here). One command cannot mix
the two (`a look opens one world`); run them one after the other. `--world <path>` still wins. A grid look takes
its close-ups from `GridWorld_Layers/`, and the plan's `world` line is checked against what Workbench opened
(`worldProblem`), so a fallback scene is never taken for GridWorld. `audit placement --grid` measures the same
world at the grid bar (`floating`, `sunk`, `unreachable`; the polish kinds are the town's), and
`session up --mission grid` boots it as a server (`LF_GridWorld.conf`, no harness build). The default
`--mission debug` still boots the town.

**Where it runs.** `look` and `look item` run on the machine with the most room for a Workbench, the
way `ci`'s Workbench steps choose (`tools/lib/wb-gate.mjs` chooseGate: live free RAM less the look's
2.2 GB, above the pool's 1.5 GB floor, weighted by speed). This PC, lf-testbox or lf-cloud: a pool
machine gets this worktree's snapshot (uncommitted files too) through the pool's run folders, and the
pictures are copied back to `tmp/look/<area>/` with `index.json` pointing at them (`machine` says
where they were taken). With no room anywhere it queues (re-read every 20 s, up to 20 min,
`LF_LOOK_WAIT_S`) instead of failing. A machine that cannot run it after all (the RAM went, its
build lock is held, or the Workbench timed out with no shot while short of RAM) hands it back and
the next machine is tried. `--local` keeps it on this PC, queuing here for RAM and the build lock;
`--machine <name>` sends it to one pool machine; `--force` runs here with no RAM check. A live
`look serve` here is always used first. On 2026-11-14, with 0.1 GB free on this PC, `look bank`
ran on lf-testbox in 297 s the first time (a new run folder: 67 s to sync) and on lf-cloud in 93 s.
Before this change it failed at once.

```
tools\cli look bank                      tmp/look/bank/contact.jpg, index.json and one PNG per shot
tools\cli look bank hospital 262,255     several areas, or any x,z, in one Workbench run
tools\cli look all                       all areas, plus tmp/look/overview.jpg (one wide per area)
tools\cli look bank --close 0            only the five area shots
tools\cli look grid:all --close 0        one set per claimed grid cell (docs/architecture/grid-cells.md is the picture index)
tools\cli look grid:first-night-room --interior 127.5,3.4,96 --radius 9   from INSIDE a building: x,y,z is the room's centre on its floor; four eye-level views from the room's sides, no oblique or top, close-ups of that floor only
tools\cli look bank --hour 19            local solar time (default 14, Clear weather)
tools\cli look bank --edit               the World Editor view instead of play mode
tools\cli look bank --measure            also print Workbench's peak working set
tools\cli look selftest                  the Node checks (also in tools\cli validate)
```

Read `contact.jpg` (sized for an agent to read in one image) and then the PNG of any tile that
looks off. Each close-up label carries the entity's world bounds in metres and its scale factor
when that is not 1; compare sizes between neighbours. The exit code is 1 when a shot is missing or
Workbench timed out, never because of a flag.

**What a picture shows.** A look shows the working tree: HEAD plus uncommitted and untracked
files. Shoot "before" from a checkout without your edit (before the first edit, `git stash`, or the
base commit), never after `dress.mjs` or any generator has already rewritten the file. Every
`index.json` carries `inputs.addons` (the git tree of `addons/` the look was asked to show) and
`inputs.rendered` (the tree the Workbench had on disk when it shot: the viewer checkout for the look
server, the snapshot for a fresh Workbench); the summary prints it as `addons=<tree>`. The look fails
when the two differ, and a routed look also fails when the box rendered another tree than this
checkout's. `tools\cli evidence <key> after <area>` refuses a pair whose before and after show the
same tree (`--same-inputs` for a change outside `addons/`). On 2026-11-17 PR #2364's general store
before and after looked the same: the "before" look ran after `dress.mjs` had rewritten the prefab,
so both pictures showed the new prefab (snapshot `f66939f0` held the 1312-line file). The look itself
was right: the same prefab, committed in another worktree, showed its thinner shelves at once.

**Flags** are printed per shot and written into the tile label and `index.json`. They are hints
that say where to look first, not verdicts:

| Flag | Means | Measured as |
|---|---|---|
| `blocked` | something stands between the camera and the target | under 20% of the ray clear (12% for a close-up) |
| `not-found` | no entity with a mesh at the layer position | by name, else nearest within 1 m |
| `white` | a close-up that is mostly colourless near-white | over 30% of the middle of the frame |
| `magenta` | the engine's missing-texture colour | over 3% |
| `dark` / `blown` / `flat` | mean luma under 30 / over 222 / spread under 7 | a 96x54 grid over the middle 70% |

On 2026-11-11 a run over all 23 areas raised no flag, while the sheets showed things to fix (a
moonshine still on its side, sack piles taller than the barrels beside them, a pole in front of the
bank's wide shot). So the flags catch broken renders; the judgment is still the reading.

### `tools\cli look serve`: one Workbench for every look

A fresh `look` starts a Workbench (about 28 s, 1.9 GB peak) and exits. With about ten agents on a
15.6 GB PC those spikes queue behind each other on the build lock. `look serve` keeps one hidden
Workbench up for the whole machine; while it is up, every `tools\cli look <area>` from any worktree
sends its plan there instead and costs a few seconds.

```
tools\cli look serve                 start it on this worktree's state (about 17 s on a PC), or say it is up
tools\cli look serve --status        pid, snapshot, idle time, health
tools\cli look serve --stop          stop it; --remove also removes the viewer checkout
tools\cli look bank --serve          use the server, starting it when none is up
tools\cli look bank --fresh          never use the server (the old path)
tools\cli look bank --budget 240     a longer base budget (default 90 s fresh, 150 s through the server) on a slow box
```

The server runs on its own detached checkout beside the main one, `Life-Framework-look-viewer`
(a git worktree with no branch and no junctions; `--stop --remove` deletes it through
`tools/link-safety.mjs`). Each request carries a snapshot commit of the caller's worktree: HEAD plus
uncommitted and untracked files, the same snapshot remote test runs use. The viewer checks it out
(git writes only the files that differ) and the server shoots it. What changed between the snapshot
the server last showed and this one decides how:

| Change since the last request | How it is served (`served=` in the summary and `index.json`) |
|---|---|
| nothing (the same snapshot) | `server-same`: shot as is |
| only files outside `addons/` (tools, docs: Workbench never loads them) | `server-reopen`: the world is reopened from disk (`SetOpenedResource`) |
| any file under `addons/` (a `.layer`, an `.et`, a `.c`, a `.meta`, an asset, an added, deleted or renamed file), or the look plugin | `server-restart`: the server Workbench is stopped, the viewer checked out, a new one started |
| no server up, or it is dead, hung or crashed | `fresh`: a Workbench of the caller's own, as before |

No addon file is on the fast path. A probe on 2026-11-14 reopened fine after editing a `.layer` or an
`.et` in place, but through the viewer, where git checkout rewrites the file, reopening after a moved
entity in a `.layer` crashed the server Workbench (access violation in `SetOpenedResource`) 4 of 4
times, on lf-testbox and on lf-cloud. A changed script crashed it 2 of 2 in the probe. Closing the
world first is no way round it: `WorldEditor.Close()` returns true and leaves the world open. So the
server pays off for repeated looks at the same state (before/after pictures, other areas, `--hour`,
`--edit`) and for changes outside `addons/`; an agent iterating on a layer gets a restart per look,
about as long as a fresh run, with one Workbench instead of one per run. `FAST_PATH_EXT` in
`tools/visual/look-serve.mjs` takes an extension only once it is proven through the viewer, several
times in a row.
`resourceDatabase.rdb` is not compared: it is Workbench's own cache of the files (`-forceUpdate`
rebuilds it), and its bytes differ between worktrees at the same commit and size, so comparing it
would restart every request. The viewer is seeded with the caller's copy once and keeps its own.

Every request still takes the build lock for its seconds (never while the server idles), and a
`look` waits up to 2 minutes for it when a server is up (`--wait` for 10). The RAM gate is asked
only when a Workbench is about to start. Before every shot Node checks the viewer is exactly the
snapshot (HEAD and `git status`), and the first status line must carry the request id, so a picture
of an older state is never returned as this one. A server that is gone, whose heartbeat
(`alive.txt`, every 3 s while idle) is stale, that dies during a request or does not answer in its
budget is killed with its children (the `CrashReporter` a crash leaves on the hidden desktop),
forgotten, and the caller runs a fresh Workbench and still gets its pictures. With no request for 10
minutes the server exits and frees its memory. `look item` stays on the fresh path.

Server-mode log lines in `console.log` under `Life-Framework-look-viewer/server/profile/visual-serve/logs/`
start with `[LFVisual]`, like a fresh run's; `ok request <id> reload=<mode>` opens each request.

### `look` on a pool machine: where the time goes, and the warm path (2026-11-16/17)

`tools\cli look <area>` from a worktree on this PC (no Workbench here) runs on a pool machine. Every look prints its
phases (`look: phases: ...`; the router adds `route-wait, sync, remote-total, copy-back`) and keeps them in `index.json`
under `timing`. Measured on lf-testbox with three other agents looking at the same time, `look grid:gun-counter --fresh`
(14 shots, the old path) took **300 s**:

| phase | s |
|---|---|
| RAM wait (route-wait: no machine had room for a Workbench) | 132 |
| the box's one Workbench slot, FIFO behind other agents' looks (slot-wait) | 100 |
| sync to the box (run folder, bundle) | 4.7 |
| Workbench start (wb-start) | 23.5 |
| world load and play | 5.2 |
| 14 shots | 11.3 |
| collect, node start and the rest on the box | about 20 |
| copy back | 3.0 |

So the look itself is 40 to 60 s of 300; the rest is queueing behind other Workbenches. Two things were cut:

- **A target that does not resolve fails before the queue.** `look grid:gunstore` says so in 0.3 s. It used to say so
  after the slot wait (217 s, once).
- **A routed look is warm by default.** The box runs it with `--serve`: one resident hidden Workbench per box (its
  `Life-Framework-look-viewer` checkout) takes the plan, so the slot is held for the request, not a Workbench start. Through
  the server the same look held the slot for **31 to 39 s** (restart 10 to 17 s when the state changed, 0.1 to 0.3 s when
  not, world 3.3 to 5.5, shots 10 to 11.5) against about 60 s cold. `--cold` is the old path (a Workbench of its own, no
  resident server). A warm look that gives no pictures is run once more cold on the same machine, automatically; a dead
  or hung server inside the box already falls back to a fresh Workbench (below).

What a look from **another worktree** costs is decided by what differs from the state the resident Workbench shows
(`mirrorMode`, `tools/lib/look-job.mjs`; the viewer is the mirror):

| Difference from the last request | Served as |
|---|---|
| nothing | `server-same`: shot as is |
| only files outside `addons/` | `server-reopen` |
| a modified `.layer` when `FAST_PATH_EXT` lists it (empty; `LF_LOOK_REOPEN_EXT=.layer` to probe). Only `.layer` can ever take this path (`REOPEN_SAFE_EXT`): a prefab, material, config or mesh stays loaded in Workbench across a world reopen, so it restarts whatever the variable says | `server-reopen`; the file is written **in place** (open, truncate, write: an editor's save), HEAD moves with no worktree write, because `git checkout` unlinks and recreates a file and a reopen after that crashed Workbench 4 of 4 (2026-11-14) |
| any other addon change (added, deleted, renamed, a script, a `.meta`, the look plugin) | `server-restart`: 10 to 17 s |

**Not proven (2026-11-17):** the in-place reopen. On the shared box every look between mine came from another worktree
whose state differed by added and deleted files (a kit `.layer` removed, one added), which always restart, so no `.layer`
edit was ever served by a reopen and `FAST_PATH_EXT` stays empty. A second look of the same cell from a different worktree
restarts the server today (31 to 39 s of slot time); it is not a few seconds. Proving a reopen needs a quiet box: run
`LF_LOOK_REOPEN_EXT=.layer tools\cli look grid:<cell>` after editing a `.layer` entity, three times in a row, with no other
agent's look between.

**RAM is the pool's admission** (`tools/lib/pool-ram.mjs`, docs/worktrees.md "RAM admission"), not a second gate. Starting or
restarting the look Workbench is a `workbench` job at its measured peak (3.6 GB seed, the p90 of observed peaks once there
are samples) plus the floor, less what every running ArmaReforger process still grows into; the resident one hands its
working set back to its own restart. A request to a live resident server needs no new RAM, so the router counts a box that
has one as having a peak more free for a warm look (`state.lookServer`, read with the Workbench slots in one ssh call).
`remoteWbState` now reads `tmp\wt\locks` with the right escaping: it read `tmpwtlocks`, so the box always showed
"0/1 busy" and the router never said "slot busy; queuing there".

**Lifetime (sessions flow like a river).** The resident Workbench exits after 10 idle minutes on its own (the plugin), is
closed by the pool after 5 (`reapRemoteLookServer`, called from the poll that closes idle sessions) and at once when a session
request waits on that machine for RAM or a lane; never during a request (no fresh heartbeat). A test waiting for RAM does
not trigger it yet: it waits for the 5 or 10 minutes. It never runs on this PC and never pins.

**Shared server.** Looks from every worktree use the one resident Workbench on the box, in turn, so two worktrees looking
alternately restart it each time (the third and fourth rows above). That is still one Workbench instead of one per look.

### `tools\cli look place:<place>/<slot>`: a town slot on the island

A town draft's slot stands on the real Everon map, which DebugWorld does not hold: there the camera found no ground
(y -253.8) and every picture was sky (#2347). `look place:saint-philippe/arrival` opens the pack's island world
(`Worlds/MainWorld/MainWorld.ent`, `ISLAND_WORLD` in `tools/lib/worlds.mjs`) and reads its viewpoint from the place's
files, never a typed coordinate (`tools/visual/look-place.mjs`):

- **Where:** the slot's building (or plot) in `docs/everon/places/<place>/mapping.json`, its footprint in `dossier.json`.
- **Front:** the first known entrance seen from the footprint centre, else the building's local +z (its yaw). The wide
  tries eye level on that side first, then look's usual fan.
- **Ground:** the plugin's `GetSurfaceY` on the island terrain (`ty -`), so cameras stand on the street, not under it.
- **Inside:** `--interior auto` stands on the ground floor (footprint centre, the dossier's floor height), four views.
- **Close-ups:** the entities of the island's `Place_*` layers within the radius (the fitted kit rows).
- `--world island` does the same for a bare `x,z`. A place: target and a town or grid area cannot share one run.

**RAM.** An island Workbench streams Everon like a server and a client in one process: `workbench:island` is seeded at
5.4 GB (`tools/lib/pool-ram.mjs`) until measured samples replace it, and the island class covers MainWorld, Eden and
CoreOnEveron. Routing reserves that peak when it picks a machine, and an island request to a live look server is gated
too (the resident grows to the island peak when it opens MainWorld). The boot budget is 210 s fresh, 270 s through the server.

**Measured (2026-11-17, lf-testbox, through the warm server).** World and play 34.8 s, 6 shots 6.2 s. The queue was the
cost: 565 s for the box's one Workbench slot and 966 s for the RAM gate behind other looks. Plan an island look as up to
30 minutes of waiting and about one minute of work; batch a slot's exterior and interior in one sitting.

### `tools\cli look item`

```
tools\cli look item Prefabs/Items/Fence/LF_Drill.et         both pictures, tmp/look/item-LF_Drill/{ground,inventory}.png + contact.jpg
tools\cli look item cement-bag --states ground              an asset slug, one state
tools\cli look item a.et b.et --at hospital                  several, on the ground of an area (default spawn-square)
```

The prefab is spawned in play mode at an open spot, posed, photographed and deleted. **ground** poses it
the way the inventory drops it (the model axis `RestingUP` names faces up, `RestingAdditiveOffsetLS`
nudges it, the lowest point on the ground; a drop is placed, not a physics fall) and takes a close-up.
**inventory** freezes it in the air and puts the camera where its `PreviewRenderAttributes` say. Values
are read from the prefab file; one it inherits and does not state is read as the engine default and
said on the first line (`not stated in the prefab file`). `RestingAdditiveRotationLS` is not applied.
It is the same hidden Workbench as `look <area>` (build lock, 2.2 GB, `--wait`, `--force`).

### How it works

`tools/visual/look.mjs` reads the layers (`agent-box/layers.mjs`) and `agent-box/areas.json`, writes
a plan of `shot` and `close` lines to `server/profile/visual/profile/lf-visual/plan.txt`, copies
`tools/visual/wb/LF_VisualShotsPlugin.c` into the git-ignored `Scripts/WorkbenchGame/LFVisual/` and
starts `ArmaReforgerWorkbenchSteamDiag.exe -wbModule=WorldEditor -plugin=LF_VisualShotsPlugin
-forceUpdate` through `tools/lib/quiet-launch.mjs`. The plugin's `RunCommandline` opens the world
(`WorldEditor.SetOpenedResource`), calls `SwitchToGameMode`, spawns a `CameraBase`, and for each
line picks the clearest of several camera candidates with a trace, waits for the view to settle,
and writes the viewport with `System.MakeScreenshot`. It writes `status.txt` after every step and
calls `Workbench.Exit`. Node then moves the PNGs, computes the pixel numbers and builds the sheets
with the same image code as `shots`. The plugin copy is removed after the run.

It holds the `build` lock while Workbench lives, so it never runs beside the headless build or the
rdb refresh; `--wait` queues for up to 10 minutes. It needs about 2.2 GB of free RAM for those
seconds and asks the pool to trim idle sessions first; under that it fails at once with one line
(`--force` skips the check). The run is killed at its budget (90 s plus about 1 s per shot) and
says `TIMEOUT`. It refuses the main checkout.

A run never hands back fallback pictures (#1147): the plugin opens the requested world or errors
(`world_not_opened`, `world_mismatch`), the Node side re-checks the world path from `status.txt`, and 3 or more
byte-identical frames fail the run. A Workbench that came up on its default scene (an empty world with a green
vegetable crate, seen under memory pressure and after `--force`) now prints `FAIL  look: the requested world did
not load ...` and keeps no pictures. The asset verify/states clothing shots use the same check. `look <area> --world
<path>` overrides the world, which is how to force the failure.

### What it does not show

- **UI.** A Workbench play capture is the 3D view only.
- **Anything a dedicated server does differently**, a second player, or late join.
- **Red fans and lines on the ground.** A Workbench debug overlay that the game client does not
  draw. Ignore them. `--edit` adds the editor's own helper shapes on top.
- **The graded look.** Workbench uses its own graphics settings, and shots are at 14:00 unless you
  pass `--hour`. Pictures for a PR come from here anyway (judge the same hour before and after); the graded
  look for media and the website still comes from `shots area` on a session.
- **Dynamic bodies after they settle.** Shots start about 3 s into play, so a log that tips off a
  table a minute later is not caught.
- The wide camera is chosen by a ray to the area centre plus a fan of level rays across the lens (42
  degrees each side, 3 degree steps, 3.5 m reach), so a lamp post or sign right in front of the lens fails
  the clearness test and the next candidate is tried (#814). Something wider than the fan or farther
  than its reach still passes. The obliques and the top view cover the rest.

## Measured (2026-11-11, this PC: 15.6 GB, with a trailer agent's sessions and encodes beside it)

| | `session up --profile full` + `shots area bank` | `look bank` | `look all` (23 areas) |
|---|---|---|---|
| Shots | 12 | 17 | 346 |
| Wall-clock | 126 s with a free lane: about 25 s to boot the server and client, 97 s of shots | 25 s: 9 s Workbench start, 3 s to settle, 0.5 s per shot | 227 s: about 10 s per area |
| Per shot | 8 s | 0.5 s | 0.5 s |
| Queue | a session lane; the pool's 301 recorded sessions waited 6.7 min on average | none (the build lock, held for the run) | none |
| RAM | server 0.7 GB + client 1.0 to 1.2 GB working set, held for the session (`docs/worktrees.md` "Session profiles") | 1.7 GB peak for 25 s | 1.9 GB peak |

The session column is one recorded run (pool log: no wait, held 2.1 min) plus the per-process
numbers measured on 2026-11-10. So `look` is 5x faster for one area with an empty queue, 16x per
shot, and avoids the queue; it does not use less RAM at its peak, it uses it for seconds.

## Measured: `look serve` (2026-11-14, lf-cloud: Windows Server 2025, 8 vCPU, Tesla T4, 31.6 GB)

`look bank --close 2` (7 shots), `--budget 240`, run over ssh from a pool run folder. lf-cloud is
slower than the PCs (Workbench start 13 to 84 s, entering play up to a minute while its caches were
cold), so compare the columns, not these numbers with the 2026-11-11 table.

| | run 1 | run 2 | run 3 |
|---|---|---|---|
| through the server, same snapshot (`server-same`) | 17.4 s | 11.6 s | 11.6 s |
| fresh Workbench (`--fresh`) | 37.7 s | 67.3 s | 126.2 s |
| through the server after a `.layer` or script change (`server-restart`) | 38.5 s | 41.3 s | 48.3 s |
| through the server after a docs-only change (`server-reopen`) | 19.6 s | | |

Starting the server took 13 to 24 s once warm (103 s on its first boot). The fresh runs went up as the
box got busier (another agent's Workbench and a Steam download beside them); the server runs did not.
Proofs on the same box: two run folders with different `Signs.layer` states, shot back to back
through one server, read back Sign_Bank at x 190.06, 193.06 and 190.06; a script change restarted
and shot correctly; a server killed 45 s into a 17-shot request fell back to a fresh Workbench that
returned all 17.

## The tools surveyed

| Tool | Renders | Verdict |
|---|---|---|
| Workbench play mode from a command-line plugin (`-plugin=`, `RunCommandline`, `SwitchToGameMode`, `CameraBase`, `System.MakeScreenshot`) | yes, hidden | **`look`.** Clean frames, run-time spawns present, 0.5 s per shot |
| Workbench World Editor view (`WorldEditorAPI.SetCamera` + `System.MakeScreenshot`) | yes, hidden | `look --edit`. Starts 2 s sooner; shows editor helper shapes and nothing a script spawns |
| `wb_*` MCP tools (`wb_launch`, `wb_play`, `wb_capture`) | yes | One shot per call through a start/poll handler on a Workbench that stays up. Good for poking at one object by hand, slow for a set |
| `wb_preview` (Resource Manager preview via `SetOpenedResource` + `System.MakeScreenshot`, edit-mode `SetCamera`) | yes | One resource with no world loaded, on a Workbench that is already up: 1.3 to 2.4 s a shot against `look item` at 28 s a cold start (measured 2026-11-14, LF_SeedCrate_Cabbage). Needs a Workbench serving NET API port 5775. The shot is the Resource Manager preview viewport while a resource is open, not the World Editor view. Handler and tool: `tools/mcp-patches/enfusion-workbench-mcp/` |
| Headless Workbench (the build and rdb path), dedicated server, `tools\cli test` server | no | No pixels (`agent-box/README.md` "Screenshots"). World queries only: this is what `audit placement` uses |
| Game client with `-world` and no server | yes | Not measured. It would capture UI, which Workbench cannot, at the cost of a client boot and the harness to drive it. The next thing to try for UI shots without a session |
| Offline `.xob` bounds cache | no | Not built. `audit placement --from` already re-ranks the last run's measured bounds without a server |
| `shots area`, `shots baseline`, `film`, `journey` | yes, game client | The real player view, UI, motion and multiplayer. Needs a `full` session |
| `tools\cli perf` | no | Cost, not looks |
