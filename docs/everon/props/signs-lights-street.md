# Signs, lights and street furniture (vanilla Everon)

Group `signs-lights-street`, generated 2026-11-16 from the installed game (build 26266485) and the vanilla Everon world dump. Data: `signs-lights-street.json` (80 items). Research only; nothing here changes the game.

How to read the tables
- Paths are under `Prefabs/`; the full `{GUID}path` is in the JSON.
- Size is metres, width x depth x height, from `tools\cli measure` (model bounds, rounded).
- Count is the vanilla Everon world total (unique by path and rounded x,z for placed structures; props from the dump's counts, which can repeat across tile edges). It covers the colour/length variants named in the row; `_pole`, glass and bar child prefabs are excluded.
- Towns: SPh Saint Philippe, SPi Saint Pierre, Mor Morton, Mon Montignac, any prop found within 450 m of that town centre.
- Act = a user action found in the prefab or its parents or children (read with game_read). Light = a light entity or lamp component found the same way. Dest = has a destruction component. All three are read from prefab text, not tested in play.

## Town and direction signs

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Village name sign (per-town prefabs, _end = leaving side) | `Structures/Signs/TownNames/SignVillage_Eden/SignVillage_02_Montignac.et` | 1.13 x 0.05 x 3.12 | 106 | SPh SPi Mor Mon | - | - |
| Direction signpost Eden set (numbered boards) | `Structures/Signs/Directions/SignDirection_Eden_01/SignDirection_Eden_01_1.et` | 1.02 x 0.09 x 3.13 | 76 | SPh SPi Mor Mon | - | - |
| Saint Philippe village name sign | `Structures/Signs/TownNames/SignVillage_Eden/SignVillage_02_SaintPhilippe.et` | 1.13 x 0.05 x 3.12 | 6 | SPh | - | - |
| Saint Pierre village name sign | `Structures/Signs/TownNames/SignVillage_Eden/SignVillage_02_SaintPierre.et` | 1.13 x 0.05 x 3.12 | 6 | SPi | - | - |

## Road, warning and tourist signs

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Stone verge post | `Structures/Signs/Traffic/VergePost/StoneVergePost_01.et` | 0.29 x 0.29 x 1.34 | 2402 | SPh SPi Mor Mon | - | - |
| Stop sign | `Structures/Signs/Warnings/SignStop_01.et` | 0.7 x 0.09 x 0.7 | 31 | SPh SPi Mon | - | - |
| Give way sign | `Structures/Signs/Warnings/SignGiveWay_01.et` | 0.9 x 0.09 x 0.79 | 98 | SPh SPi Mor Mon | - | - |
| Priority road sign | `Structures/Signs/Warnings/SignPriorityRoad_01.et` | 0.68 x 0.09 x 0.68 | 109 | SPh SPi Mor Mon | - | - |
| Pedestrian crossing sign | `Structures/Signs/Warnings/SignPedCrossing_01.et` | 0.5 x 0.09 x 0.5 | 45 | SPh SPi Mor Mon | - | - |
| Parking sign | `Structures/Signs/Warnings/SignParking_01.et` | 0.5 x 0.09 x 0.7 | 32 | SPh SPi Mor Mon | - | - |
| Bend ahead sign (left/right/double variants) | `Structures/Signs/Warnings/SignBendLeft_01.et` | 0.9 x 0.09 x 0.79 | 157 | SPh SPi Mon | - | - |
| Fuel station sign | `Structures/Signs/Warnings/SignFuelStation_01.et` | 0.5 x 0.09 x 0.7 | 8 | SPh SPi | - | - |
| Bus stop sign | `Structures/Signs/Traffic/SignBusStop_01.et` | 0.3 x 0.61 x 2.9 | 39 | SPh SPi Mor Mon | - | - |
| No smoking sign | `Structures/Signs/Generic/SignGeneric_01_NoSmoking.et` | 0.7 x 0.08 x 0.5 | 21 | SPh SPi Mon | - | - |
| Tourist arrow board (Left/Right, blue/red/green/yellow) | `Structures/Signs/Tourist/TouristSign_01/TouristSign_01_Right_blue.et` | 0.42 x 0.08 x 0.14 | 81 | SPh SPi Mor Mon | - | - |
| Trail marker plate (rectangle/arrow/curve, 4 colours) | `Structures/Signs/Tourist/TrackSign_01/TrackSign_01_Flat_rectangle_blue.et` | 0.26 x 0 x 0.18 | 1272 | SPh SPi Mor Mon | - | - |

## Shop and building signs

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Pub sign (V1-V3, FoodMarket variant) | `Props/Commercial/SignPub_01_V1.et` | 0.69 x 0.59 x 1.01 | 60 | SPh SPi Mor Mon | - | - |
| Shop wall sign (Grocery, CarService, GasStation, Drugstore, HouseholdWares, ClothesStore) | `Structures/BuildingParts/Signs/Commercial/SignCommercial_01_GroceryStore.et` | 4.27 x 0.05 x 0.89 | 37 | SPh SPi Mor Mon | - | - |
| Wall plate sign (PostOffice, MunicipalOffice, ShippingCompany) | `Structures/BuildingParts/Signs/SignWall/SignWall_01_90x45_PostOffice.et` | 0.9 x 0.04 x 0.45 | 17 | SPh SPi Mor Mon | - | - |
| Rooftop sign (office) | `Structures/BuildingParts/Signs/SignRooftop/SignRooftop_Office_E_01.et` | 7.44 x 1.12 x 2.89 | 6 | SPh SPi | - | - |

## Posters and boards

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Poster column | `Structures/Commercial/PosterColumn_01.et` | 1.24 x 1.21 x 2.76 | 6 | SPh SPi | - | - |
| Message board small (filled variants exist) | `Structures/Civilian/MessageBoard/MessageBoardSmall_01/MessageBoardSmall_01.et` | 1.2 x 0.14 x 0.95 | 128 | SPh SPi Mor Mon | - | - |
| Map board | `Structures/Civilian/BoardMap_01/BoardMap_01.et` | 2.6 x 0.66 x 3.32 | 20 | SPh SPi Mor Mon | - | - |
| Cork notice board | `Props/Civilian/NoticeBoardCork_01.et` | 1.8 x 0.02 x 1.1 | 227 | SPh SPi Mor Mon | - | - |
| A4 notice sheet (V1-V9, vertical/horizontal) | `Props/Commercial/Posters/Notice_A4_Vertical_V1.et` | 0.21 x 0 x 0.3 | 263 | SPh SPi Mor Mon | - | - |
| Single poster (RadioEveron, WashYourHands, FootballMatch, Cans, Library...) | `Props/Commercial/Posters/PosterSingle_27x41_01_RadioEveron.et` | 0.35 x 0.01 x 0.53 | 218 | SPh SPi Mor Mon | - | - |

## Street and other lamps

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Street lamp E_01 4m/6m (_off = unlit) | `Structures/Infrastructure/Lamps/LampStreet_E_01/LampStreet_E_01_4m.et` | 0.7 x 0.7 x 4.25 | 254 | SPh SPi Mor Mon | - | yes |
| Street lamp E_02 (single/addon/double) | `Structures/Infrastructure/Lamps/LampStreet_E_02/LampStreet_E_02_single.et` | 0.68 x 2.33 x 8.75 | 248 | SPh SPi Mor Mon | - | yes |
| Street lamp E_03 (single/addon) | `Structures/Infrastructure/Lamps/LampStreet_E_03/LampStreet_E_03_single.et` | 0.72 x 3.59 x 11.25 | 163 | SPh SPi Mor Mon | - | yes |
| Interior lamp 01 | `Props/Furniture/LampInterior_01.et` | 0.51 x 0.5 x 1.53 | 327 | SPh SPi Mor Mon | - | - |
| Interior lamp 02 (colour shades) | `Props/Furniture/LampInterior_02/LampInterior_02_Yellow.et` | 0.76 x 0.5 x 1.84 | 130 | SPh SPi Mor Mon | - | - |
| Industrial lamp | `Structures/BuildingParts/Lights/LampIndustrial_01.et` | 0.58 x 0.56 x 0.45 | 81 | SPh SPi Mor Mon | - | - |
| Kerosene lamp | `Props/Civilian/LampKerosene_01/LampKerosene_01_green.et` | 0.2 x 0.17 x 0.4 | 60 | SPh Mor Mon | yes | yes |
| Fluorescent lamp exterior (lit variant) | `Props/Furniture/LampFluorescent/LampFluorescent_01_exterior.et` | 1.32 x 0.21 x 0.08 | 22 | SPh SPi Mor | - | yes |
| Industrial lamp lit exterior variant | `Structures/BuildingParts/Lights/LampIndustrial_01_on_exterior.et` | 0.58 x 0.56 x 0.45 | 0 | - | - | yes |

## Wall lights

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Wall light 01 | `Props/Furniture/LightWall_01.et` | 0.29 x 0.32 x 0.29 | 2064 | SPh SPi Mor Mon | - | - |
| Industrial light (wall/outdoor) | `Structures/BuildingParts/Lights/LightIndustrial_01.et` | 0.38 x 0.16 x 0.34 | 142 | SPh SPi Mor Mon | - | - |
| Industrial light lit exterior variant | `Structures/BuildingParts/Lights/LightIndustrial_01_on_exterior.et` | 0.38 x 0.16 x 0.34 | 0 | - | - | yes |

## Ceiling lights

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Ceiling light 01 | `Props/Furniture/LightCeiling_01.et` | 0.83 x 0.86 x 0.82 | 921 | SPh SPi Mor Mon | - | - |
| Ceiling light 03 | `Props/Furniture/LightCeiling_03.et` | 0.39 x 0.41 x 1.02 | 782 | SPh SPi Mor Mon | - | - |
| Fluorescent lamp 01 | `Props/Furniture/LampFluorescent/LampFluorescent_01.et` | 1.32 x 0.21 x 0.08 | 651 | SPh SPi Mor Mon | - | - |
| Ceiling light 01 lit variant (emits light) | `Props/Furniture/LightCeiling_01_on.et` | 0.83 x 0.86 x 0.82 | 0 | - | - | yes |

## Benches and seating

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Street bench 02 | `Props/Civilian/BenchStreet/BenchStreet_02.et` | 2.19 x 0.7 x 1.03 | 206 | SPh SPi Mor Mon | - | - |
| Street bench 01 (green/red/yellow_red) | `Props/Civilian/BenchStreet/StreetBench_01/BenchStreet_01_green.et` | 2 x 0.81 x 1.01 | 406 | SPh SPi Mor Mon | - | - |
| Wooden bench 01 | `Props/Furniture/BenchWooden_01.et` | 1.45 x 0.39 x 0.52 | 250 | SPh SPi Mor Mon | - | - |
| Wooden bench 03 | `Props/Furniture/BenchWooden_03.et` | 1.52 x 0.43 x 0.5 | 312 | SPh SPi Mor Mon | - | - |
| Church pew (short/long) | `Props/Furniture/BenchChurch_02/BenchChurch_02_long.et` | 4.81 x 0.7 x 1.06 | 54 | SPh Mor Mon | - | - |
| Office/waiting bench | `Props/Furniture/BenchOffice_01/BenchOffice_01_Base.et` | 1.36 x 0.61 x 0.96 | 156 | SPh SPi Mor Mon | - | - |
| Picnic table with benches | `Props/Civilian/PicnicTable/PicnicTable_01.et` | 1.99 x 2.38 x 0.97 | 295 | SPh SPi Mor Mon | - | - |

## Bins

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Street bin | `Props/Garbage/Bins/BinStreet_01.et` | 0.4 x 0.34 x 1.58 | 165 | SPh SPi Mor Mon | - | - |
| Trash bin 02 | `Props/Garbage/Bins/TrashBin_02.et` | 0.64 x 0.67 x 1.09 | 411 | SPh SPi Mor Mon | - | - |
| Garbage container (with lid) | `Props/Garbage/Bins/GarbageContainer_01.et` | 1.6 x 1.1 x 1.49 | 412 | SPh SPi Mor Mon | - | - |
| Street ashtray post | `Props/Civilian/AshtrayStreet_01.et` | 0.3 x 0.3 x 0.92 | 90 | SPh SPi Mor Mon | - | - |

## Bus shelter

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Bus shelter (blue/green) | `Structures/Infrastructure/Bus/BusShed_E_01/BusShed_E_01_blue.et` | 3.95 x 2.32 x 3.68 | 26 | SPh SPi Mor Mon | - | - |

## Booths and kiosks

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Phone booth (lit variant _Light) | `Structures/Infrastructure/Landlines/PhoneBooth_E/PhoneBooth_E_01.et` | 1.72 x 1.62 x 3.37 | 24 | SPh SPi Mor Mon | yes | yes |
| Wall payphone | `Props/Services/PhoneWall_01/PhoneWall_01_grey.et` | 0.33 x 0.19 x 0.58 | 24 | SPh SPi Mor Mon | - | - |
| Newspaper kiosk (redyellow/redwhite) | `Structures/Commercial/Booth/NewspaperBooth_E_01/NewspaperBooth_E_01_redyellow.et` | 4.13 x 5 x 4.13 | 17 | SPh SPi Mor Mon | yes | - |

## Fences

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Wooden pole fence 01 (3m/6m) | `Structures/Walls/Pole/PoleFence_01/PoleFence_01_6m_A.et` | 6.27 x 0.28 x 2.67 | 10055 | SPh SPi Mor Mon | - | - |
| Wire net fence 02 (2/4/8m) | `Structures/Walls/Net/NetFence_02/NetFence_02_8m.et` | 8 x 0.08 x 4.01 | 2059 | SPh SPi Mor Mon | - | - |
| Wooden fence 01 (2/4/8m) | `Structures/Walls/Wooden/WoodenFence_01/WoodenFence_01_8m.et` | 8.04 x 0.17 x 1.57 | 1214 | SPh SPi Mor Mon | - | - |

## Barriers, cones, bollards

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Guardrail (4m/8m, V1/V2) | `Structures/Walls/Crashbarrier/CrashBarrier_01/CrashBarrier_01_8m_V1.et` | 8 x 0.34 x 2.33 | 1119 | SPh SPi Mor Mon | - | - |
| Bar gate (boom barrier) | `Structures/Infrastructure/Barriers/BarGate_01/BarGate_01.et` | 8.74 x 0.47 x 4.59 | 28 | SPh Mon | yes | - |
| Traffic cone (red; stack variant) | `Props/Infrastructure/ConeTraffic/ConeTraffic_01_red.et` | 0.45 x 0.45 x 0.55 | 41 | SPh SPi Mon | - | - |
| Road block | `Props/Infrastructure/Roadblocks/RoadBlock_01.et` | 2.21 x 0.52 x 1.17 | 30 | SPh Mor Mon | - | - |
| Sea bollard (harbour) | `Structures/Infrastructure/Naval/SeaBollards/SeaBollard_01.et` | 0.6 x 0.45 x 0.52 | 418 | SPh SPi Mor | - | - |

## Fountains and statues

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Fountain | `Structures/Cultural/Fountains/Fountain_01.et` | 7.1 x 7.1 x 3.23 | 3 | Mor Mon | - | - |
| Plague column monument | `Structures/Cultural/Statues/PlagueColumn_01.et` | 3.63 x 3.63 x 12.93 | 5 | SPh SPi Mor Mon | - | - |
| Soldier statue | `Structures/Cultural/Statues/StatueSoldier_02/StatueSoldier_02.et` | 1.44 x 1.43 x 5.19 | 4 | SPh SPi | - | - |

## Other street furniture (pavement, planters, flags, hydrants, mailbox, poles)

| Item | Path | Size m | Count | Towns | Act | Light |
|---|---|---|---|---|---|---|
| Small power pole (scenery) | `Structures/Infrastructure/Power/Powerlines/PowerLine_01/PowerLine_01_pole_small_hook.et` | 0.53 x 0.27 x 9.72 | 1304 | SPh SPi Mor Mon | - | - |
| Mailbox | `Props/Services/Post/Mailbox_02.et` | 0.49 x 0.37 x 2.01 | 19 | - | - | - |
| Street fire hydrant | `Structures/Infrastructure/Waterworks/HydrantStreet_01/HydrantStreet_01.et` | 0.4 x 0.23 x 1.3 | 24 | SPh SPi Mor | - | - |
| Newspaper stand | `Props/Commercial/NewspaperStand_01.et` | 0.5 x 0.4 x 2.02 | 46 | SPh SPi Mor Mon | - | - |
| Market stall (umbrella variant) | `Props/Commercial/MarketStand_01.et` | 2.53 x 1.29 x 1.03 | 112 | SPh SPi Mor Mon | - | - |
| Concrete kerb 2m | `Structures/Infrastructure/Pavements/ConcreteKerb_01/ConcreteKerb_01_2m_v2.et` | 2.02 x 0.15 x 0.31 | 4453 | SPh SPi Mor Mon | - | - |
| Asphalt pavement strip (narrow/wide 4m) | `Structures/Infrastructure/Pavements/AsphaltPavement_01/AsphaltPavement_01_Narrow_4m_A.et` | 4.84 x 2.25 x 0.46 | 3267 | SPh SPi Mor Mon | - | - |
| Wall flag holder | `Structures/BuildingParts/Misc/FlagHolder_01.et` | 0.21 x 0.12 x 0.28 | 54 | SPh SPi Mor Mon | - | - |
| Flag pole (no flag) | `Structures/Military/Flags/FlagPole_01/FlagPole_01_empty.et` | 0.13 x 0.28 x 6.81 | 13 | SPh | - | - |
| Street planter box (rect/octa, flowers by colour) | `Props/Decorations/Flowerpots/FlowerpotStreet_01/FlowerpotStreet_01_rect_long_begonia_red.et` | 1.49 x 0.66 x 0.81 | 262 | SPh SPi Mor | - | - |
| Pelargonia flowerpot (colours) | `Props/Decorations/Flowerpots/FlowerpotPelargonia_01_red.et` | 1.37 x 0.52 x 0.51 | 416 | SPh SPi Mor Mon | - | - |
| Flag 1:2 (Everon; also Blue, Green, Red, RedCross, SovietArmy, USArmy, USSR, US, White) and 2:3 sets | `Props/Fabric/Flags/Flag_1_2_Everon_01.et` | 0.03 x 2.01 x 1.01 | 0 | - | - | - |

## Findings that change planning

- Vanilla signs are baked art. None of these prefabs takes text; village sign boards exist only for the villages listed in the village-sign row. Shop fronts use fixed names (Grocery, Pub, CarService, GasStation, Drugstore, HouseholdWares, ClothesStore, PostOffice, MunicipalOffice). Custom text goes through Life Framework's own `LF_Sign_*` prefabs and `LF_SignComponent` (`Feature/Signs/Feature.md`).
- Vanilla benches, pews and bus shelter carry no sit action. Life Framework has invisible sit places for `BenchStreet_01_green`, `BenchStreet_01_red` and `BenchStreet_02` only (`Prefabs/Props/Seats/LF_Seat_*.et`, F key); other benches need a new LF_Seat prefab.
- Furniture lights (`LightWall_01`, `LightCeiling_01/03`, `LampFluorescent_01`, `LampInterior_*`, `LampIndustrial_01`, `LightIndustrial_01`) are meshes with no light entity. The lit variants are `LightCeiling_01_on` (used in the LF drug store), `LampFluorescent_01_exterior`, `LampIndustrial_01_on_exterior` and `LightIndustrial_01_on_exterior`; those last three exist in the pak but are not placed in the vanilla world.
- Street lamps (`LampStreet_E_01/02/03`) light through a child glass prefab with a `StreetLampComponent` (script API: `IsLightOn`, `SetBroken`). They have no user action. Unlit `_off` variants exist for E_01 and E_03.
- The only item with a player light switch is the kerosene lantern (`SCR_SwitchLightUserAction`). Doors with actions: phone booth, newspaper kiosk, bar gate.
- Flags are placeable (`Prefabs/Props/Fabric/Flags/Flag_*`, none placed in vanilla Everon) on `FlagPole_01_empty` or `FlagHolder_01`.
- Not found as vanilla placeable prefabs: a street bollard (only the harbour `SeaBollard_01`), a billboard, a planter other than the flowerpot family (asset_search for planter finds nothing).

## Dressing a street (verified facts)

1. Start with `ConcreteKerb_01_2m_v2` and `AsphaltPavement_01_Narrow_4m_A` for the pavement edge; neither is destructible, and they are the most placed pavement pieces on the map (4453 and 3267).
2. Lamps: `LampStreet_E_01_4m` is the most placed street lamp (254); put one every few metres and they light themselves through their glass child, with no switch.
3. Add benches (`BenchStreet_02`, `BenchStreet_01_green/red`) with a `LF_Seat_*` anchor to make them sittable, a `BinStreet_01`, `TrashBin_02` or `GarbageContainer_01`, and `HydrantStreet_01`.
4. Road signs: `SignStop_01`, `SignGiveWay_01`, `SignPedCrossing_01`, `SignParking_01`, `SignBusStop_01` each need their `_pole` prefab next to them (except the bus stop sign); add the village name board at each entrance.
5. Colour: `FlowerpotStreet_01_*` planter boxes and `FlowerpotPelargonia_01_*` pots are the vanilla street planters (most used in Saint Philippe and Saint Pierre); `BusShed_E_01_blue/green` and `PhoneBooth_E_01` add landmarks.

## Dressing a shop front (verified facts)

1. Pick the baked wall sign that matches the shop (`SignCommercial_01_*`, `SignWall_01_*`, or the `SignPub_01_V*` hanging sign); the name cannot be changed.
2. Lighting: a facade fixture such as `LightIndustrial_01_on_exterior` or `LampFluorescent_01_exterior` actually emits; the plain `LightWall_01` does not.
3. Street frontage: `MarketStand_01`, `NewspaperStand_01`, `FlowerpotPelargonia_01_*` and a `BenchStreet_*` with a seat anchor.
4. Notices in the window or on the wall: `Notice_A4_*` and `PosterSingle_*` are cheap baked paper; for readable text use the LF notice (`LF_Sign_Notice`) instead.
5. Door-side kiosk: `NewspaperBooth_E_01_*` has a door action (SCR_DoorUserAction) and is a kiosk front you can open.

## Gaps

- `hasAction`, light and destruction flags come from reading prefab text (own file, parents, and children to depth 2), not from running the game. Whether `StreetLampComponent` switches on by time of day is assumed from its name and API, not tested.
- Destruction flags show a destruction component exists; a base with `Enabled 0` (for example BenchStreet_02) may still not break in play.
- World counts for props rely on the dump's C rows and may double count at tile edges.
- No Workbench here, so sizes are model bounds only and nothing was looked at.
