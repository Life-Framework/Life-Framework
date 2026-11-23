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

### The informant's tip-off — `Feature/Laundering/LF_InformantHook.c` (#2921)
- ✅ Dialogue hook `Informant` (`LF_DialogueHook_Informant`; a dialogue file names it as reply action and condition, no param):
  "Heard where the man is buying?" costs `m_iInformantFee` cash (core 300, 0 is free) and puts a private mark, labelled
  `#LF-Informant_Mark`, on today's open buyer spot (`LF_BuyerRotation.OpenSpot`) for that player only (a PLAYER row of
  `LF_MapShares`, key `informant:<uid>`). The mark expires when the buyer's period ends, so it never shows a spot he has left.
- ✅ The server re-checks everything and refuses in words with nothing taken: dead or downed and rate-limited asks do nothing;
  police on duty, or underworld standing below `m_iInformantMinTier` (core 1), get "Never heard of him." (same words, so a
  refusal does not say which); no spot placed refuses before any cash moves; a player who already paid this period is told so
  and charged nothing; too little cash refuses; a mark that cannot be put pays the fee back. The reply is hidden from a
  character the informant would not talk to.
- ✅ Nothing is saved: a restart clears the marks and a player may buy the tip again. No dialogue file ships in core; the pack's
  informant NPC wires the hook.
- Test: `LF_Test_InformantTip` (WORLD).

