# Quantity — Design Requirements

> Context file for the item-stack engineering layer. Written against
> `docs/design-philosophy.md`.

## Intent

Quantity is **engineering, not a player-facing feature**. It is the layer that
lets items stack, split, and transfer with conservation of count. The player
never sees it as "the quantity system" — they see a shelf with 6 cans and take
2. Everything else (cash, food, gathered ore) relies on it.

## Interaction pattern

No direct player-facing rung. It exists so rung-1/2 interactions can carry
stacks: `EL_QuantityComponent` (virtual stacks, combine/split), `EL_InventoryUtils`
(quantity-aware add/remove returning actual amounts), and
`ScriptedInventoryStorageManagerComponent` (RPC endpoints for split/transfer/
intent).

## V1 (shippable)

1. **Ownership validation on the RPCs**: anti-cheat is range-only — any entity
   within 10 m can trigger splits/transfers of another player's items. Bind
   split/transfer to the owner/server intent.
2. Fix `Split` with `splitSize` 0 (spawns an item then deletes it via
   `SetQuantity(0)`).
3. `ExtractQuantityComponents` inserts null components with no check; guard it.
4. `HandleOnItemAdded` may refresh UI on a just-deleted entity — null-guard.

## Iteration path

- **V2** — overflow carry (partial pickup into partial stacks), drag-copy of
  stacks.
- **V3** — bulk-transfer between owned storages.

## Current state

- `EL_QuantityComponent` / `EL_InventoryUtils` (`Feature/Quantity/`) — stacks,
  combine, split, actual-amount add/remove. ⚠️ `AddAmount` fills largest stacks
  first while `Remove` drains smallest (asymmetric by design, untested).
- `ScriptedInventoryStorageManagerComponent` — split/transfer/intent RPCs,
  range-only gate. ⚠️ **no ownership validation**.
- `Persistence/` — quantity save data.

## Dependencies

- `Money` (cash stacks), `Shop`/`Trader` (stack-aware purchase/sale), `Survival`
  (consumables), `Character` (inventory integration).