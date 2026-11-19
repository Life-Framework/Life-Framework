# Carry

### `LF_ShoulderCarry` — `Feature/Carry/LF_ShoulderCarry.c` (#3263, 2026-11-19)
- ✅ Hoist a load with `LF_ShoulderCarryComponent` (`ShoulderCarryItem_Base.et`) onto the shoulder through the hand-carry slot (F on the ground load); the weapon is holstered and the hands are busy. The carrier's dynamic speed is capped by the load's profile (softwood log 0.7, hardwood 0.5, ore sack 0.75, crate 0.6), so no sprint. A second hoist is refused; drop sets it down and lifts the cap; a downed carrier drops it. Tests: `carry/shoulder-carry` (WORLD); session proof `shoulder-carry`.
- ⏳ No log, sack or crate prefab uses the base yet (#3265 builds the truck-bed load on it); the two-player and late-joiner pictures need a placed load in the debug grid.

### `LF_CargoBedComponent` — `Feature/Carry/Cargo/LF_CargoBedComponent.c` (#3265, 2026-11-19)
- ✅ A vehicle's cargo bed: load a shoulder-carried log at the tailgate (server deletes it, counts its units), see the stack on every machine (local copies, `RpcDo_SetStack` plus `RplSave`/`RplLoad`), unload the top item onto the shoulder. Capacity: 1989 truck 12, vans 6, cars 0; softwood log 1 unit, hardwood 2. Full bed or no bed refuses with a toast. Tests: `carry/cargo-bed` (WORLD); session proof `cargo-bed`.
- ✅ Both log prefabs are shoulder loads; the inventory screen can no longer put one in a pack (`LF_CargoHaulingGate`).
- ⏳ The bed stack does not persist (#3266); van bed positions are estimates; the two-player and late-joiner pictures need a placed truck in the debug grid.
