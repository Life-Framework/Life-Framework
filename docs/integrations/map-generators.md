# Making a map for Life Framework

For people who write map generators and map tools (Reforger Mapper and the like). It says what Life Framework
looks for on a map, what each place needs to be, and what to leave alone, so a generated map can host a
roleplay server without a script change.

Everything here is read from the code and data in this repository. Each claim names the file it comes from.
The numbers are the ones in the repository on 2026-11-19; the commands named at the end of each section print
the current ones.

## How Life Framework finds places

The game never looks for a place by world coordinate, entity name, layer name or map name. A place is an entity
that carries a small component. When the world loads the component adds its entity to a table, `LF_Places`
(`addons/LifeFramework/Scripts/Game/Core/LF_Places.c`), under two strings:

- a **kind**: what the place is (`FishingSpot`, `Workplace`, `JailCell`, `StoreCounter`).
- an **id**: which one of that kind. The id is the key a feature saves, so it is stable and unique within its
  kind. A second entity that asks for a taken kind and id is refused and logged.

A system then asks for "the `JailCell` called `cell`", "every `FishingSpot`" or "the `StoreCounter` nearest this
player". If a map has none of a kind, that one feature switches itself off with one log line and the rest of the
game carries on (`LF_Systems.CheckPlaces`, `Scripts/Game/Feature/Systems/LF_Systems.c`). A generator that
leaves out a kind is allowed; it just does not get that feature.

The rule is written down in [core vs pack](../architecture/core-vs-pack.md) under "Map independence". Two parts
matter to a map maker:

1. **Core script holds no map knowledge.** What this particular island is (where things stand, what they are
   called, what they cost) lives in a pack, which is data. `addons/LF_Everon1989/` is the pack for the Everon
   1989 server. A pack for your map ships the same kinds of data.
2. **A place is a component on an entity.** Two ways to make one:
   - the generic marker `LF_PlaceComponent` (`Scripts/Game/Core/LF_PlaceComponent.c`): set `m_sKind`, `m_sId`
     and optionally `m_iOrder` (a series, such as the stops of one route, registers as `id#order`) and
     `m_iLiveState` (a map state that keeps the place dark until a server turns it on; `-1` is always on).
     If `m_sId` is empty the entity name is the id.
   - a feature's own component, which registers the kind for you and also does the feature's work (a
     workplace desk, a fishing spot, a jail cell). Most rows below are of this kind.

Registration happens when the entity is created, on the server and on every client. A kind whose map row is
private (for example the police armoury, shown only to police on duty) is held on the server only, and the server
sends each player the ones that player may see (`Scripts/Game/Feature/Map/LF_MapPrivatePlaces.c`).

## What reads a marker

A marker kind with no reader does nothing, so the tables below list the kinds that something looks up. The
readers are:

| Reader | What it does with a kind | Where |
|---|---|---|
| A feature's own code | Finds its places directly (the bank finds `BankVault`, the jail finds `JailCell`) | the file named in each row |
| The map's POI table | A kind with a row in `LF_MapPois.conf` becomes an icon on the map; a pack adds rows | `Configs/Map/LF_MapPois.conf`, `LF_Everon1989/Configs/Map/` |
| Resident timetables | A resident's home and work are place kinds; the resident walks to the place | `Scripts/Game/Feature/Residents/LF_ResidentAnchors.c`, `LF_Everon1989/Configs/Residents/` |
| The town board | A card points at a place by kind and id, or the nearest of a kind | `Scripts/Game/Feature/TownBoard/LF_TownBoard.c` |
| The systems catalogue | A system lists the kinds it cannot run without (`m_aPlaces`) and switches off when none is placed | `Configs/Systems/LF_SystemCatalog.conf` |

## Place kinds

### Places that do something on their own

These are placed as a prefab that already carries the component. Drop the prefab, set its id, done. Prefab
paths are under `addons/LifeFramework/Prefabs/`, except two marked "pack", which live in
`addons/LF_Everon1989/Prefabs/` (a pack of your own would carry its own copy of those, or inherit the component
onto a prefab of its choice).

