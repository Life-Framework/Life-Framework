# Vanilla props: vehicles, fuel, garage, dock, industrial, farm

Factual catalogue for the Everon fit-out, generated 2026-11-16 from the installed game paks and the vanilla Everon world dump. Data file: `docs/everon/props/vehicles-dock-industrial.json` (99 entries; GUIDs and `{GUID}path` refs live there). No game change.

How to read: **Size** is w x d x h in metres from the model bounds. **Everon** is the number of placements on the vanilla map (variant family summed, structures deduplicated by position); `n/a` means the world dump has no vehicle rows. **Act** is whether a user action exists in the prefab chain (yes with the action named in the JSON). Drivable vehicles are marked D, destructible X, inventory storage S in the flags column.

## Summary: how to dress a port, a garage and a farm

- **Port.** The map has 2570 unique naval structure rows; the biggest harbour clusters are Morton Bay (5350,3668; 451 rows), Lamentin (972,6121; 406), Charlet Bay beside Saint Philippe (4222,10943; 405), Saint Pierre (10081,1464; 216) and Meaux (4263,9512; 129). Base it on `PierConcrete_01_20m` (343), `Pier_Wood_Long_01` (634), `SeaBollard_01` (418), `BreakwaterStone_01_stack_wall` (198), a `BoatShed_01` and `HarborCrane_01` (ladder action only, no loading animation), then stack `CargoContainer_01_*` / `CargoContainer_02_20ft_*` on `Pallet_01`, `WoodenPalletBox_01`, `MarsBox_01_*` with `PalletJack_01`, and add `FishingNet_0*`, `NetHanger_01_*`, `LobsterTrapWooden_01`, `LifeBuoy_01`, `Buoy_01_*`. `BoatWreck_01` is the only small hull; vanilla has no drivable boat and no trailer.
- **Garage.** Shell = `Workshop_01_Garage` (+ Main/Side) or `Garage_E_02`; inside: `Workbench_01/02`, `Workbench_Vice_01`, `ToolBox_0*`, `HydraulicCarJack_01`, `EngineCrane_01`, `GasWelder_01`, `AirCompressor_01`, `RampVehicle_01_metal`, `TirePile_01_*`, `Tire_*`, `VehicleGarbage_01_pile_*`. All of these are inert statics (no user action), so the garage reads as dressing; a parked `S105_*`, `V2101_*` or `T601_*` beside the jack finishes it.
- **Farm.** `Barn_E_01` plus `Silo_01` / `Silo_02_*`, then `HayBale_01_square_big` (1860 on Everon) and `_small`, `HayPile_01`, `HaystackDryer_01`, `Trough_01`, `MilkCan_01`, `WaterTank_01_v*`, `Wheelbarrow_01`, `CultivatorWreck_01`, `PlowWreck_01`. The only tractor is `Z7211_red_CIV` (drivable, has storage); a `UAZ469_*_CIV_*` jeep or `Ural4320_transport_CIV_base` completes the yard.
- **Fuel.** The only working pump is `FuelStation_E_01_tankpistol` (`SCR_FuelSupportStationComponent` + `SCR_FuelNode`, refuels vehicles through the vehicle-side refuel action); `FuelStation_E_01_V1` is the kiosk Life Framework already uses for its till and clerk. The three `FuelTank_*` structures and `BarrelFuel_01_*` are scenery. The world `Jerrycan_01` prop is a static mesh; the carryable `Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et` has pick-up, refuel and drain actions.
- **Compat.** Every drivable vanilla vehicle chain here carries `Persistence`, `SCR_BaseLockComponent` and `RplComponent`, so a raw vanilla car placed as scenery is a real, drivable, persisted vehicle; LF's lot spawns thin locked `EL_*` children of S105, V2101, T601, S1203, UAZ452, UAZ469 and M923A1. Use the `*_wreck_static` props (mesh only) for inert derelicts.

## Harbour and port clusters (naval structure rows, 200 m cells merged)

