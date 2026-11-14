# Visual testing

What can look wrong, the cheapest evidence that catches each kind, and which tool gives it.
Use the cheapest tool that can see the problem; a game session is the last step, not the first.

```
tools\cli audit placement [--area a]     numbers, no pixels, test server, about 10 s      (docs/placement-audit.md)
tools\cli look <area | x,z> [more]       pictures, hidden Workbench, no session, about 25 s
tools\cli look all                       every area of agent-box/areas.json in one run, about 4 min, plus overview.jpg
tools\cli look item <prefab|slug> ...   a dropped item and its inventory camera view, no session, about 30 s
tools\cli shots area <name>              pictures from a game client on a `--profile full` session (docs/agent-area-polish.md)
tools\cli evidence <issue> before|after <area>   keep a look picture for the PR; `evidence <issue> table` prints the embed
```

What right looks like is `docs/style-guide.md`; the checklist to walk against the pictures is the
`polish` skill. `wt ship` refuses a player-visible change whose PR body embeds no picture.

## Use `look` unless you need motion, UI, input or a second player

Ask in this order and stop at the first yes.

1. **Numbers or files are enough?** (a placement, a GUID, a prefab value, a scale) `validate`, `audit placement`, `asset states <slug>`. No picture.
2. **A still of a place, a placed prop, a sign, an NPC standing?** `look <area>`. Close-ups of every placed prop are in the sheet.
3. **A still of one item: lying on the ground, or as the inventory sees it?** `look item <prefab|slug>` (`--states ground,inventory`, `--at area|x,z`).
4. **Two or three variants side by side?** `look item a b c`: one sheet each, 3 m apart.
5. **Does it move, play a sound, or react to input?** A session (animation, the hold pose, a door opening, a hit, a siren).
6. **Is it a UI sheet, HUD, or the inventory slot frame?** A session (Workbench play captures no UI).
7. **Does a second player or a late joiner see it?** A session with `--clients 2`.
8. **Is the picture for a PR or media?** `look` is for you while you iterate; the PR picture of a held item or a sheet comes from the session, and `tools\cli evidence` keeps a `look` pair for a world change.

What an agent session used to be needed for, and what does it now:

| Job | Before | Now |
|---|---|---|
| A dropped item at rest | `asset states --shots` or `itemdrop` and a free camera, 2 clients | `look item` (ground) |
| The inventory preview camera of an item | `asset states --shots`, preview sheet | `look item` (inventory): the 3D view, not the slot UI |
| A prop, a placed object, a sign | `shots area` | `look <area>` |
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

```
tools\cli look bank                      tmp/look/bank/contact.jpg, index.json and one PNG per shot
tools\cli look bank hospital 262,255     several areas, or any x,z, in one Workbench run
tools\cli look all                       all areas, plus tmp/look/overview.jpg (one wide per area)
tools\cli look bank --close 0            only the five area shots
tools\cli look bank --hour 19            local solar time (default 14, Clear weather)
tools\cli look bank --edit               the World Editor view instead of play mode
tools\cli look bank --measure            also print Workbench's peak working set
tools\cli look selftest                  the Node checks (also in tools\cli validate)
```

Read `contact.jpg` (sized for an agent to read in one image) and then the PNG of any tile that
looks off. Each close-up label carries the entity's world bounds in metres and its scale factor
when that is not 1; compare sizes between neighbours. The exit code is 1 when a shot is missing or
Workbench timed out, never because of a flag.

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
  pass `--hour`. Pictures for a PR or for media still come from `shots area` on a session.
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