| Kind | Component and prefab | What the place must be | Looked up by |
|---|---|---|---|
| `Workplace` | `LF_WorkplaceComponent`, `Prefabs/Jobs/LF_Workplace.et` | A desk or punch clock at a job's building. Set the job id (for example `FARMER`, `LUMBERJACK`) and a place id. Inside or beside a building the player can walk to | `Jobs/LF_WorkplaceComponent.c` (`LF_Workplace.FindAll(job, into)`), the map |
| `JobDrop` | `LF_JobSiteComponent` with site `DROP`, `Prefabs/Props/Roleplay/LF_ParcelDrop.et` (pack) | A mailbox or drop-off on a road or at a door. **The entity must be named**; its name is its id | `Jobs/LF_JobMapMarks.c`, `Jobs/LF_CivilianJob.c` |
| `taxi` | `LF_TaxiStandComponent`, `Prefabs/Jobs/LF_TaxiStand.et` | A kerb a car can pull up to | `Jobs/LF_TaxiStandComponent.c` |
| `FishingSpot` | `LF_FishingSpotComponent`, `Prefabs/Systems/Fishing/LF_FishingSpot.et` | On a quay, jetty or shore with sea or pond water within the spot's radius (default 12 m) | `Fishing/LF_FishingSpotComponent.c` |
| `JailCell` | `LF_JailCellComponent` | A room with a door, inside the police building. One per map; its id is `cell` | `Police/LF_Jail.c` |
| `BankVault`, `BreachTarget` | `LF_BankVaultComponent`, `LF_BreachTargetComponent` (pack prefabs `Prefabs/Props/Roleplay/BankVault/` and `Prefabs/Buildings/Bank/Bank_Kit.et`) | A closed room behind a door inside the bank. The vault id is a save key | `Crime/LF_Bank.c`, `Crime/LF_Breach.c` |
| `HospitalSite` | `LF_HospitalSite` on the hospital till and the supply cabinet | Inside the clinic | `Crime/LF_Hospital.c`, the map |
| `PlotForSale` | `LF_PlotComponent`, `Prefabs/Props/Property/LF_PlotSign.et` | A sign at the edge of a free plot of open ground. Its plot key names a row in `Configs/Property/` | `Property/LF_PlotComponent.c`, the town board, the map |
| `HideoutDoor`, `HideoutBaseDoor`, `OrgHideout` | Hideout door components | A door on a building a gang or organisation can hold | `Hideouts/LF_Hideout.c` |
| `contested-site` | `LF_ContestedSiteComponent` | Open yard in front of a farm barn or hut, away from town, that an organisation can hold | `Sites/LF_ContestedSites.c`, `Organizations/LF_OrgWarBoard.c` |
| `chain_buyer` | `LF_BuyerSpotComponent`, `Prefabs/Systems/Laundering/LF_BuyerSpot.et` | A quiet corner where a buyer stands in turn | `Laundering/LF_BuyerRotation.c` |
| `race_checkpoint` | `LF_RaceCheckpointComponent`, `Prefabs/Props/Racing/LF_RaceCheckpoint.et` | A gate on a road | `Racing/LF_RaceCheckpointComponent.c` |
| `restart-siren` | `LF_RestartSirenComponent` | Anywhere a restart warning should be heard; optional | `Ops/LF_RestartSirenComponent.c` |
| `Sign` | `LF_SignComponent` | A wall or post; the first sign with an id stands for it | `Signs/LF_SignComponent.c`, the town board |
| `event_waypoint`, `event_spawn`, `event_target`, `event_item` | `LF_EventWaypointComponent` | Scripted scenes; optional | `Events/LF_EventWaypointComponent.c` |

### Generic markers

These carry no behaviour of their own. Place `LF_PlaceComponent` with the kind and the id; something above
(the map, a resident, a board card, the systems catalogue) finds it.

