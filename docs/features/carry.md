# Carry

### `LF_ShoulderCarry` — `Feature/Carry/LF_ShoulderCarry.c` (#3263, 2026-11-19)
- ✅ Hoist a load with `LF_ShoulderCarryComponent` (`ShoulderCarryItem_Base.et`) onto the shoulder through the hand-carry slot (F on the ground load); the weapon is holstered and the hands are busy. The carrier's dynamic speed is capped by the load's profile (softwood log 0.7, hardwood 0.5, ore sack 0.75, crate 0.6), so no sprint. A second hoist is refused; drop sets it down and lifts the cap; a downed carrier drops it. Tests: `carry/shoulder-carry` (WORLD); session proof `shoulder-carry`.
- ⏳ No log, sack or crate prefab uses the base yet (#3265 builds the truck-bed load on it); the two-player and late-joiner pictures need a placed load in the debug grid.

### `LF_CargoBedComponent` — `Feature/Carry/Cargo/LF_CargoBedComponent.c` (#3265, 2026-11-19)
- ✅ A vehicle's cargo bed: load a shoulder-carried log at the tailgate (server deletes it, counts its units), see the stack on every machine (local copies, `RpcDo_SetStack` plus `RplSave`/`RplLoad`), unload the top item onto the shoulder. Capacity: 1989 truck 12, vans 6, cars 0; softwood log 1 unit, hardwood 2. Full bed or no bed refuses with a toast. Tests: `carry/cargo-bed` (WORLD); session proof `cargo-bed`.
- ✅ The drawn logs lie along the bed floor, side by side across `m_fBedWidth`, placed by the mesh's own bounds (the log mesh's pivot is at one end). Session proof cell: `slice-3265` in GridWorld (`CargoBed_Truck`, `CargoBed_Log`..`Log3`).
- ✅ The bed follows the vehicle lock: a locked truck can be neither loaded nor unloaded (same rule as the trunk, `LF_VehicleStorageAccessControl`).
- ✅ Both log prefabs are shoulder loads; the inventory screen can no longer put one in a pack (`LF_CargoHaulingGate`).
- ✅ A parked truck keeps its bed load across a restart (#3266, save section `cargobed`). ⏳ Van bed positions are estimates; the two-player and late-joiner pictures need a placed truck in the debug grid.

### `LF_CapacityConfig` — `Feature/Vehicles/LF_Capacity.c`, `Configs/Vehicles/LF_Capacity.conf` (#4189, 2026-11-22)
- ✅ One table for what the body (1 sack), packs (3), car (4), pickup (6), van (8) and truck (14) carry: native sacks plus cargo bed units, so a van is 2 native + its 6-unit bed. Each chain's best loop names the vehicle it needs (coal 5 needs a pickup, iron 8 a van, lumber 12 a truck), one rung above what the smaller class holds. The crime scope reads the same table. Tests: `vehicles/capacity-table` (WORLD) checks the ladder, each vehicle row's bed units against its prefab's `LF_CargoBedComponent` and the native storage; `validate-capacity-ladder` (selftest `capacity-ladder-selftest`) checks the file text. ⏳ Nothing in play reads the table yet; the work and crime cards do.
