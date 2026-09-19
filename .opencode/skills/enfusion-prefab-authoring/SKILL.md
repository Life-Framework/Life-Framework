---
name: enfusion-prefab-authoring
description: "Use when creating, editing, duplicating, or reviewing Life Framework .et prefabs. Use resolved ancestry, correct GUID references, and component validation rather than hand-copying vanilla data."
---

# Prefabs

`AGENTS.md` owns the prefab rules. Use this sequence:

1. Find the closest existing base and inspect the full ancestry with `prefab`.
2. Prefer inheritance and a minimal delta. Use `game_duplicate` for vanilla
   sources; never hand-copy GUID-bearing files.
3. Find components with `component_search`. Set required properties and verify
   implicit signal, collider, bone, and mesh names against a working sibling.
4. Use `{GUID}path` references from `asset_search`; register new resources so
   their `.meta` GUID is generated. Check references before moving a resource.
5. Run `tools\cli validate` and the cheapest runtime/boot proof for the prefab.

An interactive entity needs a real `MeshObject.Object` `.xob`; component
dependencies and inherited values are resolved by `prefab inspect`.
