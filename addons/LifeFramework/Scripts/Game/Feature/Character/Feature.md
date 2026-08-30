# Character — Design Requirements

> Context file for the player character — spawning, inventory, hand-carry.
> Written against `docs/design-philosophy.md`.

## Intent

The character is how the player **exists in the world**. Everything here is
physical: you spawn into the world at a location, you carry things in your
hands and inventory, you pick items up and put them down. No menu replaces any
of it.

## Interaction pattern

Rung 1.

- **Spawning** — account-aware: loads the account, spawns the active
  character's prefab at a faction-appropriate spawn point, applies the default
  loadout.
- **Inventory** — the vanilla inventory system plus `Quantity` stacks and the
  hand-carry state machine (`NONE→READY→AWAIT_HOLSTER→ACTIVE→AWAIT_SWAP→NONE`).

## V1 (shippable)

1. Hand-carry: the weapon-holster flow is client-only (the server never sees
   the transition) and `OnGadgetModeSet` lacks the `HasLocalControl` guard —
   this must converge to a server-visible state.
2. Spawning fail-safe — DONE (verified 2026-08-30): `GetCreationPosition`
   returns `bool` and `CreateCharacter` aborts the spawn when no spawn point
   resolves instead of dropping the player at origin.
3. Loadout fill: an item that finds no matching storage is silently dropped —
   it should log.
4. **Death — defined (verified 2026-08-30):** the dead body stays in the world
   with everything it carried (only removed by a server restart or cleanup);
   the player respawns through the death screen
   (`EL_DeathScreen`, opened by `SCR_RespawnComponent.GetOnRespawnReadyInvoker_O`),
   whose Respawn button re-runs the account-aware spawn via
   `RpcAsk_EL_Respawn` → `EL_SpawnLogic.RespawnPlayer_S`.

## Iteration path

- **V2** — carry physics polish, two-handed carry, drop/place interactions.

## Current state

- `Feature/Character/Spawning/` — `EL_SpawnLogic` (account-aware spawn, death →
  death screen), `EL_DeathScreen` + `EL_RespawnComponent` (death-screen bridge),
  `EL_SpawnPointsProvider`, `EL_FactionSpawnPoint`.
- `Feature/Character/Inventory/` — `HandCarry/` state machine + modded vanilla
  inventory wiring; `Quantity`'s `ScriptedInventoryStorageManagerComponent`
  adds the RPC split/transfer endpoints.
- `Components/InventorySystem/` — trader/inventory storage components
  (see `Trader`).

## Dependencies

- `Account` (identity), `Quantity` (stacks), `Survival` (body state),
  `Money` (cash in inventory), `Houses` (owned storage).