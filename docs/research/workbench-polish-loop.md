# Workbench polish loop: can the polish pass run in a live editor?

Spike, 2026-11-10. Question: can an agent open `DebugWorld` in a Workbench, move
and add entities, capture the viewport and save the layer, so a polish pass
skips game sessions and their restarts?

**Verdict: not proven, and not yet viable.** No edit or capture cycle was ever
measured, because no Workbench on either machine could be driven. What the
spike did establish is why, and the short list of things that must change
before the loop can be tried. The biggest gap is `wb_capture`: by its source it
works in play mode only, so it does not capture the editor viewport.

## What was measured

| Fact | Number | How |
|---|---|---|
| Workbench cold start on this PC (project loaded, scripts compiled) | 5.2 to 7.0 s | `Workbench startup took` in `console.log`, four launches |
| Workbench idle RAM after start | 800 to 860 MB working set | `Get-Process`, two instances of mine |
| Workbench RAM with a maintainer's session on the laptop | 1.7 GB | `Get-Process` on `lf-testbox` |
| Free RAM on this PC with one server + one client up (other agents) | 0.5 to 2.0 GB of 15.6 GB | `Win32_OperatingSystem`, `Memory\Available MBytes` |
| Free RAM on the laptop | 33.5 of 47 GB with server, client and a Workbench up | same, over ssh |
| Time lost per failed Workbench bring-up | 5.6 min (240 s quiet launch wait, then 90 s MCP wait, then its own second launch) | `time node tools/mcp-call.mjs wb_state` |
| Session restart cycle (for comparison) | about 12 s boot plus queue, per `agent-box/README.md` | not re-measured here |

Questions 1 to 4 of the brief (list entities, move/add/snap/save with a clean
`.layer` diff, capture at a chosen pose and time, edit-capture round trip on
the quarry or moonshine) are **unanswered**. There are no timings or
screenshots for them and this document does not estimate any.

## Why nothing could be driven

1. **The handler bundle did not compile after the EL_ to LF_ rename.**
   `EMCP_WB_Capture.c` in the Workbench MCP clone calls `EL_ScreenshotCapture`
   and `EL_ScreenshotShot`; the class is now `LF_ScreenshotCapture`. A WorkbenchGame
   compile error takes the whole module down, so no `EMCP_WB_*` handler
   registers: `wb_state`, `wb_entity_*`, everything reports "Handler scripts
   were installed but Workbench did not recompile them". Evidence: the
   Workbench `console.log` of every launch before the fix lists 33 `SCRIPT (E)`
   lines starting `EMCP_WB_Capture.c,80: Can't find variable 'EL_ScreenshotCapture'`;
   after replacing the prefix in the clone, the launch log has none.
   This PR carries the fix as a tracked patch
   (`tools/mcp-patches/enfusion-workbench-mcp/mod/...`) and teaches
   `tools\cli mcp install/update` to re-apply `mod/` as well as `src/`.
   The clone in the main checkout was patched in place (it is git-ignored
   tooling; the same sed).
2. **A stale orphan Workbench holds NET API port 5775 on this PC.** Pid 72348
   (started 2026-11-09 23:23, parent process gone, no window on the user's
   desktop, main checkout project) listens on 5775 and still runs the
   pre-fix script module, so its handlers are missing (`Failed to call not
   existing Net API function 'EMCP_WB_Ping'` in its log). `ensureQuietWorkbench`
   sees the open port, prints `skip: a Workbench already serves NET API port
   5775`, and every `cli call wb_*` then talks to the dead-handler instance
   and fails with the misleading 30 s "did not recompile" error. It is not a
   process I started, so I left it running. Until it is closed, `cli call wb_*`
   on this PC cannot work.
3. **One NET API port per machine, and I could not move it.** The port is the
   per-user registry value `HKCU\Software\Bohemia Interactive\Arma Reforger
   Workbench\Workbench\netApiPort` (default 5775; the Workbench binary also has
   `enableNetApiLSP` and `netApiLSPPort`, and on the laptop one Workbench
   listens on both 5775 and 5776, so 5776 is probably the language-server
   port). I set it to 5776 and 5780 and launched my own instance on the hidden
   desktop; neither came up on its port within 120 to 240 s. I could not
   separate "the setting is not honored" from "my instance never finished
   loading", because log directories from other agents' Workbench runs
   interleave with mine and I attributed some lines wrongly at first. The
   registry value is restored to 5775 on this PC (and removed on the laptop,
   where it was unset).
4. **The laptop Workbench is a launcher, not a project.** The maintainer's
   Workbench on `lf-testbox` (pid 6672, no `-gproj`, project `NewEnfusionProject`)
   answers on 5775 but has no handlers (`EMCP_WB_Ping` missing). The NET API
   is not reachable from this PC directly (a plain `wb_connect` to `PC1:5775`
   failed; through an ssh tunnel it connected but found no handlers). To use
   it, a project with the handler bundle must be loaded, which means touching
   the maintainer's editor. A second Workbench started over ssh from a
   non-interactive shell did not appear at all (no process, no log), so
   launching there needs the box's desktop-session mechanism
   (`tools\cli testbox start`), which I did not get to.
