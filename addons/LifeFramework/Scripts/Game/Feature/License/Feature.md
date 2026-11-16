# License — Design Requirements

> Context file for licenses (weapon, vehicle, job access). Written against
> `docs/design.md` (in-world first).

## Intent

A license is a **document you go get** — at the DMV / agency / police station,
in person. You fill the requirement (level, whitelist, fee), you pay, you carry
the license. It is the physical gate that says "this person is allowed to do
X." No buying licenses from a menu anywhere.

## Interaction pattern

Rung 4 today: `LF_LicenseManagerComponent` holds a 26-entry catalog,
`CanUnlockLicense` (owned/config/whitelist/job level), `PurchaseLicense`
(checks, then unlocks; skill points no longer exist since #749), `HasLicense`, persistence
restore.

Target: rung 3 — a DMV / agency counter where the player requests a license,
meets the check, and pays (cash or bank). The `LF_ShowIDAction` ID card is the
in-world document; a license is another document you carry.

## V1 (shippable)

1. **Fix the double-charge — DONE** (verified 2026-08-30): `PurchaseLicense`
   validates before `UnlockLicense`. Since #749 the level gate is a job level
   (`LF_LicenseGate`): the licence family's job at the catalog level, or a
   `jobs.licenceGates` config row.
2. **Fix the hard-coded POLICE whitelist — DONE**: license types map to their
   correct whitelist job, including MEDIC.
3. Keep `SetUnlockedLicenses` as the persistence restore seam; it is not a
   player purchase path. Duplicate restored licenses are deduplicated.
4. Localize and honor `m_bIsInitialLicense` (never read today).

## Iteration path

- **V2** — DMV counter interaction (rung 3), license as a physical document.
- **V3** — driving tests (practical), license revocation on infractions.

## Current state

- `LF_LicenseManagerComponent` (`Feature/License/`) — catalog + gates +
  purchase + persistence. ✅ purchase ordering, whitelist mapping, and
  persistence-list deduplication are guarded. Remaining localization work.

- The component is on the player body (`Prefabs/Characters/Core/Character_Roleplay.et`) since #988.
  Before that no prefab carried it, so no player held a licence and every licence check refused
  (`catalogue/gun-counter` caught it). A new body gets the initial licences, then its saved ones
  (`LF_Progression.ApplyToCharacter`).
- `FIREARM_PERMIT` (#988): a civilian permit an outlet's gate asks for (the gun counter's M9), and
  the licence that makes carrying a `licensed` catalogue item lawful. `/licence <NAME>` grants one in
  dev worlds and to administrators (`LF_LicenceGrantCommand.c`).
- **Licence desk** (#1766, `LF_LicenceDesk.c`, `LF_LicenceDeskAction.c`, prefab
  `Prefabs/Props/Commercial/LF_LicenceDesk.et`): a player applies for the firearm permit in person.
  Refused while wanted, with an open crime report, convicted within the last `licences.permitConvictionDays`
  in-game days (default 7, 0 is off; `LF_ConvictionRecord.IsRecent`, #1995), below the licence's gate, or short of the fee
  (`licences.permitFee`, core default 250, carried cash). An application takes the fee once and is an
  account flag `permit_due:<in-game minute>` in the progression store; after `licences.permitWaitDays`
  (default 1) in-game days the player collects the permit at the desk, the record checked again (a conviction
  after paying holds the permit back, keeps the application open and refunds nothing; the player collects once the
  look-back has passed. A holder keeps the permit: only the police revoke it). The
  wait runs on the in-game clock, which vanilla persistence saves; a clock set back waits one term,
  never longer. On-duty police revoke the permit (and any open application) with the "Revoke firearm
  permit" player interaction (`LF_LicenceRevokeRole`). The test desk stands in GridWorld cell c0r5; the
  town desk (`SquareLicenceDesk`, pack `Village_Dressing`, town-licence-desk) stands on the spawn square beside the registry
  counter, so a player in the playtest town can get the permit and the gun counter (#1903) opens.

## Dependencies

- `Account` / `Whitelist` (eligibility), `Level` (level gates), `Money`
  (fees), `Police` (revocation), `Character` (carried documents).
