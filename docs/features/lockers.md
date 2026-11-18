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
- ✅ Saved as the `lockers` payload (save v57); a pre-v57 save loads with nobody renting. Proof: `LF_Test_LockerPersist`
  (PERSISTENCE), `tools\cli wt e2e`.
- ⚠️ No bank placed in a world yet; a pack placement sets `m_sSiteId`.

### `LF_LockerStorageManagerComponent` — `Feature/Lockers/LF_LockerStorageManagerComponent.c`, `LF_LockerOpenAction.c`, `LF_LockerPlayerController.c`
- 🧪 Open your own locker (2026-11-18, #2661): "Open your locker" on the bank. The server finds the bank from the
  character's position and the renter from their own record key; a non-renter and an out-of-reach character are refused
  with a toast and nothing is built. The contents are an `LF_StorageSnapshotRecord` (owner = character key, slot
  `locker:<siteId>`) in the existing `carried` payload: no new save section. Each open builds a transient container
  (`Prefabs/Props/Lockers/LF_LockerContents.et`), fills it from the snapshot (`LF_StorageSnapshot.Apply`) and opens it on the
  renter's client only. A change recaptures the snapshot; close, disconnect, death, walking away (`reach + 3 m`) and every
  save point (`CaptureAllOpen`, one line in the `carried` payload write) capture it before the container is deleted. Release
  also captures the renter's carried inventory in the same step, so an item that moved between them is in exactly one
  snapshot. Death clears `carried`, never `locker:<siteId>`. The engine's access check (`HandleAccessValidation`) lets only
  the renter's own inventory touch an open locker. Weight capacity is 60 kg on the container prefab (the config's
  `weightCapacity` is not read by the prefab yet). Logs `[LFDebug:Lockers]`.
  Proof: `LF_Test_LockerRoundtrip`, `LF_Test_LockerIsolation` (WORLD), `tools\cli wt e2e` (a stored handgun survives the restart).
- ⚠️ A snapshot is current at every capture, not marked "open" in the save: a crash between a change and the next capture
  loses that change, it never duplicates an item (the opener's carried inventory is captured with it). The lost-and-found
  sweep and reclaim are wave 2.
