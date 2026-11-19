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
   `mapper-<file name>` unless `--slug`. Then `place ground x,z <ref> --layer Grid_<slug>.layer --yaw a --name ...
   --y-hint 0 --no-verify` per prop (offline overlap, carriageway, door-swing and scale checks, the ground probe), one
   `place settle` for the lot and `audit placement --grid --strict --layer Grid_<slug>.layer`. A prop the checks refuse is
   reported and the exit is 1; the others stay written. If nothing was placed the claim is freed again. A second import
   of the same slug is refused until `grid free <slug>`.
5. **Slot mode.** `--slot <Layer.layer>` writes into a layer of the **playtest town** (DebugWorld and Everon1989Town
   layers, the ones `place` reaches), only on a `ws/town-*` branch (`world-zones.md` "The guard"), only with `--at x,z`
   (the tool never invents a town coordinate), then `place verify` on what it wrote. A `Grid_` layer, an unknown layer and an
   island slot layer (`MainWorld_Layers/Place_*`) are refused with a message that says which.
6. `--dry` prints the plan and writes nothing; it needs no pool machine.

Time: each prop is one test-server boot for its ground probe (about 25 s with the queue), plus one for the settle and
about 35 s for the audit. The spike's 4 props took about 3.5 minutes; a 30-prop layout is a quarter of an hour. Batching
the probes of all props into one boot is the obvious speed-up (`groundPlans` in `tools/place/place.mjs` already takes a
list; `place ground` exposes one).

## Sample and proof

`tools/test/fixtures/mapper/junkyard-small.json` is shaped like Mapper's junkyard module output: one wall shape, two
wrecks, a pallet pile, a cardboard pile and an oil-stain decal. Run on 2026-11-19 (cell `c13r1`, slug
`mapper-junkyard-small`): 4 props placed, the decal dropped (no mesh), `place settle` skipped the two wrecks as vehicles
and moved the two piles by under 1 mm, `audit placement --grid --strict` flagged 0 of 85 entities. The layer is committed
as `Grid_mapper_junkyard_small.layer`; `tools\cli look grid:mapper-junkyard-small` made the picture in the PR.
`tools/validation/mapper-selftest.mjs` covers the converter with fakes (fixture to calls, unknown prefab dropped, a quoted
vector impossible, slot guards).

**The fixture is hand-written, not exported by running Mapper.** The format is small and documented, the junkyard module
is TypeScript behind a map UI, and the card allowed either. A real export, and a check that Mapper's yaw turns props the
same way `angleY` does in a shot, are still open (both writers use the Enfusion yaw, so it should).

## Not done

- **Island slots.** `place` only reaches the playtest town's layers; the island's `Place_<place>_<slot>.layer` files
  (`MainWorld_Layers`) are written by `town fit`, with claims, a host building and a place marker. Importing into an
  island slot needs a `town fit`-style writer for loose props: a follow-up card. Slot mode is checked by guards and `--dry`
  only; no `--slot` run was made in a `town-` worktree.
- **Shapes as props.** Walls and fences (`WG_*`) and hedge lines (`PG_*`) could become rows of our fence and hedge
  prefabs along the polyline; today they are reported and skipped.
- **Rotating a layout** to face a cell's gap, or fitting a layout bigger than the free block, are not handled: the layout
  keeps Mapper's orientation and `--span` or a larger cell is the answer.
- **Batching** the per-prop probes (above).

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
