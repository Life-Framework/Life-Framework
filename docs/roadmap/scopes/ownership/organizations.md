# Organizations

Phase: Release
Extends: `Account`, `Banking` rules, persistence

## Player loop

Players found a group that survives restarts. The group has a name, ranks, members, a shared fund, and upgrades bought with that fund. The first use is a criminal outfit with a hideout. The same record can be a business later. The framework does not ship a turf war.

## In scope

- Create, name, and persist an organization. Identity is a stable id, never a session entity id.
- Ranks and membership. The founder can invite and kick. A member can leave.
- Shared fund: cash deposited into the fund and withdrawn by a rank that is allowed. The fund is not a second currency and not a personal ATM balance.
- Upgrades are config flags paid from the fund: extra storage, a processing slot, a parked-vehicle slot. Buying one logs the price and the new flag.
- One organization membership per character for the first slice.

## Out of scope

- Territory capture, gang wars, or custom gang compounds.
- A separate illegal faction slot. Civilians join an organization; police work stays on the police faction.
- Paying upgrades with skill points.

## Assets

No new meshes. The fund moves MoneyStack through the same rules as `EL_BankAccount` (a balance keyed by organization id, not a second currency and not `LNGC_cash`). Upgrades flip flags on the [hideout](hideouts.md). LNGC has no organization scripts. See [sources](../../sources.md).

## First slice

Create an organization, add a member, deposit cash, buy the processing-slot upgrade, restart, and see the same members, balance, and flag.

## Depends on

- Persistence (versioned save; load is idempotent)
- `Money` for the cash moving into and out of the fund
- [Hideouts](hideouts.md) to give the upgrades a place to apply

## Proof

- Logic: a rank without withdraw permission is refused; the price is all-or-nothing.
- Persistence: restart restores members, fund, and upgrades.
- The personal ATM balance does not change when the fund pays for an upgrade.
