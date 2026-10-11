# Jailbreak

Phase: Release
Extends: [forced entry](forced-entry.md), [jailing](../police/jailing.md)

## Player loop

Someone outside (or a prisoner who still has a charge) blows the jail door. The door stays open until police repair it. Prisoners serving a sentence can walk out while it is open. Leaving does not clear the sentence; recapture continues it.

## In scope

- The jail door is a forced-entry target.
- While the door is open, the sentence volume does not teleport escapees back.
- While the door is locked, a player with time left who leaves the volume is returned to the cell.
- Repair uses the shared repair action. Police are notified when the charge is armed.
- The breaker gains wanted level. Prisoners who walk out stay wanted.

## Out of scope

- A separate blast mechanic from forced entry.
- Guards as AI, riot scripts, or a new prison building.
- Clearing wanted because the player escaped.

## Assets

Door: `{787617D5B683FE41}Prefabs/Structures/BuildingParts/Doors/EL_JailDoor.et` inherits vanilla `Prefabs/Structures/BuildingParts/Doors/Door_PrisonCell_E_A_Left_INT_ST_B.et` and adds `EL_LockComponent` the same way as `EL_Door_Base`. Cell volume: port `LNGC_JailCellComponent` (`Prefabs/LNGC_JailCell.et`) and skip confinement while this door is open. Charge is the forced-entry default, `DemoBlock_M112.et`. See [sources](../../sources.md).

## First slice

DebugWorld jail from the jailing scope. Lock holds a sentenced player. Breach lets them walk out. Repair holds the next prisoner again.

## Depends on

- [Forced entry](forced-entry.md)
- [Jailing](../police/jailing.md) sentence and cell volume

## Proof

- Logic: open door does not pull the player back; locked door does; escape does not zero the timer.
- World: the walk-out and the repair, on a dedicated server.
