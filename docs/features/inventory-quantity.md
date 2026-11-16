# Inventory / Quantity

### `LF_QuantityComponent` — `Feature/Quantity/LF_QuantityComponent.c`
- ✅ Virtual stack system: `AddQuantity`/`RemoveQuantity` clamp to
  `[0, max]` (0 = unlimited), `SetQuantity(0)` deletes the entity,
  `CanCombine`/`Combine` (conservation: source decrease == dest increase ==
  transferred), `CombineQuantities` (2 and 3 yield 5), `Split`,
  `QuantityRemainingAfterSplit` (5 into 2 leaves 3), `SortByQuantity`
  (descending), `GetRemainingCapacity` (verified 2026-10-11).
- ✅ **In-inventory split mint closed** (verified 2026-11-07): the
  split stack is sized and marked keep-separate before `TryInsertItemInStorage`,
  so `HandleOnItemAdded` no longer merges the spawned init quantity (100 for
  MoneyStack) into the source. Split 1 used to add 99. Pin test
  `security/inventory-split-no-mint`.
- ✅ Stack tier meshes hold their `Resource` while applied, so a tier swap no longer logs
  `Usage of released BaseResourceObject` (2026-11-08, `quantity/operations` visual path).
- ✅ **Transfer intent lifetime** (2026-11-08, test `robustness/lifetime`): `OnDelete` removes
  the entity's intent (no dead keys); an intent older than `INTENT_TTL_MS` (5 s)
  belonged to a failed move and is ignored by the next add.
  `LF_DefaultLoadoutItemQuantityComponent` logs `LF_Debug.Error` instead of
  crashing on a prefab without `LF_QuantityComponent`.
- ✅ **Storage panel refresh joins vanilla batching** (#977, 2026-11-14): the quantity menu's three
  direct `Refresh()` calls now call `RequestRefresh()`, so a split or merge rebuilds each panel
  once; on gamepad the focused source panel consumes the request itself. The 10 ms debounce stays.
- ⚠️ `Split` with `splitSize` 0 spawns an item then deletes it via
  `SetQuantity(0)`; `ExtractQuantityComponents` inserts null components with
  no check; `HandleOnItemAdded` may refresh UI on a just-deleted entity.

### `LF_InventoryUtils` (Quantity) — `Feature/Quantity/LF_InventoryUtils.c`
- ✅ Quantity-aware `GetAmount`/`AddAmount`/`RemoveAmount` returning *actual*
  amounts; AddAmount fills stacks then spawns; RemoveAmount drains smallest
  stacks first (ascending sort).
- ⚠️ `AddAmount` fills **largest** stacks first (descending) while Remove
  drains smallest — asymmetric by design, untested. Non-quantity
  `RemoveAmount` permanently deletes entities. `GetAmount` returns −1 on
  null target.

### `ScriptedInventoryStorageManagerComponent` (Quantity) — `Feature/Quantity/ScriptedInventoryStorageManagerComponent.c`
- ✅ RPC endpoints for split/transfer/intent (`RPC_LF_RequestQuantitySplit`,
  `RPC_LF_QuantityTransfer`, `RPC_LF_SetTransferIntent`), all server-side,
  range-gated.
- ✅ **Ownership gate added** (verified 2026-08-30): `LF_CanManipulateOwned`
  rejects another living player's carried items (only your own storage or
  unowned containers/ground loot); the old range-only anti-cheat is closed.
- ✅ **Locked owners refused** (verified 2026-11-07):
  `LF_CanManipulateOwned` also refuses a storage whose owner has a locked
  `LF_VehicleLockComponent` or `LF_LockComponent` (`LF_IsStorageOwnerLocked`).
  Pin test `security/locked-storage-refused`.

### Hand-Carry — `Feature/Character/Inventory/HandCarry/`
- ✅ `LF_HandInventoryStorageComponent` state machine
  (`NONE→READY→AWAIT_HOLSTER→ACTIVE→AWAIT_SWAP→NONE`), hands-only storage,
  modded vanilla SCR inventory/gadget wiring.
- ✅ (#1846) The courier parcel left this system: it is a core hand tool (`LF_HandToolComponent`, weapon quick slot), so `LF_HandCarryComponent` no longer forwards slot changes to it; `ShouldGadgetBeDropped` still keeps a living carrier's parcel in the hands (sprint, stance). The money bag and contraband crate are the hand-carry users.
- ✅ Missing `CharacterControllerComponent`, weapon manager, or inventory
  manager logs `LF_Debug.Error("HandCarry", ...)` and skips that step instead of
  a null dereference (2026-11-07).
- ⚠️ Weapon-holster flow is client-only (server never sees the transition);
  `OnGadgetModeSet` lacks the `HasLocalControl` guard its siblings have;
  `SCR_CharacterControllerComponent` destructor touches `GetOwner()` at
  teardown.

