# Playtest town parity: the gap list (TP-0, #4240)

Plan: [next-release-0.3.0.md](next-release-0.3.0.md), chapter 7. This is the list the TP cards work from. It was
read from the layers and configs on `main` at 4deeada6 (2026-11-22); no layer was touched.

**Sources compared**

- **Island**: `addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_*.layer` (plus `Homes_`, `Locks_`,
  `Lights_`, `Plots_`), the pack configs under `addons/LF_Everon1989/Configs/`, and the mission header
  `Missions/LF_Everon1989.conf` (90 config overlays).
- **Town**: the DebugWorld village layers (`addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/`, minus `Annex`
  and `Tests`), the pack world `Worlds/Everon1989Town/Everon1989Town_Layers/Village_*.layer`, and the mission header
  `Missions/LF_PlaytestTown.conf` (20 overlays).

## What the diff says in three lines

1. **The town mission loads about a fifth of the island's configs.** `LF_PlaytestTown.conf` lacks 70 of the 90
   island overlays: jobs, map POIs, map runs, bus stops, welcome gift, quest giver table, handbook and quiz, market,
   parked cars, garage, property, processing, police kit, medic, sites and more. A prefab placed in the town
   without its config does nothing, so **the overlay edit is the first card (TP-6a below)**.
2. **The town has the same systems in their old debug form.** Legacy actions stand where the island has a placed
   prefab with an `LF_PlaceComponent`: `ATM_Debug`, the `MiningChain` stand, the `AppleChain` trader tables, the
   `Street_Jobs` job sites, the `Street_Jail` cell. Only 8 town entities carry an `LF_PlaceComponent` at all (the
   bank bag, the cash van stops, the fuel dispatch sources, the film till), and the POI and run configs I read
   name none of those kinds. So the map shows nothing for the town.
3. **Island stops are found by `kind` and `id`** (`LF_Places.Find`, [core-vs-pack](../architecture/core-vs-pack.md)
   "Map independence"). The map runs (`LF_MapRuns_Everon1989.conf`) name ids such as `coalmans-brow.mine`,
   `saint-philippe.orchard`, `saint-philippe.market`. A town card gives its entities the **same `kind` and `id`
   the island uses** (one shared config, no second table) unless the card finds a reason not to. I did not read what
   a run does when a stop's place is absent; TP-6b checks that first.

## The gap list

Town spots are in world metres (x, z) from the layers. "Island source" names the layer or kit; "Town today" is what
stands there now. The last column is the card that takes the row.

