# Reforger Mapper import (spike #3839)

Spike, 2026-11-19. Maintainer ask: use layouts sketched in Reforger Mapper as input to **our** pipeline, never straight
into the island. Result: `tools\cli mapper import <project.json>` turns the placed-prefab part of a Mapper project into one
debug-grid cell through the same writer and checks as `tools\cli place` and `grid place`. The format notes below are
what an importer needs; the code is ours (`tools/mapper/convert.mjs`, `tools/commands/mapper.mjs`).

**Credit and licence.** Reforger Mapper, https://github.com/benjibossk/mappingenfusiontest, MIT, (c) 2026 Benji
(notice kept in `tools/test/fixtures/mapper/LICENSE-Mapper.txt`, entry in `THIRD_PARTY.md`). The format was read from its
published docs and sources (`docs/ARCHITECTURE.md`, `docs/CONTRACT.md`, `docs/ENFUSION_FORMATS.md`, `src/project.ts`,
`workbench/Mapgenerator/.../MG_ApplyProjectPlugin.c`); no code of Mapper's is copied. We made no issue, PR, star or message
there; any outreach is the maintainer's call.

## What Mapper is

A Vite + TypeScript browser tool that sketches maps and site layouts (forests, junkyard, orchard, pasture, parking, camp,
house lots, roads) and sends them to Workbench: browser -> a small Node bridge (`POST /apply`, port 5780) -> a Workbench
NET API plugin (`MG_ApplyProject`) that creates the shapes, generators and prefabs in the open world. Everything goes
through one JSON file, the **project** ("pivot format", `mapgen.json`, `version: 0`). It uses vanilla assets only.

## The project JSON (version 0)

Source of truth: `src/project.ts` in Mapper, mirrored by `MG_Project` / `MG_ProjectEntity` / `MG_ProjectShape` in the plugin.

```json
{
  "version": 0,
  "shapes":   [ { "id": "JY1_wall", "kind": "polyline", "closed": true, "points": [x0, z0, x1, z1, "..."],
                  "generator": "{GUID}path.et", "paramKeys": ["..."], "paramValues": ["..."], "lift": [0], "avoid": {} } ],
  "entities": [ { "id": "JY1_wreck_1", "prefab": "{GUID}path.et", "x": 4120.5, "z": 2207.75, "yaw": 90, "ycm": 0, "role": "clutter" } ],
  "remove":   [ x, z, "..." ],
  "ffBiome":  1
}
```

### Placed prefabs: `entities[]`

| Field | Meaning | Notes for an importer |
|---|---|---|
| `id` | name of the placed entity (a string, any text) | Not unique-checked. We do not reuse it: layer names are `Grid_<slug>_M<n>`, and the id is only printed in reports |
| `prefab` | `"{GUID}path.et"` or a bare `path.et` | Bare is resolved by the plugin through `Workbench.GetResourceName`; a wrong or missing GUID is a mistake the plugin cannot see. We resolve every ref ourselves, take the game's GUID for the path and report a wrong one. Some paths are Mapper's own library (`PrefabLibrary/DefaultLibrary/...`: decals, generators) and have no mesh |
| `x`, `z` | world X/Z in metres, flat map coordinates | **There is no Y.** The plugin sets Y from `GetTerrainSurfaceY(x, z)` |
| `yaw` | integer degrees, Enfusion yaw | The plugin writes `angles "0 <yaw> 0"` (banks, ids containing `_BANK_`, follow the slope instead) |
| `ycm` | absolute height in cm; absent or 0 = on the terrain | Used for bridges only. We ignore it and report it: props stand on the ground the placement tool probes |
| `role` | `essential` / `pc` / `clutter`, optional | Mapper's own budget tag; we ignore it |

What is **not** in the format: no **scale** (always 1), no pitch or roll, no per-entity **layer** (the plugin puts every
entity in one layer, `MG_RoadSigns`), no parent/child, no component overrides. Everything is a prefab reference with a
yaw. So the import loses nothing a layer entry of a plain prop would carry, but a layout cannot say "this one is bigger".

### Terrain and shape only: not imported

- `shapes[]`: polylines and splines with a **generator** prefab (`FG_Forest_*` forests, `PG_*` hedge/bushline prefab
  generators, `LG_Lake_*` lakes, `R_River*` rivers, `RG_Road_*` roads, `WG_*` wall/fence generators). They are lines of
  X/Z points (`points` flat pairs, `closed`, `kind`), generator parameters (`paramKeys`/`paramValues`), a water `lift`
  and an `avoid` set (`m_bAvoid*`). A generator fills them when Workbench runs; a layer has no such entity as a plain
  prop, and the objects a generator makes are baked into the `.layer` only by Workbench. The importer counts shapes and
  names their families in a note.
