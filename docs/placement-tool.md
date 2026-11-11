# Placement tool

`tools\cli place` puts a world prop where it belongs. You name a surface, a wall or a spot; the tool
works out `coords`, `angleY` and the layer entry from the real mesh bounds and from rays traced on the
dedicated test server, checks the result, writes the layer, and then runs the placement audit
(`docs/placement-audit.md`) on what it wrote. A placement the audit flags is rolled back.

Do not type coordinates into a `.layer` for a prop. Every failure in the table below came from a
typed number that nobody could see.

```
tools\cli place on <surface> <prefab> --layer L.layer [--name N] [--at center|north|south-east|u,v] [--near x,z] [--yaw deg | --facing north|deg]
tools\cli place against <target> <prefab> --layer L.layer (--near x,z | --face north [--along 0..1]) [--hang m] [--front +z|-z|+x|-x] [--allow-uneven]
tools\cli place ground <x,z> <prefab> --layer L.layer [--yaw deg | --facing north|deg | --face-to x,z] [--allow-road]
tools\cli place arrange on <surface> <prefab>... --layer L.layer [--count n] [--spacing m] [--at north]
tools\cli place arrange row <x1,z1> <x2,z2> <prefab>... --layer L.layer [--yaw deg]
tools\cli place bounds <prefab|entity>      size, base, scale, rigid body; no server
tools\cli place probe <surface>             trace a surface's real top into tools/place/surface-cache.json
tools\cli place verify <entity>...          the placement audit on those entities only
```

- `--move <Entity>[,<Entity>...]` in place of `<prefab>` re-places entities a layer already has. Only
  their `coords` and `angleY` lines change.
- `--in Prefabs/X.et` works on a prefab's own children (the surface is `root`): `place on root --move
  DutyClock --in Prefabs/Police/LF_PoliceDutyDesk.et`. This is offline; `validate-prop-on-surface`
  covers the result.
- `<prefab>` is a full `{GUID}Prefabs/X.et`, a path, or a file name the addon already references. The
  tool never invents a GUID: it reads the prefab's `.meta` or an existing reference, and refuses a
  prefab it cannot resolve.
- An unnamed entity is addressed by its prefab file name and a point: `LampStreet_E_01_6m@196,255.4`.
- `--dry` prints the plan and writes nothing. `--no-verify` skips the audit boot (run `place verify`
  before you ship). `--keep` leaves a flagged placement in the file. `--force` writes despite an
  offline finding. `--scale s` sets a scale; outside 0.5 to 2 is refused.

Run it from a worktree. A probe or an audit is one boot of the test server, about 11 s, queued on the
test lane like `tools\cli test`; no session, no client.

## Examples (the props re-placed in the PR that added the tool)

```
tools\cli place on Shop_GeneralStore_Debug --move Shop_GeneralStore_Till
  move Shop_GeneralStore_Till -> coords 179.056 1.039 244.276 angleY 0
       top 1.039 m from probe; footprint 0.6 x 0.51 m in 1.44 x 0.9 m
  audit Shop_GeneralStore_Till: clean (on Shop_GeneralStore_Debug, gap -0.002 m, backed)

tools\cli place against Villa_E_2I01_Police --move Roadmap_Police_ArmoryStore --near 231,239.5
  probing the 0 deg face of Villa_E_2I01_Police from 231, 239.5 (127 rays, one test-server boot)
  move Roadmap_Police_ArmoryStore -> coords 231 0 238.184 angleY 0
       face 1.586 m from the start, floor 0 m
  audit Roadmap_Police_ArmoryStore: clean (on terrain, gap -0.001 m, backed)

tools\cli place ground 241.4,241.3 --move Police_TrashBin_1
  move Police_TrashBin_1 -> coords 241.411 0 241.28 angleY 0
       ground 0 m (0 m across the footprint); standing on DebugTerrain

tools\cli place on Roadmap_Org_Desk --move Roadmap_Org_Desk_Clock --near 287.6,255.95
tools\cli place on Farm_StandTable --move Farm_StandCabbage_1
```

And what it refuses:

```
tools\cli place ground 239.9,242.05 --move Police_TrashBin_1
  PROBLEM overlaps Roadmap_Police_Payphone [Street_Police.layer] by 1.02 m
  PROBLEM stands in the swing of a door of Roadmap_Police_Payphone (door at 239.89, 242.52)