| # | System | Island source | Town today | Town spot | Card |
|---|---|---|---|---|---|
| 1 | **ATM kiosks** | `Place_saint-philippe_atms`: 3 x `LF_ATM` (`atm.saint-philippe.b79`, `.b97`, dock `.b08`); the bank counter and teller in `Place_saint-philippe_bank` | `ATM_Debug` (a debug-looking stand with `LF_ATMAction`, `ATM.layer` x 175..179, z 262..264) and `Bank_Teller`; `Annex_ATM` in the test annex. No `LF_ATM` prefab, no place id | One kiosk at the bank front (x 193..197, z 259..262, the `Street_Bank` bag is there), one at the square (x about 129, z 250), one at the fuel station (x 266, z 238). Retire `ATM_Debug` from the street | TP-1 |
| 2 | **Bus stops** | `Place_saint-philippe_bus-stops`: 9 x `LF_BusStop`; one more in `Place_coalmans-brow_mine`; fares in `Configs/Travel/LF_BusStops_Everon1989.conf` | `BusShed_E_01_blue` as scenery in `Village_Dressing` only. No `LF_BusStop`, and the fares conf is not in the mission | Stops at the square, the post office (x 160), the hospital (x 254..272), the fuel station, the clothing end (x 360), the farm (x about 140, z 300) and the mine (x about 176, z 167). Reuse the existing bus shed model where it stands | TP-1 |
| 3 | **Boards** | `Place_saint-philippe_board`: `LF_TownBoard` (map board, kind `MapBoard`); `Place_montignac_depot`: another; cards in `Configs/TownBoard/LF_TownBoard_Everon1989.conf` | `Sign_TownMap` (`LF_TownBoard`, `Signs.layer` x 128, z 250) already stands and the town board conf is loaded. A bus board at each stop (price and next stop) does not exist | Keep the square board; one board per bus stop from row 2 | TP-1 (stop boards), TP-6b (tag the map board's kind and id) |
| 4 | **Taxi rank** | `Place_saint-philippe_taxi`: an `S105_beige`; fares in `LF_TaxiFares_Everon1989.conf` | None | The square, beside the bus stop | TP-1 |
| 5 | **Onboarding: arrival and clerk** | `Place_saint-philippe_arrival`: `LF_Npc_Clerk_1` at `TownHall_furniture_01`, plate, 4 waypoint markers and a spawn marker; welcome gift (`LF_WelcomeGift_Everon1989.conf`, quest `arrival_register`) | `Quest_Clerk` (`Quests.layer`, x 137, z 241) and the `Street_SpawnHub` desk (x 119, z 244) with weather and time debug actions. No gift conf, no arrival markers, no clerk place | The square, clerk at the municipal office (x 182, z 267) or the existing clerk spot; the spawn hub debug actions go to the grid | TP-2 (world), TP-6a (gift conf) |
| 6 | **Onboarding: hostel** | `Place_saint-philippe_hostel`: 2 x `LF_FirstSpawnBed`, bunk bed, suitcases, fascia sign | None. The player spawns at `SpawnPoint_LIFE` (`Gamemode.layer`) | A vanilla house on the square (candidate: the `House_Town_E_2I01` at x 156, z 266; TP-2 confirms it is unused) | TP-2 |
| 7 | **Onboarding: starter car and car lot** | `Place_saint-philippe_carlot` (3 till counters, 14 cars with offers), `Place_saint-philippe_parked-cars` (6 x `LF_ParkedCarMarker`), `Place_saint-philippe-districts_garage`, `Place_saint-philippe_police-cars` (3 markers); `LF_ParkedCars_Everon1989.conf`, `LF_VehiclePrices_`, `LF_Garage_` | `Street_Vehicles` (x 201..246, z 257..274): 7 `LF_VehicleOffer`, 3 spawn points, a garage and an impound desk, legacy form; four parked cars in `Village_Dressing`. No markers; the three vehicle confs are not loaded | The lot where it stands; markers on the four dressing cars | TP-2 |
| 8 | **Onboarding: orchard and press** | `Place_saint-philippe_orchard`: 7 trees, `AppleJuicer`, `MarketStand_01`, cart, `LF_Workplace` (`saint-philippe.orchard`) | `AppleChain` (x 130..215, z 233..310): 3 apples, an `AppleJuicer`, two trader tables, one `LF_GatherAction`; `PlumChain`; `Survival` fruit trees. No `LF_Workplace`, no place id | Keep the `AppleChain` ground, swap the legacy tables for a workplace desk with the orchard id | TP-2 |
| 9 | **Onboarding: market** | `Place_saint-philippe_market`: `LF_MarketStall`, `LF_UniversalTrader_Everon1989`, `LF_PriceBoard`; `Configs/Market/*` (6 confs) | `TraderChain` (`UniversalTrader`, `LF_Npc_Trader_1`, x 198..215, z 233) and `MarketStand_01` in `Shops.layer`. No stall prefab, no price board, no market conf loaded | The general store side of the square (x 179..215, z 233) | TP-2 (stall, price board), TP-6a (market confs) |
| 10 | **Mining: desk and rocks** | `Place_coalmans-brow_mine`: `LF_Workplace` (the mine desk), 6 x `IronOre`, 9 granite boulders, cart, picks, lantern, bus stop; `LF_Jobs_Everon1989.conf`, `LF_Catalogue_MinerTools_Everon1989.conf` | `MiningChain` (x 148..206, z 146..193): a miner stand with legacy display, 3 `LF_OreNodeComponent` (iron, gold, copper), 23 granite stones and cliffs, `ConeTraffic`, a miner NPC. No workplace desk | The `MiningChain` ground | TP-3 |
| 11 | **Mining: furnace** | `LF_MineFurnace` (pack prefab) in the mine | A `Furnace` in `CrimeChain` (x 298, z 277) which belongs to the crime chain, not to the mine | Beside the mine desk | TP-3 |
| 12 | **Mining: pickaxe counter** | `Place_saint-philippe_hardware`: 2 x `LF_ShopCounter`, clerk, tool rack, fascia (`saint-philippe.hardware`) | None. No hardware shop in the town | **New: a store**. Candidate: `House_Village_E_1I06` (x 318, z 236) or a shed at the mine; vanilla building, clerk and counter | TP-3 |
| 13 | **Mining: iron buyer** | `LF_IronBuyer_Everon1989` and `LF_WholesaleBuyer_Everon1989` in `Place_saint-philippe-dock_wholesale` (id `saint-philippe-dock.wholesale`, 2 clerks, price board); `LF_BuyerDemand_Everon1989.conf` | A `TraderChain` table only | A buyer yard at the mine road end (x about 190, z 190), clerk and price board | TP-3 |
| 14 | **Postal run** | `Place_saint-philippe_parcel`: counter and plate (`ParcelDepot`, `saint-philippe.parcel`); `Place_saint-philippe_parcel-drops` 5 x `LF_ParcelDrop`; `Place_saint-philippe_postal-drops` 2 x `LF_PostalPoint_Everon` | `Street_PostOffice` (x 160, z 260): a clerk, notebook, boxes; `Street_Jobs`: legacy job sites with sign-up and drop-off actions, `CounterPostOffice_01`; `PostOfficeBox_01_Stand` in dressing. No `LF_ParcelDrop`, no `LF_PostalPoint` | Depot at the post office counter, 5 drops on the street houses, 2 postal points (square and the clothing end) | TP-4 |
| 15 | **Clothing: mannequins and counter** | `Place_saint-philippe_clothing`: 5 mannequins (`LF_Mannequin_Denim`, `_Raincoat`, `_Knit`, `_Suit`, `_Backpack`), `LF_ClothesShopCounter`, `LF_ClothesTill`, 3 shelves, coat racks | `Village_ClothingStore` (x 360, z 266) and `Village_ClothingStore_Clerk` only; `ClothingStore_Store` prefab has its own shelving, no mannequins, no pack counter. `LF_Catalogue_Clothing` and `LF_ShopPrices_ClothingStore` are loaded, the first is not | Inside the existing building | TP-5 |
| 16 | **Shop counters of the stores the town already has** | Pharmacy (`LF_PharmacyCounter`, 3 `LF_PharmDisplay_*`), gun counter (`LF_GunCounter`, 2 weapon rack displays, clerk 2), general store (`LF_Radio_Store`, `CashierShop_01`, 2 till counters) | Drugstore (x 140, z 266, pharmacist), gun store (x 378, z 266, shopkeeper), general store (x 179, z 234), supermarket (x 76, z 96, which is in the test annex, not on the street): the shop prefab counters, no pack counter or display prefabs | Same buildings | TP-5 |
| 17 | **Shops the town has no building for** | `bakery`, `butcher`, `cafe`, `barber`, `newsagent`, `farm-supply`, `seed-feed`, `furniture` places (counters, displays, tills) | None | Not hours 0 to 2 except bakery and cafe food; see TP-10 | TP-10 (new) |
| 18 | **Map POIs** | `LF_MapPois_Everon1989.conf` (6 kinds) and `LF_MapPois_SaintPhilippe.conf` (21 kinds: Clerk, StoreCounter, ClothingCounter and so on), `LF_MapAreas_Everon1989.conf` | None of the confs is in the mission. No town entity has a POI kind | Every row above gets its kind and id | TP-6a, TP-6b |
| 19 | **Run chains** | `LF_MapRuns_Everon1989.conf`: 19 stops in `mining`, `apples`, `postal`, `lumber`, `fishing` | Not loaded | Hours 0 to 2 need `apples`, `mining`, `postal`; `lumber` and `fishing` come with TP-10 | TP-6a, TP-6b |
| 20 | **Welcome guide and HUD rows** | F3 handbook `LF_Handbook_Everon1989.conf`, quiz `LF_Quiz_Everon.conf`, F2 "what to do next" fed by the runs, welcome gift | None of the four is in the mission | No placement; config only | TP-6a |
| 21 | **Jail doors** | `Place_saint-philippe_jail`: 4 x `EL_JailDoor`, a cell door, 2 `JailCell` places, laundry tub, kitchen sink, yard broom (the work actions) | `Street_Jail` (x 231..236, z 219..245): one `EL_JailDoor`, one `EL_JailCell`, 2 `LF_JailWorkAction`, a put-in-vehicle action, a debug jail-me action; `LF_JailBreak_Everon1989.conf` is not loaded | The police yard where it stands | TP-7 |
| 22 | **Bail phone** | `LF_JailBailPhone` (core prefab, `Prefabs/Police/`) | None | Inside the cell block, reachable from the cell | TP-7 |
| 23 | **Homes, locks, plots, night lights** | `Homes_saint-philippe` (32 `LF_HouseMarker` and sale signs), `Locks_saint-philippe` (69 `LF_BuildingLock`), `Plots_saint-philippe`, `Lights_saint-philippe` (31 window glows, night lamp heads); `LF_Property_Everon1989.conf` | `HousesKeys.layer`: one key desk and one `EL_Door_Base`. The town houses have no marker, no lock, no sign | Every house on the street | TP-9 (new) |
| 24 | **Fuel** | `Place_saint-philippe_fuel` (2 x `LF_FuelPump`, attended till, clerk, `FuelKiosk_Display`), `Place_saint-philippe_fuel-drops` (4), `Place_montignac_depot`, road fuel and road farm stops; `LF_Fuel_Everon1989.conf` | `Street_Fuel` (x 257..284, z 235..244): `LF_FuelTill_Attended`, a payphone, 2 townsfolk, `LF_FuelPumpHum`. No `LF_FuelPump`, no drop | The fuel station on the street | TP-10 (new) |
| 25 | **Later chains: lumber, wheat, honey, scrap, still, smokehouse, fishing** | `woodlot`, `everon-sawmill`, `wheatfield`, `farm`, `scrapyard`, `stillbarn`, `dock_smokehouse`, 3 fishing places | Legacy `LoggingChain`, `FarmingChain`, `Street_Moonshine`, `Street_ChopShop`; no water in the one-street town | Not hours 0 to 2. Fishing is island only | TP-10 (new, after TP-8) |
| 26 | **Police, hospital, org, range, chess, events, sites** | `Place_saint-philippe_police` (9 KB: punch clocks, cabinets), `_clinic`, `_org` (`LF_OrgWarBoard`), `_range` (3 targets), `_chess`, event markers in 5 places, `farmcompound` and `hilltopmast` sites | `Street_Police`, `Street_Hospital`, `Street_Org` stand and work in legacy form; range, chess, events and sites absent | Not hours 0 to 2 | TP-11 (new, after TP-8) |

Already at parity, nothing to do: payphones (`LF_Payphone` stands at the fuel station, the hospital and the police
station), the bank bag and cash van, the chop bench, the dispatch boards, the town board on the square, and the pub radio
and square ambience.

## TP-1 to TP-7: confirmed or re-cut

The plan's seven cards hold; the rows above sit inside them. Four changes:

- **TP-6 splits.** `LF_PlaytestTown.conf` is one file every other card needs edited, so one card does it
  **first**: **TP-6a** adds the 70 missing island overlays (or the subset the hours 0 to 2 need: jobs, bus stops, map
  POIs, areas and runs, welcome gift, quest giver table, handbook, quiz, market and stalls, parked cars, garage,
  vehicle prices, property, processing, police kit, medic) and runs the town once for config errors. It is a config
  edit, no layer, and lands before TP-1 to TP-5 and TP-7 start placing. **TP-6b** (the plan's TP-6) then tags every
  town entity from the other cards with its island `kind` and `id` so the POIs and runs resolve, and answers what a
  run does for a stop with no place.
- **TP-1** takes the **taxi rank** and the **bus stop boards**, and retires `ATM_Debug`.
- **TP-3** takes the **hardware counter** (row 12): the town has no shop that sells the pickaxe, so the mining run
  cannot start without it.
- **TP-5** is the clothing store and the three shops the town already has (pharmacy, gun, general). The other shop
  types are TP-10.

## New cards

| Card | What | When |
|---|---|---|
| TP-6a | Add the island's config overlays to `LF_PlaytestTown.conf`; boot the town and read the log | Day 2, first |
| TP-9 | Homes, door locks, plots, sale signs and night lights for the street houses (row 23) | Day 3 or later |
| TP-10 | The shop types the town has no building for (bakery, butcher, cafe, barber, newsagent, hardware if TP-3 slips, farm supply, seed feed, furniture), the fuel pumps and drops, and the later chains (rows 17, 24, 25) | After TP-8 |
| TP-11 | Police station and clinic detail, the org war board, range, chess, events and sites (row 26) | After TP-8 |

TP-8 (the parity check) walks hours 0 to 2: rows 1 to 22. Rows 23 to 26 are filed, not blocking.

## Notes for the card agents

- A card places with `tools\cli place`, then `audit placement --strict`, with `look` pictures before and after;
  the maintainer approved the town edits for this chapter (plan, chapter 7).
- Overlays on one mission header are the one shared file: TP-6a goes first, the others do not touch it.
- A pack prefab used in the town lands in the pack (`LF_Everon1989`); a core prefab (`LF_ATM`, `LF_BusStop`,
  `LF_Workplace`, `LF_JailBailPhone`) is the same one the island uses. No new prefab in this chapter unless a row
  above has none (hardware store counter set).
- What I did not read: the runtime behaviour of a run or POI with no matching place, and whether the pack world's
  terrain has water. Both are assumptions in rows 19 and 25; TP-6b and TP-10 settle them.
