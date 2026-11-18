# Lockers

### `LF_LockerRules` — `Feature/Lockers/LF_LockerRules.c`
- ✅ Pure locker rules (2026-11-17, #2658): `CheckRent` (one locker per character per site, price in hand),
  `PaidUntil` (a renewal stacks on unexpired time), `LapseMoment` (paid-until plus grace), `StateAt`
  (`ACTIVE`, `GRACE`, `LAPSED`), `MoveToLostAndFound`, `CheckReclaim`. `rentPeriodDays 0` never lapses.
  Proof: `LF_Test_LockerRules` (LOGIC).
- ⚠️ No stored contents, renewal, reclaim or lapse sweep yet (later slices of #1038).

### `LF_LockerConfig` — `Feature/Lockers/LF_LockerConfig.c`
- ✅ Owner section `lockers` over `Configs/Lockers/LF_Lockers.conf`: `rentPrice` 100, `rentPeriodDays` 7,
  `gracePeriodDays` 2, `reclaimFee` 50, `weightCapacity` 60 kg. A bad value keeps the shipped one;
  `/configreload` re-reads it.

### `LF_LockerBank` — `Feature/Lockers/LF_LockerBank.c`, `LF_LockerBankComponent.c`, `LF_LockerRentAction.c`
- ✅ Rent a locker at a locker bank (2026-11-18, #2659): "Rent a locker" on `Prefabs/Props/Lockers/LF_LockerBank.et` takes
  `rentPrice` in cash and records `{siteId, characterKey, paidUntil}`. The server finds the bank from the character's
  position; refused out of reach, already renting at the site, or short of cash (nothing taken).
  Proof: `LF_Test_LockerRent` (WORLD).
- ✅ Saved as the `lockers` payload (save v56); a pre-v56 save loads with nobody renting. Proof: `LF_Test_LockerPersist`
  (PERSISTENCE), `tools\cli wt e2e`.
- ⚠️ No bank placed in a world yet; a pack placement sets `m_sSiteId`.
