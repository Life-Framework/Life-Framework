---
name: enfusion-resource-authoring
description: "Use when creating, editing, duplicating, or reviewing Life Framework resources: .et prefabs, .conf configs (mission, faction, catalog), .layout UI, input confs, localization, DebugWorld .layer entities, and world interactables. Use resolved ancestry and the MCP generators/validators instead of hand-guessing Enfusion serialization."
---

# Enfusion resources

`AGENTS.md` owns the golden rules (unique immutable GUIDs, `{GUID}path` with
real casing, localization keys). This skill is the authoring sequence.

## Prefabs (.et)

1. Find the closest existing base and inspect the full ancestry with `prefab
   inspect`. Read resolved values with `wb_read_props`; never hand-walk
   `parent` chains.
2. Prefer inheritance and a minimal delta. Duplicate vanilla prefabs with
   `game_duplicate`; never hand-copy GUID-bearing files. Every new resource
   needs a `.meta` with a unique GUID (let Workbench or the MCP generate it).
3. Find components with `component_search`. Override an inherited component by
   its GUID instead of adding a second one of the same class. Verify implicit
   signal, collider, bone, and mesh names against a working sibling.
4. A `MeshObject` needs a real `.xob` model or the entity is invisible.
5. Use `{GUID}path` references from `asset_search`. Map references before a
   move or rename; it breaks every reference, saved worlds included.

## Materials

A mod item that must show a texture bakes its real `.emat` into the `.xob`
(MoneyStack.xob bakes CashMaterial.emat; Apple.xob bakes Apple.emat). A prefab
`MaterialAssignClass { AssignedMaterial }` override over an `.xob` that bakes
the placeholder `Common/Materials/Default.emat` is fragile: when the override
does not apply, the mesh renders white. The mining bars, nuggets, ores, and
WoodPlank shipped that shape and went white. `validate-material-chains` (part
of `tools\cli validate`) checks every `AssignedMaterial`/texture reference and
warns on the placeholder bake. Texture rendering can only be proven visually
(rendering Workbench play mode or the agent box); the dedicated server has no
renderer and `MeshObject.GetMaterials` returns empty there.

## World interactables (action contexts)

A new world interactable runs `node tools/actions/place-action-contexts.mjs` so
its `UserActionContext` Offset, Radius, and Height come from the mesh. The
point sits on the part the player uses (the fruit, the rock, the door leaf),
not the pivot; the vanilla default Offset `0 0 0`, Radius `0.650`, Height `0`
is the door hinge. A mesh taller than about 1.5 m gets a Height so the volume
is a capsule along the mesh. Handheld pickups stay as they are.
`validate-action-contexts` fails on an interactable left at the default.

## DebugWorld layers

`Worlds/DebugWorld/DebugWorld.ent` is 0 bytes on purpose; content lives in
`Worlds/DebugWorld/DebugWorld_Layers/*.layer`. The generic world entity is
declared in `default.layer`; props in a layer loaded before it get no physics
body and the player walks through them. Features have their own layer files;
the roadmap street (`Roadmap_*` entities) lives in `Roadmap.layer`. Do not
"repair" the empty `.ent`. `validate-debugworld-layer-order` and
`validate-debugworld-layout` (overlapping area boxes, raised ground props,
footprint intersections) run in `tools\cli validate`.

## Configs (.conf)

- Mission, faction, catalog, editor-placeable: use `config_create`, then
  inspect references and keep the existing mission structure.
- A same-GUID `.conf` override merges over the inherited file; it does not
  replace it.

## Layouts and input

- Layout: use `layout_create` or `layout_recipe`; inspect the base layout before
  inheriting. Preserve inherited component GUIDs, give widget instances unique
  GUIDs and names, and create the sibling `.layout.meta`. Detail and the input
  binding rules are in **enfusion-blast-radius**.
- Input: confirm the action exists, is in the active context, and has keyboard
  plus `gamepad0:` bindings.

## Localization

Add the key to `Language/everonlife_localization.st` (append; never rewrite the
table) with a translator `Comment`, then run `tools\cli regen-localization` to
rebuild the per-language `.conf` tables. Reference `#Key` from layouts and
scripts. A key missing from a language renders as the raw key.

## Finish

Run `tools\cli validate`, then the cheapest runtime or boot proof for the
resource (an `EL_Test` that loads it, or the DebugWorld).
