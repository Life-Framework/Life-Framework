# Money & Economy

### `LF_MoneyUtils` — `Feature/Money/LF_MoneyUtils.c`
- ✅ Cash is the `MoneyStack` inventory item. `GetCash` (‑1 on fault),
  `AddCash`/`RemoveCash` (actual amount, 0 on fault).
- ⚠️ Inconsistent sentinels (`-1` vs `0`); doc comments wrong on
  Remove/Take.

### `LF_Ledger` — `Feature/Money/LF_Ledger.c` (#330, 2026-11-09)
- ✅ Append-only log of every cash and bank change: id, unix time, character uid, account
  (`cash`/`bank`), signed change, reason key, counterparty uid, balance after. Features never
  call it: `LF_MoneyUtils.AddCash`/`RemoveCash` (cash, player targets only) and
  `LF_BankAccount.Deposit`/`Withdraw`/`Charge` (bank) publish `LF_OnMoneyMoved`, and the ledger
  subscribes (`LF_Ledger.EnsureSubscribed`, game-mode start). The reason is an optional last
  argument at the call site (`shop-buy`, `atm-deposit`, `give-cash`, `ticket-fine`, ...);
  none given records `cash` / `bank`. A hand-over is two lines that sum to zero and name each
  other. Persisted as the `ledger` payload (save version 14), trimmed by `Configs/Money/LF_Ledger.conf`
  (`m_iMaxEntries`, `m_iMaxAgeDays`). `Last(uid, account, n)` lists newest first (what the
  incident log reads); `RebuildBalance`/`VerifyBank` rebuild the bank from the log and compare.
  Cash lines are a trail, not a chain: picking up or dropping a note outside `LF_MoneyUtils` is
  not logged. Cash moved into a bag or an AI is not a player ledger line. `validate` rule
  `validate-ledger-writes` fails direct `SetBalance`/`PREFAB_CASH` writes. Tests `money/ledger-records`,
  `money/ledger-persist`, `e2e/pass2-verify-load`.

### `LF_BankAccount` + `LF_ATMManager` — `Feature/ATM/`
- ✅ **The canonical bank** (verified 2026-08-30): every payout in the game pays
  cash; `LF_ATMManager` + `LF_BankAccount` (per-player, keyed by UID, persisted)
  is the only bank, and the ATM is the only cash↔account boundary.
  `Deposit` only if `>0`, `Withdraw` only if sufficient; balance never negative
  through these paths. Manager is a session map registry + async loader.
- ✅ `SetBalance` clamps corrupt negative persistence values, and
  `CreateAccount` is idempotent and rejects empty IDs (verified 2026-08-30).
- ✅ **No bank wipe on the first connect after a restart** (2026-11-07, test
  `restart/bank-survives-connect`, verified 2026-11-07): the game mode used to
  `new LF_ATMManager()` on the first connect, and the constructor stole the
  singleton the save load had filled. `LF_ATMManager`, `LF_JobManager`, and
  `LF_CharacterCreationManager` now share one pattern: `static ref s_Instance`,
  created only by `GetInstance()`, the constructor never registers itself, and
  the destructor only clears the singleton when it is that instance. The game
  mode keeps no manager fields; `LF_GameModeRoleplay.ResolveConnectManagers()`
  only resolves them.

### ATM terminal — `Feature/ATM/LF_ATMTerminal.c` (was `LF_ATMMenu`)
- ✅ (2026-11-08, M1-13) **Terminal screen** on `LF_UIScreen` with the
  `LF_UISkin.Terminal()` skin: green phosphor (vanilla `robotomono` face) in a
  bezel, balance and carried cash from the server, an `AMOUNT >` entry line, a
  status line, a 3x4 keypad (CLR, 0, DEL), DEPOSIT, WITHDRAW, EXIT. Keys: mouse,
  pad focus that moves like a grid (MenuUp/Down keep the column, MenuLeft/Right
  walk a row), keyboard digits and numpad (`LF_Keypad0..9`), Backspace or pad X
  (`LF_KeypadBack`), Delete or pad Y (`LF_KeypadClear`). The entry cannot pass
  `MAX_TRANSACTION_AMOUNT`. Closes on EXIT, MenuBack, death, or 3.5 m from the
  machine. `LF_ATMAction` runs PerformAction only on the acting machine
  (`HasLocalEffectOnlyScript`) and opens only when the user is this machine's
  character (`LF_CounterAccess.IsLocalActor`).
