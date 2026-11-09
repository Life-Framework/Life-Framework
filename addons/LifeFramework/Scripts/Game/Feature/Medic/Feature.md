# Medic - Design Requirements

> Context file for the Medic epic (#146): downed state, bleed-out, revive.
> Written against `docs/design.md` (in-world first).

## Intent

Lethal damage to a player does not end their session: they go down where they
are, helpless but alive, and other players decide what happens next (help,
rob, leave). Death, the hospital wake, and the murder report come later, from
the bleed-out timer or the player giving up.

## Interaction pattern

Rung 1 for the people acting on a downed player (the vanilla inventory on the
body, later a revive action); the downed player's own state is world-visible
(they lie on the ground, vanilla unconsciousness).

## V1 (shippable) - #255, the downed state

1. `LF_Downed`: lethal damage to a player is cut at the Health hit zone to leave
   `m_fDownedHealthScaled`, the player is marked downed and publishes
   `LF_OnPlayerDowned`. Config `Configs/Medic/LF_Medic.conf` (`LF_MedicConfig`).
2. The flag is an RplProp on the character; vanilla unconsciousness
   (`INCAPACITATED`) is held while it is set. A downed player takes no further damage.
3. `LF_Downed.Finish` is the real death (vanilla path: murder hook, hospital
   wake). `LF_Downed.Clear` ends the state. Only the dev `die` command calls
   `Finish` today.
4. `m_bDownedCanBeSearched` gates opening the body's inventory (searched or robbed).

## V1.1 - #256, bleed-out and give-up

`LF_BleedOut` (server clock, shared tick, throttled RplProp copy), `LF_GiveUpController`
(hold `LF_GiveUp`, server validates), disconnect while downed is death. Expiry and give-up
call `Finish` with the downer as instigator. Screen/sound effects are vanilla unconsciousness.

## V1.2 - #257, first aid

`LF_FirstAid` hooks the vanilla bandage, tourniquet and saline bag `ApplyEffect`; on a downed
patient it adds config seconds (capped) to the bleed-out clock and publishes `LF_OnFirstAid`.

## V1.3 - #258, the medic job

The hospital duty desk (`Prefabs/Medic/LF_MedicDutyDesk.et`, `LF_DutyAction` with `m_eRole MEDIC`) is the
police desk's twin: same swap through `LF_DutyLocker`, own slot `medic_duty`, open to every registered
player, duty pay at clock-out. On duty the account's faction is MEDIC and `LF_MedicDuty` /
`LF_ServiceCalls.JobFor` make routing see the medic job (board, take, pager). The kit lives in the
game mode's MEDIC `LF_FactionLoadout` and includes `LF_Pager`.

## Next

- #256 bleed-out timer (server-owned), give-up, screen and sound effects on the
  downed client, disconnect while downed. It calls `Finish`; the murder lands on
  `SCR_ChimeraCharacter.LF_GetDownerUid` because vanilla keeps the knockout's
  instigator as the killer.
- #259 revive by an on-duty medic. It calls `Clear`; the character keeps the
  sliver of health.

## Constraints

- NPCs are not downed (they die as in vanilla); tests opt a spawned character in
  with `LF_Downed.SetTestAllEligible`.
- The downed state is not persisted: the body is gone when the player leaves.
- Hit zone, not damage manager: `HijackDamageHandling` does not see the damage a
  limb passes on to the Health zone, `SCR_CharacterHealthHitZone.ComputeEffectiveDamage` does.