### Selling chain goods to the buyer — `Feature/Laundering/LF_BuyerSaleAction.c`, `LF_BuyerPrices.c` (#2920)
- ✅ `LF_BuyerSaleAction` ("Sell the goods", `#LF-BuyerSale_Action`) sits on `LF_IllicitBuyer.et`. `LF_BuyerSale.ServerSell` takes
  every chain good (`LF_ChainSearch.IsGood`, the `m_bGoods` outputs) the character carries that the price list
  (`Configs/Crime/LF_BuyerPrices.conf`, `LF_BuyerPrices`) names, and pays the sum: marked notes when the dirty-money switch
  (`LF_LaunderConfig.m_bCrimePaysMarked`) is on, cash when it is off. The core list prices moonshine; the 1989 pack layers its own prices over it (#2922).
- ✅ Server only, and checked again there: a living standing seller, not cuffed (`LF_CuffEnforcement`), the buyer is the live one
  and stands at the spot the rotation has open now, within `m_fReach` (4 m), not repeating inside `m_iMinActionMs` (1 s), something to sell.
- ✅ All or nothing on the inventory side: goods are taken first; a short take, or a payout that does not land, puts every unit
  back (and takes back any part paid). Cash is booked on the ledger with reason `buyer-sale`; marked notes get a `marked` trail line.
- Test: `LF_Test_BuyerSale` (WORLD).

### The under-the-counter outlet — `Feature/Laundering/LF_CounterOutlet*.c` (#4200, 0.3.0 crime B5)
- 🧪 A barkeeper buys smuggled cartons and bottles under the counter. `LF_CounterOutletComponent` on the barkeeper (or the counter) names a buyer id; the pack's buyer-demand rows for that id (`LF_BuyerDemandConfig`, a period row such as two cartons a week) say which goods, how many and at what price. The shared purse book (`LF_BuyerDemandBook`, saved as the `buyerdemand` section) holds the cap across sales and restarts.
- 🧪 `LF_CounterOutletAction` ("Sell under the counter", `#LF-CounterOutlet_Action`): `LF_CounterOutlet.ServerSell` checks, on the server, a living standing seller, not cuffed, **not on-duty police** ("Not with that badge on, officer."), within `m_fReach` (3 m), not repeating inside `m_iMinActionMs` (1.5 s), a good the rows name, room in a purse. Each good is sold up to what its purse takes; the rest stays with the seller and a spent bar says "That's enough for this week."
- 🧪 Pays marked notes when the dirty-money switch is on, cash when it is off (like the illicit buyer); ledger reason `counter-sale`, a `marked` trail line for notes. All or nothing: a short take or a payout that does not land returns the goods, takes back what was paid and restores every purse.
- ⚠️ Not here: the barkeeper prefab, Josette at the Chotain bar and the pack's carton and bottle rows (later B cards). Test: `LF_Test_CounterOutlet` (WORLD).

### The buyer's spots on Saint-Philippe — `Place_saint-philippe_buyer-spots.layer` (#3416, Refs #3400)
- 🧪 The island has three `LF_BuyerSpot` markers (the core prefab, `LF_BuyerSpotComponent` on a traffic cone), so `LF_BuyerRotation` has spots to rotate through and the police cannot camp one. Each stands out of sight against a wall: the dock warehouse (`saint-philippe-dock` b22), the east end of a concrete vehicle ramp in the districts' back lane (b21) and a barn at the village edge (b64). The spot id is the entity name (`Fit_saint_philippe_<place>_buyer_spots_drop_<building>`). All three are 200 m or more outside both safe circles (#3350).
- Fitted by `tools\cli town drops saint-philippe buyer-spots` (a new `buyer-spots` entry in `DROP_KITS`); claim `tools/town/claims/saint-philippe-buyer-spots.json`; `audit placement --strict` clean, `town check saint-philippe` shows the slot fitted.

### The 1989 pack turns dirty money on (#2922, B4 of #1025)
- ✅ `LF_LaunderConfig` and `LF_BuyerPrices` read pack overlays (`ApplyLayers`, `LF_ConfigOverlay.LoadPackOverlays`, pattern of #2977). The launder overlay sets only the fields it writes; a buyer price row replaces the core row of the same prefab or adds one. Test: `LF_Test_LaunderPackOverlay` (LOGIC, test overlays in `Configs/Tests/`).
- ✅ `LF_Everon1989/Configs/Crime/LF_Launder_Everon1989.conf`: robberies and vault bags pay marked notes, the house cut is 35 %, a batch takes up to 1500 notes and 90 s, the daily cap is 6000, the informant's tip costs 150 and needs underworld tier 1.
- ✅ `LF_BuyerPrices_Everon1989.conf`: the buyer pays 80 for a bottle of moonshine (its plums and cabbages cost 30 in the supermarket).
- ✅ `LF_Dialogue_NightShift.conf` (a pack-only dialogue cannot be patched from another file, `validate-dialogue-patch`): the night-shift worker (the informant) offers "Heard anyone's buying, quiet like?" after he has talked about work (a menu shows four replies at most, so it takes the place of the work reply's repeat); shown only to a player he would tell (the `Informant` hook as condition). His answers and the station's cap, loaded and closed toasts and the buyer's two refusals are pack strings (same Ids as the core rows, so the pack's words win).
- ⚠️ Not here: tobacco and paper chains (no such chain exists yet, only moonshine is priced) and the buyer's pack work jacket (the outfit is outside this card). Test: `LF_Test_EveronDirtyMoney` (pack leg).
- 🧪 `agent-box/proofs/dirty-money-island.mjs` (#3405): the loop on the island through harness verbs (`node agent-box/session.mjs dirty-money-island` in `session up --mission island`): standing, moonshine, the night-shift informant's tip, the buyer at the marked spot, the sale in marked notes at 80. Prints each `[LFDebug:Laundering]` line, exits non-zero when a step is missing. Steps 1 and 2 pass; step 3 fails because the harness cannot address a spawned resident by id (`use LF_DialogueTalkAction` reaches a neighbour); steps 4 to 6 wait on that.
- 🧪 **A sale is heard by quests (#3768).** `LF_BuyerSale.ServerSell` calls `LF_QuestSignals.ReportBuyerSale` after a paid sale: one SIGNAL `buyer.sold:<prefab GUID, upper case>` per good paid for, so a quest can ask for a particular good to be sold to the buyer. The 1989 pack prices the shipment sample (`LF_ShipmentSample.et`, a pack child of the contraband crate) at 480 in its own file under `LF_BuyerPrices_Everon1989/`; the plain crate has no price, as the convoy start hands one out on every press. Test: `laundering/buyer-sale` (a quest waiting for the moonshine sale completes).
