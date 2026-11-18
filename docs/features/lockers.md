# Lockers

### `LF_LockerRules` — `Feature/Lockers/LF_LockerRules.c`
- ✅ Pure locker rules (2026-11-17, #2658): `CheckRent` (one locker per character per site, price in hand),
  `PaidUntil` (a renewal stacks on unexpired time), `LapseMoment` (paid-until plus grace), `StateAt`
  (`ACTIVE`, `GRACE`, `LAPSED`), `MoveToLostAndFound`, `CheckReclaim`. `rentPeriodDays 0` never lapses.
  Proof: `LF_Test_LockerRules` (LOGIC).
- ⚠️ Rules only: no locker prop, rent action, stored contents, persistence or lapse sweep yet (later slices of #1038).

### `LF_LockerConfig` — `Feature/Lockers/LF_LockerConfig.c`
- ✅ Owner section `lockers` over `Configs/Lockers/LF_Lockers.conf`: `rentPrice` 100, `rentPeriodDays` 7,
  `gracePeriodDays` 2, `reclaimFee` 50, `weightCapacity` 60 kg. A bad value keeps the shipped one;
  `/configreload` re-reads it.
