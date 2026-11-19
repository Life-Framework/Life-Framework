# Processing — Design Requirements

> Context file for raw → product transformation. Written against
> `docs/design.md` (in-world first).

## Intent

Processing is a **machine or bench you operate**. You carry the raw material to
the apple press, put it in, and get the product out. The machine is the
interface; standing at the bench and running it is the interaction. No "open a
processing menu from anywhere."

## Interaction pattern

Rung 1 today: `LF_ProcessAction` is an interact action that consumes all
configured inputs and produces all configured outputs (into inventory or
force-dropped).

Target: a physical processing prefab (bench, press, smelter) whose interaction
point *is* the machine, with an operation animation/timer, and the machine
visibly holding/expelling the product.

## V1 (shippable)

1. **Fix the free-output exploit — DONE** (verified 2026-08-30): the server
   validates the full input set and recipe shape before producing anything;
   failed input/output mutations roll back.
2. Server-side re-validation (client-evaluated `CanBePerformedScript` is not a
   gate).

## Iteration path

- **V2** — physical machine prefabs with operation timers.
- **V3** — quality/grade of output tied to input quality; machine upgrades.

## Current state

- `LF_ProcessAction` (`Feature/Processing/`) — recipe consume/produce with
  server-side recipe/input validation and rollback on failed mutations.

## Dependencies

- `Gathering` / `Resources` (raw inputs), `Quantity` (stack-aware consume),
  `Money` (product value via `Trader`), `Jobs` / `Level` (processing perks).

## Recycle rules (#3347)

- **Player sees:** nothing yet; this is the rule a later recycler uses (an item breaks into part of its inputs).
- **Owner and side:** `LF_RecycleRules` (pure logic) decides on the server; `LF_RecycleConfig` loads the recipes.
- **Data:** `Configs/Processing/LF_Recycle.conf`, empty in the core (a pack adds recipes). A recipe is an item id,
  a return percent (1 to 99) and the parts it is made of; each part comes back at the percent, rounded down. At load a
  recipe is refused (logged as `[LFDebug:Processing]` with the reason) when the percent is 100 or more, an item or part
  is not in the catalogue, nothing would come back, or the parts that come back are worth as much as the item at catalogue
  `m_iPrice` (no profit loop, same prices as #2848). An item with no recipe gives nothing. Nothing is persisted.
- **Proof:** `processing/recycle-rules` (LOGIC).
