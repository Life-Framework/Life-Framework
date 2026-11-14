# Building pipeline spike (2026-11-14)

What we learned driving [ArchBuilder](https://arribas22.github.io/archbuilder/) 2.42.0 (Blender 5.3) from a script, and
what it took to get its export into the addon. The playbook is [docs/building-pipeline/README.md](../building-pipeline/README.md).
The proof building is a 1989 Everon supermarket, `tools/buildings/supermarket/spec.json`.

## Headless works with no context override

ArchBuilder's generate and export operators run under `blender --background` called as plain `bpy.ops` calls. They read
`context.scene`, the 3D cursor, `selected_objects` and the view layer, which background mode has. Nothing needs a 3D view,
a region or a window. The interactive draw tools (walls, openings, cutters) do need a 3D view; the pipeline does not use
them.

- The add-on must load from the user's preferences, so the job runs **without** `--factory-startup`. Blender then also
  loads the user's other add-ons; on this machine Enfusion Blender Tools logs two import errors in background mode
  (no GPU module). Harmless; the runner filters them out of failure messages.
- The module name depends on the extension repo it is installed in (`bl_ext.user_default.arch_builder` for a plain
  install, `bl_ext.lf_local.arch_builder` for the maintainer's local checkout). The job script finds the enabled module
  whose name ends in `.arch_builder`.
- Every operator is locked until the add-on is licensed. The job checks `license.is_full(context)` and exits with code 4,
  which the pipeline reports as SKIP, not as a failure.
- An extension has no `bl_info`, so the version comes from `blender_manifest.toml`.

## Timings (one small building, this PC)

| Step | Time |
|---|---|
| Blender start-up with the user's add-ons | 1 to 5 s per stage |
| `ab.generate_building` (supermarket, 18 x 14 m, one storey with rooms) | 0.2 to 0.6 s |
| Four review renders, workbench engine, 1280 x 900 | about 8 s |
| `ab.rf_export` into the staging addon | 0.1 to 0.3 s |
| Workbench compile of the model (hidden, kept open until done) | 16 to 23 s |
| A whole `building run` from spec to prefab check | about 50 s |

## What the export writes

For a building named `<Name>` into an addon folder (the pipeline uses `tmp/buildings/<slug>/export/`):

```
addon.gproj                                     its own project (ID = the folder name), depends on the base game only
ArchBuilder_import.txt                          its Workbench queue (OPEN line + the .fbx to compile)
Scripts/WorkbenchGame/ArchBuilder/ArchBuilderImportPlugin.c    its own Workbench plugin (rebuilds the queued .fbx)
Assets/ArchBuilder/<Name>/<Name>.fbx            one model: _EXT_LOD0..2, _INT_LOD0, one UTM_ collider per physics material
Assets/ArchBuilder/<Name>/<Name>.xob.meta       material assignments, GeometryParams (layer, .gamemat), sockets
Assets/ArchBuilder/<Name>/EXPORT_INFO.txt       notes, with "Licensed to: <buyer> [edition]"
Assets/ArchBuilder/Materials/AB_*.emat(.meta)   one MatPBRBasic per material over vanilla _SharedData textures
Prefabs/ArchBuilder/<Name>/<Name>_Base.et       SCR_DestructibleBuildingEntity on Building_Base.et: the model,
                                                vanilla door sets, windows, glass panes and a ProbeHouse as children
Prefabs/ArchBuilder/<Name>/<Name>.et            inherits _Base, adds Furniture/<Name>_furniture_01.et when there is any
```

- **Doors and windows are vanilla prefabs**, not ArchBuilder meshes, when `vanilla_openings` is on: the generator cuts each
  opening to the size of a game window or door set, and the export places that prefab in it. The supermarket got one
  `DoorSet_Services_E_A_Double_UNI_B_EXT` (the glazed double entrance), four `DoorSet_Village_E_01_*_INT_COV_B` (stockroom,
  office, WCs) and eight `Glass_198x190` panes (the two shop windows). The export's own summary says "0 doors": it counts
  door leaves it modelled itself, not vanilla door sets.
- **GUIDs are not random.** Each is a hash of the resource path and a fixed salt, made unique within the export folder.
  Two people exporting a building of the same name get the same GUIDs, and an addon holding two such exports would
  collide. The import replaces every resource GUID (README "Where the export lands").
- **Base-game references** are written from ArchBuilder's bundled scan of the game data. All 43 in the supermarket
  (prefabs, `.gamemat`s, `_SharedData` textures, platform configs) match the 1.9 game's resource database by path and
  GUID.
- **The light system** (`ab_rf_light_system`, on by default) writes an Enforce script with working light switches into
  the addon, under ArchBuilder's own class prefix. The pipeline turns it off: our classes are `LF_` (AGENTS.md rule 5), and
  a script from a paid add-on would be its code in our repo.

## The collider bug: Workbench exits before it has written the surfaces

The first compile of every building came out with most of its colliders on Workbench's placeholder surface
(`{536BF67B2052B869}material/metal.gamemat`) or with no surface at all. Workbench also re-saves the `.xob.meta` after a
build, and the re-saved file had kept the GeometryParams of only the first one to four colliders in FBX order. Which ones
varied from run to run. The experiments:

| Tried | Result |
|---|---|
| Check the `.gamemat` GUIDs against the game's resource database | All eight correct |
| Reverse the GeometryParams order in the `.meta` | Still the first colliders in **FBX** order kept |
| Rotate the gamemats between colliders | Same colliders kept, whatever their surface |
| Eight colliders sharing four gamemats | Still cut down |
| Rewrite the `.meta` and rebuild (no `.fbx` change, so no start-up reimport) | Still cut down |
| **Same rebuild, Workbench left open for 60 s** | **All eight kept; the `.xob` bakes all eight surfaces** |

With `-exitAfterInit`, Workbench reports "Build successful" and quits while it is still writing the collider surfaces.
The asset pipeline never saw this because its models have one collider. ArchBuilder's own Workbench step keeps Workbench
open and watches the output, which is why it works there. `runWorkbenchPlugin` (`tools/cli.mjs`) now takes `doneWhen`:
Workbench is started without `-exitAfterInit`, the import polls every 2 s until the `.xob` is rebuilt, bakes every planned
material and surface, and the re-saved `.meta` matches the plan, then stops it 3 s later.

Two side findings:

- The `.xob` string table names **one** collider whatever the count, so "is collider X in the model" cannot be read from
  it. The check reads the baked `.gamemat` references instead: one per planned surface, and nothing else.
- The MCP file index has no GUIDs and does not list `Common/`. The game's own `addons/data/resourceDatabase.rdb` holds
  every path with its GUID (the path, a NUL, six bytes, then the GUID as 8 bytes little-endian).
  `tools/buildings/pipeline/gamedb.mjs` reads it; the `prefab` stage checks every base-game reference against it.

## Proof

- `tools\cli building run supermarket`: every stage ok. 3434 triangles, 18 x 14 m, 8.8 m to the top of the rooftop unit.
  23 resources moved with new GUIDs; a rerun keeps all 23. Model compiled with 21 materials and 11 collider surfaces.
- `tools\cli place ground 76,95 ... --layer Annex.layer`: placed in the DebugWorld test annex and audited clean on the
  dedicated test server (it loaded the prefab and found it standing on the terrain). No resource error naming the building
  in the server log.
- `tools\cli building selftest`: the offline checks, red-proofed (header of `tools/validation/building-pipeline-selftest.mjs`).
- `building-proof` on a full-quality session: the F key offers the vanilla door (`SCR_DoorUserAction`) and the server
  opens and closes it. The harness `stand` put the player inside the sales hall (its side offset works from smaller
  bounds than the building's), so the door it opened was a stockroom door; the entrance itself was not driven.
- `look`'s `white` flag on the first run was an untinted material, not a broken render: a test swapping the wall texture
  for our charcoal texture and tinting the roof showed UVs, materials and the vanilla textures all load. The base game's
  `_SharedData` plaster and bitumen textures are near-white and expect a `Color` tint, and `AB_Stucco_White` has none.
  The spec now uses `AB_Stucco_Beige`; in evening light it still reads pale.

## Decisions and open questions

| Question | Who | State |
|---|---|---|
| Which building types the flagship needs, and how many of each (village houses, shops, a town hall, a police station, farms) | **Human** | Open. ArchBuilder has residential, government, services and military archetypes and a realistic-building set (town hall, church, ...) |
| Buildings land in the core addon with a `pack` row in the inventory, until the pack loads (AGENTS.md rule 9) | Decided | Per the rule |
| One export per spec, not a street at once | Decided | Simpler GUID and review story; revisit for village blocks |
| Furniture: ArchBuilder's vanilla furniture, or ours | **Human** | Open. Off for the supermarket so the shop can fill it (below) |

## Supermarket next steps

Research for the follow-up that furnishes the supermarket and opens it as a store. Not done in this PR: the building is
exported with `do_furniture` off, so it has rooms, doors, windows and ceiling lamps and nothing else.

### (a) What ArchBuilder can put inside a supermarket

The `SUPERMARKET` plan is one `market` hall with a strip of service rooms along the back wall: two stockrooms, an office
and two WCs. It is one storey (its storey roll is fixed at one). With `do_furniture` on, ArchBuilder furnishes each room
by kind, and with the Reforger export's vanilla props on (`ab_rf_vanilla_props`, the default) it swaps each piece for a
base-game prefab:

| Room | What ArchBuilder places | Vanilla prefab in the export |
|---|---|---|
| market, entrance wall | up to 3 checkouts | `Prefabs/Props/Commercial/CounterShop_01.et`, a till `CashierShop_01.et` on it, `ChairOffice_01` behind |
| market, far short wall | up to 6 coolers | `Prefabs/Props/Commercial/CoolingBoxShop_01.et` (else `Freezer_01`, else `Fridge_01`) |
| market, long walls | up to 14 wall shelves (2 m runs) | `Prefabs/Props/Commercial/ShelfUniversal_01_wide.et` (or `_Base`, `_narrow`), one per about 1.45 m |
| market, floor | double-sided 5 m gondola islands, 3 m aisles | `Prefabs/Props/Commercial/ShelfShop_01.et`, two per 2.5 m, back to back |
| market, ceiling | panel lamps | `LampFluorescent_02` |
| stockrooms | shelving | `Bookshelf_01` / `Bookshelf_02` |
| office | desk, chair, monitor, shelves, a plant | `Desk_01_*`, `Computer_E_01_Monitor_base`, `Bookshelf_*` |
| WCs | cubicles, basin | `Toilet_01`, `Sink_01` |
| street front | fascia sign | `SignCommercial_01_GroceryStore` (today's export has a blank board because furniture is off) |

- **No goods.** The only "products" are coloured boxes modelled into ArchBuilder's own shelf meshes, and those are not
  exported when vanilla props are on. The vanilla shelves arrive empty, which is what the store wants.
- **Nothing is baked.** With vanilla props on, a piece with no room to stand is moved to another free wall or dropped;
  it is not baked into the building mesh.
- **Where it lands.** Every piece is one child entity (prefab, `coords`, `angleY`) of
  `Prefabs/Buildings/<Name>/Furniture/<Name>_furniture_01.et`, which inherits the base game's `BuildingFurniture_base.et`.
  `<Name>.et` places that composition at the building's origin, so a shelf's coordinates are in the building's own frame.
- **Settings.** `archbuilder.do_furniture: true` in the spec, with `export.furniture`, `export.vanilla_props` and
  `export.lamps` left on.

### (b) How a shelf becomes part of our store

Our physical store is already slot-based (`Feature/Shop/`, features.md "Physical store", #202):

- `LF_StoreComponent` (`Feature/Shop/LF_StoreComponent.c`) sits on the shop entity beside `LF_ShopComponent`. It has a
  store key, a boundary box (leaving with unpaid goods is shoplifting), a restock interval and a list of `LF_StoreLine`.
- Each `LF_StoreLine` (`Feature/Shop/LF_StoreRules.c`) is one product prefab with max, start and restock amounts and
  `m_sSlots`: points local to the store entity, written `"x y z | x y z"`. The store spawns real items at those points
  (`LF_StoreComponent.c` around line 485); a player picks one up, it is tagged unpaid, and they pay with
  `LF_StoreTillAction` at a till that carries the same store key.
- Prices come from `Configs/Shop/ShopPrices.conf` by prefab path. The working example is the DebugWorld general store in
  `Shops.layer`: a `CounterShop_01` with both components and a till.

The plan for the follow-up:

1. **Leave every market shelf, gondola and cooler empty.** They are the store's display. Stockroom and office shelves stay
   decoration.
2. **Do not edit the generated prefabs.** A new prefab inherits `Prefabs/Buildings/Supermarket/Supermarket.et` and adds
   `LF_ShopComponent` and `LF_StoreComponent` to the building root. Building space is then store-local space, so shelf
   coordinates from the furniture prefab can be used as they are, and the boundary box is the market hall.
3. **Generate the slots.** A new pipeline step (`tools/buildings/pipeline/store-slots.mjs`, run after `prefab`) reads the
   furniture prefab, finds the `ShelfUniversal_01_*`, `ShelfShop_01` and `CoolingBoxShop_01` children, and writes
   `m_sSlots` from each child's position and yaw plus the board heights of each shelf model, measured once from the
   vanilla `.xob` bounds or a probe. A spec block says which products go on which fixture kind (drinks in the coolers,
   food on the gondolas).
4. **Make the checkout a till.** Override the `CashierShop_01` child to carry the actions manager and
   `LF_StoreTillAction` with the store key, or put our till prefab (as the general store does) on the counter.
5. **One script change.** `SnapToSurface` (`LF_StoreComponent.c:373`) traces only against the store entity itself, so a
   slot over a child shelf needs an exact height. Letting it also hit the store's child entities makes the generated
   heights forgiving.

Open for the human: which products a 1989 Everon supermarket sells, and whether the shopkeeper NPC and the shoplifting
boundary come with the first furnished version.