| Centre x,z | Naval rows | Nearest named place | Harbour marker |
|---|---|---|---|
| 5350, 3668 | 451 | MortonBay (303 m) | 445 m |
| 972, 6121 | 406 | Lamentin (356 m) | 93 m |
| 4222, 10943 | 405 | CharletBay (220 m) | 235 m |
| 10081, 1464 | 216 | StPierre (403 m) | 167 m |
| 4263, 9512 | 129 | Meaux (262 m) | 47 m |
| 6389, 9597 | 119 | Kermovan (71 m) | none within 500 m |
| 11629, 2269 | 102 | Lancre (66 m) | none within 500 m |
| 10615, 11816 | 91 | Erquy (322 m) | 53 m |
| 11485, 10881 | 82 | ErquyIsland (625 m) | none within 500 m |
| 7596, 6685 | 81 | FishermansBay (282 m) | none within 500 m |
| 3702, 8562 | 80 | Gravette (873 m) | none within 500 m |
| 3151, 2187 | 61 | Thollevast (109 m) | none within 500 m |
| 10864, 8699 | 39 | Argent (65 m) | none within 500 m |
| 5969, 9549 | 33 | Kermovan (415 m) | none within 500 m |

23 clusters have at least 8 rows; the full list is in the JSON `harbours` array. Place names come from the `M|` map-location rows (`StPierre`, `MortonBay`, `CharletBay`, `Lamentin`, `Meaux`, `Erquy`, ...).

## Cars and jeeps (drivable)

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Vehicles/Wheeled/S105/S105_beige.et` | 1.89 x 4.44 x 1.36 | n/a | yes | DXS | Skoda S105 car (10 colours). Civilian saloon; colour = sibling prefab (beige, darkblue, darkbrown, darkred, green, lightblue, lightgreen, red, white, yellow). Used on the LF vehicle lot (locked child |
| `Vehicles/Wheeled/V2101/V2101_beige.et` | 1.76 x 4.26 x 1.33 | n/a | yes | DXS | Dana V2101 car (9 colours). 1970s saloon; colours beige, darkBlue, darkRed, green, lightBlue, lightGreen, red, white, yellow. On the LF lot (EL_V2101_beige). |
| `Vehicles/Wheeled/T601/T601_beige.et` | 1.6 x 3.64 x 1.58 | n/a | yes | DXS | Sputnik T601 small car (6 colours + two-tone). Cheap small car; beige, blue, green, grey, orange, white plus _twoTone beige/blue/orange. On the LF lot (EL_T601_beige). |
| `Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et` | 1.94 x 4.09 x 1.77 | n/a | yes | DXS | UAZ469 jeep, covered (civilian). Farm/forestry jeep; blue, brown, forest, red, teal; JZD, PTS, VAK are liveries. On the LF lot (EL_UAZ469_covered_CIV_blue). |
| `Vehicles/Wheeled/UAZ469/UAZ469_uncovered_CIV_blue.et` | 1.94 x 4.09 x 1.77 | n/a | yes | DXS | UAZ469 jeep, open (civilian). Open-top jeep; blue, brown, red, teal. |
| `Vehicles/Wheeled/M151A2/M151A2.et` | 2.03 x 3.69 x 1.59 | n/a | yes | DXS | M151A2 military jeep. Military; M151A2_transport has a bench. Not civilian; surplus-dealer or army yard only. |
| `Props/Wrecks/UAZ469_wreck/UAZ469_wreck_static.et` | 2.27 x 4.09 x 1.63 | 9 | no | - | UAZ469 wreck (static). Static derelict: mesh and rigidbody only, no components. Also _blue_static and _rusty_static. Scrapyard or roadside. |
| `Props/Wrecks/S105_wreck/S105_wreck_static.et` | 2.48 x 4.53 x 1.46 | 30 | no | - | S105 wreck (static). Static derelict car for a scrapyard. |

## Vans (drivable)

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Vehicles/Wheeled/S1203/S1203_transport_beige.et` | 1.95 x 4.46 x 2.51 | n/a | yes | DXS | S1203 minibus (transport). Passenger van with inventory storage; beige, blue, brown, red, white, yellow (_red prefab is only 578 B, check before use). On the lot (EL_S1203_transport_beige). |
| `Vehicles/Wheeled/S1203/S1203_cargo_brown.et` | 1.95 x 4.46 x 2.51 | n/a | yes | DXS | S1203 cargo van. Delivery van body; beige, blue, brown, red, white, yellow. Courier or shop-delivery dressing. No SCR_UniversalInventoryStorageComponent in its chain. |
| `Vehicles/Wheeled/UAZ452/UAZ452_cargo_CIV_blue.et` | 2.29 x 4.53 x 1.89 | n/a | yes | DXS | UAZ452 cargo van (civilian colours). Boxy van; CIV colours blue, grey, orange exist only on the _cargo_CIV_ and _transport_CIV_ branches. The LF lot sells UAZ452_base (no colour choice). |
| `Vehicles/Wheeled/UAZ452/UAZ452_transport_CIV_blue.et` | 2.29 x 4.53 x 1.89 | n/a | yes | DXS | UAZ452 passenger van (civilian colours). Same body as the cargo van, with seats; blue, grey, orange. |
| `Props/Wrecks/S1203_wreck/S1203_wreck_static.et` | 1.83 x 4.59 x 1.8 | 0 | no | - | S1203 wreck (static). Static derelict van. |
| `Props/Wrecks/UAZ452_wreck/UAZ452_wreck_static.et` | 2.27 x 4.57 x 1.87 | 0 | no | - | UAZ452 wreck (static). Static derelict van. |

