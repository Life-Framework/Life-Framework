# LicensePlate — Design Requirements

> Context file for vehicle registration plates. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

A plate is a **physical object on a physical vehicle** that ties the car to its
registered owner. Registration is a DMV counter event; the plate is what the
police check. This is the visible proof of the `VehicleLock` ownership chain.

## Interaction pattern

Rung 1 for the plate itself (attached to the vehicle), rung 3 for
registration (DMV counter).

Today: `EL_LicensePlateManager` and generators produce `"AA BB NNNN"` plates;
prefabs under `Prefabs/Vehicles/LicensePlate/` and
`Assets/Vehicles/LicensePlate/`; `UI/Layouts/LicensePlate/`.

## V1 (shippable)

1. **Fix the generator off-by-ones**: `Math.RandomInt(0, 25)` excludes `Z` and
   `Math.RandomInt(100, 9999)` excludes `9999`.
2. Add a **registry of issued plates** — no uniqueness guarantee today, so two
   cars can share a plate and police checks are meaningless.
3. `EL_LicensePlateManager` needs a destructor to clear `s_Instance`.

## Iteration path

- **V2** — DMV registration counter issuing plates to owned vehicles.
- **V3** — police plate lookup (reads the registry), plate-related infractions.

## Current state

- `EL_LicensePlateManager` + `EL_LicensePlateGeneratorGeneric`
  (`Feature/LicensePlate/`) — ❌ off-by-ones, no uniqueness registry, no
  destructor.

## Dependencies

- `VehicleLock` (ownership), `Police` (enforcement), `Houses`/`Account`
  (registration identity).