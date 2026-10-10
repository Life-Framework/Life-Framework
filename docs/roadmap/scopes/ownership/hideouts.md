# Hideouts

Phase: Release
Extends: `Houses` locks, [organizations](organizations.md)

## Player loop

An organization owns one building. Members use its door, its storage, and its processing machine. The contraband convoy starts here.

## In scope

- One hideout prefab: an existing structure with a lockable door, a storage box, and a process machine.
- Ownership is the organization id. Members can lock and unlock. Others cannot.
- The processing slot from [organizations](organizations.md) turns the machine on. Without that upgrade the machine refuses and logs.
- A placed convoy start marker for [contraband](../crime/contraband.md).
- The building is placed in the world by the server setup, then assigned to the organization. Players do not noclip a base into the map.

## Out of scope

- Player-bought private homes. That is [housing](../later/housing.md).
- Instanced interiors, furniture, or a new building mesh.
- More than one hideout per organization in the first slice.

## Assets

Shell: `Prefabs/Structures/Houses/Garage/Garage_E_01/` or a village house, parented like `EL_Building_Base` (`Building_Base.et`). Door and key: `EL_Door_Base`, `EL_LockComponent`, `HouseKeyItem.et`, identifier set to the organization id by `EL_HouseManagerComponent`. Machine: `ItemProcessor_Base`. Stash prop from LNGC: `Prefabs/Props/Crates/ShineStash.et`. See [sources](../../sources.md).

## First slice

A DebugWorld hideout assigned to a test organization. A member unlocks the door and processes. A non-member is refused at the door and at the machine.

## Depends on

- [Organizations](organizations.md)
- `Houses` door, key or lock component, adapted to an organization id
- `Processing`
- [Contraband](../crime/contraband.md) for the job that uses the building

## Proof

- World: member in, non-member out.
- Persistence: ownership and lock state survive a restart.
