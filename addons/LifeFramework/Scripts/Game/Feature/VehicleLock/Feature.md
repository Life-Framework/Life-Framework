# VehicleLock — Design Requirements

> Context file for vehicle ownership and locking. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

A vehicle is a **physical thing you own and lock**. You approach your car, unlock
it with your key, get in, drive; you lock it when you leave. Ownership is
established physically — at the car-lot purchase (`Shop`) or a private sale —
and the key is a carried object, not a flag in a menu.

## Interaction pattern

Rung 1. `EL_VehicleLockComponent` (lock state), `EL_VehicleKeyComponent` (key
item), `EL_OpenVehicleLockAction` / `EL_VehicleLockAction` (interactions),
`EL_VehicleStorageComponent` (owned trunk). `EL_VehicleKeyManager` coordinates.

Target: the full ownership loop — registered purchase → keys handed over →
lock/unlock at the vehicle. `LicensePlate` is the visible registration of the
same ownership chain.

## V1 (shippable)

1. Verify the lock/unlock and trunk-access paths are server-authoritative and
   key-validated (who can unlock, from where) — ownership must never be a
   client-side toggle.
2. Keys must be persisted per vehicle across restarts (restart proof, not
   in-session).

## Iteration path

- **V2** — multi-key vehicles (owner + copies), remote lock.
- **V3** — hot-wiring / car theft as a `Crime` event, alarm systems.

## Current state

- `EL_VehicleLockComponent`, `EL_VehicleKeyComponent`, `EL_OpenVehicleLockAction`,
  `EL_VehicleLockAction`, `EL_VehicleKeyManager`, `EL_VehicleStorageComponent`
  (`Feature/VehicleLock/`).
- Prefabs under `Prefabs/Vehicles/`.

## Dependencies

- `Shop` (car-lot purchase), `LicensePlate` (registration), `Police` / `Crime`
  (stolen vehicles), `Account` (ownership identity).