tools\cli place ground 230,250 TrashBin_02 --layer Street_Police.layer
  PROBLEM stands on the carriageway of Village_MainStreet (0 m from its centre line, 7 m wide). Move it to the verge, 3.5 m or more further out
tools\cli place arrange on Farm_StandCrate_1 Cabbage --count 4 --layer FarmingChain.layer
  place: they do not fit: 4 side by side need 3.5 x 0.91 m and the top has 0.55 x 0.35 m
```

## The four primitives

| Command | What it solves | Where the numbers come from |
|---|---|---|
| `on` | The prop's mesh base rests on the surface's working top and its whole footprint stays inside the edges (3 cm inset). `--at` picks a side or corner by compass word or `u,v` in -1..1; `--near x,z` clamps a world point to the nearest spot that fits. The prop turns with the surface; `--yaw` is relative to it. | Prop and surface bounds from the `.xob` header. The top from a grid of rays traced down onto the surface (`place probe`, cached per mesh): the most common height is the working top and the cells at that height are the usable rectangle, so a counter's lower shelf or a workbench's raised back is outside it. |
| `against` | The prop's back face stands 2 cm off the face, its front points away from it, its base is on the floor in front (or `--hang m` above it). | Horizontal rays from the start line toward the face at every 0.15 m of height across the prop's width, and rays down along the centre line for the floor. The face is what the rays hit behind the prop at its own height; the rays pass through the prop being moved. |
| `ground` | The mesh base (not the pivot) stands on the ground at `x,z`, which is where the footprint centre goes. | Rays down under the centre and four corners; the higher of the hit and the terrain height. More than 0.25 m between them is refused (a slope, a kerb, something standing there). |
| `arrange on` / `arrange row` | Several props side by side on one top, centred, 5 cm apart, none over an edge; a prop too deep for the top is turned. `row` spaces props evenly between two ground points. | The same top as `on`; the same ground rays as `ground`. |

## Checks before the write

Each command refuses, with the reason and what to change:

- **does not fit**: the footprint is larger than the usable top (the cabbages wider than their crate).
- **not level under the footprint**: a lip, a raised back, a hole, a kerb, a step.
- **the face is not flat behind the prop**: a window, a door, a plinth or a post behind part of it;
  move along the face or pass `--allow-uneven` to stand off the nearest part. Rays that reach nothing
  are an opening.
- **something stands between the start and the face**: most rays stop early; start nearer.
- **overlaps `<entity>`**: oriented mesh boxes interpenetrate by more than 8 cm (the bin in the
  payphone booth, the cabinet through the till, the price sign in a car). A prop resting on top of
  another, or the contents of a shelf or crate, is not an overlap.
- **on the carriageway**: the centre or a footprint corner is on a road of `Village_Roads.layer`
  (under 15 m wide; the market, square and fuel apron are not roads). Vehicles and road furniture are
  exempt; `--allow-road` for anything else that is meant to be there.
- **in the swing of a door / in the walk-up to a door**: the box is inside a door leaf's clearance
  (1 m both ways) or within 3 m in front of a ground-floor door, outside that door's own building. The
  door leaves come from the last server boot (`tmp/place/doors.json`), so the first command in a fresh
  worktree checks doors only in its audit.
- **scale outside 0.5 to 2**: use a prefab of the right size.
- **a simulated rigid body taller than twice its base on a raised surface**: it tips when a client
  simulates it (the logs on the logging stand). Any simulated body prints a note to confirm it in a
  shot.

## Checks after the write

The tool boots the test server once more and reads the placement audit's row for each entity it wrote.
It fails, and restores the file, when:

- the audit ranks any problem for the entity, or a problem of another entity names it;
- the base is more than 5 cm above or below what it rests on (the audit's own limit is 15 cm);
- a hung prop has nothing within 0.3 m behind it;
- the engine reads its facing differently from the yaw that was written;
- the entity is not in the world at that position (the prefab reference did not load).

`wt gate` runs `audit placement --strict` on any branch that changed a layer, so a hand-typed number
that slips through still meets the audit there.

## What the tool writes

- A moved entity: its `coords` line and its `angleY` line (added after `coords` when it had none and
  the yaw is not 0). No other byte of the layer changes. A child's coords are written in its parent's
  frame.
- A new entity: appended at the end of the layer in the form Workbench writes
  (`Class Name : "{GUID}path" {`, ` coords x y z`, ` angleY a`, ` scale s`, `}`), millimetre precision,
  LF line ends. The class comes from the prefab's root. A world interactable still runs
  `node tools/actions/place-action-contexts.mjs` afterwards.
- `tools/place/surface-cache.json`: one line per surface mesh (the probed grid, the entity it was
  probed on, the date), sorted by mesh path, so two branches that each probed a surface merge as two
  added lines. `place probe <surface>` refreshes an entry after a mesh changes.

## Failure classes (2026-11 trailer review, PRs #740 to #743)

| Failure | Root cause | Caught by |
|---|---|---|
| Till 0.34 m off its counter | bounds: footprint over the surface edge | `on` clamps the footprint inside the usable top |
| Punch clock 0.4 m above a desk | surface height: the mesh base is not at the pivot | `on` rests the mesh base on the probed top |
| Radio sunk into a desk | surface height | `on` |
| Locker and armory half inside a wall | wall face: the wall was assumed where the building's box is | `against` traces the face |
| Vault door 1 m off the wall | wall face | `against` (a door leaf is hinged at its pivot; check the swing in a shot) |
| Notice boards in mid-air | wall face, hung | `against --hang`; the audit fails a hung prop with nothing behind it |
| Price signs inside cars | composition: overlap | offline overlap check |
| Bin inside a payphone booth | composition: overlap, door blocking | offline overlap and door-swing checks |
| Cabinet through a till | composition: overlap | offline overlap check |
| Cabbages wider than their crate | bounds and scale | `arrange on` refuses "do not fit" |
| Tree on asphalt, junk across a road | path | carriageway check in `ground` and `arrange row` |
| Dirt lane under the beds | path | only when the lane is a road in `Village_Roads.layer`; a decal or terrain paint is not measured |
| Physics logs falling over | physics | tall simulated bodies on a raised surface are refused; every simulated body gets a note |
| Spawned items at the wrong size (#740) | scale lost at spawn | not a layer placement: `LF_Utils.PrefabScale` and its tests; `place bounds` prints a prefab's root scale |
| Lamp behind a strongbox | composition: what the camera sees | not measurable; shots |

## Still needs a shot or a human

- **Facing.** `against` and `--facing` take the prop's front as local `+z`. A mesh whose front is
  another axis needs `--front`; the tool cannot tell which side of a cabinet has the doors.
- **Looks.** The wrong asset, a prop that reads odd at a legal scale, a scene that is cluttered or
  blocks the view of something else, junk that is "strewn" though each piece is legally placed.
- **Simulated bodies.** The server measures where the layer puts them; a client may roll or tip them.
- **Open meshes.** Bounds are boxes. A frame counter or an NPC's pose has a box bigger than what you
  see, so an overlap can be a false alarm and a real clip inside a hollow box can pass.
- **Paths that are not roads.** Footpaths, decals, terrain paint, market squares and aprons.
- **Door leaves themselves** and anything that moves: the swing is checked for props near a door, not
  for the door.
- **Prefab children.** `--in` is checked offline only (bounds and overlaps with siblings); the audit
  measures them only where a layer places that prefab.
- **Runtime spawns.** Items a script spawns are not in a layer; their scale and position are the
  spawning code's job and its tests.

Take the shots with `tools\cli shots area <name|x,z> --eye-only` (`docs/agent-area-polish.md`), all
edits first, one session.

## How it is built

- `tools/place/place.mjs`: the pure primitives (`restOn`, `againstWall`, `onGround`, `arrangeOn`,
  `rowCentres`, `offlineProblems`, `setTransform`, `appendEntity`, `judge`) and the commands around
  them. Bounds come from `tools/actions/place-action-contexts.mjs` (`boundsOf`), the layer parser from
  `tools/actions/prop-on-surface.mjs`, the pair checks and the audit run from
  `agent-box/place-audit.mjs` (`measureTown`).
- `LF_PlacementAudit.RayLine` traces one probe ray with `BaseWorld.TraceMove` on every physics layer,
  skips characters and the prop being moved, and reports distance, the entity hit, its root, the hit
  point and `GetSurfaceY` under it. `RunBatch` answers `ray ...` lines in the audit's input file, so
  the probes and the measurements share one boot of `LF_Test_PlacementAuditTown`.
- Tests: `tools/validation/place-tool-selftest.mjs` (in `tools\cli validate`; one red-proof per
  primitive, listed at its top) and `LF_Test_PlacementProbe` (WORLD: a ray down onto a spawned table,
  a ray from its side, a ray through open air).
