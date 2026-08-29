---
name: principle-subtract-before-you-add
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are sequencing an addition, refactor, or rewrite of Enfusion code or prefabs. Remove dead weight first, then build on the simpler base. Never auto-load for routine work."
---

# Subtract Before You Add

Remove dead weight, redundant validators, and stub references first, then build on the simpler base.

**Why:** Building on top of a messy base bakes the mess into the new work. A stub prefab that half-works, a component that is never attached, or a config key nothing reads will silently shape the design around it.

**Rule:**
- Before extending a subsystem, inventory it. What is referenced, what is dead, what is half-built.
- Delete the dead and the stub first. Finish or remove the half-built; do not build around it.
- Only then add the new behavior, on a base that has one fewer thing to reason about.
- Sequence it explicitly: subtract is phase one, add is phase two. Never interleave them into one undifferentiated diff.

**In this repo:**
- Search for `EL_` classes with no attach site and prefabs with no referencing config before you add to an area.
- `tools\validation\validate-repo.ps1` already enforces structural hygiene (no orphan `.meta`, no duplicate GUIDs). Run it after a subtract so the cleanup is proven, not asserted.
- A feature area that was already removed (Group, Garage, Property, FurnitureShop) should stay gone. Do not resurrect scaffolding for it.