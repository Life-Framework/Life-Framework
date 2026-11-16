# Everon catalog

Facts about vanilla Arma Reforger Everon, for planning the real-Everon flagship world and the town pipeline. Docs and data only; nothing here changes the game.

| file | what |
|---|---|
| [catalog-buildings.md](catalog-buildings.md) / [.json](catalog-buildings.json) | every building prefab on Everon: `{GUID}path`, footprint, floors, interior, towns, counts; every named place; all 6.5 k building instances with coordinates |
| [saint-philippe.md](saint-philippe.md) / [.json](saint-philippe.json) | Saint Philippe building by building (450 m), civic buildings, housing, layout map, props placed around town |
| [town-counts-400m.json](town-counts-400m.json) | building-type counts within 400 m of every town, village and city centre |
| [catalog-props.md](catalog-props.md) / [.json](catalog-props.json) | the vanilla props useful for roleplay fit-out, grouped by use, each with `{GUID}path`, size, world count and action flags |
| [props/](props/) | the per-group prop catalogs the merged file is built from |
| [method/](method/) | how the data was made: the in-engine dump script and the build scripts |
| [pictures.md](pictures.md) | pictures of the building types, taken with `look` on CoreOnEveron (files in `website/public/docs-images/everon/`) |

## How the data was made

`Eden.ent` (the vanilla Everon world) is a compressed binary and `game_read` stops at 500 KB, so the entity list was read from the running world instead. A throwaway server-side script ([method/LF_ZzEveronDump.c.txt](method/LF_ZzEveronDump.c.txt)) ran 20 s after game start on `Worlds/CoreOnEveron` (a dedicated session server, no client), walked the whole map in 400 m tiles with `QueryEntitiesByAABB` and printed one line per structure entity (prefab, position, yaw, model bounds, door children) plus counts of every other prefab and every map descriptor. 1.44 million entities were seen; 151 k structure rows came back before de-duplication across tile edges. Footprints, cellars and floors come from `tools\cli measure --surfaces`. The node scripts that turn the dump into these files are in [method/](method/) (rename `*.txt` back to run).

The throwaway script was never committed under `addons/`. The `CoreOnEveron` test boot (`tools\cli test --world`) crashed right after world init on both pool machines on 2026-11-16 (even the existing `LF_Test_PlacementRoad`), so the dump ran as a session server instead (`tools\cli session up --mission everon --clients 0`). The client crashed while loading Everon on the laptop.

## Reading the numbers

- **Coordinates** are Enfusion world metres, x east, z north. Everon spans about x -107..12348, z -100..12654.
- **Interior** is a heuristic (door entities plus at least one walkable floor in the model). It has not been checked by walking every building.
- **Floors** are counted from the colliders' horizontal surfaces and ignore roofs and attics under 15 % of the footprint.
- **Places** are the map's own descriptors (`#AR-MapLocation_<key>`); Saint Philippe is `StPhillipe`. A building belongs to the place it is nearest to in radius units, so a hamlet inside a town's radius can steal a few buildings.
- **Vanilla interiors are furnished**: a town house holds roughly 140-170 Furniture and Decorations props inside its footprint (loose count, see the `furn` column).

## Facts worth knowing

- **Vanilla Everon places no vehicles.** The dump has zero `Prefabs/Vehicles` entities (only a handful of `Props/Wrecks` and tyre piles), so parked cars on the real map are the mod's to add. Every drivable vanilla vehicle prefab carries `Persistence`, `SCR_BaseLockComponent` and `RplComponent` ([props/vehicles-dock-industrial.md](props/vehicles-dock-industrial.md)).
- **Interiors come furnished**: several thousand Furniture props sit inside the buildings of Saint Philippe's 450 m alone (loose footprint test).
- **The world's own building roster is small**: 382 village/wooden/prefab houses, 40 town houses, 18 shop-houses, 10 pubs, 12 churches and 24 chapels on the whole island (plus sheds, garages, farm and industrial buildings, see the summary table).
- Several services buildings Everon already has are easy to miss from the file list: `MunicipalOffice_E_01` (town hall, plus a `_school` variant), `Villa_E_2I01_Police`, `FireStation_E_01`, `Office_E_01`, `MountainHotel_01`.

## Open items

- Interiors are not walked: whether doors are lockable, which rooms exist and room sizes are not recorded.
- Roads, forests, terrain heights and the AI navmesh are not catalogued.
- Props are catalogued by group from the game's prefab library; the world counts only cover the four candidate towns' 450 m surroundings and the whole-map totals of non-vegetation props.