## Trucks (drivable)

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Vehicles/Wheeled/Ural4320/Ural4320_transport_CIV_base.et` | 2.75 x 7.44 x 2.26 | n/a | yes | DXS | Ural 4320 cargo truck (civilian). Flatbed truck with SCR_UniversalInventoryStorageComponent; _CIV_forest and _randomized exist. Dock, farm and forestry haul. |
| `Vehicles/Wheeled/Ural4320/Ural4320_tanker_CIV_orange.et` | 2.75 x 7.44 x 2.26 | n/a | yes | DXS | Ural 4320 fuel tanker (civilian). Orange or _pohona livery; has SCR_FuelNode plus SCR_ResourceComponent. Park beside a pump or tank farm. |
| `Vehicles/Wheeled/M923A1/M923A1_transport.et` | 3.19 x 8.25 x 2.66 | n/a | yes | DXS | M923A1 cargo truck. US military 5-ton; on the LF lot as EL_M923A1_transport. Only MERDC camo variants, no civilian colours. _covered and _covered_closed exist. |
| `Vehicles/Wheeled/M923A1/M923A1_tanker.et` | 3.19 x 8.25 x 2.66 | n/a | yes | DXS | M923A1 fuel tanker. Military green tanker; SCR_FuelNode present. |
| `Props/Wrecks/Ural4320_wreck/Ural4320_wreck_static.et` | 3.16 x 7.62 x 2.61 | 16 | no | - | Ural 4320 wreck (static). Static derelict truck. |

## Tractor (drivable)

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Vehicles/Wheeled/Z7211/Z7211_red_CIV.et` | 2.2 x 4.11 x 2.43 | n/a | yes | DXS | Zetor 7211 tractor (red). Only tractor in vanilla (Z7211_base plus this red CIV). Drivable, has storage. Not on the LF lot. Pair with hay bales and plough wrecks for a farm yard. |

## Boats and hulls (static only)

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Props/Wrecks/BoatWreck_01/BoatWreck_01.et` | 1.41 x 5.13 x 0.8 | 86 | no | - | Wrecked small boat. Only small-boat mesh in vanilla: no drivable boat prefab exists (Prefabs/Vehicles/Water is empty). Beached or dry-dock hull. |
| `Props/Wrecks/ShipWreckWooden_01/ShipWreckWooden_01.et` | 6.64 x 23.29 x 9.89 | 0 | no | - | Wooden ship wreck. Large hull; variants _sunk and _Underwater_01. Harbour-edge set piece. |

## Trailers (static only)

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Props/Industrial/CompressorTrailerWreck_01/CompressorTrailerWreck_01_Yellow.et` | 2.44 x 7.37 x 2.83 | 0 | no | X | Compressor trailer wreck (yellow). Only trailer-like prop (Gray and Yellow). SCR_DestructibleEntity. Not a hitchable trailer: no vanilla trailer vehicle exists. |

