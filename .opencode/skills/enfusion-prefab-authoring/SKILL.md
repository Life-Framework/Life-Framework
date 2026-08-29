---
name: enfusion-prefab-authoring
description: "Use when creating, editing, duplicating, or reviewing entity prefabs (.et files) for this mod. Encodes prefab inheritance, component wiring, GUID references, and the pitfalls that break a prefab silently. Trigger on 'new prefab', 'edit this prefab', 'duplicate prefab', 'add a component to', 'inherit from', '.et file'. Use ONLY when the prefab lives under addons/LifeFramework/Prefabs."
---

# Prefab Authoring

Prefabs are the data shape of the mod. A prefab that inherits, wires components, and references resources correctly is half the feature. A prefab with a broken reference is a silent failure: it compiles, and the entity is invisible, inert, or missing at runtime.

**Canonical source.** The factual rules here are mirrored in `AGENTS.md` ("Enfusion data rules"), the portable contract every AI tool reads. When they diverge, AGENTS.md wins. This skill adds the workflow: how to author and inspect prefabs.

## Inheritance

- Prefer inheritance over duplication. A new item extends its base (`Food_Base`, `Drink_Base`, `RoleplayItem_Base`, `ConsumeableItem_Base`, `HandCarryItem_Base`) and changes only what differs.
- `prefab inspect` (the MCP `prefab` tool) reads the full ancestor chain and returns the merged component set. Use it to know what a prefab actually provides before editing. A child can override inherited component values; the merge is the truth.
- Duplicate base-game prefabs with the MCP `game_duplicate` tool, which resolves the ancestor chain and gives the copy a new GUID. Hand-copying a vanilla `.et` produces duplicate GUIDs, which `tools\cli validate` rejects.
- Before moving or renaming a prefab, check `find-references` / `list-dependencies`: saved worlds and configs reference prefabs by `{GUID}path`. A move without updating references breaks worlds.

## Components

- A scripted behavior is a component on the prefab. Find the right component with `component_search` first, then add it with its properties set.
- The `MeshObject` component's `Object` property must point at a real `.xob` model or the entity is invisible in-game. Interactive prefabs without a visible mesh are the classic silent bug.
- Hard component dependencies are enforced by the engine, not by you: a `DamageManager` needs `RplComponent` and `HitZone`; a `WeaponComponent` needs a `RigidBody`. The `prefab` tool flags these. Respect them.
- Signal names, collider names, and bone PivotIDs are implicit couplings the tools cannot check. When you wire an entity that interacts with bones or signals, verify the names against the actual model and the vanilla prefab that proves the pattern.

## Resources and references

- Every prefab has a sibling `.meta` with a `{GUID}`. New prefabs get registered via the Workbench (`wb_resources` register) so they get a GUID. Never invent a GUID.
- References inside a prefab use the `{GUID}path` resource form. The GUID, not the path, is the identity. `asset_search` returns the correct `{GUID}path` for base-game assets.
- After creating or editing prefabs, run `tools\cli validate` (repo hygiene) and `tools\cli test` (world boots, resources resolve).

## This repo's bases

- Item bases: `Prefabs/Items/Core/*`, `Prefabs/Items/Food/Food_Base`, `Prefabs/Items/Drinks/Drink_Base`.
- Character bases: `Prefabs/Characters/Core/Character_Base`, `Character_Roleplay`, `Npc_Base`.
- Vehicle base: `Prefabs/Vehicles/Core/Vehicle_Base`.
- Processor base: `Prefabs/Processor/ItemProcessor_Base`.
- Tool base: `Prefabs/Tools/Tool_Base`.
- When in doubt which base to extend, look at the sibling prefab in the same folder that already does the closest thing.