| Kind | Put it | Read by |
|---|---|---|
| `CashVan`, `CashVanStop` | A van route and its ordered stops (`m_iOrder` 0, 1, 2). The route id is shared by the van and its stops | `Crime/LF_CashVanRoute.c` |
| `BankBag` | Inside the vault room, where the money bag sits | `Crime/LF_Bank.c` |
| `DispatchSource` | Where a police dispatch call appears to come from | `Comms/LF_DebugDispatch.c`, the harness |
| `Bank`, `PoliceStation`, `PoliceArmoury`, `HospitalSite`, `Workplace`, `PlotForSale` | In front of the building's door, on the ground the player stands on. These six have a POI row in the core table | `Configs/Map/LF_MapPois.conf` |
| `StoreCounter` | At the shop counter. The id ends in `.<slot>` (`saint-philippe.bakery`); a pack gives each slot its own icon with a row `StoreCounter/<slot>` | map POI table, resident timetables |
| `House` | One per resident home. Give each its own id so a resident finds their own door (`m_sHomePlaceId`) | `Residents/LF_ResidentAnchors.c` |
| `CafeTable`, `ParcelDepot`, `FuelTill`, `BankCounter` | At the counter or table where the resident works | resident timetables, map |
| `GunCounter`, `ClothingCounter`, `BarberChair`, `HostelBed`, `Clerk`, `TaxiRank`, `TownBoard`, `WholesaleCounter`, `Smokehouse`, `Fence` | Inside or at the front of the building they name | the pack's map POI table (`LF_Everon1989/Configs/Map/`) |

A resident's timetable names a kind and optionally an id. With no id the resident uses the nearest place of that
kind, and a named id that is not placed logs one warning and falls back to the nearest. So a map with one
`House` marker is playable, and a map with a marker per home is better.

## The buildings the Everon pack chose

The town tool reads a **template** per kind of settlement and picks a building for each **slot**
(`tools/town/templates/*.json`, `tools/town/map.mjs`). The template holds no coordinates and no names. What it
asks of a building is the best guide to what a generated building should look like, so the table lists those
asks. "Area" is the building's ground footprint in square metres. "Distance" is the farthest it may stand from
the centre of the settlement. Types are the names in `tools/scout/types.mjs`.

The `town` template (a hub: the first town a player sees), slots a server needs:

| Slot | Building asked for | Area | Distance | Ground-floor door | The marker the pack puts in it |
|---|---|---|---|---|---|
| arrival | town hall: `government`, `townhouse`, `house`, `villa`, `hotel`; interior; prefer large | 90+ | 80 m | no | the spawn, the clerk, the licence desk |
| hostel | `hotel`, `townhouse`, `villa`, `house`; interior | 80+ | 110 m | no | `HostelBed` |
| cafe | `pub`, `shop`, `townhouse`; interior | 50+ | 90 m | yes | `CafeTable` |
| bank | `townhouse`, `government`, `villa`, `house`; interior, with a room for a vault | 70+ | 100 m | no | `Bank`, `BankVault`, `BankCounter` |
| police | `police`, `villa`, `townhouse`, `government`, `house`; interior | 80+ | 110 m | no | `PoliceStation`, `JailCell`, desk, armoury |
| clinic | a drugstore shop-house, `townhouse`, `villa`, `house`; interior | 70+ | 130 m | no | `HospitalSite` |
| general-store | a general shop, `shop`, `townhouse`; interior | 60+ | 100 m | yes | `StoreCounter` |
| clothing | a clothes shop, `shop`, `townhouse`; interior | 50+ | 100 m | yes | `ClothingCounter` |
| gun-counter | `shop`, `townhouse`, `house`; interior, beside the police | 50+ | 110 m | yes | `GunCounter` |
| fuel | `fuel` station building, or a free plot of 16 by 14 m | 40+ | none | no | `FuelTill` |
| fence | `farm`, `farm-small`, `garage`, `shed`, `industrial`, or a free plot of 12 by 10 m, away from the square | 25+ | none | no | the fence yard |
| board | open kerb, street furniture | none | 60 m | none | `TownBoard`, `MapBoard` |

