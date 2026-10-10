# Vehicles

Phase: Release
Extends: `VehicleLock`, `LicensePlate`, `Shop`

## Player loop

A player buys a base-game vehicle from a lot, receives a key, locks it, and registers a plate. Other players do not drive it unless they hold a key.

## In scope

- A vehicle shop sells a config list of vanilla vehicle prefabs for cash (large purchases may require the ATM balance, paid at the lot).
- Purchase spawns that prefab, assigns an owner, and gives one key. Extra keys are a config action for the owner.
- Lock and unlock use the existing vehicle lock components.
- Plates use the existing generator, with the current off-by-one and uniqueness bugs fixed so two vehicles do not share a plate by construction.
- Store the vehicle record by a stable id so a restart can restore ownership. The world spawn follows the persistence rules already used for characters.

## Out of scope

- New vehicle models, aircraft, or armed variants authored for this mod.
- Tuning, paint shops, or a garage interior. A parked-vehicle slot for organizations is only a flag in [organizations](organizations.md).
- Impound. Police impound can follow after plates and ownership are reliable.

## Assets

Keep `EL_VehicleLockComponent` and `{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et`. `LNGC_VehicleKeyLockComponent` is a prototype that keys a virtual item; use it as the replication checklist (`RplProp` plus `BumpMe`), not as the class to drop in.

Garage persistence is the piece we do not have. Port `LNGC_GarageComponent`, `LNGC_GarageDialog`, and `LNGC_VehicleSpawnPointComponent`. Lot pad: `Garage_E_01`. First catalog, all base game, several already wrapped as locked prefabs in LNGC:

- `Prefabs/Vehicles/Wheeled/S105/S105_beige.et` (and the other colors)
- `UAZ469_covered_CIV_*.et` / `UAZ469_uncovered_CIV_*.et`
- `S1203_transport_beige.et`
- `UAZ452` locked base
- `M923A1_transport.et` for cargo
- Police-only, not on the civilian lot: `M151A2_Police_Base.et`, LNGC `S105_police.et`

No aircraft. See [sources](../../sources.md).

## First slice

Buy one configured vanilla vehicle, lock it against another player, restart, and still be the owner with a working key.

## Depends on

- `EL_VehicleLockComponent` and keys
- `EL_LicensePlateManager` (fix the generator's character and number range, and remember issued plates)
- `Money` and the shop purchase rules
- Persistence

## Proof

- Logic: plate generator can emit the full alphabet and the top number; a second issue does not collide with a live plate.
- World: purchase, key, lock.
- Persistence: owner and key still match after a restart.