5. **The MCP's launch fallback is not quiet.** When the quiet launch does not
   open the port in 240 s, the MCP then runs its own `wb_launch` ("Workbench
   not running, auto-launching"), and I saw a second Workbench process appear
   that way. That is the path the LF-Agents desktop rule exists to avoid.

## What the source says about the six questions

Read from `tools/mcp/enfusion-workbench-mcp/src/tools/` and
`mod/Scripts/WorkbenchGame/EnfusionMCP/`, not exercised:

1. **List and read.** `wb_entity_list` (paged, name filter, top-level only per
   `docs/WORKBENCH-OPERATIONS.md`), `wb_entity_inspect` (components, position,
   children), `wb_layers`. Bounds are not an obvious field; the polish rig's
   bounds checks come from in-game raycasts, not from this API.
2. **Edit and save.** `wb_entity_create` (needs the `{GUID}path` form or it
   saves a zero GUID), `wb_entity_modify` (move, rotate, rename, reparent;
   component-level property writes fail per the operations notes),
   `wb_terrain` (height at x,z, so snapping is one call plus a modify),
   `wb_save`. Whether the saved `.layer` diff is clean (ordering, GUID
   churn, `DebugWorld.ent` staying 0 bytes) is the main unknown.
3. **Capture.** `wb_capture` drives `LF_ScreenshotCapture`, whose header says
   it runs on a rendering instance in play mode; the tool help says "Requires
   `wb_play`". It aims a spawned camera at a position from an offset, can pin
   `timeOfDay` and `weather`, and writes a BMP converted to PNG. There is no
   edit-viewport capture. Output size is the window's, with no width
   parameter. So the loop would be edit, then Play, then capture, then Stop,
   not edit then capture. Play entry is what `docs/` notes can hang the
   bridge ("Calling `wb_play` on a world already open in edit mode can hang
   the bridge") and `LF_PlaytestTown` content with scripts runs a game mode.
4. **Round trip.** Cannot be stated. The structural cost is a play-mode enter
   and exit per capture, not a world reload.
5. **RAM and instances.** Two Workbenches ran side by side on this PC at about
   830 MB each, so RAM is not what stops a second one: the single NET API
   port is, and `docs/worktrees.md` already says concurrent instances collide
   on the shader cache. This PC has 0.5 to 2 GB free while sessions run;
   the laptop has 33 GB free and could carry a Workbench plus its session
   lanes (3 sessions + 3 tests are about 8 GB). One Workbench per machine
   that switches projects (`wb_projects` can open a `.gproj`) is the only
   shape the port allows today.
6. **Gaps.** Editor and game lighting were not compared. The one crash-like
   event seen: the orphan's log shows `Out of memory. Free system 353 MB` and
   later missing handlers after a script reload, on a machine at 0.5 GB free.

## Recommendation

Do not move the polish pass to the editor yet. Order of work that would make
the experiment cheap to finish:

1. Close pid 72348 (maintainer or lead; it is not an agent's to stop) and
   make `ensureQuietWorkbench` check for the handlers (`EMCP_WB_Ping`), not
   just an open port, so a stale instance fails loudly in one call.
2. Merge this PR's handler patch so a launched Workbench compiles.
3. On the laptop (47 GB, `workbench` cap), start one Workbench for a run
   folder through the box's desktop mechanism, hold a `workbench` lock, and
   run the measurements below.
4. Add an edit-viewport capture: either a handler using the Workbench editor
   camera plus `System.MakeScreenshot`, or accept the play-mode round trip.
   Without this the "10x" claim does not hold, because the session boot the
   loop avoids comes back as a play-mode enter per shot.

Measurements still owed, in order: `wb_entity_list` on the moonshine area;
move one prop, `wb_save`, `git diff` of the `.layer` (the clean-diff test);
`wb_capture` timing in play mode at golden hour (`timeOfDay` 13.77 game hours
matches the rig's local 17.9); five edit-capture cycles; Workbench and system
RAM during them.

## Sketch: `tools\cli wb-polish`

Only worth building after step 4 above.

```
tools\cli wb-polish <area> [--machine laptop|local]
  1. take the workbench lock (one per machine, queue like sessions)
  2. ensure one quiet Workbench with the handler bundle, project = this worktree
  3. wb_open_resource Worlds/DebugWorld/DebugWorld.ent
  4. area entities from agent-box/areas.json radius -> wb_entity_list/inspect -> numbers
  5. edit verbs: snap <Entity>, move <Entity> x y z, rot <Entity> yaw, add <guid> x z
     (snap = wb_terrain height + wb_entity_modify)
  6. shot <pose> -> wb_play, wb_capture (timeOfDay), wb_stop, one PNG
  7. wb_save; print the layer diff stat
  8. release the lock; leave the Workbench for the next run
```

## Unproven

- Everything in questions 1 to 4 and 6: no entity was listed, moved, saved or
  captured through the editor.
- Whether `netApiPort` can be set per instance. Needs one clean attempt with
  the log directory identified by pid (the log folder name carries the start
  time) and no other Workbench running.
- Whether the editor and game render the same light and post-processing.