- `remove[]`: X/Z origins of objects in the Mapper world to delete. We never delete; ignored and reported.
- `ffBiome`: a Freedom Fighters flag; ignored.
- Outside the project file: heightmap `.asc`, surface-mask PNGs (`POST /masks`), the terrain, and the **contract v1**
  (`docs/CONTRACT.md`: heightmap, water, roads, zones in; masks, vegetation polygons, a budget report out). That
  contract is a separate ground-engine format, not the project; we use none of it.

## How a project maps to our `.layer`

| Mapper | Our layer entry | Where it comes from |
|---|---|---|
| `prefab` | `<Class> <Name> : "{GUID}path.et" {` | `resolvePrefab` (addons, then the paks) gives the real `{GUID}path`; the class is the prefab root's, read by `place` |
| `x`, `z` | `coords x y z`, x/z only | The layout's bounding-box centre moves onto the target (a cell's centre, or `--at`): **translation only**, no rotation, no scaling |
| (none) | `coords` y | `place ground`'s probe rays (`--y-hint 0`), then `place settle` (1 mm over the support). Vehicles and wrecks keep the probed height, as the settle step skips them |
| `yaw` | `angleY a` | Same Enfusion yaw, normalised to -180..180; `place ground --yaw a` |
| `id` | entity name `Grid_<slug>_M<n>` | Names must be unique in the world, Mapper's are not |
| `scale` | no `scale` line | Mapper has none |
| layer | `Grid_<slug>.layer` (GridWorld, or the pack grid when a prefab is pack content) | One layer, claimed through `grid place` |

Vectors are written by the placement tool's `appendEntity`, bare (`coords 183.963 0.029 37.514`), never quoted
(`validate-quoted-vectors`). The importer passes `x,z` text and a yaw to `place ground`; it writes no vector itself.

## What the command does

```
tools\cli mapper import <project.json> [--cell cXrY] [--span WxH] [--slug s] [--dry] [--no-verify] [--no-fetch]
tools\cli mapper import <project.json> --slot <Layer.layer> --at x,z [--dry]        (a ws/town-* branch only)
tools\cli mapper import <project.json> --slot Place_<place>_<slot>.layer [--at x,z] [--dry] [--keep]   (an island slot; a ws/town-draft-<place>-<slot>[-word] branch)
```

1. Parse. A project that is not JSON, not `version: 0`, or has no `entities` (shapes only) is refused with the reason.
   Bad rows (no prefab, no number for x/z) are dropped and listed.
2. **Resolve every prefab** against `addons/` and the game's paks. An unknown prefab, or one with no mesh the placement
   tool can measure (a decal), is **dropped and reported**, never written. A project whose every prefab is dropped
   places nothing and exits 1.
3. Size: the layout's extent plus the grid's 4 m walk-around margin gives the cells it needs (`spanFor`, the same as
   `grid place`); `--span` overrides.
4. **Cell mode (default).** `grid place <slug> [--cell] --span --no-verify` claims the cell (yard fence, label sign, claim
   file, world-zone checks, the claim clash checks against `origin/main`, open PRs and sibling worktrees); the slug is
   `mapper-<file name>` unless `--slug`. Then ONE `place ground-many <plan.json> --layer Grid_<slug>.layer --no-verify --y-hint 0`
   for every prop (the plan is a JSON array of `{ at: [x, z], prefab, yaw, name }`: the offline overlap, carriageway, door-swing
   and scale checks, one ground probe boot and one write; all or nothing, and a refused batch falls back to `place ground` per
   prop to name the refused one and keep the others), one `place settle` for the lot and
   `audit placement --grid --strict --layer Grid_<slug>.layer`. A prop the checks refuse is reported and the exit is 1;
   the others stay written. If nothing was placed the claim is freed again. A second import of the same slug is refused until
   `grid free <slug>`.
5. **Slot mode, playtest town.** `--slot <Layer.layer>` writes into a layer of the **playtest town** (DebugWorld and Everon1989Town
   layers, the ones `place` reaches), only on a `ws/town-*` branch (`world-zones.md` "The guard"), only with `--at x,z`
   (the tool never invents a town coordinate), then `place verify` on what it wrote. A `Grid_` layer and an unknown layer
   are refused with a message that says which.
