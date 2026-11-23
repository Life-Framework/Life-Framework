# Clearing base-map clutter in a stamped building

Card #3882 (spike G2 of the placement epic #3877, `docs/architecture/placement-system.md`). The question: how does the
pack take vanilla clutter out of an island building it furnishes (the clothing store's own Broom, Bucket, Chair,
Stepladder and CoatRack next to the kit's set), without editing the base map?

## Answer: a local runtime hide by prefab inside the building's box

`LF_BaseWorldClear.HideInBuilding(building, prefabs)` (`Feature/Placement/LF_BaseWorldClear.c`) queries the building's
own oriented box (`GetBounds` plus `GetWorldTransform`, `BaseWorld.QueryEntitiesByOBB`) and hides every entity in it
whose prefab is on the clear list. The hide is the one felled trees use (`LF_FelledTreesReplication.HideTree`): not
drawn, not traced, simulation state `NONE`, so no collision.

Why it needs no network and survives a restart: base-map entities are loaded by every machine from the same map
data, so the server, each client and a late joiner each hide their own copy at load. Nothing is replicated and nothing
is saved; the clear list is data in the island layer, applied again on every load. The carrier is
`LF_BaseWorldClearComponent` (building prefab, clear list, search radius): at `OnPostInit` it waits one frame, finds the
nearest copy of its building prefab within the radius and calls the hide.

Proof: `LF_Test_BaseWorldClear` (`placement/base-world-clear`, WORLD) spawns two clothing stores
(`{92E8F78BCF17D93D}ShopHouse_E_2I01t_clothesstore.et`), clears StepladderWooden_01 and CoatRack_01 from one, and checks
both are hidden with no collision while the building, its other furniture (Table_01) and the second store's
stepladder stay. A second apply hides nothing more. Red proof: `--red` with the hide skipped fails.

Log line: `[LFDebug:Placement] base-world clear: hid 2 vanilla props in {92E8F78BCF17D93D}...clothesstore.et at <x,y,z>`.

## What the prototype found

- **Building furniture is not parented.** The furniture child (`{74A94121A3F00C4F}..._furniture_clothesstore_01.et`)
  and its props have no `Hierarchy` component, so each spawns as its own world entity: `building.GetChildren()` finds
  none of them. Matching must be spatial (the building's box), not by hierarchy.
- **Inline prefab children have no prefab name.** `GetPrefabData().GetPrefabName()` is empty for a prop that is an
  inline child of the furniture prefab. `SCR_BaseContainerTools.GetPrefabResourceName(data.GetPrefab())` walks the
  container ancestry to the real prop (`Prefabs/Props/Civilian/StepladderWooden_01.et`). `LF_BaseWorldClear.PrefabOf`
  wraps it; the stamp's audit sweep needs the same call to name a building's own furniture.
- **Prefab paths are matched without the GUID, lower case** (`PathKey`), so a clear list may name `{GUID}path` or a
  bare path.

## The two rejected options

| Option | Why not |
|---|---|
| Pack override of the furniture child by GUID (the Overthrow way) | Changes every copy of the building type: Everon has three clothing stores (Saint Philippe, Morton, Saint Pierre; `docs/everon/catalog-buildings.json`). A server that loads Overthrow beside us gets two addons overriding the same GUID, and the last loaded wins. Fine only when every copy is the same shop; kept as the optional path in placement-system.md. |
| A sub-scene layer that deletes a base-map entity | No such API (placement-system.md "How other projects place shop interiors"). |

## Limits

- It hides, never deletes: a script that queries by prefab still finds the entity. Hidden props are untraceable, so
  the F key and physics ignore them.
- The box is the building's model bounds. Clutter outside a building (dock yard crates) needs its own box: the same
  query with a kit-local box instead of the building's.
- A hidden prop with its own `RplComponent` (a destructible vanilla prop) is hidden only locally on each machine, which
  is what we want; its replicated state is untouched.

## The stamp writes the carrier (#3924)

`town stamp` (and `town fit`, through `planFit`) writes one `Fit_<place>_<slot>_clear` row per stamped copy when the kit
has a clear list: `kit.stamp.clear` (what `town author extract --clear` records, entries `{ prefab }`) or a top-level
`kit.clear` for a kit that is fitted rather than stamped. The row is a `GenericEntity` at the building's x and z with
`LF_BaseWorldClearComponent` (`m_sBuilding` = the building prefab, `m_aPrefabs` = the list). The row's height is only the
building origin; the game finds the building by prefab within 3 m on x and z, so the layout stores no measured height. An
empty list writes no carrier. Selftest: `town-stamp-selftest` (the carrier row in both copies, none for an empty list).

## Outdoor clutter: the same hide with its own box (#3924)

Clutter outside a building (the base map's crates and pallets in the dock yard) has no building box to search. The carrier
with `m_sBuilding` empty and `m_vBoxHalfM` set (half size on x and z, `m_fBoxHeightM` tall) hides the listed prefabs inside
a box centred on the carrier, turned by its yaw, standing on the terrain under it (`GetSurfaceY`, 1 m below to
`m_fBoxHeightM` above), so this row stores no height either. `LF_BaseWorldClear.HideInBox` is the same query and the same
hide. Test: `LF_Test_BaseWorldClearBox` (`placement/base-world-clear-box`).

## One door of a prefab the building repeats: a boxed kit entry (2026-11-22)

A kit's `clear` entry with a `box` (`offset`, `half`, `height`, in the kit's frame) makes `town fit` write a box-mode carrier at that spot (`clearBoxCarriers` in `tools/town/fit.mjs`): the same hide, limited to a small box, with no building. The bank's `Office_E_01` uses the interior door set in several rooms, so the building mode would have hidden all of them; the box takes only the one behind the vault door. Hiding is not recursive (`HideTree` clears the flags of that entity only), so the door set (frame) and its leaf are two entries. `agent-box/proofs/island-bank-door-clear-proof.mjs` checks the hide log on the server and both clients, a trace through the doorway, the neighbouring doors and the vault drill.

## Vanilla signs, murals and graffiti: one slot, one carrier per wall (#4391)

The base map's Czech and Russian lettering (wall signs, murals, graffiti, posters) is world-level and child entities of the base buildings, so the box mode hides each by prefab path. The slot `czech-signs` of Saint-Philippe (an outposts slot whose drops are the buildings the props hang on) owns 38 carriers in `Place_saint-philippe_czech-signs.layer`: a 6 x 6 m box, 16 m tall, at the prop's x and z, listing only that wall's prefabs. A bare path (no GUID) is enough: the clear list is matched by path (`PathKey`), nothing loads it. The prop positions come from the placement audit's `scan x z r` over the town (86 k entities, filtered by prefab name). Where a shop needs lettering back, the same slot's layer carries our own fascia (the household-wares shop's `HOUSEWARES`).
