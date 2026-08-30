# License — Design Requirements

> Context file for licenses (weapon, vehicle, job access). Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

A license is a **document you go get** — at the DMV / agency / police station,
in person. You fill the requirement (level, whitelist, fee), you pay, you carry
the license. It is the physical gate that says "this person is allowed to do
X." No buying licenses from a menu anywhere.

## Interaction pattern

Rung 4 today: `EL_LicenseManagerComponent` holds a 26-entry catalog,
`CanUnlockLicense` (owned/config/whitelist/level), `CanAffordLicense` (SP
cost), `PurchaseLicense` (spends SP + unlock), `HasLicense`, persistence
restore.

Target: rung 3 — a DMV / agency counter where the player requests a license,
meets the check, and pays (cash or bank). The `EL_ShowIDAction` ID card is the
in-world document; a license is another document you carry.

## V1 (shippable)

1. **Fix the double-charge — DONE** (verified 2026-08-30): `PurchaseLicense`
   validates before `UnlockLicense` spends SP.
2. **Fix the hard-coded POLICE whitelist — DONE**: license types map to their
   correct whitelist job, including MEDIC.
3. Keep `SetUnlockedLicenses` as the persistence restore seam; it is not a
   player purchase path. Duplicate restored licenses are deduplicated.
4. Localize and honor `m_bIsInitialLicense` (never read today).

## Iteration path

- **V2** — DMV counter interaction (rung 3), license as a physical document.
- **V3** — driving tests (practical), license revocation on infractions.

## Current state

- `EL_LicenseManagerComponent` (`Feature/License/`) — catalog + gates +
  purchase + persistence. ✅ purchase ordering, whitelist mapping, and
  persistence-list deduplication are guarded. Remaining localization work.

## Dependencies

- `Account` / `Whitelist` (eligibility), `Level` (level gates), `Money`
  (fees), `Police` (revocation), `Character` (carried documents).
