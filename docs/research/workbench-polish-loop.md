# Workbench polish loop: can the polish pass run in a live editor?

Spike, 2026-11-10, second pass after the stale Workbench was closed. Question:
can an agent open `DebugWorld` in a Workbench, move and add entities, capture
the result and save the layer, so a polish pass skips game sessions?

**Verdict: partly.** Editing is fast and works. Reading entities works.
Capture works but only in play mode, and needs the fixes in this PR. Saving is
the problem: the editor rewrites the whole layer, so the diff is not PR-clean.
Use the editor to look and to find numbers; keep making the layer edits as
text. Biggest gap: the editor save format (second: no edit-mode capture).

## Measured (this PC, RTX 3080, one quiet Workbench on the LF-Agents desktop)

| Step | Time | Notes |
|---|---|---|
| Cold Workbench start, NET API up | 14 s | `cli call wb_state`, quiet launch, project loaded, handlers compiled |
| Open `DebugWorld.ent` in the editor | under 3 s | 833 entities, edit mode (`wb_open_resource`) |
| List an area | 0.2 s | `wb_entity_list nameFilter=Moonshine`: 8 rows, named ones carry a position |
| Inspect an entity | 0.2 s | name, class, position, rotation, component classes. No bounds |
| Terrain height | 0.2 s | returned 0 everywhere: DebugWorld ground is flat at y 0 |
| move / rotate / create / save, per call | 5 to 10 ms of engine time | one batch of 3 edits plus save: 16 s including the cold start |
| `wb_play` | call returns in about 16 ms; the world is up about 3 to 8 s later | a capture sent at once answered `error:no_world`; one `wb_state` first was enough |
| `wb_capture` (1 s settle) | 3.7 s including the state call; PNG 1292 x 714, 0.7 to 1.5 MB | pose and time of day applied after the fix below |
| Session restart, for comparison | about 12 s boot plus the queue (about 7 min, per the lead) | |

Edit, play, capture, stop therefore costs roughly 5 to 12 s of engine time and
needs no queue slot, against about 12 s plus minutes of queue for a session.
That holds even with the play-mode detour. I did not get one clean
back-to-back cycle logged (see "What went wrong"), so treat the cycle figure
as a sum of measured parts, not a stopwatch run.

## 1. Read: yes

`wb_entity_list` and `wb_entity_inspect` give name, class, position, rotation
and components. Not available: bounds, layer names (`wb_layers` prints
"(unnamed)" with entity counts), and anonymous entities (the prefab props in a
layer, like pallets and sacks, list as "(unnamed)" without a position). The
polish rig's floating and clipping numbers come from in-game raycasts and have
no editor equivalent yet.

## 2. Edit and save: edits yes, clean diff no

Moved `Moonshine_Barrel_3`, rotated `_2`, created a sack pile, `wb_save`.
All three landed (`coords 385 0 300.9`, `angles 0 60 0`, a new entity). But the
saved `.layer` is the editor's canonical form, not the file's old text:

- `Street_Moonshine.layer`: +122 / -121 lines for a one-barrel move. Entities
  are reordered, same-prefab entities are merged into `$grp` blocks, `angleY 40`
  becomes `angles 0 40 0`, component blocks are reordered. The content is the
  same (barrel materials and station keys survive) but a reviewer cannot read
  the diff.
- `default.layer` (the world entity) changed too: +12 / -6 for the BSP, terrain
  bounds, sky material list and sun angles, none of which anyone edited.
- A new entity lands in `default.layer`, not the area layer, unless a
  `layerPath` is passed (layer names are not readable, so I did not find the
  path form).
- Saving with no changes writes nothing (zero diff).

So the live editor cannot be the writer of the `.layer`. The workable shape:
the agent edits the layer text as today and the editor is the look-and-numbers
tool. A normalizing pass that saves each layer once through the editor would
make later editor saves small, at the cost of one big reorder commit per layer.
I reverted my layer changes; none are in this PR.

## 3. Capture: works in play mode only; two bugs fixed

- `wb_capture` drives `LF_ScreenshotCapture` and needs play mode. The API search
  (`WorldEditorAPI`, `MakeScreenshot`, `SetCamera`) found no edit-viewport
  screenshot call. `System.MakeScreenshot*` exists and `EMCP_WB_GetCameraPos`
  already reads the editor camera, so a handler that sets the editor camera and
  calls `MakeScreenshot` is a lead, untested.
- **Bug fixed: pose ignored.** `CAMERA_PREFAB` had no GUID
  (`RESOURCES (E): Wrong GUID for resource {0000000000000000}Prefabs/Editor/Camera/ManualCameraBase.et`),
  so the camera never spawned and every shot was the default view. It is now
  `{D6DE32D1C0FCC1C7}Prefabs/...`, and the log says `aimed camera at ...`.
  The one aimed shot I took looked across an empty field with the town on the
  horizon, not at the barrels, so whether the aim points at the target is
  still unconfirmed.