## Fuel

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Structures/Commercial/FuelStations/FuelStation_E_01/FuelStation_E_01_V1.et` | 30.99 x 7.83 x 6.21 | 24 | no | X | Fuel station (full composition). Eden kiosk + canopy + platform. LF Street_Fuel uses this exact prefab for the till and clerk. Count = V1 plus building/building_small/platform/roof parts. Composite: no o |
| `Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_tankpistol.et` | 1.37 x 0.73 x 1.91 | 19 | yes | - | Fuel pump with nozzle. The working pump: SCR_FuelSupportStationComponent + SCR_FuelNode + ActionsManager + SCR_FuelNozzle child. Vehicles refuel via SCR_RefuelAtSupportStationAction (on the veh |
| `Props/Commercial/FuelNozzle_01.et` | 0.08 x 0.35 x 0.58 | 19 | no | - | Fuel nozzle. SCR_FuelNozzle with InventoryItemComponent; child of the pump, can be placed alone as a dressing prop. |
| `Structures/Industrial/Containers/FuelTanks/FuelTank_01/FuelTank_01_grey.et` | 5.16 x 4.64 x 10.2 | 22 | no | X | Fuel tank, horizontal (FuelTank_01). Large bulk tank (grey, green, orange, rust; _Ruin variants exist). Scenery. |
| `Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_Pump_grey.et` | 11.17 x 2.82 x 7.67 | 3 | no | X | Fuel tank with pump housing (FuelTank_02_Pump). Grey, green, yellow, USSR variant. Scenery only: no SCR_FuelSupportStationComponent in its chain, so it does not refuel; pair with the pump prop. |
| `Structures/Industrial/Containers/FuelTanks/FuelTank_03/FuelTank_03_grey.et` | 16 x 17.55 x 12.55 | 8 | no | X | Fuel tank, upright (FuelTank_03). Vertical tank; grey, green, orange. Scenery. |
| `Props/Industrial/BarrelFuel_01_closed.et` | 0.67 x 0.67 x 1.01 | 551 | no | X | Fuel barrel, closed. Steel fuel drum; destructible (SCR_DestructionMultiPhaseComponent, particle on destruction). Count sums Base, closed, closed_old. |
| `Props/Industrial/BarrelMetal_01_rust.et` | 0.76 x 0.76 x 1.1 | 493 | no | X | Metal drum (rust / grey / blue / military). Generic oil drum; rust, grey, blue, blue_open, military. Destructible. Workshop and dock clutter. |
| `Props/Industrial/GasTank_01_blue.et` | 0.34 x 0.34 x 0.64 | 345 | no | X | Gas bottle (GasTank_01 and _02, 4 colours). Propane-style cylinder; blue, red, rusty. Destructible. Garage or kitchen clutter. |
| `Props/Civilian/Jerrycan_01.et` | 0.16 x 0.37 x 0.52 | 574 | no | - | Jerrycan (world prop). Static mesh only: no InventoryItemComponent, so it is not pick-up-able. The carryable item is Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et (used by the LF fuel-can  |
| `Items/Fuel/Jerrycan_01/Jerrycan_01_item.et` | 0.16 x 0.37 x 0.52 | n/a | yes | - | Jerrycan (item). Pickup item used by LF job Roadmap_Job_FuelCan. Not a world-count prop. |
| `Props/Military/Camps/PalletFuel_01.et` | 1.38 x 2.08 x 1.25 | 40 | no | - | Pallet of fuel drums. Military camp prop with particle spawn on destruct; yard stack. |

## Garage and workshop

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Props/Industrial/Workbench_01.et` | 2.6 x 1.13 x 1.2 | 154 | no | - | Workbench 01. Static bench for a garage back wall. |
| `Props/Industrial/Workbench_02.et` | 2.47 x 0.74 x 1 | 127 | no | - | Workbench 02. Variant bench. |
| `Props/Industrial/Workbench_Vice_01.et` | 0.25 x 0.45 x 0.3 | 224 | no | - | Workbench with vice. Bench with vice; the most common workshop prop. |
| `Props/Industrial/ToolBox_01/ToolBox_01_red.et` | 0.55 x 0.24 x 0.36 | 26 | no | X | Toolbox (red / grey / lightblue). ToolBox_01 grey and red, ToolBox_02 lightblue; destructible. Count sums all variants. |
| `Props/Industrial/Repair/HydraulicCarJack_01.et` | 0.34 x 0.51 x 1.19 | 29 | no | - | Hydraulic car jack. Static trolley jack; place beside a car. |
| `Props/Industrial/Repair/EngineCrane_01.et` | 0.81 x 1.7 x 1.49 | 17 | no | - | Engine crane. Static shop crane; no hoist action. |
| `Props/Industrial/Repair/GasWelder_01.et` | 0.81 x 0.51 x 1.48 | 24 | no | - | Gas welder set. Welding trolley with bottles. |
| `Props/Industrial/Repair/VehicleGarbage_01_pile_medium.et` | 1.72 x 2.26 x 0.91 | 137 | no | X | Vehicle-parts junk pile (small / medium / large). Scrap-part piles; destructible. Scrapyard filler. |
| `Props/Industrial/AirCompressor_01/AirCompressor_01.et` | 0.78 x 1.35 x 1.31 | 0 | no | X | Air compressor. Workshop compressor; destructible. |
| `Props/Garbage/Tires/TirePile_01_small.et` | 2.01 x 2.27 x 0.64 | 407 | no | X | Tyre pile (small / medium). Stack of tyres; destructible. Count covers small + medium. |
| `Props/Garbage/Tires/TireTractor_01_single.et` | 1.4 x 1.4 x 0.38 | 198 | no | X | Tractor tyre (single). Big tractor tyre; destructible. Farm yard or dock fender. |
| `Props/VehicleParts/Tires/Tire_UAZ469.et` | 0.85 x 0.84 x 0.22 | 195 | no | - | Loose vehicle tyres (UAZ469, BTR70, M151A2, M998, M939A1, Ural, with _pile). Loose tyres and per-model *_pile stacks; Wheel_* are tyre plus rim. |
| `Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et` | 28.12 x 18.85 x 13.9 | 10 | no | X | Garage building (E_02). Destructible building shell; interior sub-prefab Garage_E_02_furniture_01. |
| `Structures/Industrial/Houses/Workshop_01/Workshop_01_Garage.et` | 8.32 x 5.82 x 6.49 | 81 | no | X | Workshop 01 garage bay. Workshop_01 comes as Main, Side and Garage parts; count sums all three. |
| `Structures/Industrial/Repair/RampVehicle_01/RampVehicle_01_metal.et` | 5.07 x 11.05 x 1.69 | 8 | no | X | Vehicle service ramp (metal). Ramp for working under a car; also _concrete. Has SCR_ResourceComponent. |

