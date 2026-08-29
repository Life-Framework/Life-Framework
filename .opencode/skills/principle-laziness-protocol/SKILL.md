---
name: principle-laziness-protocol
description: "Use ONLY when explicitly directed by the life-mode skill or another skill, or when you are tempted to add abstraction, layers, or signal threading to Enfusion code or prefabs. Bias to deletion and the smallest change that solves the problem. Never auto-load for routine work."
---

# Laziness Protocol

Bias toward deletion and the smallest change that solves the problem.

**Why:** Every line, component, and prefab node in a mod is a future maintenance cost. Life frameworks bloat precisely because contributors add systems "just in case". The smaller the change, the fewer the places a bug can hide and the easier the in-game verification.

**Rule:**
- Solve the problem in front of you, not the one you predict.
- The best change is often a deletion. Dead feature folders, redundant validators, stub prefabs, and commented-out branches are candidates first.
- Before adding an abstraction, count its callers. One caller means the abstraction is a wrapper, not a design.
- A new class, component, or config field must earn its place by what it removes from the code that would otherwise exist.

**In this repo:**
- This mod already deleted whole feature groups (Group, Garage, Property, FurnitureShop). Follow that precedent.
- Prefer editing an existing `EL_` component over adding a sibling that duplicates it.
- A new prefab that inherits a base and changes one value is smaller than a copy of the base with edits baked in. Inheritance is the lazy move that stays lazy.