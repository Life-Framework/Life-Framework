# Building pipeline: spec to Reforger building

**Buildings are made with [ArchBuilder](https://arribas22.github.io/archbuilder/)**, a Blender add-on by Arribas. It is
the preferred way to make a building for Life Framework: it builds walls, floors, stairs, roofs and rooms, cuts the
openings to the size of the game's own doors and windows, and exports a Reforger model with colliders, materials and a
building prefab. Buy it, install it and activate your own key (see Setup). The repository holds none of its source.

`tools\cli building` turns a committed spec into a building in the addon by driving your installed copy of ArchBuilder in
headless Blender, then moving its export into the addon and compiling it in a hidden Workbench. What the first runs
taught us is in [`docs/architecture/building-pipeline-spike.md`](../architecture/building-pipeline-spike.md).

```
tools\cli building run <slug> [--from <stage>] [--only <stage>] [--force] [--full]   --full: compile every model again
tools\cli building adopt <slug> [<file.blend>]   keep a scene polished by hand as the export source
tools\cli building plan <slug>      the facts an agent brief needs: prefabs, sizes, rooms, doors, store slots, parallel parts
tools\cli building proof <slug>     doors, scale, audit, pictures and the building's proof.json on a full session
tools\cli building review <slug> --shots   Blender review renders and a contact sheet, no game (polish.md "Review shots")
tools\cli building check <slug> [--fix]   no small prop floats: bottom within 2 cm of the top under it, offline, about 1 s (starter-packs/building.md "Props stand on something")
tools\cli building status [<slug>]
tools\cli building doctor      Blender, the add-on, its licence, one headless generate
tools\cli building selftest    the offline checks (also part of tools\cli validate)
```

Run it in a worktree. The spec is `tools/buildings/<slug>/spec.json` and is committed. Everything a stage produces is
cached in `tmp/buildings/<slug>/`, which is not. Placement is not a stage: put the building in a world with
`tools\cli place ground` ([placement tool](../placement-tool.md)).

## Setup

1. Buy ArchBuilder at [arribas22.github.io/archbuilder](https://arribas22.github.io/archbuilder/) (sold with a personal key
   through [ko-fi.com/arribas](https://ko-fi.com/arribas)). The pipeline was built against version 2.42.0.
2. Install it in **Blender 5.3** (`C:\Program Files\Blender Foundation\Blender 5.3\blender.exe`, or set
   `LF_BLENDER_ARCHBUILDER`), enable it, and activate your own key in its License panel. The key and your buyer id stay on
   your machine: never commit them, the add-on's files, or its `LICENSE.json`.
3. Tell the pipeline where the add-on's files are with `LF_ARCHBUILDER_DIR` if they are not in one of the two places it
   looks: `Documents\Reforger\blender-addons\arch_builder` (a local checkout loaded as a Blender local extension repo) or
   `%APPDATA%\Blender Foundation\Blender\5.3\extensions\user_default\arch_builder` (a plain install). Blender finds the
   add-on by its own preferences; the pipeline only reads the version and the licence file from that folder.
4. Arma Reforger Tools (Workbench), which `tools\cli build` already uses.
5. `tools\cli building doctor` checks all of it and runs one headless generate.

A machine without ArchBuilder (CI, the test laptop) can still run everything after `export`: the `generate`, `review` and
`export` stages print `SKIP` and the `import` stage uses an export made on the ArchBuilder machine. The selftest needs
neither.

## Stages

| Stage | Does | Who judges the result |
|---|---|---|
| `spec` | Checks the spec: Reforger proportions, vanilla openings, budget, folders, the red line | The agent writes the spec |
| `generate` | Headless Blender: sets `scene.ab_gen` from the spec, runs ArchBuilder's generate (or its realistic-building generator for a `landmark`), saves `building.blend` | Review |
| `review` | Front, side, top and three-quarter renders in `review/`, and `review/stats.json`: triangles per part, storeys, footprint and height in metres. Over the spec's budget fails | The agent reads the pictures (Review below) |
| `polish` | Renders what a player sees into `polish/`: a plan section, the four sides and the street corner from eye height, the hall from inside. Lists the spec's hand edits; warns when a hand-polished `polished.blend` was made with other settings than the spec. Never fails | The human or agent polishing it by hand: [polish.md](polish.md) |
| `export` | ArchBuilder's Reforger export (its scene export of `tools/buildings/<slug>/polished.blend` when there is one) into a staging addon, `tmp/buildings/<slug>/export/`, with its Workbench step off. Gives every `AB_*` material its game colour (Materials below). Fails on any export warning, any furniture ArchBuilder left out of the game, no vanilla props with furniture on, and fewer doors than `budget.min_doors`. Applies `spec.polish.entities` and warns about three or more door sets in a row on one wall | The tool |
| `import` | Refuses any file carrying the licence watermark, moves the export into the addon with new GUIDs, writes only the files that changed, compiles a changed model in a hidden Workbench (on a pool machine when this PC has no Workbench) and reads the `.xob` back (Fast polish iterations below) | The tool |
| `prefab` | Every reference in the building's files resolves: our own by `.meta` (in the building's addon or the core), the base game's by path and GUID in the game's resource database. Door sets, windows, the model and its collider surfaces are there | The tool; then a placement and a look |
| `store` | Only with a `store` block: writes `<prefab_dir>/<store.prefab>.et`, the building as one of our physical stores (Store below). Prints each fixture with the index `pick` uses | The tool; then a session: take, pay, walk out |

A finished stage is skipped on the next `run`. `--only review` reruns one stage, `--from export` reruns it and everything
after it, and `--force` reruns from the start (and compiles every model again, like `--full`).

- **A failed import** can leave new files beside old ones in the addon. Rerun `--from import`: it writes every file that
  differs from the plan and keeps every GUID. Commit only after the `prefab` stage passes.

## Fast polish iterations

A polish iteration is an edit to `polished.blend` (or the spec's `polish` block) and `tools\cli building run <slug> --from polish`.
Two things keep it short:

- **One Blender per run.** Every Blender stage of a run (generate, review, polish, export and the export's materials job)
  goes to one headless Blender (`archbuilder_job.py --serve`, `BlenderSession` in `archbuilder.mjs`): each job opens its
  own `.blend` there, its log is still `tmp/buildings/<slug>/<stage>-blender.log`, and the whole session's log is
  `tmp/buildings/<slug>/blender-session/blender.log`. Blender's start-up was most of each stage (ArchBuilder's generate
  and export take under a second). The Enfusion Blender Tools add-on prints the same `ar_mqa` and `object_visualization`
  import errors on every start; they stay in the log and are left out of what a failed stage prints.
- **Only what changed is imported.** The export runs Blender with `PYTHONHASHSEED=0`, so the same scene gives the same
  `.fbx` byte for byte apart from its creation time stamp (Blender's FBX writer derived each object's id from Python's
  per-process salted `hash()`). The import hashes every planned file against what is in the addon and writes only those
  that differ, and it compiles a model in Workbench only when its `.fbx` or planned `.xob.meta` changed since the last
  import, or the `.xob` in the addon is not the one that import made (a `git checkout`). The record is
  `tmp/buildings/<slug>/import-manifest.json`, written only after a good import. With nothing changed the import says
  `nothing changed since the last import ... Workbench skipped` and takes a second. `--full` compiles every model again.
- **Where Workbench runs.** This PC has no `workbench` cap (agents never start a headless Workbench here,
  `tools/lib/wb-route.mjs`), so the compile goes to a pool machine that has one: the pool syncs the worktree (the planned
  `.fbx` and `.xob.meta` included), runs `tools\cli building compile <slug> <model.fbx>` there in its signed-in desktop
  session (ssh has no GPU device), and copies the `.xob`, its re-saved `.meta` and the `.txo` back. Keeping one Workbench
  open between runs is not possible with the plugin runner: Workbench runs its plugin once, at start-up.

Measured on the supermarket, 2026-11-15, this PC under load (0.5 GB RAM free), Workbench on lf-cloud:

| Case | Before | After |
|---|---|---|
| Full run, `--force` | Blender stages 39 s (17 + 9 + 5 + 8), then the import refuses here; 72 to 136 s of Workbench where one may run | 97 s: Blender stages 16 s (11 + 2 + 1 + 2), import 80 s (compile 38 s, sync and copy back the rest) |
| One mesh changed, `--from polish` | polish and export 67 s, import refused here | 103 s: polish and export 33 s, import 69 s (compile 41 s) |
| Nothing changed, `--from polish` | polish and export 32 s, import refused here | 14 s: polish 10 s, export 2 s, import 1 s with no Workbench |

The Blender times vary a lot on this PC (one polish stage took 4 s on one run and 26 s on the next); the first Blender job
of a run carries the start-up.
- **Renaming a building** (`name`) makes a new building with new GUIDs; saved worlds keep naming the old ones. The old
  `Assets/Buildings/<Old>/` and `Prefabs/Buildings/<Old>/` stay until you delete them, once nothing references them.
- **The import refuses** a machine with no ArchBuilder licence file (it could not check the watermark), any file the add-on
  would share between buildings other than an `.emat` material, and any file still holding one of ArchBuilder's GUIDs.

## What the spec holds

```json
{
  "slug": "supermarket", "name": "Supermarket",
  "concept": "what the building is in 1989 Everon, and why these ArchBuilder settings",
  "red_line": "pack",
  "archbuilder": { "archetype": "SUPERMARKET", "region": "AUTO", "storeys": 1, "seed": 1989, "width": 18, "depth": 14,
                   "wall_mat": "AB_Stucco_Beige", "roof_style": "FLAT", "do_interior": true, "do_furniture": true },
  "export": { "furniture": true, "lamps": true, "vanilla_props": true, "lods": true, "probes": "ONE", "bsp": false },
  "budget": { "tris": 60000, "footprint_m": [19, 15], "height_m": 9, "min_doors": 1 },
  "asset_dir": "Assets/Buildings/Supermarket", "prefab_dir": "Prefabs/Buildings/Supermarket"
}
```

- `archbuilder` holds ArchBuilder's own building settings by their names (`scene.ab_gen`: archetype, region, footprint,
  storeys, seed, width, depth, materials, interiors, roof...). An unknown name fails the generate stage. `landmark` instead
  of `archetype` builds one of its realistic buildings from measured plans (a town hall, a church).
- `proportions` is always `REFORGER` and `vanilla_openings` always true: the game character fits through the doors, and
  the windows and doors are the game's own (destructible glass, doors that open). The spec stage refuses anything else.
- `export` maps to ArchBuilder's export settings (`scene.ab_rf_*`). `light_system` is refused: it writes an Enforce script
  with ArchBuilder's class names into the addon (AGENTS.md rule 5).
- **Furniture, props and the shop sign need `export.furniture` on.** ArchBuilder records its furniture calls (shelves,
  counters, lamps, switches, the fascia sign) as vanilla props only while it exports furniture; with it off it records
  none, `vanilla_props` does nothing and the sign is a blank board. The spec stage refuses `vanilla_props` with furniture
  off, and `do_furniture: false` with furniture on (rooms with nothing to export). A bare shell says `vanilla_props: false`.
- The same seed and settings build the same building. A different look is a different seed.
- `polish` (optional) is the building's hand polish: `edits`, one line per hand edit carried by
  `tools/buildings/<slug>/polished.blend`, and `entities`, nudges to the game entities the export places
  (`{ "prefab": "SignCommercial_01_GroceryStore", "move": [x, y, z], "why": "..." }`), and `props`, vanilla props placed by hand
  that ArchBuilder never places (`{ "prefab": "{GUID}Prefabs/...et", "at": [x, y, z], "yaw": 90, "why": "..." }`). [polish.md](polish.md)
- `store` (optional) makes the building a store (Store below).
- `red_line` is `core` or `pack` ([core-vs-pack](../architecture/core-vs-pack.md)). A building is what this server looks
  like, so it is usually `pack`; until the pack loads it lands in the core with a row in
  [core-pack-inventory.md](../architecture/core-pack-inventory.md).

## Where the export lands

| ArchBuilder writes | The addon gets |
|---|---|
| `Assets/ArchBuilder/<Name>/<Name>.fbx`, `.xob.meta` | `asset_dir/` |
| `Assets/ArchBuilder/Materials/AB_*.emat` | `Assets/Buildings/Materials/` of the building's addon, shared by every building there. An export that would change one another building uses is refused |
| `Prefabs/ArchBuilder/<Name>/<Name>.et`, `<Name>_Base.et`, `Furniture/` | `prefab_dir/` |
| `EXPORT_INFO.txt`, `ArchBuilder_import.txt`, `addon.gproj`, its Workbench plugin | nothing: notes, its own queue, its own project and plugin |

`<Name>_Base.et` is the shell: the model plus the vanilla door sets, windows and the light probe as children.
`<Name>.et` inherits it and adds `Furniture/<Name>_furniture_01.et`: every piece of furniture, lamp, switch and the shop
sign as a vanilla prefab child (nothing is baked into the model). Place `<Name>.et`.

**Materials.** ArchBuilder writes each `AB_*.emat` as a `MatPBRBasic` over a base-game `_SharedData` texture with
`Color` set to its Blender colour, raw. In the game that read white: `Color` multiplies a near-white texture, and a
Blender colour is brighter than the base game's own walls (spike: "Why the walls read white"). The export stage rewrites
each material's `Color` so it reads like Blender: the material's Blender viewport colour, scaled so ArchBuilder's white
plaster lands on the base game's white plaster (one factor for a material that faces outside, one for inside), divided by
the texture's linear mean, measured from the game's own `.edds`. It prints one line per material. `tools/buildings/pipeline/materials.mjs`.
A light finish on a room's walls and ceiling (a "room white") is calibrated lower still, with a warm tint on neutral whites, and the export adds ceiling fixtures with lights placed from the room sizes (polish.md "Interior brightness"; `lighting.mjs`, `blender/lf_rooms.py`; a spec `lighting` block tunes it, `{ "off": true }` skips it).
A material another building's model uses is never changed by a re-export; one only this building uses is.

**GUIDs.** ArchBuilder derives its GUIDs from the path and a fixed salt, so everyone who exports a building of the same
name gets the same GUIDs. The import gives every resource a GUID of ours: the one already in the addon's `.meta` for that
path, so a re-export keeps every saved world and layer reference working, or a fresh one no repo addon holds. References
to the base game are left as they are, and the `prefab` stage checks each one against the game's own resource database.
Component GUIDs inside a prefab are not touched: an inherited component is overridden by its GUID.

**The licence watermark.** ArchBuilder writes the buyer's name into `EXPORT_INFO.txt`, which is never copied. Every other
file is searched for the buyer id and the key from the add-on's licence files (any case, plain or UTF-16) before anything
is written, and the import stops if one is found. The message names the file, never the mark.

## Review

Read the four pictures in `tmp/buildings/<slug>/review/` before the export:

- Is it a 1989 Everon building? Rendered walls, brick, timber; no curtain walls, no glass towers.
- Is the street front where you expect it? ArchBuilder builds with the front on Blender's -Y, the prefab's -Z.
- Does every room have a way in, and does the roof close?
- Is the size right for the plot (the stats)?

Change the seed, the size or the settings and rerun `--from generate`. The pictures are Blender renders with
ArchBuilder's own material colours, not the game's textures: the game look is checked in the game after placing it.

## Proof in the game

1. Place it: `tools\cli place ground <x,z> <{GUID}Prefabs/Buildings/<Name>/<Name>.et> --layer <Layer>.layer --name <Entity>`.
   The audit boots the dedicated test server, so a placement that passes has loaded the prefab there.
2. `tools\cli look <x,z> --close 14` for a first look. Its `white` flag on a building usually means an untinted
   material: the base game's `_SharedData` plaster and bitumen textures are near-white and expect a `Color` tint.
   Pick a coloured material (`AB_Stucco_Beige`, not `AB_Stucco_White`) rather than trusting the flag.
3. `tools\cli building proof <slug>`: one command, on a **full-quality** session (never lowspec: a building's check
   looks at its materials, glass, LODs and doors, which the lowspec profile's lowest textures hide). It finds the
   placed entity in the world layers and checks, in order:
   - the prefab resolves to its model, with colliders (`measure`);
   - its height against the Everon buildings standing within 150 m (0.4 to 2.5 times their median);
   - `tools\cli audit placement --strict` on its layer (the dedicated server loads the prefab);
   - in one `session up --profile full`: every door set of `<Name>_Base.et` is offered by the F key, opens, leaves a
     doorway a character wide clear at 0.5, 1.0 and 1.6 m (traces), and closes, with a picture shut and open; the four
     sides, the street corner and the hall from inside. A building made from a vanilla one (`vanilla` block) keeps the
     vanilla door sets on the model's sockets, with no coordinates to walk to, so the door walk is skipped (it prints
     `doors: skipped`) and its `proof.json` carries the proof. The world loader reads a placement's turn from `angleY`
     only: write `angleY 180`, not `angles 0 180 0`, or every proof point lands on the wrong side;
   - the building's own scenario, `tools/buildings/<slug>/proof.json`, in the same session.

   Pictures land in `tmp/buildings/<slug>/proof/`; the last line is `SUMMARY building-proof ok|fail`. `--no-session`
   runs the offline checks only, `--skip-audit` skips the audit, `--entity <Name>` picks one of several placements.
4. Walking through the rooms and a second player seeing the door move stay manual (the PR's `## Manual testing`).

### The scenario file

`tools/buildings/<slug>/proof.json` is what this building is for, as player steps (`tools/buildings/pipeline/scenario.mjs`
has the format). Points are building space, so it survives moving the building. A step is one of:

| Step | Does |
|---|---|
| `{ "shot": "street", "look": [x,y,z], "from": [dx,dy,dz] }` | stands at `look + from` (the feet) facing `look`, takes a picture |
| `{ "ui": "talk" }` | a picture of the screen |
| `{ "pickup": "LF_Crackers" }` | the F-key pickup of the nearest copy (harness `pickup`: the vanilla `SCR_PickUpItemAction` on the client, as the key performs it) |
| `{ "do": { "verb": "use", "className": "LF_StoreTillAction", "entityName": "$entity" } }` | any harness verb; `$entity` is the placed building's name |
| `{ "state": "paid" }` | keeps the server's `state` fields as `paid.cash`, `paid.wanted`, ... |

Any step takes `expect` (a regex on the reply), `wait` (ms after it) and `why` (printed when it fails). Then `checks`
compare kept values (`{ "lt": ["paid.cash", "before.cash"] }`, `gt`, `eq`, `ne`, `in`) and `log` reads the server log
(`{ "re": "paid \\d+ for 3 item" }`, or `{ "last": "alarm sounding at <([^>]+)>", "near": [x,y,z], "within": 1.5 }`).
The supermarket's file talks to the shopkeeper, picks a torch, crackers and a water, pays, leaves clean, then walks out
with a dressing and checks the alarm on the shop-front box.

## Props stand on something: `surface.mjs` and `building check`
A typed height floated phones, lamps and small items over desks and counters (maintainer, 2026-11-17). Never type a prop's y
from a guess. `tools/buildings/pipeline/surface.mjs` snaps it at write time, offline (bounds from `prefabInfo`, the reader
`grid place` sizes by; support tops from the .xob colliders, `tools\cli measure --surfaces`): y = the top of the support under
the prop's footprint centre, minus the prop's bottom offset.
```
import { prefabChildren, snapItems, specSupports } from "../pipeline/surface.mjs";
put(P.desk, x, F, z, 0, 1, false, { id: "desk" });              // a row an item can stand on
put(P.phone, x + 0.2, 1.1, z, 200, 1, false, { on: "desk" });   // y is a hint; it lands on the desk top
put(P.lamp, x, 1.1, z);                                         // no `on`: the top under it, when one is within 0.4 m
const fixed = [...prefabChildren(prefabTextWithoutYourBlock), ...specSupports(spec)];  // what the prefab holds, the host, spec.check_supports
const snap = await snapItems(items, { fixed });                 // mutates item.y; snap.summary, snap.changes for a --report
```
- `on` is an item `id`, the 16-hex ID of an entity already in the prefab (or the store prefab), or a row index. A prop with no
  surface under its centre stops the generator by name.
- Without `on` a prop snaps only when it is more than 2 cm off a top under it. Wall kinds (posters, notices, clocks, ceiling
  lamps, switches) and `free: true` rows are left alone. Gun-store `decor.mjs` is the model; the town hall, general store and
  clothing store do the same.
- A support the prefabs do not hold (a counter the layer places, such as the general store till): `spec.json`
  `check_supports: [{ prefab, at, yaw, id, why }]`. The host building's own colliders count as supports.
- No generator (a hand-written prefab)? `tools\cli building check <slug> --fix` moves each floating or sunk prop onto its top.
- A prop that stood on a desk or shelf your cut removed (a typewriter on a pruned desk) goes with it (town-hall `shell.mjs`).

**`tools\cli building check <slug>` is required.** About 1 s, no Workbench: every small prop (under 0.8 m high and 1.2 m
across, not a wall or ceiling kind) of the building's prefabs must have its bottom within 2 cm of the top of what is under it,
a floor slab or the ground. `wt ship` runs it for a PR that touches `tools/buildings/<slug>/` or the pack's
`Prefabs/Buildings/<Dir>/` and refuses the ship while it fails (`tools/lib/pr-quality.mjs`); a machine without the game files
skips it. A prop that must look odd goes in `spec.json` `floaters_allow: [{ "match": "<entity ID or prefab regex>", "why": "..." }]`.
Keep `audit placement --grid --strict` as the in-world check once per pass: it sees what the offline check cannot.

## Planning a building's work

`tools\cli building plan <slug>` prints, as markdown (also `tmp/buildings/<slug>/plan.md`), what an agent brief needs
from the spec and the export: the prefabs to place and edit as `{GUID}path`; every furniture and fixture prefab with its
count, `measure` size and the heights things stand on; the hand-placed props; the rooms (ArchBuilder exports no room
outlines, so a room is the span of the furniture that names it: toilet and basin a WC, desk an office, racks and pallets
storage, gondolas and tills the sales hall) and the doors near each; every door and window with its wall; the store's
fixtures by `pick` index, its lines, tills and alarm; which parts can go to parallel agents and which are one agent's at
a time (`polished.blend` is a binary); and the proof list. Paste the sections a brief needs instead of retyping values.

## Changes to ArchBuilder

The pipeline uses ArchBuilder's operators, settings and licence check as an installed user would. It also reads,
without changing, the report its `reforger.export.run` returns (warnings, props placed per kind, furniture left out),
which the operator only summarises. The material fix is ours, in the export stage.

One change inside the add-on, made in the maintainer's local copy (`Documents\Reforger\blender-addons\arch_builder`,
a commit on its `main`) and copied to lf-testbox's copy at the same path:

- **One service door for a hall's back rooms** (2026-11-14, `interiors.py` `_layout_hall`, `civic.py`). A hall program
  (`('hall', kind, [rooms])`, a shop, supermarket, restaurant, workshop) gave every back room its own door onto the
  hall: the supermarket's stockroom, office and two WCs were four doors in a row on its back wall. A room written
  `@kind` in the list is now the only one that opens onto the hall; the rooms listed either side of it open into it
  through the partitions, near the hall end. ArchBuilder's `SUPERMARKET` ground program is `office, @storage:3, wc`
  (it was `storage:2, office, wc, wc, storage:2`). A stock ArchBuilder still builds the row of doors; to make the
  same supermarket elsewhere, make the same edit to those two places.
- **Shop front canopy, staff back rooms** (2026-11-14, #1143, `civic.py` `canopy`, `claim_extras`, `extras`, `sign`;
  `interiors.py` `_layout_hall`, `_furnish`). The SUPERMARKET profile has `canopy='SHOPFRONT'`: one slab cantilevered
  from the wall along the whole glazed front, no posts, a deep fascia in the profile's sign colour with returns at both
  ends, and the sign 1.0 m above the door head, clear of it (was a 3 m plate over the door on two thin posts). The back
  rooms of a hall program with an `@` hub are staff rooms (`room.staff`): the WC gets one toilet and a basin instead of
  a row of cubicles, and the storage room shop shelving (`ShelfUniversal_01_wide`) and pallets instead of bookshelves
  (`ShelfDecorative_01`, a living-room divider in the game).

## Store

A spec with a `store` block becomes one of our physical stores (`Feature/Shop`, #1143). The `store` stage writes a prefab
that inherits `<Name>.et` and adds `LF_ShopComponent` (prices from `Configs/Shop/ShopPrices.conf` by prefab),
`LF_StoreComponent` (stock, shelf points, boundary, restock) and one "Pay for goods" context with `LF_StoreTillAction`
at every register the export placed. Place that prefab, not `<Name>.et`. The whole file is written from the spec on every
run; never edit it by hand.

```json
"store": {
  "prefab": "Supermarket_Store", "key": "everon_supermarket",
  "boundary": { "center": [0, 0, 0], "half": [8.6, 0, 6.6] }, "restock_seconds": 120, "till": "CashierShop_01",
  "lines": [ { "item": "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et", "fixture": "ShelfShop_01", "pick": [4, 5], "boards": [2] } ]
}
```

- `key` is the store's saved-stock key (`storestock`); keep it when the building changes.
- `boundary` is store space (the building's): leaving it with unpaid goods is shoplifting. The supermarket's is the
  whole building, so the stockroom is inside.
- A line takes the fixtures `pick` names (the stage prints each fixture with its index, position and front) and the
  `boards` of each (the stage prints those too, lowest first). Each board is filled from measured data: its width and
  depth and the height of the surface above it, and the item's footprint. One or two rows by the board's depth, as many
  copies per row as the width takes, evenly spread, at most 20 on a board. A point is the fixture's position plus the
  board point turned by its yaw. `max` is one copy per shelf point and `restock` a sixth of it, unless the line gives
  them; `per_board` and `rows` lower the fill. An item too tall for the gap above a board is refused.
- **Folded piles.** A line with `"stack": N` turns every shelf point into a pile of N copies, one measured item
  height apart, bottom first, each with a seeded twist of up to 8 degrees written as the point's fourth number
  (yaw, relative to the store); the prefab gets `m_iStack N` and `max` counts every copy. The store stands only
  the bottom copy on the board, closes a gap when a middle copy is taken, and after a restart adopts each
  restored copy into its own pile slot. A pile taller than the air under the board above is refused.
- **Nothing in the layout is typed by hand.** `tools/buildings/pipeline/fixtures.json` is generated by
  `tools\cli fixtures` from `tools\cli measure` (collider surfaces of the vanilla models: a gondola's boards, a
  cooler's deck, a counter top; the bounds of every item a spec sells). Rerun it when a spec names a new fixture or item;
  `fixtures --check` compares the file to a fresh measure, as does `measure-selftest` where the game is installed.
  `store-fixtures-selftest` checks the script constants that depend on it (`LF_StoreComponent`'s snap and stray
  distances against the smallest gap between boards, the heights in `LF_Test_StoreSnapToBoard` against the boards).
  The store spawns each copy on the surface found a short trace under the point, so a few centimetres off still lands
  on the board.
- Saved stock is keyed by the store key and the item prefab, not by slot, so a new layout (or a new `max`) loads from
  an old save: the count is kept and the shelf shows as many copies as it has points (`docs/persistence.md` item 15).
- The item needs a price in `ShopPrices.conf`.
- `alarm` (optional): `{ "prefab": "{GUID}Prefabs/...et", "at": [x, y, z], "yaw": 180, "offset": [x, y, z], "reach": 10 }`,
  a child entity (a bell box on the shop front) with `LF_AlarmMountComponent`: a walk-out's site alarm, raised at the
  store's origin, sounds on it. `reach` must cover the box's distance from the store's origin (at most 12 m).

### A store in a vanilla building

A shop does not need an ArchBuilder export: a spec with a `vanilla` block (and no `archbuilder` block) names our own
prefab that inherits a base-game building, the way the vanilla variants (`ShopHouse_E_2I01t_foodstore.et`) add windows,
doors, a furniture child and a sign to their `_Base`. `generate` to `import` are not needed and pass at once (no
ArchBuilder, no Blender); `prefab` checks that our prefab inherits `vanilla.base`, that our files have their `.meta`,
and that every base-game reference in them is a real path with its real GUID; `store` writes the store prefab on top.
`budget` is optional (`min_doors` counts the door sets our prefab adds). `building plan` and `building proof` read the
same files.

```json
{
  "slug": "gun-store", "name": "GunStore", "red_line": "pack", "addon": "LF_Everon1989",
  "concept": "...", "prefab_dir": "Prefabs/Buildings/GunStore",
  "vanilla": {
    "base": "{9064E65F34097C8F}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_Base.et",
    "building": "ShopHouse_E_2I01t_gunstore",
    "furniture": "Furniture/ShopHouse_E_2I01t_furniture_gunstore_01"
  },
  "store": { "prefab": "GunStore_Store", "key": "everon_gunstore", "fixtures": [], "counters": [], "lines": [] }
}
```

Three store rows matter most for such a building (all optional, any store can use them):

- `fixtures`: `[{ "prefab": "{GUID}Prefabs/...et", "at": [x, y, z], "yaw": 90, "why": "..." }]`, shop fittings (shelves,
  a counter, a till, a display rack) written as children of the store prefab. `pick` counts them together with the
  furniture prefab's own children, so a line can stand goods on a shelf placed here, and a `CashierShop_01` placed here is
  a till.
- A **display-only** line, `{ "item": "{GUID}...et", "display": true, "fixture": "LF_WeaponRackCabinet_Display",
  "pick": [0], "kind": "long", "slots": [0, 1, 2] }`: the item's model stands in a display rack's slots. The copy is a
  plain entity with the item's `MeshObject` only, so it can be looked at and never picked up or taken; it is no stock and
  not on the till's list (sell it over a counter). Racks: `LF_WeaponRackCabinet_Display` and `LF_WeaponRackStand_Display`
  (core prefabs that land with the gun store), the FIA arsenal racks' models without the arsenal, service point and faction components, so they hand out
  nothing. Their slots (`racks` in `fixtures.json`, 10 `long` upright slots each, the cabinet also `pistol` and `other`)
  are read by `tools\cli fixtures` from the vanilla racks' `SCR_WeaponRackSlotEntity` children; `kind` picks long,
  pistol or other, `slots` indexes those (default all of that kind). A display line's item needs its model measured
  (`items[...].mesh`, also from `tools\cli fixtures`).
- `counters`: `[{ "prefab": "{F7EEC900E44C3CCA}Prefabs/Props/Commercial/LF_GunCounter.et", "at": [x, y, z], "yaw": 180,
  "outlet": "gun_counter" }]`, a counter child whose own `LF_ShopComponent` (overridden by its GUID) has
  `m_sOutletId` set: it sells that item-catalogue outlet's offers over the counter, behind their gates (the
  `FIREARM_PERMIT` on `gun_counter`). The prefab must carry `LF_ShopComponent` and the outlet must have offers in an
  `LF_ItemCatalogue.conf`.

A licensed counter has its own proof step in `proof.json` (`tools/buildings/pipeline/scenario.mjs`):
`{ "permit": { "licence": "FIREARM_PERMIT", "look": [x,y,z], "from": [dx,dy,dz], "rows": [0, 3], "items": ["Handgun_M9",
"Magazine_9x19_M9_15rnd_Ball"] } }`. A buyer with cash and no licence orders those price-list rows and is refused (the
slip must say "Not without a ..."); the harness `licence` verb then sends the `/licence` request (dev worlds and harness
builds only) and the same order is paid; the server log must show the refusal, the grant and `bought item=` for each item.

## Not done yet

- A delivery door at the back: ArchBuilder puts exterior doors on the street front only, so the supermarket's
  stockroom has none ([polish.md](polish.md) "Patterns").

- Several buildings in one export (ArchBuilder can export a street at once); the pipeline does one per spec.
- A building in the pack addon (`addon: LF_Everon1989`) is wired through the same `addon` field as the asset pipeline
  but untried.
