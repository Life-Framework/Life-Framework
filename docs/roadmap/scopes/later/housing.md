# Private housing

Phase: After release
Extends: `Houses` doors and keys, `Money`, persistence

## Player loop

A player buys a house at a real-estate office, receives a key, and stores items inside. The door is the one already implemented.

## In scope

- Purchase at an office, paid in cash or from the bank at that office.
- Ownership and key persist across restart.
- Interior storage in the existing building.

## Out of scope

- Organization hideouts, which ship at [release](../../phases/release.md).
- Furniture placement, instanced interiors, and police raids. Raids reuse [forced entry](../crime/forced-entry.md) later, on a house the player owns.

## Assets

`EL_Door_Base`, `EL_LockComponent`, `HouseKeyItem.et`, `EL_HouseManagerComponent`, `EL_HouseSaveData`. LNGC has a home-ownership license and no door scripts. The license flag is not ownership. See [sources](../../sources.md).

## First slice

Buy one DebugWorld house, lock it, store an item, restart, and find both the lock and the item.

## Depends on

- Release [hideouts](../ownership/hideouts.md) only as the pattern for "a group owns a building." Player housing is a different owner: the character.
- `Houses`, `Money`, persistence

## Proof

- Persistence restart for owner, key, and storage.
- A non-owner cannot unlock.