## Dock and port

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Structures/Infrastructure/Naval/Piers/Pier_Wood_Long_01.et` | 9.21 x 0.66 x 1 | 634 | no | - | Wooden pier, long. Count covers Pier_Wood Long/Short/Broken and PierWooden_01 (10m, 25m, abandoned, destroyed, poles). Plain GenericEntity (no destruction component in chain). |
| `Structures/Infrastructure/Naval/Piers/PierConcrete_01_20m.et` | 20.01 x 8.25 x 8.12 | 343 | no | X | Concrete quay, 20 m. Modular quay: 20m, 20m_T, wall 5/10/20, corners, flat, slipway, 10m_end. Count sums all PierConcrete_01_* parts. |
| `Structures/Infrastructure/Naval/Piers/FloatingDock_01/FloatingDock_01.et` | 17.64 x 50.05 x 30.66 | 0 | no | X | Floating dock. Pontoon with bridge part. |
| `Structures/Infrastructure/Naval/Piers/Pier_Tires_01.et` | 0.87 x 1.64 x 1.44 | 60 | no | - | Jetty with tyre fenders (01 and 02). Small jetty with tyre bumpers. |
| `Structures/Infrastructure/Naval/SeaBollards/SeaBollard_01.et` | 0.6 x 0.45 x 0.52 | 418 | no | X | Mooring bollard. Most-placed naval structure on Everon; destructible entity. |
| `Structures/Infrastructure/Naval/Cranes/HarborCrane_01.et` | 17.19 x 13.54 x 13.57 | 12 | yes | X | Harbour crane. Quay crane (multicolour and green variants); ActionsManager with a ladder action (SCR_LadderDoorUserAction). Not animated for loading. |
| `Structures/Infrastructure/Naval/Slipway_01/Slipway_01_cradle_01.et` | 4.26 x 5.9 x 2.43 | 13 | no | X | Slipway boat cradle. StaticModelEntity cradle for a boat on rails (Slipway_01 base, rail, winch, cradle_02). |
| `Structures/Infrastructure/Naval/Sheds/BoatShed_01/BoatShed_01.et` | 6.97 x 8.67 x 10.71 | 40 | no | X | Boat shed. Open-water shed (BoatShed_01, _weathered, _land, floor). |
| `Structures/Infrastructure/Naval/Breakwater/BreakwaterStone_01_stack_wall.et` | 39.61 x 21.98 x 9.72 | 198 | no | - | Stone breakwater block. Indestructible environmental entity; wall, wall_half, slope. |
| `Props/Naval/HarborWinch_01.et` | 1.61 x 1.25 x 1.6 | 7 | no | X | Harbour winch. Deck winch; destructible. |
| `Props/Naval/FishingNet_01.et` | 2.54 x 1.7 x 0.46 | 322 | no | - | Fishing net (01 / 02 / 03). Net heaps; three mesh variants. |
| `Props/Naval/NetHanger_01_long.et` | 0.95 x 7 x 1.94 | 34 | no | - | Net-drying rack. Pole rack for hanging nets: long, short, stick_A, stick_B. |
| `Props/Naval/FishingRope_01.et` | 0.96 x 0.49 x 0.09 | 124 | no | - | Rope coil. Coiled fishing rope. |
| `Props/Naval/FishingGear_01.et` | n/a | 19 | no | - | Fishing gear (composite). Composite entity of fishing kit; no own mesh, so no sizeM. |
| `Props/Naval/LobsterTrapWooden_01.et` | 0.93 x 0.48 x 0.38 | 246 | no | - | Lobster trap (01 and 02). Wooden pot. |
| `Props/Naval/Buoy_01_red.et` | 3.98 x 4.22 x 9.68 | 156 | no | X | Buoy (red / green / white / chain / lit). Count covers Buoy_01 variants (lightsource red/green, grounded). Destructible. |
| `Props/Naval/LifeBuoy_01.et` | 0.76 x 0.73 x 0.15 | 191 | no | - | Life ring (single / set / stand). Wall or quay life ring; _set and _stand variants. |
| `Props/Naval/MooringBuoy_01.et` | 20.13 x 20.13 x 28.91 | 25 | no | - | Mooring buoy. Water-surface marker. |

## Containers and pallets

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Props/Industrial/CargoContainers/CargoContainer_01/CargoContainer_01_20ft_green.et` | 2.52 x 6.14 x 2.6 | 105 | no | X | Cargo container 20 ft. Blue, green, grey, olive, red plus _rusty. Destructible. Stack for a port yard. |
| `Props/Industrial/CargoContainers/CargoContainer_01/CargoContainer_01_40ft_blue.et` | 2.52 x 12.27 x 2.6 | 8 | no | X | Cargo container 40 ft. Blue, green, grey, olive, red. |
| `Props/Industrial/CargoContainers/CargoContainer_01/CargoContainer_01_10ft_grey.et` | 2.52 x 3.06 x 2.6 | 70 | no | X | Cargo container 10 ft. Blue, green, grey, olive, red. |
| `Props/Industrial/CargoContainers/CargoContainer_01/CargoContainer_02_20ft_ENP.et` | 2.52 x 6.14 x 2.6 | 144 | no | X | Branded 20 ft container. Liveries ENP, BaltaShipping, Chernarussian, Duvanblade, Emerald, HansaAtlantic, KAL, Starline, SZD; count sums all. |
| `Props/Construction/ContainerMetal_01/ContainerMetal_01_green.et` | 2.08 x 4.04 x 1.82 | 263 | no | - | Site container (metal). Small job-site container: base, blue, green, grey and _low_ variants. |
| `Props/Industrial/Pallets/Pallet_01.et` | 0.81 x 1.23 x 0.14 | 807 | no | - | Wooden pallet. Plain pallet; see Pallet_01_Pile for a stack. |
| `Props/Industrial/Pallets/Pallet_01_Pile.et` | 0.99 x 1.45 x 0.98 | 626 | no | - | Pallet pile. Stack of empty pallets. |
| `Props/Industrial/Pallets/WoodenPalletBox_01.et` | 1.14 x 1.32 x 1.01 | 852 | no | X | Pallet box (wood). Boxed pallet; _Wood variant; destructible. |
| `Props/Industrial/Pallets/MarsBox_01_blue.et` | 0.91 x 1.35 x 0.66 | 497 | no | - | Mars box (plastic crate on pallet). Colours blue, green, white, wood; MarsBoxContainer variants hold stacks. |
| `Props/Construction/PalletJack_01.et` | 0.56 x 1.4 x 1.28 | 43 | no | - | Pallet jack. Hand pallet truck. |

