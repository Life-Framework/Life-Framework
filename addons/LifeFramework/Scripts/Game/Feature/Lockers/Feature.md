# Lockers

## Intent

A character with something to keep safe rents a locker at a site (a station, a bank hall) and finds their
own things there next time. Rent is paid ahead; a locker that is not paid for does not eat items forever
and does not delete them: the contents wait in the site's lost-and-found until the owner pays to take
them back. Specs #753, #776; chapter #1038.

## Interaction pattern

Rung 1 once the world pieces land (a locker bank prop with a use action). This slice (#2658, L1) is the
rules and config only: pure script, no prop, no UI, no save data yet.

## Rules (#2658)

- **One locker per character per site.** A site is a bank of lockers keyed by a stable site id; each renter
  gets their own contents, so there is no physical locker number to fight over. A second rent at the same
  site is refused (`ALREADY_RENTED`); a renter short of the price is refused (`CANNOT_AFFORD`).
- **Paid until.** A fresh rent runs one period from now; a renewal before the end adds a period to the
  unexpired time, one after the end counts from now.
- **Lapse.** Paid-until plus the grace period is the lapse moment. Before paid-until the rent is `ACTIVE`,
  after it `GRACE` (contents still in the locker), from the lapse moment `LAPSED`: the contents move to the
  site's lost-and-found (decision 2026-11-11).
- **Reclaim.** Taking the contents back from lost-and-found costs `reclaimFee`; short of it is refused.
- **`rentPeriodDays 0` never lapses** and has no paid-until or lapse moment.
- Times are UTC seconds, passed in by the caller (`LF_LockerRules` reads no clock and no config), so the rules
  test without a world.

## Data

`Configs/Lockers/LF_Lockers.conf` (`LF_LockerConfig`), owner JSON section `lockers`: `rentPrice` 100,
`rentPeriodDays` 7 (0 = never lapses), `gracePeriodDays` 2, `reclaimFee` 50, `weightCapacity` 60 kg. Server
only; `/configreload` re-reads it.

## Not built here

The locker bank and lost-and-found props, the rent and reclaim actions and their sheet, the stored
contents and their persistence, and the daily lapse sweep are later slices of #1038.
