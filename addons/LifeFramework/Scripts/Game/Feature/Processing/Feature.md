# Processing — Design Requirements

> Context file for raw → product transformation. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

Processing is a **machine or bench you operate**. You carry the raw material to
the apple press, put it in, and get the product out. The machine is the
interface; standing at the bench and running it is the interaction. No "open a
processing menu from anywhere."

## Interaction pattern

Rung 1 today: `EL_ProcessAction` is an interact action that consumes all
configured inputs and produces all configured outputs (into inventory or
force-dropped).

Target: a physical processing prefab (bench, press, smelter) whose interaction
point *is* the machine, with an operation animation/timer, and the machine
visibly holding/expelling the product.

## V1 (shippable)

1. **Fix the free-output exploit**: `PerformAction` grants all outputs even
   when inputs are missing (`RemoveAmount` clamps to available). The server
   must re-validate the full input set before producing anything, and an empty
   input list must mean *no* outputs, not free outputs.
2. Server-side re-validation (client-evaluated `CanBePerformedScript` is not a
   gate).

## Iteration path

- **V2** — physical machine prefabs with operation timers.
- **V3** — quality/grade of output tied to input quality; machine upgrades.

## Current state

- `EL_ProcessAction` (`Feature/Processing/`) — recipe consume/produce.
  ❌ **free-output exploit**, no server re-validation, empty input list =
  free outputs.

## Dependencies

- `Gathering` / `Resources` (raw inputs), `Quantity` (stack-aware consume),
  `Money` (product value via `Trader`), `Jobs` / `Level` (processing perks).