## Crates, boxes, sacks

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Props/Crates/Crate_01_green.et` | 0.61 x 0.41 x 0.23 | 4351 | no | - | Plastic crate (green / red / brown / yellow / orange / ivory / white). Everon's most common crate; also a PrefabLibrary/Props/Shop copy (not counted here). |
| `Props/Crates/CrateWooden_01_plain.et` | 0.88 x 1.45 x 0.83 | 1180 | no | - | Wooden crate (01). Plain and blue variants. |
| `Props/Crates/CrateWooden_02/CrateWooden_02_1x1x1.et` | 1 x 1.01 x 1.01 | 105 | no | X | Wooden crate (02, sized). 1x1x1, 1x1x2, 1x15x3 sizes; destructible. |
| `Props/Crates/BoxWooden_01.et` | 0.5 x 0.37 x 0.28 | 1995 | no | - | Wooden box. Small wooden box; top crate count on Everon. |
| `Props/Crates/CardboardBoxStack_01.et` | 0.68 x 0.83 x 0.83 | 1139 | no | - | Cardboard box stack. Stacked boxes; CardboardBox_01..09 for single boxes. |
| `Props/Civilian/SackUniversal_01.et` | 0.79 x 0.54 x 1.36 | 1075 | no | - | Sack (and sack pile). Grain or cement sack; _Pile stack. |
| `Props/Civilian/SackTruck_01.et` | 0.61 x 0.52 x 1.22 | 390 | no | - | Sack truck (hand trolley). Hand truck. |

## Farm and silos

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Props/Agriculture/CultivatorWreck_01.et` | 2.64 x 2.09 x 1.62 | 93 | no | - | Cultivator wreck. Rusting tractor implement (also _Damaged). Farm-yard filler. |
| `Props/Agriculture/PlowWreck_01.et` | 1.24 x 2.18 x 1.7 | 68 | no | - | Plough wreck. Rusting plough. Farm-yard filler. |
| `Props/Agriculture/Farm/HayBale_01_square_big.et` | 1.74 x 3.16 x 1.76 | 1860 | no | X | Hay bale, large square. Count includes the _bottom variant. Destructible. The most-used farm prop. |
| `Props/Agriculture/Farm/HayBale_01_square_small.et` | 0.85 x 1.06 x 0.65 | 167 | no | X | Hay bale, small square. Destructible. |
| `Props/Agriculture/Farm/HayPile_01.et` | 2.69 x 3.44 x 0.81 | 472 | no | - | Hay pile. Loose hay heap; _green variant. |
| `Props/Agriculture/Farm/HaystackDryer_01.et` | 1.68 x 1.77 x 2.57 | 501 | no | X | Haystack dryer rack. Drying rack (also _folded); count includes HaystackSmall_01 and Haystack_01. |
| `Props/Agriculture/Farm/Trough_01.et` | 0.71 x 3 x 0.59 | 25 | no | - | Animal trough. Static trough. |
| `Props/Agriculture/MilkCan_01.et` | 0.37 x 0.36 x 0.6 | 458 | no | - | Milk can. Churn. |
| `Props/Agriculture/WaterTank_01_v1.et` | 1.43 x 4.08 x 2.04 | 102 | no | - | Farm water tank (v1 / v2). Static tank. |
| `Props/Civilian/Wheelbarrow_01.et` | 0.87 x 2.16 x 0.67 | 317 | no | - | Wheelbarrow. Static. |
| `Structures/Industrial/Containers/Silos/Silo_01/Silo_01.et` | 2.49 x 3.65 x 9.05 | 11 | no | X | Silo 01. Tall grain silo; destructible building entity. |
| `Structures/Industrial/Containers/Silos/Silo_02/Silo_02_white.et` | 3.47 x 4.38 x 16.05 | 6 | yes | X | Silo 02 (white / red-yellow / blue-red). Has an ActionsManager context; destructible. |
| `Structures/Agriculture/Barn/Barn_E_01/Barn_E_01.et` | 15.12 x 10.77 x 16.01 | 164 | no | X | Barn E_01. Barn_E_01/02/03 families; count sums all barn parts. |

## Other

| Prefab | Size w x d x h (m) | Everon | Act | Flags | Note |
|---|---|---|---|---|---|
| `Structures/Industrial/Houses/Warehouse_01/Warehouse_01.et` | 41.26 x 19.23 x 11.06 | 28 | no | X | Warehouse 01. Industrial warehouse shell (V3 and others). |

Paths are relative to the game's `Prefabs/` folder with real casing. Gaps: two prefabs have no resolvable GUID in any scanned source (`S1203_wreck_static`, `FloatingDock_01`); vehicle world counts are unknown (see JSON caveats).
