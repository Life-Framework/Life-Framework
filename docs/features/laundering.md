# Laundering

Dirty money (M7, parent #1025; framework #644). Money stays physical: there is no dirty balance.
Stolen money is a separate item that never counts as cash.

### `LF_MarkedCash` — `Feature/Laundering/LF_MarkedCash.c` (#2850)
- ✅ Marked notes are a stack of the `markedcash` prefab slot (`LF_PrefabSlots.conf`; core prefab
  `Prefabs/Items/Money/LF_MarkedCash.et`, inherits the cash `MoneyStack`, name `#LF-Item_MarkedCash`;
  a pack swaps the look). Same quantity component, so stacks split and merge like cash, but only with
  other marked notes (merging and counting match the exact prefab). The notes are salmon-tinted (#2871): own
  `Assets/Items/Currencies/MarkedStack.xob` baking `MarkedCashMaterial.emat` (the cash texture with a red `Color` multiplier),
  used for the prefab mesh and the quantity stack model, so ground, hand and inventory preview read apart from cash.
- ✅ `Add(target, amount, reason[, dropOverflow])`, `Count(target)`, `Take(target, amount[, reason])`,
  the `LF_MoneyUtils` shape. `LF_MoneyUtils.GetCash`, shops, the ATM and hand-overs never count or
  spend them. Nothing is published as `LF_OnMoneyMoved`; each change logs `[LFDebug:Laundering]`
  with its reason. A slot with no usable prefab makes `Add` log `LF_Debug.Error("Laundering", ...)`
  and return 0 (`GetLastError`).
- ✅ No save payload: they are items in inventories and persist like any other item.
- ✅ Dirty money is money: with `persistence.keepCash` off, a wipe strips marked notes from every slot it
  strips cash from (`LF_PersistenceKeep.StripMarkedNotes`: carried, civilian stash, duty kits, car cargo).
  Test: `LF_Test_MarkedCashWipe` (LOGIC).
- ⚠️ Looks the same as cash for now (same mesh, only the name differs); a band or tint is a follow-up.
- Test: `LF_Test_MarkedCash` (WORLD).

### Dirty payouts — `Feature/Laundering/LF_LaunderConfig.c` (#2851)
- ✅ `LF_LaunderConfig` (`Configs/Crime/LF_Launder.conf`, the feature's one config; a pack overrides the
  file) carries `m_bCrimePaysMarked`. Core ships it **off**: every payout is exactly today's cash.
- ✅ Switch on: a till robbery (`LF_RobAction.PayCash`) pays the same amount as marked notes and no cash
  (reason `robbery`; a short add is taken back and the robbery refused, like the cash path), and a bank
  vault's money bags (`LF_Bank.WriteWorldBagCash`: the vault bag, the heist spill, the bag re-filled on
  load) hold marked notes (`LF_Bank.ReadWorldBagMarked`). A bag's marked stack never reads as cash.
- ✅ Chop shop and fence payouts stay clean cash (the fence is already the dirty exit for goods).
- Test: `LF_Test_DirtyPayout` (WORLD).
- ✅ The illicit buyer's open spot (#2856, beat 2 of #1025): `LF_BuyerSpotComponent` registers a place in
  `LF_Places` under kind `chain_buyer` (server only); `LF_BuyerRotation.OpenSpot(unixNow)` names the one
  open now. Time is cut into periods of `m_iBuyerPeriodHours` (core 4 h); a cycle of `count` periods visits
  every spot once in a hashed shuffle, so the spot changes every period inside a cycle. A pure function of
  unix time and the seed, nothing saved, so a restart keeps it. The period hands out one spot (a spot added
  mid-period joins at the next period). No spot placed: none, one warning.
- ✅ The seed is a server secret, not config or script: made at random on first use and kept in
  `$profile:LifeFramework/buyer-rotation-seed.txt`. Delete the file to reshuffle the buyer. The sale and the
  pack's spots come next. Test: `LF_Test_BuyerRotation` (WORLD).
- ✅ The buyer stands at the open spot (#2919): `LF_BuyerNpc` (started from the game mode, server only) sweeps every 5 s.
  A man in a work jacket (core prefab `Prefabs/Characters/LF_IllicitBuyer.et`, a denim jacket; a pack dresses him, B4)
  is spawned on the open `chain_buyer` spot when a player is within 150 m of it, then loiters (`LF_NpcLoiter.Settle`:
  a free seat nearby, else leans or smokes). When the rotation moves he is removed from the old spot and appears at
  the new one; with no player within 200 m of him he is removed. One buyer at most, none at the other spots, nothing
  saved. The spot prefab `Prefabs/Systems/Laundering/LF_BuyerSpot.et` carries `LF_BuyerSpotComponent`.
  Test: `LF_Test_BuyerNpc` (WORLD). Look: `grid:slice-b1` (cones stand in for spots: a live spot in a grid layer
  would join every test's rotation).

### Wash rules — `Feature/Laundering/LF_LaunderRules.c` (#2852)
- ✅ Pure logic, no entity: `LF_LaunderRules.Quote(key, marked, nowSec, cutPct, batchMax, dailyCap)` gives the
  notes accepted, the clean cash and what stays marked. Config (`LF_LaunderConfig`): `m_iCutPct` 30,
  `m_iBatchMax` 2000, `m_iBatchSeconds` 60, `m_iDailyCap` 10000. 1000 marked yields 700 and 3 yields 2 (integer maths: the clean share
  rounds down and the remainder stays with the house, so no split of a wash pays more); a bigger batch is cut to the max; at the cap a wash gets 0 and the
  reason `daily_cap`, and the cap window ends 24 real hours after the first wash.
- ✅ A cut outside 0..90 %, or a zero batch or cap, is refused at load (`LF_Debug.Error("Laundering", ...)`): the
  wash rules read as missing (`LF_LaunderConfig.GetRules` null) while the dirty-money switch still applies.
- ⚠️ The per-character day total is session-only in this wave: a restart forgives the day.
- ✅ Called by the laundering station (#2853, below).
- Test: `LF_Test_LaunderRules` (LOGIC).

### Laundering station — `Feature/Laundering/LF_LaunderStationComponent.c` (#2853)
- ✅ `LF_LaunderStationComponent` on `Prefabs/Props/Laundering/LF_LaunderStation.et` (a vanilla workbench), actions
  `LF_LaunderStationAction` LOAD (`#LF-Launder_Load`) and COLLECT (`#LF-Launder_Collect`, `#LF-Launder_Washing` while it washes).
- ✅ Server only. Load takes the character's marked notes first (`LF_MarkedCash.Take`, reason `launder-in`), up to the batch
  and daily limits of `LF_LaunderRules`; a short take is put back once and refused. The ledger books `launder-in` on the
  new `marked` account (`LF_Ledger.ACCOUNT_MARKED`, a trail line, never a balance).
- ✅ The batch washes for `m_iBatchSeconds`, then only the character who loaded it collects: `LF_MoneyUtils.AddCash(...,
  "launder-out", "", true)` (overflow at the feet). A short pay keeps the unpaid rest in the station for the same character.
  Walking away mid-wash leaves the batch waiting; anyone else gets `#LF-Launder_NotYours`.
- ✅ Refusals in words: dead or downed, cuffed (`LF_CuffEnforcement`), out of reach (3.5 m), faster than one action per second, busy, no notes,
  `#LF-Launder_DailyCap`, not yet washed.
- ✅ Heat: each batch adds `m_iHeatPerBatch` (10) work units to the station's chain site (`m_sSiteId`, heat settings of
  `m_sChainId`, default `cider`) through `LF_ChainSites.AddWork` and publishes `LF_OnChainWork`, so the smell trace and
  the police lead work as at a still.
- ✅ Saved (#2894): the batch is the `launder` save section (`LF_LaunderSavePayload`, `Feature/Laundering/LF_LaunderSave.c`),
  one row per loaded station keyed by its chain site id (`m_sSiteId`): owner, marked notes held, clean cash still owed (after a
  short pay, the unpaid rest), seconds of wash left and seconds it has waited washed (world time restarts at 0, so no clock is
  saved). A restart gives the batch back to its owner only. A row whose station has not appeared 10 min after the load is dropped
  with `LF_Debug.Warn`; a station deleted mid-session keeps its batch as a pending row for the next save. A save without the
  section leaves every station empty. Day totals stay session-only (`LF_LaunderRules`).
- ✅ Forfeit (#2894): a washed batch its owner leaves uncollected for `m_iForfeitSeconds` (station attribute, default 7200 s; 0
  never forfeits) is forfeited: `LF_Debug.Info("Laundering", ...)` names owner, marked and clean amounts, nobody is paid, and the
  station frees for anyone. A timer frees it without anyone touching it; the clock is part of the save.
- Test: `LF_Test_LaunderStation` (WORLD), `LF_Test_LaunderStationPersist` (PERSISTENCE, restart check under `wt e2e`).
