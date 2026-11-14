# Building pipeline: spec to Reforger building

**Buildings are made with [ArchBuilder](https://arribas22.github.io/archbuilder/)**, a Blender add-on by Arribas. It is
the preferred way to make a building for Life Framework: it builds walls, floors, stairs, roofs and rooms, cuts the
openings to the size of the game's own doors and windows, and exports a Reforger model with colliders, materials and a
building prefab. Buy it, install it and activate your own key (see Setup). The repository holds none of its source.

`tools\cli building` turns a committed spec into a building in the addon by driving your installed copy of ArchBuilder in
headless Blender, then moving its export into the addon and compiling it in a hidden Workbench. What the first runs
taught us is in [`docs/architecture/building-pipeline-spike.md`](../architecture/building-pipeline-spike.md).

```
tools\cli building run <slug> [--from <stage>] [--only <stage>] [--force]
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
| `export` | ArchBuilder's Reforger export into a staging addon, `tmp/buildings/<slug>/export/`, with its Workbench step off. Counts the vanilla door sets and windows; fewer doors than `budget.min_doors` fails | The tool |
| `import` | Refuses any file carrying the licence watermark, moves the export into the addon with new GUIDs, compiles the model in a hidden Workbench and reads the `.xob` back | The tool |
| `prefab` | Every reference in the building's files resolves: our own by `.meta`, the base game's by path and GUID in the game's resource database. Door sets, windows, the model and its collider surfaces are there | The tool; then a placement and a look |

A finished stage is skipped on the next `run`. `--only review` reruns one stage, `--from export` reruns it and everything
after it, and `--force` reruns from the start.

- **A failed import** can leave new files beside old ones in the addon. Rerun `--from import`: it rewrites every file and
  keeps every GUID. Commit only after the `prefab` stage passes.
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
                   "wall_mat": "AB_Stucco_White", "roof_style": "FLAT", "do_interior": true, "do_furniture": false },
  "export": { "furniture": false, "lamps": true, "vanilla_props": true, "lods": true, "probes": "ONE", "bsp": false },
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
- The same seed and settings build the same building. A different look is a different seed.
- `red_line` is `core` or `pack` ([core-vs-pack](../architecture/core-vs-pack.md)). A building is what this server looks
  like, so it is usually `pack`; until the pack loads it lands in the core with a row in
  [core-pack-inventory.md](../architecture/core-pack-inventory.md).

## Where the export lands

| ArchBuilder writes | The addon gets |
|---|---|
| `Assets/ArchBuilder/<Name>/<Name>.fbx`, `.xob.meta` | `asset_dir/` |
| `Assets/ArchBuilder/Materials/AB_*.emat` | `Assets/Buildings/Materials/`, shared by every building. An export that would change one is refused |
| `Prefabs/ArchBuilder/<Name>/<Name>.et`, `<Name>_Base.et`, `Furniture/` | `prefab_dir/` |
| `EXPORT_INFO.txt`, `ArchBuilder_import.txt`, `addon.gproj`, its Workbench plugin | nothing: notes, its own queue, its own project and plugin |

`<Name>_Base.et` is the shell: the model plus the vanilla door sets, windows and the light probe as children.
`<Name>.et` inherits it and adds the furniture prefab when there is one. Place `<Name>.et`.

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
3. The door and the PR pictures on a **full-quality** session, never lowspec: what a building's check looks at is its
   materials, glass, LODs and doors, which the lowspec profile's lowest textures hide.
   ```
   node agent-box/session.mjs up --profile full
   node agent-box/session.mjs building-proof --entity <Entity> --side s --shot
   tools\cli shots area <x,z>
   node agent-box/session.mjs down
   ```
   `building-proof` stands on the street side, checks the F key offers the vanilla door, and has the server open and
   close it (`agent-box/proofs/building-proof.mjs`).
4. Walking through the rooms and a second player seeing the door move stay manual (the PR's `## Manual testing`).

## Changes to ArchBuilder

None. The pipeline uses only ArchBuilder's operators, settings and licence check as an installed user would. If a change
inside the add-on is ever needed, it is listed here in our words, so other users can make the same change or fall back.

## Not done yet

- Shop interiors: the supermarket is exported without furniture so our shop can fill its shelves. Next steps:
  [the spike](../architecture/building-pipeline-spike.md#supermarket-next-steps).
- Several buildings in one export (ArchBuilder can export a street at once); the pipeline does one per spec.
- A building in the pack addon (`addon: LF_Everon1989`) is wired through the same `addon` field as the asset pipeline
  but untried.
