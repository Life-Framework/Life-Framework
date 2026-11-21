# Placement system

Maintainer, 2026-11-19, after playing Saint-Philippe: "flying cars, many flying lights, random cones around, For sale
signs going through doors. There is no point building out the roadmap until you sort out a system for proper object
placement." Island roadmap work is paused until the gate in this doc is on.

This doc has three parts: what is wrong today and why no check caught it, the one placement path and the audit and gate
built around it, and the cleanup cards.

## What is wrong today

Measured on `ws/placement-system` (base `80ea009f`), 2026-11-19:

- `tools\cli audit placement --world MainWorld` over every Saint-Philippe layer (`tmp/place-audit-sp/report.json`):
  **252 of 1068 layer rows flagged**, 36 rows not found in the world.
- The new runtime sweep, the same audit with `runtime` on (`tmp/place-audit-rt/report.json`): 51 runtime findings before this change (31 unattached night lights, 14 orphan loose props, 6 parked cars), 22 after it (2, 14, 6; no car floats or hangs off a kerb any more, the 6 left clip a fence, wall or kerb because of where the marker stands: card #3879).
- `look place:saint-philippe/{arrival,general-store,bakery,harbour,fuel}` at 14:00 and 22:00, plus three homes for sale
  (sp:b01, sp:b10, sp:b16) by `x,z --world island`.
- Maintainer play reports passed on by the lead (clothing store, hostel bed, hostel sign, price boards, general store, pub).

Pictures: `website/public/docs-images/placement-system/` (before set).

| Failure class | Count | Example | Source | Why no check caught it |
|---|---|---|---|---|
| Runtime car not on the ground (flying or parked on a planter) | 6 runtime rows (1 floating 0.24 m, 2 hanging off a pavement edge; 0 floating after this change) | a parked car standing on the shop-street planter beside the general store (look `xz-4519-10715`, `general-store` wide) | `LF_ParkedCarMarkerComponent.SpawnCar` puts the car at the marker's origin + 0.3 m with no ground probe; `tools/town/parked-cars.mjs` takes the marker's height from the **dossier** (`groundAt`: the nearest street prop within 18 m, else a building floor), never from the terrain (#3532/#3540) | The audit measured layer rows only. A marker has no mesh, so it was a "point" and skipped every check; the car it spawns was never in any list |
| Light hanging in the air | 31 `unattached-light` (2 after this change) | `Lamp_saint_philippe_*`: 8 of the 10 new vanilla lamps are "not found in the world" at their layer height; window glows at a fixed 2.1 / 5.3 m over one ground height per building | `tools/town/nightlights.mjs` (#3534): glow y = the building's dossier `y` + `FLOOR_Y`, 0.35 m off the footprint rectangle (not a real wall or window); lamp y = `groundNear`, the average height of nearby dossier props | `LF_WindowGlow` and `LF_LampHeadNight` have no mesh: the audit marks them `point` and `singleProblems` returns nothing for a point. Nothing asked "what is this light attached to" |
| Stray cones and clutter no slot owns | 14 `orphan` | maintainer: "random cones around" | Not in any island layer (every layer row is `Fit_` with a claim, `validate-island-zones`). Candidates: police roadblocks (`LF_Roadblock`, cones that expire only by timer), base-map Everon clutter in the town, props a proof left on the alpha server | The audit only reads layer rows; nothing swept the town for entities nobody owns. `RX.roadFurniture` (`Cone|Barrier|...`) also exempts cones from the road and contact checks |
| Sign or prop in a door's swing or walk-up | 23 rows (`blocks-door`) | `HomeSign_saint_philippe_sp_b44` 0.19 m into a door swing; `Fit_saint_philippe_dock_payphones_drop_b14` in the swing of a door of House_Town_E_2I02_v2; the hostel and police kits | `tools/town/homes.mjs` seats the For sale sign "on the entrance wall" through `tools/town/drops.mjs`, which checks wall contact, not the door leaf's swing; kit rows extracted with absolute offsets | The audit **found** these, but on the island every finding is a warning: `town-findings.mjs` treats any pack town layer as maintainer ground and never blocks. The pre-merge gate was removed (2026-11-15), and the main guard audits DebugWorld and GridWorld, never MainWorld |
| Prop floating or sunk | 12 floating + 11 sunk + 103 contact | `Fit_saint_philippe_furniture_furniture_bed_double` 1.19 m over the terrain; the woodlot lights 2.4 to 2.7 m up; the clinic bed sunk 8.8 cm | Kit rows seated on the dossier's floor height where the host has a raised floor or a slope; open-ground rows without `--terrain` | Same: warnings only on the island |
| Prop inside another prop or a wall | 88 clips + 34 overlaps + 37 overhang | the bank vault hanging 0.96 m off a container's edge and through the vault door; `ResidentPlate_saint_philippe_sp_b09` inside a kitchen wall over the sink (look `general-store`) | kits, the resident-plate generator (`town residenthomes`) placing on the footprint rectangle, not the real wall | Same; also buildings are exempt from clip checks by design, so a plate through a building wall is invisible to the audit |
| Prop on the wrong surface | look | the fuel pumps and a payphone booth (`drop_b97`) stand on the canopy or kiosk roof level instead of the forecourt; both pumps are "not found" by the audit | rows placed at "vanilla pump spots" with a height from the dossier; a `missing` row means the entity is not where its layer says | A `missing` row is printed, never judged |
| Action reach blocked by props | 1 (`unreachable`) + maintainer | the clothing store till cannot be used; props block the entrance and exit (fix: `town-draft-saint-philippe-clothing-fix`) | the clothing kit | `unreachable` only asks for a standing spot within 1.5 m, not whether a player can walk there from the door, nor whether the action's ray is blocked |
| Door, entrance and aisle clearance in shops | maintainer | the general store is so full of shelves you cannot walk it | the general-store and supermarket kits | No check for a walk lane from door to till (1.2 m wide) exists |
| Display stock that reads as empty | maintainer | general store tuna cans look empty | stock prefabs and slot points (`LF_StoreComponent`, `town shelf-stock`) | The audit measures stock rows for contact, not for what they show; no picture check |
| Player spawn or teleport height | maintainer | first spawn wakes in the hostel bed and stands up in the air above it (fix: `fix-bed-spawn-float`) | the spawn/wake-up code puts the character at the bed's height | No check measures a player spawn or teleport point; the audit never sees characters |
| Two spawners filling one spot | maintainer | two pub NPCs spawn inside each other | two NPC spawners (residents and a scene) with no shared occupancy | Characters were never measured |
| Signs and boards that look bad | maintainer | the hostel sign; "Prices today" board titles | sign defs and board layouts | Not placement: a `look` and `polish` item; listed so the cleanup cards cover it |

Totals, layer rows (audit by kind): contact 103, clips 88, overhang 37, overlaps 34, size 26, blocks-door 23, floating 12,
sunk 11, odd-scale 5, unreachable 1; 36 not found.

### Why our checks passed all of it

1. **The island audit is advisory.** `town fit` runs `audit placement --strict --world MainWorld --layer <its file>`, but
   `town-findings.mjs` makes every finding on a pack town layer a warning: the rule was written for the maintainer's
   hand-edited playtest town and was extended to the island, where no human edits anything.
2. **Generators skip the audit.** `town parked-cars`, `town homes`, `nightlights.mjs`, `town residenthomes` write layers
   from dossier numbers; most say "the audit judges the rest" and pass `--no-audit` or ignore its warnings.
3. **Heights come from the dossier, not the ground.** The dossier holds one `y` per building and per vanilla prop. Every
   generator that needs a ground height picks the nearest of those, which is wrong on a slope, on a raised floor or next to a
   tall prop.
4. **Runtime spawns are invisible.** Cars, characters, scene props and lights spawned or switched by script are never in
   the audit's list (store stock was added in #919; nothing else was).
5. **Meshless entities are skipped.** A `point` (no mesh bounds) row gets no problem at all; every light and every marker
   is a point.
6. **No gate.** The pre-merge gate is gone; the main guard audits DebugWorld (`--strict`, town findings warn) and GridWorld
   (`--grid --strict`), never MainWorld.

## The system

### Author in the grid, stamp into the world (the core)

Maintainer, 2026-11-19: "place all shops in the grid world first, and then apply them to the world as overrides." Every
shop, interior and fitted building is made this way. Nobody authors on the island.

```
grid cell (GridWorld)                    kit (building-local)                 island (MainWorld)
copy of the SAME vanilla building  ──>   tools/town/kits/<kit>.json     ──>   every mapped instance of that building
  look, walk, audit --grid --strict      rows: prefab, local transform,       Place_<place>_<slot>.layer (stamped rows)
  fix until clean                        attach target, clear lanes,          + island audit at that building
                                         clear list (vanilla clutter)          re-stamp = same bytes (idempotent)
```

1. **Author** (`tools\cli town author <kit> [--from <place>/<slot>]`): claims an authoring bay (a 2x2 at the east end of the grid, `docs/architecture/world-zones.md` "Authoring bays"; the extract below releases it, `town author --reopen <kit>` builds it again) and puts a copy of the island
   building's vanilla prefab in it (the dossier's `prefab` for the mapped building, the same `{GUID}path`). With `--from`
   it seeds the cell from the slot's kit so a broken island shop is rebuilt where it can be looked at and walked:
   `look grid:<slug>`, `look grid:<slug> --interior`, a session walk for lanes, `audit placement --grid --strict`.
2. **Store relative** (`tools\cli town kit extract Grid_<slug>.layer <group> --kit <kit> --origin building`): the rows
   are written in the building's own frame (local position, local yaw), with each row's attach target (`floor`, `wall`,
   `shelf:<row>`, `counter:<row>`, ...) and the kit's **clear lanes** (door to till, door to each shelf face, 1.2 m) and
   **clear list** (vanilla clutter the building prefab brings that this shop removes or treats as an obstacle). No
   world coordinate is in a kit (already true for `tools/town/kits/*.json`).
3. **Stamp** (`tools\cli town stamp <kit> [--place <place>]`): finds every island building the place mappings assign
   that kit (mapping slot -> dossier building id -> prefab + position, and the building actually standing there, found
   by prefab within 1 m), composes building transform x row transform, resolves each row's attach through the probe
   (the island floor or wall the row names, not the dossier height), writes `Place_<place>_<slot>.layer` and its claim,
   then runs `audit placement --world MainWorld --layer <that layer> --strict --floating` and refuses the stamp on a
   hard finding. The island audit is the stamp's verdict, not a warning.
4. **Idempotent, and the grid is the source**: stamping twice gives the same bytes (rows sorted by kit row id, numbers
   rounded to the mm). A kit change re-stamps every island copy (`town stamp <kit> --all`); `validate-island-zones`
   gains a check that a stamped layer equals what its kit stamps now (a stale copy fails, like `nightlights --check`).
5. **Vanilla clutter in the island building**: the building prefab's own furniture is the same in the grid copy, so the
   kit already accounts for it (obstacle, or on the clear list). Base-world clutter around or inside the island
   building that the prefab does not bring (the dock screenshot: crates and pallets of the base map in the yard) is
   found by the runtime sweep at stamp time and listed per slot; removal needs a base-world hide mechanism, which is a
   spike card (G2) because a sub-layer cannot delete a base-map entity today.

The floating-object reseat in this change is the same rule applied to what already exists: the night-light generator
now asks the island for the wall each glow hangs on (`tools/town/probes/lights-<place>.json`), and parked cars are
seated on the surface under them at spawn. This reseat (cars, lights, payphones) ships first, before any shop is
rebuilt; the two LF fuel pumps belong to the gas-station agent and are left alone here.

### Height frame: what a layer's y means

**Rule.** A top-level layer row's `coords` y is the height over the terrain under its x and z when the entity's flags carry
`EntityFlags.RELATIVE_Y` (0x80000), and a world height otherwise. The flags are the row's own `Flags` line, else the nearest
`Flags` up its prefab chain, else none. A row nested in another entity is in its parent's frame. This is the engine's rule (the
Workbench script that saves a position subtracts the terrain on that bit, `ViewOrientationTool.c`), not a class list: the layer's
class token (`SCR_DestructibleEntity`, `GenericEntity`) says nothing about it.

Which vanilla bases carry it (island audit of 2026-11-21, #3938): `Lamps_Base` 0x80403, `Wall_Base` 0x80003 (net and wooden
fences), `Building_Base` 0x80403 (cabins, the weighbridge, the payphone), `FuelStation_E_01_tankpistol_base` 0x80000 (the pumps),
`Tree` 0x80103. `DestructibleMultiPhase_Props_Base` 0x403 (benches, bins, flowerpots, sign boards) does not.

`tools/lib/height-frame.mjs` is the one implementation (`frameOf`, `layerY`, `annotateFrames`). Every writer plans in world
heights and converts at the point it writes the row: `town fit` (`framedRows`, ground heights from the slot's `--terrain` pins; a
row that needs one and has none is refused), `town stamp` and `town drops` (through the same render, refused when a terrain-frame
row has no probed ground), the night-light lamps (the probe's terrain `ty` beside each seat), `mapper import --slot` (the pivot is
probed too) and `place` (the town and grid terrains are flat at 0, where both frames agree). `validate-layer-height-frame` fails an
island row in the wrong frame: a terrain-frame y above 3 m, or a world-frame y under 3 m with the world-frame rows within 40 m at
least 5 m higher. The audit sends a terrain-frame row as `rel`, so it measures it at terrain + y.

### One placement path

Everything a player sees, layer row or runtime spawn, gets its transform from one resolver with a stated attach target.

```
LF_PlaceSpot.Resolve(prefab, wanted position, yaw, attach) -> transform or a logged refusal
```

| Attach | Probe | Rule |
|---|---|---|
| `ground` | trace down from 2 m over the wanted point (terrain and every physics layer) | the mesh bottom on the highest support under a 3x3 footprint (`LF_PlaceSnap`), upright unless the kind follows the slope (vehicles, banks: tilt to the 4-point normal, at most 12 deg) |
| `floor` | same, inside a building: the floor under the point | refuse when the support spread under the footprint is over 3 cm (it straddles a step) |
| `wall` | trace along -forward from 0.6 m out | back on the wall plane + 1 cm; refuse when no wall within 0.6 m |
| `pole` / `ceiling` | trace up or to the named host | the light or head sits on the host's top or under its ceiling; refuse when nothing within 0.3 m |
| `shelf` / `table` / `counter` | the host's top surface (`LF_PlaceSnap.Measure` on the host) | inside the host's top face, 2 cm margin |
| `vehicle` | the host vehicle's slot | as the game does |

Then the same clearance tests for every attach kind:

- **Footprint fit**: the oriented box inside its support (no overhang past 20% of a side).
- **Doors**: the box is out of every door leaf's swing (leaf pushed 1 m both ways, 2.2 m high) and out of a ground-floor
  door's 3 m walk-up; a sign is seated beside the door frame, never on the leaf's arc.
- **Walk lanes**: inside a shop a 1.2 m lane from each entrance to the till and to each shelf face stays free; a 0.9 m lane
  past every interactable.
- **Action reach**: an interactable has a standing spot within 1.5 m that is reachable from the entrance by the lane
  test, and the line from that spot to the action point is not blocked by another prop.
- **Road lanes**: out of a carriageway except road furniture and parked cars on the kerb side.
- **Other props and building shells**: no interpenetration beyond 8 cm; no ray clip into a building's walls
  (the current blind spot: buildings are exempt).
- **Characters**: a spawn or teleport point is a `floor` or `ground` spot for a 0.4 m capsule: feet within 5 cm of the
  floor, no other character within 0.5 m (the spot is reserved until the character leaves it), not inside a bed or chair
  mesh (a sleeping pose is a scripted animation, never a raised origin).

Offline (tools) and in game (script) share these numbers: `tools/place/limits.json` read by the Node side, mirrored as
constants in `LF_PlaceSpot` with a validate check that they agree.

**Writers.** Every island generator (`town fit`, `homes`, `parked-cars`, `nightlights`, `residenthomes`, `drops`) stops
computing heights from the dossier. It writes the wanted x/z, yaw and attach kind, and a server probe (the `ray`/`settle`
lines `tools\cli place` already uses) resolves the transform before the layer is written. Spawners call `LF_PlaceSpot`
at spawn time: a parked car is resolved on the ground under its marker, an NPC on the floor of its spot.

**Ownership.** Every entity a player can see in a town has an owner: a layer row with a claim (enforced by
`validate-island-zones`), or a runtime spawner that registers what it spawned (`LF_PlaceSpot.Note(entity, owner)`). The
sweep calls anything else in the town circle an orphan.

### The audit

`tools\cli audit placement --world MainWorld` (no `--layer`) now boots the island, lets the spawners run, and sweeps the
town circle (`runtime <x> <z> <r>` input line, `LF_PlacementAudit.RuntimeLines`, `agent-box/place-runtime.mjs`):

| Row | Measured | Fails on |
|---|---|---|
| `(runtime: parked cars)` | each marker's car, measured like a prop (bounds bottom against the surface under it) | `floating` over 15 cm, `sunk`, overlaps, `no-car` |
| `(runtime: vehicles)` | any other vehicle in the circle | the same |
| `(runtime: night lights)` | each `LF_NightLightComponent` owner: distance to the nearest surface up, down and on 16 horizontal directions (22.5 deg apart; six axes alone read a glow 0.33 m from a diagonal wall as 0.66 m) | `unattached-light`: nothing within 0.6 m |
| `(runtime: characters)` | each character's feet against the surface under it, and every pair | `char-floating` over 15 cm, `char-sunk` over 30 cm, `char-overlap` closer than 0.45 m |
| `(runtime: loose props)` | street clutter (`LOOSE_WORDS`: cones, barriers, pallets, barrels, crates, tyres) standing alone | `orphan`: no layer row of that prefab within 1.5 m |

`--layer` runs (what `town fit` does for its slot) skip the sweep unless `--runtime` is passed, so a slot is judged on its
own rows. Still to build (cards below): the shop walk-lane and action-reach checks, the building-shell clip for props,
judging `missing` rows, player spawn points.

### The gate (on since #3888; `docs/placement-audit.md` "The island gate")

- **Island findings block.** `town-findings.mjs` keeps the warning rule for the DebugWorld playtest town only; a finding
  on a MainWorld layer, or a runtime row, is hard for a branch that changed that layer, kit, marker, spawner or generator.
- **`wt ship` preflight** runs `audit placement --world MainWorld --strict --layer <changed island layers>` (seconds on a
  warm server) when the diff touches `MainWorld_Layers/`, `tools/town/kits/`, `tools/town/*.mjs` generators, a marker
  prefab or a spawner script (`placement-gate.mjs` triggers).
- **The main guard** runs the full island audit with the runtime sweep after every merge that touches the same set, and
  turns the head red on a new hard finding (a baseline file holds today's findings so the gate starts at zero new ones and
  the cleanup cards burn the baseline down).

### What we reuse, and what in it is broken

| Tool | Keep | Broken today |
|---|---|---|
| `tools\cli place on/against/ground` | the probe rays, `settle`, the layer writer | DebugWorld and grid only; island writers do not call it |
| `audit placement` | every measured check, the ranking, the allowlist | island findings are warnings; points are skipped; runtime spawns were invisible (fixed here for cars, lights, characters, loose props); `missing` rows are not judged; buildings exempt from clip so props through walls pass |
| `look contact` / `look place:` | pictures for every card | camera picks for some slots land in a tree (`arrival` wide) |
| `town fit` / `--check` / `seat.mjs` | seating kit rows on the host's floors and walls | runs the audit but ignores its result; open-ground rows need `--terrain` by hand |
| `validate-island-zones` | claims, footprints, branch guard (layer ownership) | nothing to fix |
| `parked-cars.mjs`, `nightlights.mjs`, `homes.mjs`, `residenthomes.mjs` | the site search | heights from the dossier; no door-swing test for signs; glows on the footprint rectangle, not a wall |

### How other projects place shop interiors

World-building research for the lead, 2026-11-19 (no code in this change):

- **Overthrow** puts a shop interior in the vanilla building's own furniture child prefab and overrides that prefab by its
  GUID. The vanilla `ShopHouse_E_2I01t_clothesstore.et` has the child `{74A94121A3F00C4F}` `..._furniture_clothesstore_01.et`;
  Overthrow ships the same GUID with a cashier and a counter in building-local coordinates. It finds shops by component
  (`QueryEntitiesBySphere` for its shop component) and has no shop rows in its sub-scene layers.
- **Bohemia slots and compositions** snap to terrain only: outdoor sites, never interiors.
- No API was found that lets a sub-scene delete a base-map entity (card #3882 stays a spike).
- The rule they share: **a layout stores no height.** It says where and what it sits on; the world supplies the height
  when it is applied. Shops are furnished lightly.

Adopted: grid-authored, building-relative kits stamped onto the island (above), plus an optional pack-only GUID override
of the furniture child for a shop type where every copy on the island can be the same shop.

### Lessons from the clothing store fix (#3901)

- **Prefab children are a blind spot.** The door-clearance test reads layer entities only, so furniture baked into a
  building prefab's children is never boxed. The same box test over the prefab's children flags all three props that
  blocked the clothing store. The audit runs the door and lane tests over prefab children too (card #3883).
- **Aim after stand.** `unreachable` asks only for a standing spot. An interactable also needs an `aim` check: what
  the F key acts on from that spot (the harness verb from #3901). The till's pay action sat on a mesh-less entity 1.2 m from
  the register, and a radio 0.45 m behind it took the F key. Static rules: no action context more than about 0.5 m from a
  mesh, and no other action within about 0.5 m of a till (card #3883).
- **Doubled furniture.** The island building keeps its own vanilla furniture, and the slot adds the pack's set on top, so
  Broom, Bucket, Chair, Stepladder and CoatRack exist twice. `town stamp` dedupes the kit against the building's own
  furniture (the clear list), or the kit overrides the furniture child by GUID as above (card #3881).

### Reforger Mapper

Studied in spike #3839 (`docs/architecture/reforger-mapper-import.md`; MIT, credited in `THIRD_PARTY.md`). What we take
from its placement model: **a layout carries no height**. Mapper's entities are x, z and yaw; its Workbench plugin sets y
from `GetTerrainSurfaceY` at apply time and lets a bank prefab follow the slope. That is the rule above: writers state
where and on what, the world answers how high. Mapper's apply plugin also reports what it built against what was sent;
our equivalent is the audit's `missing` count, which the gate will judge. No Mapper code is copied in this change.

### Rebuild notes

- **Authoring has its own bays.** `town author` no longer claims a permanent normal cell: it takes one of six 2x2 authoring bays and `town author extract` frees it (the kit and its archived source layer stay). Build in a bay, extract, stamp; to edit the shop again, `town author --reopen <kit>`. A building bigger than 2x2 does not fit a bay.

Written by the pilot rebuild (#3890, bakery) for R1 and R3 to R8. What was hard, and what to do instead:

- **Name the grid rows after the kit, not the seed.** `town author` seeds rows as `Kit_<kit>_<id>`; extracting them gives ids
  like `kit-bakery-bakery-till` and island names `Fit_..._kit_bakery_bakery_till`. Rename the cell's rows to `<Shop>_<id>`
  (`sed 's/ Kit_bakery_bakery_/ Bakery_/'`) and extract with the group `'<Shop>_*'`; the ids then stay `bakery-till`.
- **Settle in two passes.** `place settle` the floor pieces (counters, racks, the clerk) first, then what stands on them
  (till, goods). Every seeded row was 5 cm low on the grid copy's floor. A row `settle` cannot seat ("support varies 18 cm")
  straddles a step or a prefab child: move it or drop it, never force it.
- **The grid audit does not box the building shell; the island stamp does.** `audit placement --grid --strict` passed a rack
  whose back was inside the west wall; `town stamp` refused it (`clips House_Town_E_2I01`). Keep wall-backed furniture
  about 15 cm off the inner wall plane in the grid (the bakery rack moved from local x -5.69 to -5.54), or the stamp loop
  costs an island boot per try.
- **Furnish lightly.** The old bakery had three racks and 27 loaves; one rack clipped the wall and the loaves "hung off" each
  other on the island. The rebuild is two counters, the till, a store clerk (`LF_Npc_StoreClerk_1`, class
  `LF_NpcBaseEntity`), one rack with three loaves on one board and two loaves on the counter: one item per spot.
- **`--floating` reads the whole town.** The acceptance audit with `--floating` also lists runtime store stock of other
  shops (clothing trousers, supermarket dried meat); judge only the rows of your own layer, and leave the rest to their slot.
- **The island floor is not the grid floor.** Rows settled to 0.1 cm in the grid came out 2.8 cm sunk on the island
  (counters and clerk, the 1 cm contact sheet): the island b06 shell sits 2.8 cm higher relative to its origin than the grid
  copy. Under the hard limits, but the stamp still copies the grid height; the plan's floor probe at stamp time would
  close it.
- **Butcher (b20, orange): the island floor is 4.3 cm above the grid copy's, not 2.8.** The 1 cm contact sheet read -4.3 cm on counters, clerk and freezer. The kit rows (all but the fascia) carry +0.043 m, and the chiller +0.045 (a floor lip along the west wall: a second chiller at local z -0.1 found "support varies 14 cm" and was dropped). The grid source layer keeps the grid heights, so a `--reopen` re-extract loses the offset until the stamp probes the floor.
- **The interior look's camera stands in the hallway** of House_Town_E_2I01; only the fifth shot (the kit close-up) shows the
  shop room. Read that one.

- **Pharmacy shelves (#3893 follow-up): clear the host's empty shelf, stand yours in its place.** `stamp.clear` hides a prefab by path across the whole building, so the kit's own shelf must be a different prefab from the one cleared (the pharmacy clears `ShelfShop_01` and stands `ShelfUniversal_01_wide`). The grid audit does not apply the clear carrier: in the bay the new shelf reads "sunk 0.43 m into GenericEntity" (the host gondola it replaces), and `place settle` rests on that gondola (+7 cm). Put the rows at the kit height by hand, add a `Grid_author_<kit>_clear` carrier row beside the rows for the picture, and let the island audit after `town stamp` judge. A `--reopen` bay carries no component blocks (the till lost its `LF_ShopComponent`): re-extract, then copy the old source layer's `components` blocks back, and restore the hand-tuned `anchoring`, `wall` and `role` of the old rows. Kit rows sit 3.5 cm above the bay's floor (the island lift): take 3.5 cm off every row before extracting and add it back after.

## Cleanup cards

Filed 2026-11-19 under epic #3877, in dispatch order. Floating-object reseat first (F), then the stamp path (G) and the
audit checks (A), then shop rebuilds through the grid (R) and the gate (C1) once the F cards burn the floating rows down.

| Card | Key | Title | Depends |
|---|---|---|---|
| #3878 | F1 | Payphone booths stand on the street, not in the air or on a roof | none |
| #3879 | F2 | Parked-car markers sited on the kerb by probe, not by the survey | none |
| #3880 | F3 | Floating street props: benches, woodlot lights, pallets, racks | none |
| #3881 | G1 | town author / town stamp: shops made in the grid, stamped onto the island | none |
| #3882 | G2 | Spike: hide base-map clutter inside or around a stamped building | none |
| #3883 | A1 | Audit: shop walk lanes, action reach, room overcrowding | none |
| #3884 | A2 | Audit: props through interior walls, rows not where their layer says | none |
| #3885 | S1 | NPC spawns reserve their spot and stand on the floor | none |
| #3886 | L1 | For sale signs clear the door swing | none |
| #3887 | L2 | Resident plates on the real outside wall | none |
| #3888 | C1 | Gate: island findings block ship and the main guard | F1, F2, F3 |
| #3889 | R1 | Rebuild police: grid first, then stamp | G1 |
| #3890 | R2 | Rebuild bakery, butcher: grid first, then stamp | G1 |
| #3891 | R3 | Rebuild furniture, hardware: grid first, then stamp | G1 |
| #3892 | R4 | Rebuild seed-feed, farm-supply, farm: grid first, then stamp | G1 |
| #3893 | R5 | Rebuild pharmacy, newsagent, barber: grid first, then stamp | G1 |
| #3894 | R6 | Rebuild clinic, chapel: grid first, then stamp | G1 |
| #3895 | R7 | Rebuild the dock wholesale yard: grid first, then stamp | G1, G2 |
| #3896 | R8 | Re-stamp tonight's hand-fixed shops through the grid | G1 |

### Clinic rebuild notes (#3894)

- **Read the host's floor plan before placing furniture.** `town fit <place> <slot> --check` reports what each row cuts and what vanilla furniture it overlaps, offline in seconds; a scratch rasteriser over `shellReader(root).shell(<prefab>)` (`walls`, `kids`) shows the ground-floor walls and the host's own furniture. The clinic's first beds crossed an inner wall at host-local z 1.3 and the grid audit never saw it; only the island stamp did.
- **The building's own furniture is a second set.** The island building keeps its vanilla furniture child (`House_Town_E_2I02_furniture_01`) under the pack's `Clinic_furniture_01` row. A kit's `stamp.clear` hides vanilla-only prefabs by path inside the building's box; a prefab that both sets contain cannot be cleared without hiding the pack's copy too (it stays and the row moves instead). The stamp refuses a row that clips either set.
- **A bed's respawn point needs 0.6 m of floor beside the mattress** (`LF_BedWakePose.BodyFitsBy`; the stand offset is 0.85 m, the sit search takes 0.4 m off the rail). The pack's infusion stand and the other bed both count as blockers. The clinic's beds sit 1.0 m apart (rail to rail) and the stand is cleared.
- **Wall rows in the grid copy.** The clinic's fascia and electricity box sat 1 m off the facade in the old kit; `audit placement --grid` read them as floating until they were backed onto the ground-floor facade plane (host-local z 5.8). The island stamp seats wall rows itself; the fascia still warns "no host wall within 6 m" at height (follow-up).