The optional shops use the same shape: supermarket (150 m², ground floor), hardware (60 m², `garage`,
`industrial` or `shop`), pharmacy (100 m²), newsagent (8 m², a `kiosk` is enough), seed and feed
(40 m², `farm`, `garage` or `shop`), bakery, butcher, barber and furniture (60 to 100 m², ground-floor
shop-house), parcel depot (8 m² kiosk, or a 6 by 6 m plot), organisation desk (15 m², interior), tow yard
(20 m², `garage` or `shed`), range (15 m² shed or garage). All are `StoreCounter` or the slot's own kind.

Also in the `town` template: church (`church`, 100 m², within 80 m), chapels (`chapel`), taxi stand, chess table,
market trader, memorial, safe-zone sign, and slots that fill several places at once (`parcel-drops`, `bins`,
`loot`, `buyer-spots`, `atms`, `payphones`, `parked-cars`, `street-life`) which place many small things
across the settlement rather than one in a building.

Smaller templates: `village` (hall, pub, one shop, fuel, garage, house, farm plot, board), `districts`
(a ring of homes with an optional fuel station, garage, shop and pub), `harbour` (dock, fish counter,
chandler, wholesale, warehouse, smokehouse, taxi, lockers), and one-slot sites (`everonsawmill`,
`chopyard`, `cove`, `lake`, `roadfuel`, `roadfarm`, `stillbarn`, `farmcompound`, `hilltopmast`).

### Homes and every building having a purpose

On Everon every enterable building is assigned a purpose: a home for sale, a shop or workplace, a resident's
home, a service (church, town hall, clinic), or locked. A building with no purpose is locked by a lock
marker. This keeps a town from being a set of open, empty houses. Today's split for Saint-Philippe is in
`docs/everon/places/saint-philippe/purpose.json`: 55 home-for-sale entries (houses plus their sheds and
garages), 27 shops and workplaces, 10 services, 6 resident homes and 32 locked buildings.

For a map maker this means: a house is worth generating if it has a door, an interior and a street frontage,
and is at least about 50 m². A home for sale carries a house marker (`Prefabs/Systems/Houses/LF_HouseMarker.et`,
which holds the lock and `LF_HouseManagerComponent`) at its footprint centre and a sale sign
(`Prefabs/Props/Property/LF_HouseSaleSign.et`) on the entrance wall. Price tiers run from a cottage to a villa
by size (`tools/town/homes.mjs`). Free plots for sale take the `PlotForSale` sign instead.

## Job sites

A job site is an area a player works in. Several are not `LF_Places` kinds at all; the game finds them by
component, by distance to the player, or through the workplace desk beside them. The "found by" column says
which.

