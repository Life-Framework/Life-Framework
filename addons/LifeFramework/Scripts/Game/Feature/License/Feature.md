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

1. **Fix the double-charge**: `PurchaseLicense` spends SP even if
   `UnlockLicense` later rejects.
2. **Fix the hard-coded POLICE whitelist** in `UnlockLicense` — a MEDIC
   whitelisted license wrongly requires the police whitelist.
3. Remove the bypass: `SetUnlockedLicenses` sets licenses with no checks at all.
4. Localize and honor `m_bIsInitialLicense` (never read today).

## Iteration path

- **V2** — DMV counter interaction (rung 3), license as a physical document.
- **V3** — driving tests (practical), license revocation on infractions.

## Current state

- `EL_LicenseManagerComponent` (`Feature/License/`) — catalog + gates +
  purchase + persistence. ❌ double-charge, hard-coded POLICE whitelist,
  unchecked bulk setter.

## Dependencies

- `Account` / `Whitelist` (eligibility), `Level` (level gates), `Money`
  (fees), `Police` (revocation), `Character` (carried documents).