- Time of day applies: `timeOfDay 12` gave full daylight, 13.77 gave a night
  sky. The rig's "local 17.9 = game 13.77" mapping does not carry over to the
  editor play session, so golden hour needs its own value here.
- Red horizontal lines appear in shots (debug shapes in the play world).
  A side-by-side with a session shot, for lighting and post-processing, was not
  done.
- No screenshots are committed: the shots I have do not show the site
  (above), and each is 0.7 to 1.5 MB.

## 4. Real bug found and fixed by the loop

Moving any chain entity in the editor threw a VME and wedged the NET API.
`LF_ChainStationComponent.OnDelete` called `GetGame().GetWorld()`, which is null
in the World Editor, and the editor deletes and recreates an entity on every
move. It now uses `owner.GetWorld()` with a null guard. Evidence before:
`Reason: NULL pointer to instance. Variable 'm_World' ... LF_ChainStationComponent.c:113
Function OnDelete ... EMCP_WB_ModifyEntity.c:207`; after, the same move returns
`Entity moved`. The same `GetGame().GetWorld()` pattern is still at
`LF_ChainStationComponent.c` (another method) and likely in other `OnDelete`s:
each one is a hang waiting for the first editor move.

## 5. RAM and instances

- One Workbench is about 0.8 GB idle and 1.3 to 1.9 GB with DebugWorld loaded
  and played. Two ran side by side without trouble, but only one NET API port
  works per machine. A second instance on another port (registry `netApiPort`
  5776 and 5780) never answered; I could not tell whether the setting is
  ignored. On the laptop one Workbench listens on 5775 and 5776 (5776 is
  probably the language-server port). Treat it as one Workbench per machine
  that opens one worktree's project at a time (`wb_projects` can open a
  `.gproj`).
- This PC has 0.5 to 2 GB free while sessions run, so a Workbench competes
  with them. The laptop (47 GB, 33 GB free with a Workbench, server and client
  up) fits one Workbench next to its 3 + 3 lanes, so its `caps` should list
  `workbench` with a single lock. The laptop Workbench I found was a launcher
  with no handlers and could not be driven from here, and a Workbench started
  over ssh from a non-interactive shell did not appear, so launching there
  needs the box's desktop-session mechanism (`tools\cli testbox start`).
  Laptop work stayed unproven.

## What went wrong (cost of the spike)

- The handler bundle did not compile after the EL_ to LF_ rename
  (`EMCP_WB_Capture.c` named `EL_ScreenshotCapture`); one error takes the whole
  WorkbenchGame module down. Fixed by a tracked patch plus `tools\cli mcp
  install` re-applying `mod/` (merged in #552).
- The reported mode is wrong in play: the RP world spawns an editor-mode
  entity, so `wb_state` says "edit" while the game runs. `wb_stop` then refuses
  and `wb_entity_modify` says "Cannot get runtime entity". Restarting the
  Workbench (14 s) is the reliable reset. The raw `EMCP_WB_EditorControl stop`
  worked in 5 ms when called directly.
- After a script VME the NET API stops answering (10 s timeouts) until the
  Workbench restarts.
- When the quiet launch does not open the port, the MCP launches its own
  Workbench (not quiet).
- My raw NET API client waited for the socket to close; the server does not
  close it. Read the length-prefixed reply instead.

## Recommendation

Build `tools\cli wb-polish` for look-and-measure, not for writing layers.

1. Merge this PR (camera GUID, `OnDelete` fix). Audit other `OnDelete`s for
   `GetGame().GetWorld()`.
2. Fix the reported mode (ask the world, not the editor-mode entity) so
   play, stop and edit chain without restarts.
3. Run it on the laptop under one `workbench` lock; this PC stays for sessions.
4. Add an area numbers verb (position, rotation, ground y) from the editor, and
   keep layer writes as the text edits agents already make.
5. Try the edit-mode viewport capture (editor camera plus `MakeScreenshot`);
   if it works the loop drops the play detour.

## Sketch: `tools\cli wb-polish`

```
tools\cli wb-polish <area> [--machine laptop|local]
  1. take the workbench lock (one per machine)
  2. quiet Workbench with the handler bundle, project = this worktree
  3. wb_open_resource Worlds/DebugWorld/DebugWorld.ent
  4. numbers: wb_entity_list + inspect for the area radius in agent-box/areas.json
  5. after a text edit of the .layer: reopen the world
  6. shot <pose>: wb_play, wb_state, wb_capture (timeOfDay), raw stop; 1280-wide PNG
  7. never wb_save into a PR; use the editor save only for a one-time normalize
```

## Unproven

- A back-to-back edit, capture, edit, capture run with a stopwatch (the pieces
  are measured; the chain hit the mode and NET API issues above).
- Whether the aimed camera points at the target (one shot, a field view).
- `layerPath` for new entities; the normalize-once idea; whether a game
  session loads an editor-saved layer identically (the content looked identical).
- Edit-mode viewport capture; editor against game lighting side by side.
- A second instance on its own port; the laptop Workbench with a project.
