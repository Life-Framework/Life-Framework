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
| 4 | **Taxi rank** | `Place_saint-philippe_taxi`: an `S105_beige`; fares in `LF_TaxiFares_Everon1989.conf` | None | The square, beside the bus stop | TP-1. **Round 3 (#4679, 2026-11-24, merged, in-dev):** the taxi job does not need a rank place to start (the job code finds the pad by its `LF_JobSiteComponent`), and the legacy `Roadmap_Job_TaxiPad` already stands beside the square bus stop. The rank is now built around it in `Village_Taxi.layer`: the `TaxiRank` place `saint-philippe.taxi` on the pad (its map icon is back: the `TaxiRank` row left the town POI overlay) and the pack's yellow `LF_S105_Taxi` as the locked job cab beside the legacy beige car, so a fare hands the driver the taxi (`HandNearestJobVehicle`, 15 m). Starting still needs the island's gates: sign up at the depot (taxi is level 6, `levelset DELIVERY 6` in a harness session) |
| 5 | **Onboarding: arrival and clerk** | `Place_saint-philippe_arrival`: `LF_Npc_Clerk_1` at `TownHall_furniture_01`, plate, 4 waypoint markers and a spawn marker; welcome gift (`LF_WelcomeGift_Everon1989.conf`, quest `arrival_register`) | `Quest_Clerk` (`Quests.layer`, x 137, z 241) and the `Street_SpawnHub` desk (x 119, z 244) with weather and time debug actions. No gift conf, no arrival markers, no clerk place | The square, clerk at the municipal office (x 182, z 267) or the existing clerk spot; the spawn hub debug actions go to the grid | TP-2 (world), TP-6a (gift conf) |
| 6 | **Onboarding: hostel** | `Place_saint-philippe_hostel`: 2 x `LF_FirstSpawnBed`, bunk bed, suitcases, fascia sign | None. The player spawns at `SpawnPoint_LIFE` (`Gamemode.layer`) | A vanilla house on the square (candidate: the `House_Town_E_2I01` at x 156, z 266; TP-2 confirms it is unused) | TP-2 |
| 7 | **Onboarding: starter car and car lot** | `Place_saint-philippe_carlot` (3 till counters, 14 cars with offers), `Place_saint-philippe_parked-cars` (6 x `LF_ParkedCarMarker`), `Place_saint-philippe-districts_garage`, `Place_saint-philippe_police-cars` (3 markers); `LF_ParkedCars_Everon1989.conf`, `LF_VehiclePrices_`, `LF_Garage_` | `Street_Vehicles` (x 201..246, z 257..274): 7 `LF_VehicleOffer`, 3 spawn points, a garage and an impound desk, legacy form; four parked cars in `Village_Dressing`. No markers; the three vehicle confs are not loaded | The lot where it stands; markers on the four dressing cars | TP-2 |
| 8 | **Onboarding: orchard and press** | `Place_saint-philippe_orchard`: 7 trees, `AppleJuicer`, `MarketStand_01`, cart, `LF_Workplace` (`saint-philippe.orchard`) | `AppleChain` (x 130..215, z 233..310): 3 apples, an `AppleJuicer`, two trader tables, one `LF_GatherAction`; `PlumChain`; `Survival` fruit trees. No `LF_Workplace`, no place id | Keep the `AppleChain` ground, swap the legacy tables for a workplace desk with the orchard id | TP-2 |
| 9 | **Onboarding: market** | `Place_saint-philippe_market`: `LF_MarketStall`, `LF_UniversalTrader_Everon1989`, `LF_PriceBoard`; `Configs/Market/*` (6 confs) | `TraderChain` (`UniversalTrader`, `LF_Npc_Trader_1`, x 198..215, z 233) and `MarketStand_01` in `Shops.layer`. No stall prefab, no price board, no market conf loaded | The general store side of the square (x 179..215, z 233) | TP-2 (stall, price board), TP-6a (market confs) |
| 10 | **Mining: desk and rocks** | `Place_coalmans-brow_mine`: `LF_Workplace` (the mine desk), 6 x `IronOre`, 9 granite boulders, cart, picks, lantern, bus stop; `LF_Jobs_Everon1989.conf`, `LF_Catalogue_MinerTools_Everon1989.conf` | `MiningChain` (x 148..206, z 146..193): a miner stand with legacy display, 3 `LF_OreNodeComponent` (iron, gold, copper), 23 granite stones and cliffs, `ConeTraffic`, a miner NPC. No workplace desk | The `MiningChain` ground | TP-3 |
| 11 | **Mining: furnace** | `LF_MineFurnace` (pack prefab) in the mine | A `Furnace` in `CrimeChain` (x 298, z 277) which belongs to the crime chain, not to the mine | Beside the mine desk | TP-3 |
| 12 | **Mining: pickaxe counter** | `Place_saint-philippe_hardware`: 2 x `LF_ShopCounter`, clerk, tool rack, fascia (`saint-philippe.hardware`) | None. No hardware shop in the town | **New: a store**. Candidate: `House_Village_E_1I06` (x 318, z 236) or a shed at the mine; vanilla building, clerk and counter | TP-3 |
| 13 | **Mining: iron buyer** | `LF_IronBuyer_Everon1989` and `LF_WholesaleBuyer_Everon1989` in `Place_saint-philippe-dock_wholesale` (id `saint-philippe-dock.wholesale`, 2 clerks, price board); `LF_BuyerDemand_Everon1989.conf` | A `TraderChain` table only | A buyer yard at the mine road end (x about 190, z 190), clerk and price board | TP-3 |
| 14 | **Postal run** | `Place_saint-philippe_parcel`: counter and plate (`ParcelDepot`, `saint-philippe.parcel`); `Place_saint-philippe_parcel-drops` 5 x `LF_ParcelDrop`; `Place_saint-philippe_postal-drops` 2 x `LF_PostalPoint_Everon` | `Street_PostOffice` (x 160, z 260): a clerk, notebook, boxes; `Street_Jobs`: legacy job sites with sign-up and drop-off actions, `CounterPostOffice_01`; `PostOfficeBox_01_Stand` in dressing. No `LF_ParcelDrop`, no `LF_PostalPoint` | Depot at the post office counter, 5 drops on the street houses, 2 postal points (square and the clothing end) | TP-4 (#4244): placed in `Village_Postal.layer`; **van run blocked**, see below |
| 15 | **Clothing: mannequins and counter** | `Place_saint-philippe_clothing`: 5 mannequins (`LF_Mannequin_Denim`, `_Raincoat`, `_Knit`, `_Suit`, `_Backpack`), `LF_ClothesShopCounter`, `LF_ClothesTill`, 3 shelves, coat racks | `Village_ClothingStore` (x 360, z 266) and `Village_ClothingStore_Clerk` only; `ClothingStore_Store` prefab has its own shelving, no mannequins, no pack counter. `LF_Catalogue_Clothing` and `LF_ShopPrices_ClothingStore` are loaded, the first is not | Inside the existing building | TP-5 |
| 16 | **Shop counters of the stores the town already has** | Pharmacy (`LF_PharmacyCounter`, 3 `LF_PharmDisplay_*`), gun counter (`LF_GunCounter`, 2 weapon rack displays, clerk 2), general store (`LF_Radio_Store`, `CashierShop_01`, 2 till counters) | Drugstore (x 140, z 266, pharmacist), gun store (x 378, z 266, shopkeeper), general store (x 179, z 234), supermarket (x 76, z 96, which is in the test annex, not on the street): the shop prefab counters, no pack counter or display prefabs | Same buildings | TP-5. Status (#4328): the gun store and pharmacy counters, the weapon racks, and the general store radio, till and clerk were already in the town store prefabs; the three pharmacy counter displays are now placed (`Village_Pharmacy.layer`); the island's second general-store till (miners supply) has no free counter top, so it waits for a counter move **Pharmacy (#4679, 2026-11-24, merged, in-dev):** the 73 single-item displays on the host gondolas that seat on the town building (the island kit `pharmacy` has 90; 17 had no shelf or sat in book clutter in the town and are left out), turned from building-local rows into world rows in `Village_Pharmacy.layer`, a `StoreCounter` place `saint-philippe.pharmacy` and a `counter` resident anchor where the placed pharmacist stood, and the placed pharmacist is gone: Etienne Garnier (`shop_pharmacist`) is a resident for the town, from the town-only `LF_Residents_PlaytestTown.conf` (all day at the counter; it also drops the core's two example residents, which the town could not place). The dispensary sheet opens at the counter. The talk is still the shopkeeper's, as on the island |
| 17 | **Shops the town has no building for** | `bakery`, `butcher`, `cafe`, `barber`, `newsagent`, `farm-supply`, `seed-feed`, `furniture` places (counters, displays, tills) | None | Not hours 0 to 2 except bakery and cafe food; see TP-10 | TP-10 (new) |
| 18 | **Map POIs** | `LF_MapPois_Everon1989.conf` (6 kinds) and `LF_MapPois_SaintPhilippe.conf` (21 kinds: Clerk, StoreCounter, ClothingCounter and so on), `LF_MapAreas_Everon1989.conf` | None of the confs is in the mission. No town entity has a POI kind | Every row above gets its kind and id | TP-6a, TP-6b |
| 19 | **Run chains** | `LF_MapRuns_Everon1989.conf`: 19 stops in `mining`, `apples`, `postal`, `lumber`, `fishing` | Not loaded | Hours 0 to 2 need `apples`, `mining`, `postal`; `lumber` and `fishing` come with TP-10 | TP-6a, TP-6b |
| 20 | **Welcome guide and HUD rows** | F3 handbook `LF_Handbook_Everon1989.conf`, quiz `LF_Quiz_Everon.conf`, F2 "what to do next" fed by the runs, welcome gift | None of the four is in the mission | No placement; config only | TP-6a |
| 21 | **Jail doors** | `Place_saint-philippe_jail`: 4 x `EL_JailDoor`, a cell door, 2 `JailCell` places, laundry tub, kitchen sink, yard broom (the work actions) | `Street_Jail` (x 231..236, z 219..245): one `EL_JailDoor`, one `EL_JailCell`, 2 `LF_JailWorkAction`, a put-in-vehicle action, a debug jail-me action; `LF_JailBreak_Everon1989.conf` is not loaded | The police yard where it stands | TP-7. **Round 3 (#4679, 2026-11-24, merged, in-dev):** the cell door already is the island's: `Roadmap_Jail_Door` (cell) and `Roadmap_Jail_OuterGate` between two wing doors are the same `EL_JailDoor` prefab with the same breach and lock components as the island's `jail_cell_door` and `jail_outer_door` (#4336), and `LF_JailBreak_Everon1989.conf` is loaded in the town header. What was missing was the island's three work stations: `Village_Jail.layer` now stands the laundry tub, kitchen sink and yard broom inside the yard (the core bucket and basket stay, because the core DebugWorld proof and its layout baseline name them). A jailed player at the door is pulled back to the cell origin, as on the island |
| 22 | **Bail phone** | `LF_JailBailPhone` (core prefab, `Prefabs/Police/`) | None | Inside the cell block, reachable from the cell | TP-7 (#4247: done, `Town_Jail_BailPhone` on an office desk against the yard's north wall, 7.8 m from the cell, inside its 8 m zone) |
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

**TP-4 result (#4244, 2026-11-23).** `Everon1989Town_Layers/Village_Postal.layer` holds the `ParcelDepot` place `saint-philippe.parcel` on the existing `Roadmap_Job_Depot` counter, 5 `LF_ParcelDrop` on house walls (east side) and 2 `LF_PostalPoint_Everon` (square, east end). Both runs still refuse in the town, by distance: the parcel drop-off needs 300 m from the start (`m_fRunMinDistance`, `LF_Jobs.conf`) and the van run needs another office 800 m away (`LF_PostalRun.conf`, loaded by fixed path, not overlayable), and the village is about 300 m by 215 m. A follow-up card sets a shorter distance for the playtest-town mission only.

**TP-6b result (#4246, 2026-11-23).** The runtime answer to "what does a run or POI do with no place": a POI with no place draws nothing, and a run stop with no place is skipped; a guide step with no place has no arrow. So the town header now carries three town-only overlays (`LF_MapPois_PlaytestTown.conf`, `LF_MapRuns_PlaytestTown.conf`, `LF_Activities_PlaytestTown.conf`) that remove every row whose place the town does not yet stand, and `LF_Test_PlaytestTownParityData` checks what is left resolves. At card time the town placed only the `ParcelDepot`, `PostalPoint`, `Orchard` and `MarketTrader` places and the miner workplace, so what stayed is the `apples` and `postal` runs. **Rows each card restores (delete the row from the overlay when its place stands):** TP-1: POI `TaxiRank`, `TownBoard`, `Bank`; TP-2: POI `Clerk`, `HostelBed`, `StoreCounter`, `Garage`, `VehicleLot`, guide `apples` (`OrchardTrees`); TP-3: runs `mining`, POI `WholesaleCounter`, guide `mining` (`StoreCounter coalmans-brow.mine-supply`, `Processor`, `WholesaleCounter`); TP-4 follow-up: guide `postal` (`LicenceDesk`); TP-5: POI `ClothingCounter`, `GunCounter`; TP-10: POI `FuelTill`, `BarberChair`, `CafeTable`, `FishingSpot`, `RoadFuel`, `FarmCompound`, `RoadFarm`, `WheatField`, `ChopYard`, `TimberCounter`, `ScrapRecycler`, runs `lumber`, `fishing`, guides `lumber`, `fishing`; TP-11: POI `PoliceStation`, `PoliceArmoury`, `HarbourMaster`, `Church`, `RangeLane`, `PlotForSale`, guides `police`, `medic`. Island-only POI kinds (`Fence`, `contested-site`, `Smokehouse`, `mine`, `plant`, `FuelDepot`) stay out of the town.

**TP-6b follow-up result (#4388, 2026-11-23).** Rows deleted from the town overlays because their place now stands, each as a `LF_PlaceComponent` marker in `Village_Onboarding.layer` or the new `Village_Places.layer` at an entity the town already had, with the island's kind and id: POI `TownBoard` (`saint-philippe.board`, the `Sign_TownMap` board), `Bank` (`saint-philippe.bank`, the teller), `PoliceStation` (`saint-philippe.police`, the duty desk), `ClothingCounter` (`saint-philippe.clothing`), `FuelTill` (`saint-philippe.fuel`), `GunCounter` (`saint-philippe.gun-counter`); guide `apples` (`OrchardTrees saint-philippe.orchard`), `police` (`PoliceDesk saint-philippe.police`), `medic` (`MedicDesk saint-philippe.clinic`). Still removed (28 POI kinds, 3 runs, 4 guides): everything whose place the layers do not hold. The mining run and guide also stay out: the miner desk registers `Workplace coalmans_brow_mine` (the `LF_Test_EveronMining*` tests and `LF_BuyerDemand` key on that id), the runs name `coalmans-brow.mine`, and the pickaxe counter, furnace and dock buyer are not placed. `playtest-town/parity-data` is now in `tools/test/town-tests.json` with its own `world` (the town header), so `test --town` and the main guard run it on the header.

**TP-8 result (#4480, 2026-11-24).** Round report: [2026-11-25-playtest-town.md](../playtest/rounds/2026-11-25-playtest-town.md). Rows 1 to 22, each done or with its card:

| Row | System | Result |
|---|---|---|
| 1 | ATM kiosks | done; `ATM_Debug` still stands, #4680 |
| 2 | Bus stops | done |
| 3 | Boards | done (square board); per-stop boards not walked, #4679 |
| 4 | Taxi rank | not placed, #4680 |
| 5 | Arrival and clerk | welcome pack not seen, #4677; round 2: the conversation opens from the customer side but the harness stops at `welcome_room`, #4725 |
| 6 | Hostel | not placed, hostel text points to it, #4676 |
| 7 | Starter car and lot | done; round 2 (#4679): garage retrieve took the car out, plate and marker shown |
| 8 | Orchard and press | done; round 2: 3 gathers give 3 apples, the press makes 1 juice, "Needs Apple" afterwards is correct |
| 9 | Market | done |
| 10 | Mine desk and rocks | done; round 2: ore strikes register (`piece 1/5`, 4 nuggets dropped); duplicate Pickaxe row, #4680 |
| 11 | Furnace | done; wording, #4680 |
| 12 | Pickaxe counter | done |
| 13 | Iron buyer | done |
| 14 | Postal run | done |
| 15 | Clothing | done; round 2: counter sheet opens (Raincoat $60, Jean Jacket $45), a mannequin purchase took $30 |
| 16 | Shop counters | general store and gun counter sheets done; pharmacy opens a generic shopkeeper talk with bare shelves, no counter sheet (minor, round 2) |
| 17 | Shops with no building | TP-10 (plan) |
| 18 | Map POIs | done; round 2: the map shows bus stops, post offices, clerk, orchard, mine, clinic, gun; a VME every frame while it is open, #4726 |
| 19 | Run chains | not walked: the run list is behind the clerk's papers, #4725 |
| 20 | Guide and HUD rows | quiz, F3 and starting money ($100, $2,000) done; F2 opens but lists no rows until the clerk's papers, #4725 |
| 21 | Jail doors | yard, timer and bail done; the door is a breach prop, not a cell door (minor, round 2) |
| 22 | Bail phone | done |

**TP-8 round 2 result (#4679, 2026-11-24).** Report: [2026-11-24-playtest-town-r2.md](../playtest/rounds/2026-11-24-playtest-town-r2.md). Rows now passing: 7, 8, 10, 15, 18. Partial: 16 (pharmacy), 19, 20 (F2), 21. Still open and filed: the harness cannot finish the clerk conversation (`welcome_room` ignores `MenuSelect`, no digit key), which keeps row 5, row 19 and the F2 rows unverified (#4725, with #4683 and #4677); the map throws a VME every frame in the town (#4726); the clerk's hostel line (#4676). Client 2 walked on its own and rejoined once: same identity, cash and bank kept.

**Island sync result (#4679, 2026-11-24).** `test --town` was red on tonight's island additions; each is now placed in the town or left out with a reason. Placed, all pack layers in `Everon1989Town_Layers/`: `Village_Org.layer` (`OrgDesk saint-philippe.org` on the existing registry desk, the resident civilian in the Office_E_01 hall is now the island's registrar `LF_Npc_Townsfolk_Registrar` with the `registrar` dialogue, and the `LF_OrgFlagWall` on the lawn by the office's east wall); `Village_Labour.layer` (`LabourOffice saint-philippe.labour`, an `LF_LabourCounter` beside the office path: the office hall's floor is full of stairs and door swings, three interior spots failed the placement audit); `Village_Bar.layer` (`PubBar saint-philippe.pub` and a barkeeper behind the Hostinec pub's own bar with buyer id `bar_saint_philippe`, two demand rows in `LF_BuyerDemand_Bars_Everon1989.conf`); `Village_TruckYard.layer` (`TruckLot saint-philippe.truckyard`, the lot counter, examiner, `TruckCheckStart truck_check_start` and four flagpole gates on open grass at x 290..345, z 190..196, no display trucks, no cones); `Village_Places.layer` (`FenceYard saint-philippe.fence` on the existing `FenceStall`, which the F2 `rumor` row needs, and an `LF_OutpostBoard` on the square; `signs/coverage` lists `outpost_stock` as pack-placed). Left out with a reason: map POI kind `Town` (`LF_MapPois_PlaytestTown.conf` removal row): its places are the island's five towns (`town.*`), and the playtest town is one street, so no place stands for it.