6. **Slot mode, island** (#3851, `tools/mapper/island.mjs`). `--slot Place_<place>_<slot>.layer` (`MainWorld_Layers`) adds the props to an
   island slot that `town fit` already made (it needs its claim in `tools/town/claims/`), only on a
   `ws/town-draft-<place>-<slot>[-<word>]` branch (the zone guard). `--at` is optional: the default is the slot's ground
   centre (the plot's or the building's, from the dossier). Every prop's footprint must stand inside that ground plus the slot's
   margin. ONE test-server boot on the island probes the terrain under every footprint (the probe `town fit --terrain` uses); the
   props stand on it by the place tool's own level-ground rule (a prop on a slope is refused); overlap with the slot's entities
   and the batch is refused; the entities are appended as `Mapper_<place>_<slot>_M<n>`; the zone check `validate-island-zones`
   runs is applied to the written layer; `audit placement --strict --world MainWorld --layer` judges it. A flagged layer is
   restored byte for byte (`--keep` leaves it); a layer that holds an import already is refused. A later
   `town fit <place> <slot> --refit` rewrites the layer from the kit and drops the props: import again after it.
7. `--dry` prints the plan and writes nothing; it needs no pool machine.

Time: three test-server boots for a whole cell import however many props (one `ground-many` probe, one settle, one audit,
about 2 minutes with the queue); the spike's per-prop `place ground` took 3.5 minutes for 4 props and a quarter of an hour
for 30. An island slot is two boots: one terrain probe and the audit.

## Sample and proof

`tools/test/fixtures/mapper/junkyard-small.json` is shaped like Mapper's junkyard module output: one wall shape, two
wrecks, a pallet pile, a cardboard pile and an oil-stain decal. Run on 2026-11-19 (cell `c13r1`, slug
`mapper-junkyard-small`): 4 props placed, the decal dropped (no mesh), `place settle` skipped the two wrecks as vehicles
and moved the two piles by under 1 mm, `audit placement --grid --strict` flagged 0 of 85 entities. The layer is committed
as `Grid_mapper_junkyard_small.layer`; `tools\cli look grid:mapper-junkyard-small` made the picture in the PR.
`tools/validation/mapper-selftest.mjs` covers the converter with fakes (fixture to calls, unknown prefab dropped, a quoted
vector impossible, slot guards).

**The fixtures are hand-written, not exported by running Mapper.** The format is small and documented, the junkyard module
is TypeScript behind a map UI, and the card allowed either. A real export is still open.

**Island slot proof (#3851, 2026-11-19).** `tools/test/fixtures/mapper/lakeshore-small.json` (a street bench at yaw 270, a bucket, a crate
and a barrel) went into `blacklake/shore` from `town-draft-blacklake-shore-mapper`: `mapper import ... --slot Place_blacklake_shore.layer`,
no `--at` (the plot's centre), 4 props placed, one island terrain probe plus the audit (2 boots), the strict island audit flagged none of
them. The four `Mapper_blacklake_shore_M<n>` entities are committed in that slot's layer. **Yaw:** the bench at Mapper yaw 270 is
written as `angleY -90` and stands facing west, towards the lake, in the `look` close-up (`from-east` and `bench-yaw` in the PR): our
`angleY` is the Enfusion yaw Mapper's plugin writes (`angles "0 <yaw> 0"`, read from its source), 0 north, 90 east. That is what the shot
proves on our side; Mapper's own export was not run.

## Not done

- **Island slots on a building host.** The writer is done and proven on a hostless plot (`blacklake/shore`); a slot with a host building keeps its kit's interior: a layout there is checked only against the building's footprint plus margin, so the import fits outdoor dressing, not furnishing.
- **Shapes as props.** Walls and fences (`WG_*`) and hedge lines (`PG_*`) could become rows of our fence and hedge
  prefabs along the polyline; today they are reported and skipped.
- **Rotating a layout** to face a cell's gap, or fitting a layout bigger than the free block, are not handled: the layout
  keeps Mapper's orientation and `--span` or a larger cell is the answer.

## Wish list for a "gameplay markers" export (upstream suggestion, for later)

Mapper's entities are dressing: a prefab, a point, a yaw. Our systems do not find things by prefab but by **place**:
`LF_PlaceComponent` registers any entity in the `LF_Places` registry under a **kind** and an **id**
(`docs/architecture/core-vs-pack.md` "Map independence"), and a system asks `LF_Places.Find(kind, id)`. For a Mapper
layout to drive a feature (a junkyard that is a scrapping workplace, an orchard that is a farm plot), an export would
need to carry, per marker:

| Field | Why | Example |
|---|---|---|
| `kind` | what the place is, as the system names it | `Workplace`, `CashVanStop`, `BankVault`, `JailCell`, `HideoutDoor` |
| `id` | which one; stable, because a save keys on it | `scrapyard`, `route-1` |
| `order` | position in a series that shares an id (stops of a route), -1 for none | `0`, `1`, `2` |
| `liveState` | map state the place belongs to, -1 for always | `-1` |
| the entity it sits on | a marker is a component on a prop, not a prop itself | a `markerPrefab` ref, or an `onEntity` id from the same project |
| `facing` | which way a counter, door or customer spot faces | `yaw` (already there) |
| extents | an area marker is a polygon (a safe zone, a pasture, a workplace footprint), not a point | the `points` of a closed shape |

With those, our importer would add an `LF_PlaceComponent` (kind, id, order, live state) to the placed entity's
component block instead of registering nothing. The kinds are ours and per feature (a system lists them in its catalog
`m_aPlaces`), so the useful upstream ask is generic: "a marker entity with a free-text `kind`, `id` and optional `order`,
plus an area marker made of a closed shape", not our kind names. Mapper already has the pieces (`role`, shape `id`s, the
junkyard `plan` of labelled cells: lane, wrecks, shed, office, tyres, scrap); exporting that plan's labels as markers would
be the first step.
