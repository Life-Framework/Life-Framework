# Lockers

## Intent

A character with something to keep safe rents a locker at a site (a station, a bank hall) and finds their
own things there next time. Rent is paid ahead; a locker that is not paid for does not eat items forever
and does not delete them: the contents wait in the site's lost-and-found until the owner pays to take
them back. Specs #753, #776; chapter #1038.

## Interaction pattern

Rung 1: a locker bank prop (`Prefabs/Props/Lockers/LF_LockerBank.et`, a vanilla metal cabinet) with a
"Rent a locker" use action (#2659). Rules and config came first (#2658).

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

## Renting (#2659)

- `LF_LockerBankComponent` marks a prop as a bank: `m_sSiteId` (the stable key rentals are saved under; set it on
  the placed instance, a bank without one is keyed by its rounded position and warns) and `m_fReach` (2.5 m).
- `LF_LockerRentAction` runs on the server for the acting character. `LF_LockerBank.RentNearest` finds the bank
  from that character's own position and its character record key (`LF_Utils.GetPlayerUID`); nothing the client
  sends picks the bank or the renter. Refused: out of reach, already renting here, short of `rentPrice` (nothing
  taken). The cash is taken only after every check; a short take is handed back.
- Saved as the `lockers` payload (save v56): rows `{siteId, characterKey, paidUntil}`. A save before v56 loads
  with nobody renting. Apply replaces the table, so a second apply gives the same table.

## Data

`Configs/Lockers/LF_Lockers.conf` (`LF_LockerConfig`), owner JSON section `lockers`: `rentPrice` 100,
`rentPeriodDays` 7 (0 = never lapses), `gracePeriodDays` 2, `reclaimFee` 50, `weightCapacity` 60 kg. Server
only; `/configreload` re-reads it.

## Not built here

Renewal, the lost-and-found prop, the reclaim action, the stored contents and their persistence, a rent
sheet, and the daily lapse sweep are later slices of #1038. No bank is placed in a world yet.
