# Jailing

Phase: Release
Extends: `Police` arrest, `Account` wanted

## Player loop

An on-duty officer arrests a wanted player. That player serves a timed sentence at a placed jail. When the timer ends, wanted drops by the configured amount and the player can leave.

## In scope

- The jail is a placed marker and a cell volume on an existing building. No hard-coded world origin.
- Sentence length is config, scaled by wanted level. The server owns the timer.
- Release slice: the arrest moves the prisoner to the cell. It does not require putting them in a vehicle. The officer's action is at the suspect, in the world.
- Inside a locked jail, leaving the volume returns the prisoner to the cell. An open door (see [jailbreak](../crime/jailbreak.md)) does not.
- Timer end is logged. A reconnect during the sentence resumes the remaining time.

## Out of scope

- The vehicle escort. That slice is next: officer loads the suspect into a base-game vehicle and the sentence starts on arrival. It ships in this scope if it is ready before release; otherwise it is the first follow-up after release.
- Bail menus, lawyer jobs, or a custom prison interior.
- Jailing a player who is not wanted.

## Assets

`EL_PoliceUtils.GetJailPosition()` currently returns the literal `"127 1 137"` (the DebugWorld police desk). Replace that with a placed cell.

Port `LNGC_JailCellComponent`, `LNGC_JailComponent` (timer, zone confine, release), and `LNGC_SendToJailAction`, including `Prefabs/LNGC_JailCell.et`. Door is `Door_PrisonCell_E_A_Left_INT_ST_B.et` plus `EL_LockComponent`. Building can be `Villa_E_2I01_Police.et` or a barracks beside it.

The escort slice ports `LNGC_ArrestAction`, `LNGC_ArrestableComponent`, `LNGC_EscortAction`, the escort tick in `LNGC_PoliceComponent`, `LNGC_PutInVehicleAction`, and `LNGC_TakeOutOfVehicleAction`. Seat the nearest free non-driver seat. Wanted stays on `EL_PlayerAccount`. Do not port `LNGC_WantedComponent` or virtual-item seizure. See [sources](../../sources.md).

## First slice

Replace the placeholder jail position. Arrest of a wanted player starts a config sentence at the marker. Early release is refused. Timer expiry reduces wanted and frees the player. Restart keeps the remaining time.

## Depends on

- `EL_PoliceUtils` sentence math and the arrest RPC
- `Account` wanted
- Persistence for the remaining sentence

## Proof

- Logic: sentence length from wanted level; expiry reduces wanted; arrest of a clean player refuses.
- World: arrest lands on the marker, not the origin.
- Persistence: restart mid-sentence resumes the timer.
- Manual: dedicated server, officer on one client, suspect on another.
