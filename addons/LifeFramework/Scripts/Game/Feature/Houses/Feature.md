# Houses — Design Requirements

> Context file for owned property. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

Housing is **physical property**: a real door you unlock with a real key, a
house you own that persists, interior storage you walk into. No "buy a house
from a menu anywhere." Buying happens at a real-estate office / agent; living
happens at the door.

## Interaction pattern

Rung 1 today on the entry side: `EL_DoorUserAction` (open), `EL_KeyComponent`
(key inventory item), `EL_LockComponent` (lock state), `EL_HouseManagerComponent`
(ownership), `EL_HouseSaveData` (persistence). The door + key loop is the
physical core.

Target: the full ownership loop in the world — sign at the real-estate office,
receive keys, walk to the property, unlock your door. Police/bailiffs raiding
(Phase 2 roadmap: "basic raiding system for police") is a physical event too.

## V1 (shippable)

1. Verify the door/lock/key loop is server-validated end-to-end (who can
   unlock, from where) and that ownership persistence restores keys correctly
   on restart.
2. House save data must round-trip idempotently (AGENTS.md persistence rules).

## Iteration path

- **V2** — real-estate office purchase (rung 3), keys handed over physically.
- **V3** — interior storage per house, tenant rights, eviction/foreclosure.
- **V4** — police raiding, forced entry via tools, alarms.

## Current state

- `EL_DoorUserAction`, `EL_KeyComponent`, `EL_LockComponent`,
  `EL_HouseManagerComponent`, `EL_HouseSaveData` (`Feature/Houses/`).
- `Prefabs/Structures/` (buildings and building parts) back the physical shell.

## Dependencies

- `Money` / `Banking` (purchase funds), `Account` (ownership identity),
  `Character` (inventory for keys), `Police` (raiding), `Persistence`
  (ownership state).