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

## V1.4 - #259, revive

`LF_ReviveAction` on the player character (context LF_ID) is offered to an on-duty medic facing a downed player
within `m_fReviveRange`; `LF_Revive.Perform` re-checks on the server and calls `LF_Downed.Clear` (flag,
bleed-out clock, unconsciousness), then publishes `LF_OnPlayerRevived`. Medic only; item cost and a treatment
animation are not in yet.

## V1.3 - #261, drag and carry

`LF_Carry` (server hold table, 250 ms follow tick, replicated carrier id on the character), four actions
on a player character: Drag, Carry, Put down, Load into vehicle. Config `m_bCarryAnyone` and the
distances in `LF_Medic.conf`. Publishes `LF_OnPlayerCarried`. No animation (ACE's are GPL).

## V1.4 - #260, treatment bill

`LF_TreatmentBills` (mirrors `LF_FineTickets`): a revive and a hospital wake each write a paper bill
(`HospitalBill.et`, `LF_HospitalBillComponent`) for a config amount; the patient pays the oldest in carried
cash at the hospital desk (`LF_TreatmentBillPayAction`), the revive medic gets `m_fBillMedicShare` of it
once at clock-out. Saved as the `bills` payload (version 19). Unpaid has no consequence in the flagship.

## Next

- #256 bleed-out timer (server-owned), give-up, screen and sound effects on the
  downed client, disconnect while downed. It calls `Finish`; the murder lands on
  `SCR_ChimeraCharacter.LF_GetDownerUid` because vanilla keeps the knockout's
  instigator as the killer.

## Constraints

- NPCs are not downed (they die as in vanilla); tests opt a spawned character in
  with `LF_Downed.SetTestAllEligible`.
- The downed state is not persisted: the body is gone when the player leaves.
- Hit zone, not damage manager: `HijackDamageHandling` does not see the damage a
  limb passes on to the Health zone, `SCR_CharacterHealthHitZone.ComputeEffectiveDamage` does.

## V1.5 - #262, the ambulance run ends in a bed

`LF_BedDelivery` + `LF_BedDeliveryAction` ("Put patient in bed") on `Prefabs/Medic/LF_HospitalBed.et` (the vanilla
`BedMedical_01` mesh, `LF_HospitalBedComponent`, context `LF_HospitalBed`). On-duty medic only; the server takes the body
the medic carries, else a downed occupant of a stretcher vehicle parked within `m_fBedVehicleRange` of the bed, moves it
onto the mattress, calls `LF_Downed.Clear`, writes the reduced revive bill (`LF_TreatmentBills.IssueForRevive`,
`m_iReviveBillAmount`; a death's wake bill is the larger `LF_Respawn.conf` amount) and publishes `LF_OnPlayerRevived`
then `LF_OnPatientDelivered`. A downed patient seated in a vehicle listed in `m_aStretcherVehicles` (flagship: the
ambulance) does not bleed out (`LF_BedDelivery.IsLoaded`, read by `LF_BleedOut.Advance`). Loading is `LF_Carry.Load`.
No stretcher-load sound yet.
