# Resources — Design Requirements

> Context file for the physical gather nodes. The backing for `Gathering`.
> Written against `docs/design-philosophy.md` (in-world first).

## Intent

The world holds **destructible resource objects** — trees, ore nodes — that the
player damages with the right tool and that yield their resource. The node is a
physical thing with a life of its own, not a menu option.

## Interaction pattern (in-world)

Rung 1. `EL_DestructibleResourceComponent` + `EL_DestructibleResourceHitZone`
gate damage to configured tools, apply per-hit damage, and play FX on hit.

Target: nodes that visibly deplete, crumble, and respawn; damage sources that
are identified robustly (the current damage-source prefab string comparison is
casing-fragile); hits that are ignored give feedback. A wrong tool silently
dealing 0 is a design bug — it must say "no".

## V1 (shippable)

1. Non-tool or unmapped damage **does not silently no-op** — log it
   (`EL_Debug`) and give the player feedback.
2. Robust damage-source identification (no casing fragility).
3. Depletion state on the node with a respawn path.

## Iteration path

- **V2** — node degradation tiers (rich → normal → depleted) that change the
  mesh, matching what `Gathering` sees.
- **V3** — tool quality interaction (better tool, faster node).

## Current state

- `EL_DestructibleResourceComponent` / `EL_DestructibleResourceHitZone`
  (`Feature/Resources/`) — tool-gated damage, per-hit damage, FX.
- ⚠️ Unmapped or wrong-tool damage deals 0 silently; damage-source string
  comparison is fragile on casing.
- Prefabs: `Prefabs/Resources/Mining/`, `Prefabs/Resources/WoodCutting/`,
  `Prefabs/Vegetation/Tree/`, `Prefabs/Vegetation/Crops/`.

## Dependencies

- `Gathering` (the actions that use these nodes).
- `Processing` (what the gathered resource becomes).
- `Particles/Gather/`, `Particles/WoodCutting/` (hit FX).