- ✅ (2026-11-08) **Refusals say why, on the screen**: the server replies
  `LF_EATMRefusal` (amount, cash short, balance short, pockets full, not at an
  ATM, no account) with the new balance and carried cash; no toast (the old
  away-from-ATM toast plus the menu toast was the duplicate). No reply in 5 s
  prints `NO REPLY FROM THE BANK`. Sounds: key click, accept and refuse are UI
  events set on the `LF_ATMAction` attributes; an accepted deposit or
  withdrawal plays `SOUND_PICK_UP` on the character for everyone nearby.
  Tests: `atm/keypad`, `atm/refusal-reasons`, `counter/refusal-text`,
  `counter/local-only`, `counter/screen-layout`.
- ✅ (2026-11-10, #590) **1989 bank teller replaces the placed ATM.** `LF_BankTeller.et` is a clerk NPC
  (Edith) whose chest context carries `LF_ATMAction` labelled "Talk to the teller", so deposit and
  withdraw use the same terminal, server seams, range check (`LF_ActionSite` finds the teller as the
  counter), ledger reasons (`atm-deposit`, `atm-withdraw`), quest signal and ATM code. The playtest town
  puts her inside the bank (#931): in the lobby of the municipal-office building behind its vanilla post-office counter at (175.4, 0.2, 264.4), the customer side at the lobby floor; the two atmosphere counters keep only their sound emitters in the lobby and the TELLER plate hangs on the wall behind her. The ATM entity `ATM_Debug` stays, hidden
  (no mesh, inside the bank), for tests and flows. Test `atm/bank-teller`; demo
  `session.mjs bank-teller-proof`.
- ✅ **Cash moves** (verified 2026-08-30): the menu forwards through
  `LF_CharacterATMComponent.RequestDeposit/RequestWithdraw`, and the server
  RPC handlers `RpcAsk_Deposit`/`RpcAsk_Withdraw` remove cash on deposit
  (partial removals rolled back) and pay cash out on withdraw (all-or-nothing
  with balance restore), guarded by `LF_ATMManager.IsValidAmount`. No money
  from thin air.
- ✅ The money-moving bodies are the static seams
  `LF_CharacterATMComponent.ServeDeposit/ServeWithdraw` (extracted 2026-11-07,
  no behavior change; the RPC handlers keep the range check and reply);
  `atm/request-cash-boundary` (WORLD) drives them with a real character
  (red-proof observed 2026-11-08). The withdraw rollback case pays into an
  entity with no inventory: a bare `Character_Roleplay` still holds MoneyStack.
- ✅ **At the ATM only** (2026-11-07): the server refuses a
  deposit or withdraw unless an entity carrying `LF_ATMAction` stands within
  `LF_CharacterATMComponent.ATM_RANGE` (5 m) of the character
  (`LF_ActionSite`, server-side search; the client names no site). A refusal
  logs `deposit refused: not at an ATM` / `withdraw refused: ...` and the
  terminal prints `REFUSED: STAND AT THE MACHINE` (2026-11-08; was a toast). On a listen host the requests and the replies call their
  handlers directly, like the shop bridge. Test: `security/service-range`.

### `LF_BankAccountComponent` — `Feature/Banking/` (deleted 2026-08-30)
- ❌ The second, unintegrated entity bank is **gone** (file deleted, no
  references remain). The $20k grant, the persistence race, and the duplicate
  money path died with it.

### `LF_ShopComponent` — `Feature/Shop/LF_ShopComponent.c`
- ✅ Catalog-driven buy: `BuyItem(item, qty, buyer)` guards null/qty, server-
  only, price × quantity, `LF_MoneyUtils.RemoveAmount`, refund on
  inventory-full. A sell or buy of a stack pays or gives one unit per
  quantity through `LF_ShopRules.StackTradeTotal` (verified 2026-10-11).
  The menu sells at most 99 of a held stack per click, the server cap
  `LF_CharacterShopComponent.MAX_TRANSACTION_QUANTITY`; a request outside
  1..99 is refused and logged. (2026-11-07)
- ✅ **Money correctness** (verified 2026-08-30): the partial-funds exploit is
  closed — `BuyItem` compares the actual removed amount against the total and
  refunds only what was removed before failing (a $50 buyer cannot buy a $100
  item), and a partial delivery refunds only the undelivered quantity.
  `SellItem` pays cash only after the goods are confirmed removable and
  claws back overpayments. `IsItemSoldHere` gates buy and sell.
- ⚠️ A shop with no `LF_StoreComponent` has no stock (infinite supply); the physical general store below does.
- 🧪 (2026-11-15) **Fence shop: talk or browse, config stock, buyback.** The fence (`LF_Fence.et`) offers **Talk** and **Browse goods** from one context (`RoleTalk`); his role talk carries *Show me what you've got.* after *what do you do*, which closes the talk and opens the price list (`LF_DialogueHook_OpenShop`, `LF_DialogueHook.AfterScene` on the client). Any role NPC with an `LF_ShopComponent` gets the reply (`LF_ShopkeeperTalk`). Stock is config: `LF_ShopComponent.m_sStockId` names a stock in `LF_ShopStockConfig` (core `Configs/Shop/LF_ShopStock.conf` is empty; the pack overlay `LF_Everon1989/Configs/Shop/LF_ShopStock_Everon1989.conf` sells lockpick 250, scanner 900, keycard 1500, drill 2500, charge 4000 and buys car parts or anything flagged stolen at half the street value). Prices are read on the server per trade; the buyback share is held to 0..1. `LF_StolenGoods` flags a robbed weapon and bank-loot weapons; a carried or stored item keeps the flag through a save, a restart and a reconnect (the `itemstolen` payload, save version 35, #1855; an item left on the ground loses it); an unlisted item sells only as its flagged copies. Buyback lines show *We pay $X* on the price list. `LF_FenceStall.et` is a workbench stall with the same stock and one Browse goods action. A DebugWorld-only boot has no pack overlay, so the fence sells nothing there (logged). Tests `shop/stock-rules`, `shop/fence-stock-trade`.
- 🧪 (2026-11-16) **Playtest cash top-up holds a reset amount (#1855).** The 60 s debug top-up (`LF_DebugMoneyTopUp`, debug game mode only) refills carried cash under 5,000 to 20,000. After the harness `resetplayer 1000` and a $250 fence buy it refilled $750 between the buy and the sell, so the $100 sale read as $20,100 (not a double credit: the ledger has a `debug-topup` row). `resetplayer <cash>` now holds that player's cash (`HoldCash`); `resetplayer` with no amount releases it. Test `money/debug-topup-hold`.
- ✅ **Physical store (#202).** The DebugWorld general store (`Shop_GeneralStore_Debug`, store key `general_store`) is also a physical store: `LF_StoreComponent` beside the price list. The goods stand on the counter as real world items (apple, cabbage, water, apple juice, field dressing). The price list stays and is the convenient way to buy. **Stock is one number per line** (`min(stock, slots)` display copies stand on the shelf, the rest waits in the back room): a copy taken by hand leaves the stock once and is tagged **unpaid** (`LF_Stores`, a record per item instance on the server); a price-list sale (`LF_ShopComponent.Buy`) checks stock, takes the cash, reserves the units and deletes the surplus display copy; every stock operation first looks for items already taken, so a pickup and a sale can never count one unit twice, and an empty line refuses with `#LF-Shop_RefuseOutOfStock`. **Pay for goods** (`LF_StoreTillAction` on the cash register itself, #582: `Shop_GeneralStore_Till`, the station register prefab `EL_FuelTill.et` standing on the counter and naming its store by `m_sStoreKey`, its action context on the customer side; the server refuses a payer more than `LF_StoreRules.TILL_REACH` (3.5 m) from the register) charges exactly what the user carries unpaid in cash (reason `store-sale`, an `LF_OnMoneyMoved`), all or nothing, clears the tags and hands over an `LF_Receipt` (Read toasts the number, items and total; the printed fields are session state, so a receipt kept over a restart reads blank). An unpaid item eaten or used up stays on the bill. **Shoplifting:** a character holding unpaid goods of a store while outside its boundary box (`m_vBoundaryCenter`/`m_vBoundaryHalf`, flat) is caught by the 500 ms store tick: the goods close as theirs, the catalog's shoplifting amount (1) of wanted levels, `LF_Dispatch.Report` with `#LF-Offence_Shoplifting` (the alarm sounds at the counter and police are radioed) and `LF_WantedReports.Identify` so the poster names them (being seen walking out of a shop counts as identified). The shelf refills from config over time: `m_iRestockSeconds` (120) adds each line's `m_iRestockAmount` up to `m_iMaxStock` and respawns the missing copies. **Stock persists** (`storestock` payload, save version 21, `LF_StoreStockRow` by store key and prefab); the shelf is rebuilt from it after a restart. The engine's own persistence also saves a loose shelf item (it carries `LF_QuantityComponent`) and restores it where it stood, so on a fill or a load the store adopts a loose copy sitting on a shelf point (or deletes it when the slot is already filled) instead of spawning a second one; `wt e2e` counts the loose apples around the store after the restart. Tests `store/rules`, `store/shelf-till`, `store/shoplift`, `persist/store-stock`; `session.mjs store-proof` (two clients: A pays, B walks out). ⚠️ Follow-ups: the market and the car lot (cars on display, each with its own buy interaction, key at the desk) are not on this pattern yet; a shelf or unpaid item is kept out of stack merges (`LF_Stores.HoldsApart`, test `store/stack-merge`), so it stays a separate stack until it is moved again after paying; receipt text does not survive a restart. **Unpaid tags survive a restart and a logout (#479):** the `storeunpaid` payload (save version 25, `LF_StoreUnpaidRow` by character record key, store key and prefab, never an entity id) saves the tag of every item a living player carries or used up, and `LF_Stores.ParkCarrier` parks them on disconnect; when that character's body spawns with its carried inventory back (`LF_SpawnLogic.OnCharacterCreated`), `LF_Stores.ReattachUnpaid` puts each tag on an untagged item of that prefab it carries (a used-up one comes back as the same debt). A tag whose item or store is gone is dropped with a warning; a tag on the floor or a dead body is not saved. A restored tag is not swept while its carrier has the store's grace (`m_iRestoredGraceSeconds`, 300 s; toast `#LF-Store_UnpaidRestored` at spawn) to come back and pay; once the carrier has stood in the store, or the grace has run out, the sweep applies as usual, so logging out inside a store is no way to leave with the goods. The till charges a restored tag as before. Tests `persist/store-unpaid`, `e2e/pass*` (a juice taken off the shelf before the save is owed and paid after the restart).
- 🧪 (2026-11-15) **Folded piles on a store shelf** (clothing store step 2): an `LF_StoreLine` with `m_iStack` N reads every run of N shelf points as one pile, bottom first. Only the bottom copy finds the board (`SnapToSurfaceAt`); the copies above keep their written height over it, so a pile never floats or sinks. A shelf point may carry a fourth number, the copy's yaw relative to the store (a three-number point still spawns unturned). A copy taken from the middle of a pile leaves the stock once and the copies above close the gap (respawned one place lower, `LF_StoreRules.StackMoves`). After a restart each restored copy is adopted into its own pile slot: a pile slot matches only the copy at its own height (half the step), so the bottom slot no longer adopts the whole pile and deletes the rest. The building pipeline writes piles from a store line's `"stack": N` (one measured item height per copy, a seeded twist of up to 8 degrees; a pile taller than the air under the board above is refused). Tests `LF_Test_StoreStackRules`, `LF_Test_StoreFoldedStack`, the pile half of `LF_Test_E2E_SaveState` (`wt e2e`), `building-vanilla-store-selftest`.
- ✅ **The general store sells a short list** (2026-11-08, M1 RC QA): `Shop_GeneralStore_Debug`
  carries a curated `m_aShopItems` (apple, cabbage, water, apple juice, field dressing,
  ALICE backpack). Without it the price list showed the whole 1312-line base-game catalog,
  starting with an SVD magazine pouch and two "!BASE KIT!" rows. Prices still come from
  `ShopPrices.conf`. The UI kit status row gives its action button more width (label 1.5,
  status 1, button 1.5), so "Order" is no longer cut to "Orde".
- ✅ **The general store is a walk-in village shop** (2026-11-15, town-general-store): the village's
  shop house at 179, 233.95 is our pack prefab `LF_Everon1989/Prefabs/Buildings/GeneralStore/GeneralStore.et`
  (inherits the vanilla `ShopHouse_E_2I01t_Base.et` with the food store variant's doors, windows and
  FoodStore sign; its furniture is the vanilla food store's, flattened into
  `Furniture/GeneralStore_furniture_01.et`, without the counter and till). `Shop_GeneralStore_Debug` (the
  counter with the price list, the store key `general_store`), its till, radio and clerk stand where the
  vanilla counter stood, inside the shop room; the boundary is the ground floor (counter space centre
  -1.57 0 0.123, half 6.1 0 7.8), and the alarm sounds on `Shop_GeneralStore_AlarmBox`, a bell box on the
  shop front (`LF_AlarmMountComponent`, reach 10). Ten self-serve lines, 201 shelf points: apple, plum,
  rowan berries, dried meat, crackers, field dressing and a torch on the `ShelfShop_01` gondola and wall
  unit, water in the cooler, apple juice and a canteen (new; price in the pack overlay
  `Configs/Shop/LF_ShopPrices_GeneralStore.conf`) on the wall shelves. Tools and work gear are left to the
  hardware store, other medicine to the drug store, navigation gear to the gun store. The counter's price list
  (cabbage, backpack, ...) is unchanged. The points come from `tools/buildings/general-store/slots.mjs`
  (the building pipeline's `storePlan` over the furniture prefab, `--check` to compare); the store stays on the
  core counter entity because the suite boots the core world without the pack. ⚠️ In the core-only test world
  the shelves are not there (the copies settle on the floor) and the canteen has no price.
- ✅ **The general store is dressed and lit** (2026-11-16, #2008). `tools/buildings/general-store/dress.mjs` writes
  the plain props (tins, bottles, bread, cartons; no Items, nothing pickable or sold) on every shelf board the
  store lines do not use and a back row on the sold boards that have room, plus the counter-top, wall, floor
  and window-display props and the ceiling lights from `spec.json` (`dress`, `dressing`, `lights`) into the
  furniture prefab; every generated child has an ID starting `D0` and `--check` compares. Seven inline
  `LightEntity` spots (always on, as the building pipeline's lighting preset) light the shop and make the window
  glow from the street at night, one more spot lights the frontage. The vanilla `SignCommercial_01_FoodStore`
  panel (which read DROGERIE) is now `LF_Sign_Fascia` with sign id `general_store`, like the gun store (#1903);
  the DebugWorld overlay `Sign_GeneralStore` is removed as the building carries the sign.

### Shop price list — `Feature/Shop/LF_ShopPriceList.c` (was `LF_ShopMenu`)
- ✅ (2026-11-08, M1-13) Typed price list on `LF_UIScreen` with the
  `LF_UISkin.PriceList()` skin (paper, mono face, ink buttons): carried cash, a
  "Look for" field that narrows the list as you type, up to 40 lines of item
  and buy price with an Order button, an order slip (quantity with One less /
  One more, 1..99), Pay <total> cash, Sell these, Leave. Buy and sell are the
  existing `LF_CharacterShopComponent` RPCs; every request now gets a reply
  with `LF_EShopRefusal` (not sold here, unpriced, cash short, pockets full,
  nothing to sell, quantity, too far, no shop), the money moved and the carried
  cash, printed on the slip. The shop no longer raises toasts. An accepted
  trade plays `SOUND_PICK_UP` on the character; UI sounds are `LF_ShopAction`
  attributes. `LF_ShopAction` opens only on the acting machine for its own
  character. Closes on Leave, MenuBack, death, or 3.5 m from the counter.
  Tests: `shop/refusal-reasons`, `counter/local-only`, `counter/screen-layout`.
- ✅ (2026-11-12, #278) `ShopPrices.conf` is the core layer of
  the config service: `LF_ShopManagerComponent.LoadConfig` applies pack
  overlays and the owner's `shops` section before it builds the catalog
  (`LF_ShopPricesConfig.ApplyLayers`). The server charges and pays the owner's
  prices.

### `LF_TraderManagerComponent` + `LF_InventoryStorageManagerComponent` — `Components/InventorySystem/`
- ✅ Sell-to-trader pays **cash** (verified 2026-10-11): inserting into a
  trader-owned storage pays one cash unit per stack quantity
  (`LF_ShopRules.StackTradeTotal`) via `LF_MoneyUtils.AddCash`
  (pay-before-delete, clawed back if the item cannot be removed); black-market
  rejects non-civilian factions. The old "payout to ATM bank" and the
  `Print("testtesttest")` debug line are gone.
- ✅ The value lookup and the black-market decision are the static seams
  `LF_TraderManagerComponent.GetUnitValue` / `CanAccessBlackMarket`
  (extracted 2026-11-07; the lookup now skips a null row instead of crashing),
  pinned by `trader/sell-rules` (LOGIC; red-proof observed 2026-11-08).
- ⚠️ Unguarded `pStorageFrom.GetOwner()` (null crash); null `m_aTradableItems`
  foreach crash; delete-then-pay not atomic; `RETCODE_ITEM_TOO_BIG` used for
  "not tradable"; black-market check fails closed but is only cache-keyed.

- ✅ (2026-11-15, gun store prep) **Stores in vanilla buildings, display racks, outlet counters (tooling).** `tools\cli building` takes a spec with a `vanilla` block (our prefab inheriting a base-game building, no ArchBuilder export); its store block can place shop fittings (`store.fixtures`), stand an item's model in a display rack's slots with no way to take it (`"display": true` lines on `LF_WeaponRackCabinet_Display` / `LF_WeaponRackStand_Display`, the FIA arsenal racks without the arsenal; slots measured from the vanilla racks into `fixtures.json` `racks`) and add a counter that sells an item-catalogue outlet (`store.counters`, `LF_ShopComponent.m_sOutletId`, e.g. `gun_counter` behind `FIREARM_PERMIT`). `building proof` has a `permit` step: refused without the licence, harness `licence <NAME>` (the `/licence` request), then paid and delivered. Docs: `docs/building-pipeline/README.md` "A store in a vanilla building". Test: `building-vanilla-store-selftest`.
- ✅ (2026-11-15, town-gunstore) **The 1989 Everon gun store ("Gunsmith") is a working store on the main street.** `Village_GunStore` (Everon1989Town `Village_Buildings.layer`, 378 0 266, `angleY 180`, north row at the east end, facing the street) places `Prefabs/Buildings/GunStore/GunStore_Store.et` (pack), written by the building pipeline's vanilla-base store stage from `tools/buildings/gun-store/spec.json`. It inherits our `GunStore.et` (the vanilla `ShopHouse_E_2I01t_Base.et` with the food store variant's doors and windows; the fascia over the shop window is an `LF_Sign_Fascia` painted "GUNSMITH", sign id `gun_store`, its MLOD also on the painted face so the street sees it at distance) and `Furniture/GunStore_furniture_01.et` (the general store's flattened food store set without the gondolas, cooler, crates, newspaper stands and food posters). Display only: M21 and vz. 58 rifles and an M9 and a PM in `LF_WeaponRackCabinet_Display` on the shop wall, rifles in `LF_WeaponRackStand_Display` in the shop window (core racks, the FIA arsenal racks' models with no arsenal). Over `LF_GunCounter` (outlet `gun_counter`, its action "Buy guns and ammunition" in the pack table): M9 500, PM 350, M21 1500, vz. 58 1100 and their magazines behind `FIREARM_PERMIT`; ART II and 4x20 scopes, M12 and PM holsters, an M14 pouch for anyone (`Configs/Catalogue/LF_Catalogue_GunStore.conf`). Self-serve on two `ShelfShop_01` (store key `everon_gunstore`, restock 300 s): ushanka, red and brown knitted hats, flat cap, folded map, B8 binoculars, Adrianov compass, paid at the `CashierShop_01` till on the counter (`Configs/Shop/LF_ShopPrices_GunStore.conf`). `Village_GunStore_Clerk` stands behind the counter and calls out a walk-out; the bell box between the window and the arch carries the alarm. Proof: `tools\cli building proof gun-store` (M9 refused without the permit, `/licence FIREARM_PERMIT`, bought; binoculars paid at the till; a compass walk-out sets the alarm on the box and a wanted level). Not yet: a way for players to get a firearm permit (#1069), stock on the back-wall shelves behind the counter, the order slip takes one line per order.
- ✅ (2026-11-16, town-gunstore-polish-2, #2012) **Gun store second polish pass.** `tools/buildings/gun-store/decor.mjs` now lights the ceiling at night (the spots all aim down and the ceiling sat 0.2 m above them, so it was black): three soft point lights hang under the fittings, the five lamp covers carry the base game's lit diffuser material, and a floor lamp and a bench lamp have a light each (10 lights in all). It fills the empty middle and right wall: a display table of cleaning kits and cases, a coat tree, a tall glass-fronted gun cabinet with a deer mount on the right wall, and a repair corner inside the shop room (a workbench with vice, toolbox, solvent tins and desk lamp, a stool, notices, a floor lamp); all plain props, none pickable. The far "DROGERIE" is the vanilla `ShopHouse_E_2I01t` distance atlas (`House_Town_E_2I01_Ext_MLOD`, one 512 px texture of walls and roof with the food-store sign baked in), not the sign entity. `tools/buildings/shophouse-far/mlod.mjs` copies that BC7 texture, paints the sign's blocks out in every mip, and writes `Assets/Buildings/Facades/LF_ShopHouse_Ext_MLOD` (.emat, `_BCR.edds`, `_BCR.tif`); `GunStore.et` assigns it to the `House_Town_E_2I01_Ext_MLOD` slot only, so the walls and interior LODs are untouched. Any other ShopHouse prefab can assign the same material.
- ✅ (2026-11-16, town-clothing-store) **The 1989 Everon clothing store ("Moděva") is a working store on the main street.** `Village_ClothingStore` (Everon1989Town `Village_Buildings.layer`, 360 0 266, `angleY 180`, north row between the village house at 343 and the gun store, facing the street) places `Prefabs/Buildings/ClothingStore/ClothingStore_Store.et` (pack), written by the building pipeline from `tools/buildings/clothing-store/spec.json`. It inherits our `ShopHouse_E_2I01t_clothesstore_lf.et` (the vanilla clothes-store variant of `ShopHouse_E_2I01t_Base.et`, its fascia's MLOD set to the painted face so the street no longer sees the vanilla "DROGERIE" distance texture) and `Furniture/ShopHouse_E_2I01t_furniture_clothesstore_lf_01.et` (the vanilla set flattened, without the broom, bucket, fallen tube, attic shelf and pub A-board; with two `LightIndustrial_02_on_interior` spot lamps over the window, a lit ceiling lamp and a coat tree on the pavement). Two dressed mannequins (`LF_Mannequin_Denim`, `_Raincoat`, #1886) in the shop window and a third (`_Suit`) inside. Self-serve on three `ShelfShop_01` and two `CounterShop_01` display counters (store key `everon_clothingstore`, restock 300 s): cotton shirt, turtlenecks, denim jacket, trousers, fisherman trousers, suit jacket and trousers, black knitted hat, blue and light-grey flat caps, leather gloves, Soviet boots, one folded copy per shelf point, paid at the `CashierShop_01` till (`Configs/Shop/LF_ShopPrices_ClothingStore.conf`). `Village_ClothingStore_Clerk` (`LF_Npc_ClothingClerk`, a shopkeeper in a grey suit) calls out a walk-out; the bell box on the shop front carries the alarm. Proof: `toolscli building proof clothing-store`. Not yet: folded piles (the `stack` store step, #1899, was reverted in #1902), raincoats on the shelves (0.56 m folded, deeper than any board), a collider on the mannequins.

- ✅ (2026-11-16, #1770) **1989 clothing and equipment outlets.** `LF_Catalogue_Clothing_Everon1989.conf` adds 32 period pieces (denim, suit and turtleneck tops, trousers, flat cap, knitted hat, hard hats, M70 surplus, canteen, binoculars, ALICE pack, police and doctor kit) and offers them at `clothing_rack`, `workwear_counter`, `police_stores` (job POLICE), `medic_stores` (job MEDIC) and `surplus_stall`; the core raincoat keeps `concealsBuild`. No Rally DLC prefab. `fixtures/everon/outlets-clothing.json` pins each outlet's must-sell and must-not-sell items. Five counters stand in the `everon-outlets` grid cell. Open: the base game has no balaclava, mask or lockpick prefab, so the black market outlet and the `concealsFace` items wait for new assets.
