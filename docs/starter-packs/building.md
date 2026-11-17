# Starter pack: building fit-out (gun-store quality, 3 passes)

One vanilla Everon building fitted out as a whole-building kit (epic #2201). The bar is the general store, drug store,
gun store (#1903, polish #2013) and clothing store. Look at the pictures of `gh pr view 2013` once, then start.

## Setup
```
tools\cli wt new building-<slug> --wait
tools\cli grid list                                    # claim ONLY the cell your brief names
tools\cli grid place <slug> --cell cXrY --span WxH     # fenced yard + claim file; one claim per building
```
- Reserved cells (your brief wins): town hall c11r2 3x2, bank c11r0 2x2, police c3r1 2x2; next c13r0, c11r4, c13r4 (2x2).
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
A typed y floated props over desks (2026-11-17). A generator gives each prop a hint y and calls `snapItems(items, { fixed })` from
`tools/buildings/pipeline/surface.mjs` (`{ on: "<id>" }` stands it on that row, else it drops onto the top under it; model: gun-store
`decor.mjs`, detail in `docs/building-pipeline/README.md`). **`tools\cli building check <slug>` is required** (1 s, offline): every small
prop within 2 cm of a top, floor or the ground. `wt ship` runs it for a PR touching `tools/buildings/<slug>/` or `Prefabs/Buildings/<Dir>/`. No generator: `building check <slug> --fix`. A kit whose rows are in a grid layer: `spec.json` `check_layer` (police station).

## Fitting a vault room (`bank_room` safe type)
The bank safe is a room in the building, not the standalone `LF_BankVault.et`. Doorway: `{B17A72DCB10A840C}Prefabs/Props/Roleplay/BankVault/LF_BankVault_RoomDoor.et`
(origin = the front's centre, hinges 1.271 m to its right); the layer entity name is the vault id, unique per door (inside a prefab set `m_sVaultId` and the breach `m_sTargetId`); bag inside: copy
`Roadmap_Bank_Bag`, `m_sId` = that id. The placement's numbers come from `bank_room`, never typed. Shop safe: `{B5A9BA64B3966CC1}Prefabs/Props/Commercial/LF_SafeBox.et`.

## Performance (docs/architecture/performance.md)
Report per pass: dynamic lights, shadow casters, decorative entities (limit in your brief). Short-range shadowless lights, a light
per room never per prop, emissive lamp covers, static props with LODs, no physics on clutter, no per-frame script on decoration.

## Ship
`tools\cli wt ship building-<slug> --refs <N> --body-file tmp\pr-body-<slug>.md`. Body: a before/after pair and a grade per
pass, the counts table, `## Manual testing`. No town layers, no `ws/town-maintainer-edits`; Saint-Philippe placement is
`town fit` later. Last line: `PR <url>, head <sha>, checks <state>`.

## Known traps
- `scenario.mjs`'s header says "-Z the street front". Every building's proof and look files use **front +z**. Follow +z.
- Wall items face the room: yaw 0 on a wall facing +z, 180 facing -z, +-90 facing +-x (trophy board: gun-store `decor.mjs` header).
- Vanilla ceiling lamps are dead meshes: add a `LightEntity` (ClipEV -10, NearPlane 0.3, short radius) and an emissive cover.
- A vanilla furniture set is big (town hall: 242 children): flatten to one child per prop, prune repeats; every entity counts.
- Your building is a pack prefab, so its cell is a pack cell: `grid place <slug> <pack prefab>` (or `--pack`) writes
  `LF_Everon1989/Worlds/Everon1989Grid/Everon1989Grid_Layers/Grid_<slug>.layer`, claims the cell in the shared lattice with
  `"pack": true`, and the layer is committed like any other. Never keep a layer untracked, never put a pack prefab in
  `GridWorld_Layers/` (`validate-addon-boundary`). A layer already written there: `grid free <slug>`, then `grid place ... --again`.
  `look grid:<slug>`, `audit placement --grid` and `session up --mission grid` load it through the pack grid world.
- A GUID is made once and kept (rule 2); typed coordinates only in a building-space row the script writes. `look` queues on the pool, never here.

## Environment
No Workbench on this PC and never pin a machine. Run in the foreground (timeout up to 600000 ms) or `t3_background_wait`;
never end the turn with a run pending. Never `--force`, `rm -rf` or `Remove-Item -Recurse` a worktree (`wt prune`).
