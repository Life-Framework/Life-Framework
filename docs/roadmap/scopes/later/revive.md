# Revive

Phase: After release
Extends: `Survival`, character death

## Player loop

A downed player can be revived by someone with a medical item before the death screen. If the timer runs out, the current death and respawn path takes over.

## In scope

- A downed state with a config bleed-out timer.
- A revive action that consumes a base-game medical item and raises the player with a config health fraction.
- Medics and civilians can both attempt it. The license can shorten the action for a medic.

## Out of scope

- Changing the respawn loadout or the corpse rules that already shipped.
- Custom defibrillator meshes.

## Assets

Port the downed state and the force-respawn wait from LNGC (`LNGC_ReviveAction`, `m_iForceRespawnWaitSeconds` default 75 on the player-data manager). The item consumed is `FieldDressing_US_01.et` or `MorphineInjection_01.et`. Skip the map-close path LNGC marks as a softlock (`LNGC_LifeStateListener`, `CloseMapIfOpen`). Death after the timer still uses `EL_DeathScreen` and `EL_SpawnLogic.RespawnPlayer_S`. See [sources](../../sources.md).

## First slice

Down a player, revive with the configured item, or let the timer reach the existing death screen.

## Depends on

- `EL_SurvivalStats` and the death-screen respawn
- Hospital items only as the same vanilla medical prefabs, not as a dependency on the robbery

## Proof

- Logic: no item refuses; timer expiry calls the existing death path once.
- Manual: two clients, dedicated server.
