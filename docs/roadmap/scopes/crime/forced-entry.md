# Forced entry

Phase: Release
Extends: door locks (`Houses`), crime alerts, police

## Player loop

A player places a charge on a configured door, gate, or container, stands clear, and the lock opens. It stays open until someone repairs it. Police are notified when the charge is placed.

## In scope

- One action: arm a configured charge item on a target that allows it.
- A fuse timer, then the lock opens. The mesh is not swapped for a custom destroyed model; the lock state changes and the door can be used.
- The open state persists until a repair action finishes, including across a restart.
- Repair is a hold action at the same object. Police can always repair. The owning organization can repair its own hideout door.
- Wanted level and a police notification fire once, when the charge is armed, using the existing crime notify path.

## Out of scope

- A new explosive weapon, a custom blast mesh, or chained structural collapse.
- Breach as an instant menu. The player is at the object.
- Different charge mechanics per robbery. Bank, jail, and hospital call this scope.

## Assets

Charge item, config, default `Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et`. The Soviet block beside it is `DemoBlock_TSh400g/DemoBlock_TSh400g.et`. Arm and disarm start from `SCR_ArmExplosiveChargeWithTimedFuzeAction` and `SCR_DisarmExplosiveChargeAction`. The lock they open is `EL_LockComponent` on `{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et`. A missing charge prefab logs and the action refuses.

LNGC has no breach script. Do not invent a second explosive class. See [sources](../../sources.md).

## First slice

One DebugWorld door with a charge item, a fuse, an open-until-repaired state, and a police repair action. Persistence of the open state is part of the slice, not a follow-up.

## Depends on

- `Houses` lock state
- `Crime` wanted and police notify
- `Account` for who armed it

## Public function

Later scopes call this. It does not detonate the vanilla charge.

`static bool Arm(string targetId, string playerUid)`

`targetId` is the configured world id (`Roadmap_Breach_Door` on the release door). `playerUid` is the account that takes the wanted level. Returns false when the target is not configured, a charge is already on it, it is already open, the player id is empty, or `{33CBDE73AB48172A}Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et` does not load. On success the wanted level and the police notify fire once, immediately, including while a fuse is still running. The lock opens when the fuse elapses and stays open until `Repair(string targetId, string playerUid)`. A fuse of 0 opens before `Arm` returns. A positive fuse stays locked until `Advance(string targetId, float elapsedSeconds)` reaches it; a placed door also schedules that step. `Export(string targetId)` / `Apply(LF_BreachRecord)` round-trip the open flag onto the same `EL_LockComponent`. Police can always repair. `SetOrganizationMember(string playerUid, string organizationId)` lets the owning organization repair its own door.

## Proof

- Logic: a target that is not configured refuses; a repaired target locks again; a second charge on an already-open target refuses.
- World: arm, wait out the fuse, use the door, repair, door locks.
- Persistence: save while open, restart, the door is still open until repaired.
