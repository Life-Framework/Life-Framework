# Starter pack: building fit-out (gun-store quality, 3 passes)

One vanilla Everon building fitted out as a whole-building kit (epic #2201). The bar is the general store, drug store,
gun store (#1903, polish #2013) and clothing store. Look at the pictures of `gh pr view 2013` once, then start.

## Setup
```
tools\cli wt new building-<slug> --wait
tools\cli grid list                                    # claim ONLY the cell your brief names
tools\cli grid place <slug> --cell cXrY --span WxH     # fenced yard + claim file; one claim per building
```
- Reserved cells (lead, 2026-11-16; your brief wins): town hall c11r2 3x2, bank c11r0 2x2, police c3r1 2x2. Next, in
  dispatch order: c13r0 2x2, c11r4 2x2, c13r4 2x2.
- The host goes into the layer as `SCR_DestructibleBuildingEntity <Name> : "{GUID}Prefabs/...et" { coords x 0 z  angleY a }`.
- Template, copy and never edit: `tools/buildings/gun-store/{spec.json,decor.mjs,proof.json}`. Newer fit-outs once merged:
  `tools/buildings/{town-hall,bank,police-station}/` (`prune.mjs` cuts the vanilla furniture set, `decor-lib.mjs` writes
  props and lights, `looks.mjs` re-shoots, `fitout.mjs --report` prints the counts).
- Measure before placing: `tools\cli measure <prefab> --surfaces` (bounds and shelf and counter tops, no game). Heights are
  snapped, not typed: see "Props stand on something".

## Commit the reproduction: `tools/buildings/<slug>/`
- `spec.json`: concept, host prefab, rooms, `red_line`.
- the decor or fit-out script: rewrites its own block (IDs start with a prefix, e.g. `DC1F`) and gives the same bytes
  twice (`--check` exits 1 when the files differ).
- `proof.json`: every shot as `look`/`from` in BUILDING space, front at +z: exterior, every room, night, the walkthrough.
  `from` is an offset from `look`, not a position (`tools/buildings/pipeline/scenario.mjs`).
- Anyone must be able to re-shoot the building from these files alone.

## The passes (one look per pass: finish every edit, then look)
1. **BUILD.** Host building as a whole-building kit in its claim. Every interactable is an existing system (read its
   `Feature.md` and `docs/features/<area>.md`; a shop is a `store` block in `spec.json`). Props: era-current 1989 vanilla
   first, then CC0, then Meshy. Look: `look grid:<slug>` plus `look --interior` per room, day and night. Grade it C.
2. **POLISH 1 (C to B).** A light in every room, the facade lit at night, walls dressed, every surface stocked or used,
   signage. Look again.
3. **POLISH 2 (B to A).** Lived-in detail, clutter with a story, night mood. Fix whatever the pictures show floating,
   clipping, empty or white. `tools\cli building check <slug>` (offline, every pass) and `tools\cli audit placement --grid --strict`
   (in the world, once per pass) clean.
```
tools\cli look grid:<slug> --close 0 [--hour 22]
tools\cli look grid:<slug> --interior x,y,z --radius 5 --close 0 [--hour 22]   # world space, room centre on its floor
tools\cli evidence <issue> before|after grid-<slug>|<png> [--name n]            # then: evidence <issue> table
```
`look` writes `tmp\look\grid-<slug>` (exterior) and `grid-<slug>-interior`, which the next room overwrites: copy each
room's folder (town-hall `looks.mjs --tag pass1 --run` does) or pass the PNG path. Pass N's after is pass N+1's before. Past about 2 h, ship BUILD and POLISH 1 and file POLISH 2.

## Props stand on something (heights are snapped, never typed)
A typed y floated phones and lamps over desks (maintainer, 2026-11-17). A generator gives each prop a hint y and calls
`snapItems(items, { fixed })` from `tools/buildings/pipeline/surface.mjs`: `{ on: "<id>" }` stands it on that row, otherwise it
drops onto the top under it. Gun-store `decor.mjs` is the model; the detail is in `docs/building-pipeline/README.md`.
**`toolsi building check <slug>` is required** (about 1 s, offline): every small prop within 2 cm of the top under it, a floor or
the ground. `wt ship` runs it for a PR touching `tools/buildings/<slug>/` or the pack's `Prefabs/Buildings/<Dir>/` and refuses
while it fails. No generator: `building check <slug> --fix`. Keep `audit placement --grid --strict` as the in-world check per pass.

## Performance (docs/architecture/performance.md covers script cost; `test --tier perf` once if you touch script)
Report in the PR, per pass: dynamic lights, shadow casters, decorative entities. The limit is in your brief. Short-range
lights with no shadow, emissive lamp covers, a light per room and never per prop, static props with LODs, no physics on
clutter, no per-frame script on decoration.

## Ship
`tools\cli wt ship building-<slug> --refs <N> --body-file tmp\pr-body-<slug>.md`. Body: a before/after pair and a grade per
pass, the counts table, `## Manual testing`. No town layers, no `ws/town-maintainer-edits`; Saint-Philippe placement is
`town fit` later. Last line: `PR <url>, head <sha>, checks <state>`.

## Known traps
- `scenario.mjs`'s header says "-Z the street front". Every building's proof and look files use **front +z**. Follow +z.
- Wall items face the room: yaw 0 on a wall facing +z, 180 facing -z, 90 or -90 facing +x or -x (a trophy board sticks
  out the other way: gun-store `decor.mjs` header).
- Vanilla ceiling lamps are dead meshes: add a `LightEntity` (ClipEV -10, NearPlane 0.3, short radius) and an emissive cover.
- A vanilla furniture set is big (town hall: 242 children). Flatten to one child per prop, prune repeats (switches,
  radiators, infirmary, extra WCs); every kept entity counts.
- A committed grid layer that names a pack prefab: the bank agent kept it out of git (local `stage.mjs`, `--remove` before
  a commit); the town hall commits it. Run `tools\cli validate` and follow what it says.
- A GUID is made once and kept (rule 2). A typed coordinate is only allowed in a building-space row the script writes.
- `look` needs a Workbench: it queues on the pool (laptop or lf-testbox), never here.

## Environment
No Workbench on this PC and never pin a machine. Run in the foreground (timeout up to 600000 ms) or `t3_background_wait`;
never end the turn with a run pending. Never `--force`, `rm -rf` or `Remove-Item -Recurse` a worktree (`wt prune`).
