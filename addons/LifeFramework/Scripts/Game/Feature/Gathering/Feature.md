# Gathering — Design Requirements

> Context file for in-world resource gathering. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

Gathering happens **in the world, on the resource**. The player picks apples
from a tree, mines ore from a mining node, chops wood from a tree — the tree
and the node are the interface. There is no "gather from a menu" anywhere in
the design.

## Interaction pattern (in-world)

Today: rung 1. `EL_GatherAction` is an interact action that grants a configured
amount of a configured item into the user's inventory, with a tool requirement
(right hand / gadget / anywhere in inventory) and a per-resource gather counter
with cooldown restock. `EL_MiningArea` scopes mining to an area.

Target: the resource is a **stateful physical object**:

- A fruit tree holds a visible number of apples; picking removes them from the
  tree (the tree visibly empties), and it regrows on a timer.
- A mining node depletes with each hit and crumbles / respawns (see
  `Resources`, the destructible-node backing).
- The tool matters: bare hands for apples, an axe for wood, a pickaxe for ore.
  The tool is re-validated **on the server at gather time**, not just in the
  client gate.

## V1 (shippable)

1. Server-side re-validation of the tool in `PerformAction` — the current
   client-only `CanBePerformedScript` gate is an exploit (a hostile RPC can
   gather without the tool).
2. One reference gather loop, pick apples from a tree, with the tree visibly
   changing state and restocking.
3. Ensure `m_GatherAmountMax` behaves sanely and the timeout attribute is read
   in the units it claims (see `docs/features.md`).

## Iteration path

- **V2** — visible depletion/regrowth on trees and nodes; area-scoped yields.
- **V3** — tool durability; resource quality/rarity (better yields from richer
  nodes).
- **V4** — gathering perks from `Level` (speed/amount bonuses already defined on
  `EL_PlayerLevelComponent`) and jobs.

## Current state

- `EL_GatherAction` — interact action; **tool not re-validated server-side**.
- `EL_MiningArea` — spatial gate for mining.
- Prefabs: `Prefabs/Vegetation/Tree/`, `Prefabs/Vegetation/Crops/`,
  `Prefabs/Resources/Mining/`, `Prefabs/Resources/WoodCutting/`;
  `Assets/Resources/Mining/`, `Assets/Resources/WoodCutting/`;
  `Particles/Gather/`, `Particles/WoodCutting/`.

## Dependencies

- `Resources` (the destructible node components this sits on).
- `Money` (gathered goods feed `Trader`/`Shop`).
- `Jobs` / `Level` (gather rewards and perks).
- `Processing` (raw → product).
- `Resources` (the destructible node components this sits on).