| Site | What the ground and building must be | Found by | Notes for a generator |
|---|---|---|---|
| **Wheat field** | Open, fairly level pasture. The field is an axis-aligned box on world X and Z (`m_fSizeX` by `m_fSizeZ`, centred on the entity, **yaw is ignored**). Default 60 by 40 m: 260 cells of 3 m, 10 kg a cell, 2600 kg a field. A reaper cuts at 0.5 to 6 m/s and holds 600 kg | The harvest board finds the nearest field within 90 m (`Harvest/LF_HarvestBoard.c`). Prefab `Prefabs/Harvest/LF_WheatField.et` | Plan fields as east-west and north-south rectangles. A gate gap on one side. A farm barn beside it, and a weighbridge (vanilla `WeightStation_01`) within about 20 m of the gate for the grain |
| **Orchard** | A strip of open grass beside a farm barn | Each fruit tree has a gather action; the workplace desk (`FARMER`) makes it show on the map | Apple and plum trees (`t_malus_domestica_2s`, `t_prunus_domestica_2s`), about 8 m apart in two columns, a juicer stall nearby. Seven trees gives a small orchard (`tools/town/kits/orchard.json`) |
| **Farm plot** | A bed of open soil | Plot key (a position if none is authored) | Cabbage beds (`Prefabs/Vegetation/Crops/FarmPlot_Cabbage.et`) next to a seed crate |
| **Beehive** | Any open ground | A hive saves by its key, or by its position to 0.1 m if it has none (`Farming/LF_HiveComponent.c`) | Players place hives; a map may add a few for dressing |
| **Fishing quay and jetties** | Hard standing at the sea edge (a quay or a pier deck) or a lake or pond shore, with water within 12 m of the spot marker. The server checks the cast lands on sea or pond water | `FishingSpot` markers (`Fishing/LF_FishingSpotComponent.c`) | Spots may overlap (the nearer marker wins a cast), but about 25 m apart spreads players along a quay. Each holds 12 fish and regrows one every 120 s by default. The Saint-Philippe dock has about 210 vanilla quay and pier pieces; spots stand on the concrete pier. Also a fish counter and a chandler in a shed or garage of 30+ m² (`harbour` template) |
| **Sawmill and forest** | An `industrial` hall of 400+ m² with an open yard in front for a truck to turn, **away from town**, with real forest within a short haul | The yard's workplace desk (`Workplace`, job `LUMBERJACK`) | The trees are the map's own: felling reads the vanilla tree prefabs under `Prefabs/Vegetation/Tree/<species>/` (pines, spruce and larch are softwood; birch, hornbeam, lime, alder and poplar are hardwood). A forest of one species class near the mill is enough. The yard holds the saws, log decks and board stacks |
| **Quarry and mines** | A hill or a cut face, a road to it, open ground for a truck | Designed, not built yet. Each mine and plant is its own town-pipeline place with a workplace desk | See the chains below |
| **Fuel station on a main road** | A vanilla fuel station, small kiosk, on a road a car can reach, ideally outside town | `LF_JobSiteComponent` FUEL, the `FuelTill` marker, the roadfuel template | A pump kiosk, a drop barrel, a payphone and a sign. The fuel job unlocks after five parcel runs |
| **Delivery depot and drops** | A kiosk or shop of 8+ m² for the depot counter; drops are mailboxes beside doors | `ParcelDepot`, and `JobDrop` markers | Every drop must be at least 300 m (straight line) from where the run starts (`m_fRunMinDistance`, `Configs/Jobs/LF_Jobs.conf`). Five or more drops spread across the settlement, each beside a door and named |
| **Scrapyard** | A farm barn, garage or industrial shed of 25+ m² with open ground in front, away from the square | The workplace desk and the recycler | In the pack today a chop yard and a recycler exist; a full scrapyard place is still to come |
| **Harbour wholesale** | One big ground-floor hall of 300+ m² with truck bays (the Everon one is 28 by 19 m) | `WholesaleCounter` | The buyer takes logs, planks, ore, metal and cement in any quantity |

### Economy facts a generator can use

Printed by `tools\cli economy` for the Everon pack on 2026-11-19.

- **Runs.** A parcel run pays $40 (about 6 minutes), a fuel run $75 (about 8 minutes, unlocks after 5 courier runs), a
  taxi fare $100 for a 2000 m trip (unlocks after 15 runs). A new player starts with $1000 in the bank.
- **Duty pay band.** Police patrol is $900 an hour and a medic orderly $792. The rule is that a civilian
  chain pays between 1.5 and 2.5 times less than that. So one player on a chain should earn about **$320 to
  $530 an hour**. A site that pays more is a problem as much as one that pays less.
- **One site per chain, and a chain is a loop.** Gather, haul, process, sell. For lumber: a tree, hand haul of
  about 150 m to the yard (or a truck for hardwood), saw, sell. For wheat: field, hitched reaper, weighbridge at
  the field, mill, bakery. For mining (design): mine, haul 2 to 6 km, plant, sell.
- **Haul distance.** Under 300 m a delivery pays nothing. About 150 m by hand is the lumber figure. A
  truck loop is a few hundred metres to a few kilometres. The mining design uses mine-to-plant hauls of about
  2.5 km (iron), 5.6 km (copper, the longest) and 2.6 km (stone, heavy so it needs a truck).
- **Wheat.** A 2600 kg field cut four times an hour is about $416 an hour to the granary, about $476 an hour
  if the grain is milled for the bakery.
- **Shops need a way to spend.** Prices are pack data. A cheap plot costs $2800, a car $700, a permit
  $250.

