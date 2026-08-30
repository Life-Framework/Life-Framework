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
2. Spawning: `GetCreationPosition` leaves `out` params unset when no spawn
   point exists, and the loadout recursion has no depth guard — fail-safe these
   (log + degrade, never VME).
3. Loadout fill: an item that finds no matching storage is silently dropped —
   it should log.

## Iteration path

- **V2** — carry physics polish, two-handed carry, drop/place interactions.

## Current state

- `Feature/Character/Spawning/` — `EL_SpawnLogic`, `EL_SpawnPointsProvider`,
  `EL_FactionSpawnPoint`.
- `Feature/Character/Inventory/` — `HandCarry/` state machine + modded vanilla
  inventory wiring; `Quantity`'s `ScriptedInventoryStorageManagerComponent`
  adds the RPC split/transfer endpoints.
- `Components/InventorySystem/` — trader/inventory storage components
  (see `Trader`).

## Dependencies

- `Account` (identity), `Quantity` (stacks), `Survival` (body state),
  `Money` (cash in inventory), `Houses` (owned storage).