Mining on Everon is a design with numbers, not built places. Three chains share the shape mine, then a plant
somewhere else, then a product. Iron: mine 2.3 km from town, coal as the reagent, smelter 2.5 km on. Copper:
mine 4.1 km out, acid bought in town, refinery 5.6 km on. Stone: quarry far out, sand from a beach, cement works at
a harbour 2.6 km on. For a generator the lesson is the spacing: put a mine, its plant and its reagent source
2 to 6 km apart on roads, not side by side.

## Naming and determinism

Saves must survive you regenerating the map. What the game saves and how it keys it:

- A place registered in `LF_Places` is saved by its **kind and id**, never by entity id or position. Make the id
  stable across regenerations: derive it from the thing it is for (`<settlement>.<slot>` such as
  `saint-philippe.bakery`, or `<settlement>.home.<building>`), not from a random number or a counter that shifts
  when one building is added. Everon's ids are in `docs/everon/places/*/`.
- **Entity names matter in two places.** A job site entity is named and that name is its id
  (`LF_JobSiteComponent`). A marker with no `m_sId` falls back to the entity name. A regenerated map must give
  the same entity the same name.
- **Position keys.** A farm plot or beehive with no authored key is keyed by its position rounded to 0.1 m
  (`hive:<x>:<z>`). A regenerated map that moves it by more than that loses the saved crop or comb. Give
  these an authored key.
- **Houses and plots** are keyed by their house identifier or plot key (`docs/persistence.md`, property rows).
  Hideouts are keyed by their place id.
- **Per seed.** The same seed should give the same ids for the same things. If a generator cannot hold that, it
  can ship an alias file: rows of `{kind: place, from: <old id>, to: <new id>}`, listed in the mission header's
  config overlays, which every load runs old saves through (`LF_SaveAliasesConfig`,
  `Scripts/Game/Persistence/LF_SaveIds.c`, `docs/persistence.md` "Stable ids and renamed content").
- A character body, a vehicle and an item are saved by their own ids and prefabs. Those do not depend on the
  map.

## What a generator should not do

- **Write a vector as a quoted string.** `coords 1 2 3`, `m_vHalfExtents 5 3 7` are bare. A quoted list loads as
  text, logs `Unexpected data at offset` and puts the entity at the world origin. `tools/validation/validate-quoted-vectors.mjs`
  fails it.
- **Leave props off the terrain.** No floating or sunk props, nothing in the road, nothing blocking a door or the
  walk-up to one, no scale outside 0.5 to 2, nothing tilted more than 5 degrees. The placement audit measures
  every one of these (`docs/placement-audit.md`, `docs/placement-tool.md`). `tools\cli audit placement --strict`
  must exit clean for a layer to ship.
- **Type coordinates into script.** Core script has no world coordinates (`core-vs-pack.md` "Map independence").
  If your map needs a place, it is a marker. If a feature needs a new kind of place, ask for a kind; do not
  edit a script to name an entity.
- **Reuse or hand-edit resource GUIDs.** GUIDs are unique and immutable. Do not copy a `.meta` file.
- **Reference a pack from the core.** The core never names a pack file, class or path
  (`tools/validation/validate-addon-boundary.mjs`).
- **Put names and flavour text in core.** Place names, sign titles and shop names go in the pack's string table,
  not in a layout or a core string.
- **Offer a building with no interior for a slot that needs one.** The tool skips a ruin, a building with no
  interior, one with no ground-level door where a ground-floor shop is asked for, and one under the slot's
  minimum area (`tools/town/map.mjs`). A generated building that carries a door entity and an interior gets
  used; a bare shell does not.
- **Overlap a settlement's dressing.** Props inside a fitted building must stand inside its footprint (plus the
  slot's margin, 2 m for most) (`tools/validation/validate-island-zones.mjs`).

## What we will add

- **An importer.** Issue [#3839](https://github.com/Life-Framework/Life-Framework/issues/3839) is a spike on
  reading a generated map's output and producing the markers and mapping for you. It is not built yet.
- **An optional marker layer.** A ready-made layer of `LF_PlaceComponent` markers a generator can place by
  rule, so you write one marker file instead of a pack. Until it exists, a pack is the route.

If anything here is wrong or missing for your map, open an issue on the repository.
