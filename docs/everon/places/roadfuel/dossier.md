# xz-5304-9844: location dossier

Generated 2026-11-19 by `tools\cli scout`. Do not edit by hand; run `tools\cli scout 5304,9843.6` to refresh. The machine-readable twin is `dossier.json`.

- **Place:** xz-5304-9844, from coordinates
- **Centre:** x 5304, z 9843.6, ground 21.3 m; **radius** 120 m
- **World:** Worlds/CoreOnEveron/CoreOnEveron.ent, Arma Reforger build 26266485 (installed 2026-11-08)
- **Other labels nearby:** Étoupe (urban, 139 m NE); farm (other, 324 m NW); power plant (urban, 501 m E); GALLOW'S HILL (natural, 684 m NW)

## At a glance

18 buildings (2 homes, 2 civic, 0 ruins), 6 enterable (a door entity below them), 16 entrances from outside among 34 door entities; 2 road(s), 1 junction(s); 7 notable props; 1159 trees, bushes and other generated vegetation; 8 free plot(s).

| Type | Count |
| --- | --- |
| shed | 10 |
| fuel | 2 |
| farm-small | 2 |
| house | 2 |
| farm | 2 |

## Overview

![from above, north up](img/overview-top.jpg)

![wide, from the south](img/overview-wide.jpg)

![from the west](img/overview-ob270.jpg)

![from the north](img/overview-ob000.jpg)

![from the east](img/overview-ob090.jpg)

## Terrain

- ground 17.2 to 26.1 m (8.9 m of relief) within 120 m; mean 21.4 m
- 57% of the ground is under 3 degrees, 40% 3 to 8, 0% over 15
- overall the ground falls 1 degrees toward the SE

## Roads and junctions

**Main street:** N-S road r01, 3 buildings within 25 m of it; its busiest stretch is at x 5324.7, z 9857.2.

![N-S road r01 (N-S), looking along it](img/main-street-wide.jpg)

![from the east](img/main-street-ob090.jpg)

| Id | Road | Length in radius (m) | Points |
| --- | --- | --- | --- |
| r01 | N-S road r01 | 348.5 | 3 |
| r03 | E-W road r03 | 92.5 | 2 |

| x | z | Kind | Arms | Roads |
| --- | --- | --- | --- | --- |
| 5347.6 | 9944.6 | t-junction | 3 | r03, r01 |

## Civic candidates

Public-facing buildings a town plan would use (church, chapel, government, police, fire, pub, shop, fuel station, hotel, recreation), nearest first. Each links its pictures.

| Id | Type | Vanilla prefab | Distance | Footprint | Floors | Interior | Entrances |
| --- | --- | --- | --- | --- | --- | --- | --- |
| b01 | fuel | FuelStation_E_01_building | 0 m N | 10.9 x 5.5 m (60 m2) | 2 | yes | right/S |
| b02 | fuel | FuelStation_E_01_building_small | 13.8 m S | 4.8 x 3.7 m (17.8 m2) | 2 | yes | left/NE |

### b01 fuel: FuelStation_E_01_building

`{00FD2B4A0BDEC271}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building.et` at x 5304, z 9843.6, facing E; footprint 10.9 x 5.5 m; interior yes (catalog: 2 door entities and a walkable floor (2 floors)).

![fuel, from the front](img/civic-b01-wide.jpg)

![fuel, from the east](img/civic-b01-ob090.jpg)

![fuel, inside (looking 000 deg)](img/interior-b01-in000.jpg)

![fuel, inside (looking 090 deg)](img/interior-b01-in090.jpg)

![fuel, inside (looking 270 deg)](img/interior-b01-in270.jpg)

![fuel, inside (wide)](img/interior-b01-wide.jpg)

### b02 fuel: FuelStation_E_01_building_small

`{627FE49D4DC922AE}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building_small.et` at x 5299.2, z 9830.7, facing E; footprint 4.8 x 3.7 m; interior yes (catalog: 1 door entity and a walkable floor (2 floors)).

![fuel, from the front](img/civic-b02-wide.jpg)

![fuel, from the east](img/civic-b02-ob090.jpg)

![fuel, inside (looking 000 deg)](img/interior-b02-in000.jpg)

![fuel, inside (looking 090 deg)](img/interior-b02-in090.jpg)

![fuel, inside (looking 270 deg)](img/interior-b02-in270.jpg)

![fuel, inside (wide)](img/interior-b02-wide.jpg)

## All buildings

| Id | Type | Vanilla prefab | x | z | Yaw | Facing | Footprint (m) | Floors | Interior | Entrances | Furniture |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| b01 | fuel | FuelStation_E_01_building | 5304 | 9843.6 | 106 | E | 10.9 x 5.5 (60 m2) | 2 | yes | 1 | - |
| b02 | fuel | FuelStation_E_01_building_small | 5299.2 | 9830.7 | 107 | E | 4.8 x 3.7 (17.8 m2) | 2 | yes | 1 | - |
| b03 | farm-small | Henhouse_01 | 5274.9 | 9856.3 | -174 | S | 3.3 x 1.2 (4 m2) | - | no | - | - |
| b04 | house | House_Village_E_1I06 | 5272.5 | 9829.2 | -163 | S | 18.3 x 14 (256.2 m2) | 2 | yes | 1 | - |
| b05 | shed | Door_HouseAddon_Shed_E_L_EXT_COV_01 | 5342.3 | 9808.4 | 103 | E | 1.9 x 0.1 (0.2 m2) | - | no | ? | - |
| b06 | shed | Door_HouseAddon_Shed_E_R_EXT_COV_01 | 5341.5 | 9805.1 | -77 | W | 1.7 x 0.1 (0.2 m2) | - | no | ? | - |
| b07 | shed | HouseAddon_Shed_E_01 | 5345.9 | 9806 | 103 | E | 4.9 x 8.5 (41.7 m2) | 0 | unknown | 3 | - |
| b08 | shed | Door_HouseAddon_Shed_E_R_EXT_COV_02 | 5349.6 | 9804.4 | -77 | W | 1.6 x 0.1 (0.2 m2) | - | no | ? | - |
| b09 | shed | Shed_02_small | 5364.2 | 9833.9 | -88 | W | 3.5 x 2.6 (9.1 m2) | 1 | yes | 1 | - |
| b10 | shed | Shed_01 | 5245 | 9824.7 | 16 | N | 3.6 x 2.7 (9.7 m2) | 1 | yes | 1 | - |
| b11 | house | House_Village_E_1I02_Furniture | 5343.6 | 9793.2 | -167 | S | 11 x 20.5 (225.5 m2) | 2 | yes | ? | - |
| b12 | shed | Door_HouseAddon_Shed_E_R_EXT_COV_02 | 5239 | 9852 | -76 | W | 1.6 x 0.1 (0.2 m2) | - | no | ? | - |
| b13 | shed | Door_HouseAddon_Shed_E_R_EXT_COV_01 | 5238.7 | 9850.7 | 104 | E | 1.7 x 0.1 (0.2 m2) | - | no | ? | - |
| b14 | shed | Door_HouseAddon_Shed_E_L_EXT_COV_01 | 5237.9 | 9847.4 | -76 | W | 1.9 x 0.1 (0.2 m2) | - | no | ? | - |
| b15 | farm | Barn_E_02 | 5234.8 | 9851.9 | -76 | W | 9.5 x 8.5 (80.8 m2) | 0 | unknown | 4 | - |
| b16 | shed | Door_HouseAddon_Shed_E_R_EXT_COV_02 | 5230.7 | 9851.5 | 104 | E | 1.6 x 0.1 (0.2 m2) | - | no | ? | - |
| b17 | farm-small | Henhouse_01 | 5345.1 | 9780.5 | 102 | E | 3.3 x 1.2 (4 m2) | - | no | - | - |
| b18 | farm | Barn_E_01 | 5361.3 | 9900 | 107 | E | 15.1 x 10.8 (163.1 m2) | 0 | unknown | 4 | - |

Footprint: `catalog` and `measure` are the model's own extents turned by the yaw; `world-bounds` is the engine's axis-aligned box. Yaw is Enfusion's (0 faces +z north, 90 faces +x east). Interior `yes` means door entities and a walkable floor (a heuristic, not walked); `unknown` means door entities but no walkable floor. Entrances are the doors on the outline; `?` means the building was not found in the engine dump. Furniture is the count of furnishing props inside the footprint where the place file has it.

## Notable props

| Kind | Vanilla prefab | x | z | Distance |
| --- | --- | --- | --- | --- |
| phone-booth | PhoneBooth_E_01 | 5305.2 | 9843 | 1.3 m |
| notice-board | MessageBoardSmall_01_pole_filled_V6 | 5303.8 | 9840.3 | 3.3 m |
| bench | BenchStreet_01_yellow_red | 5270.5 | 9838.5 | 33.9 m |
| bench | BenchStreet_01_yellow_red | 5255.3 | 9834.1 | 49.6 m |
| bench | BenchWooden_02_Worn | 5337.4 | 9792.9 | 60.8 m |
| bench | BenchStreet_01_red | 5349.3 | 9796 | 65.7 m |

Counted, not listed: 1 street-lamp.

## Free plots

| Id | x | z | Size (m) | Area (m2) | Ground (m) | Nearest road | Nearest building | From centre |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| p1 | 5276 | 9902.6 | 96 x 70 | 6720 | 19.1 to 26 | E-W road r03 58.9 m | b03 farm-small 45.7 m | 65.3 m |
| p2 | 5340 | 9757.6 | 52 x 36 | 1872 | 18.9 to 20.3 | N-S road r01 41.2 m | b17 farm-small 21.1 m | 93.2 m |
| p3 | 5389 | 9887.6 | 38 x 36 | 1368 | 20.5 to 21.9 | N-S road r01 53.1 m | b18 farm 24.7 m | 95.7 m |
| p4 | 5213 | 9885.6 | 26 x 40 | 1040 | 23.9 to 26.1 | E-W road r03 94.2 m | b15 farm 33.8 m | 100.2 m |
| p5 | 5395 | 9799.6 | 26 x 36 | 936 | 20.9 to 22.3 | N-S road r01 83.6 m | b11 house 43.8 m | 101.1 m |
| p6 | 5370 | 9919.6 | 40 x 16 | 640 | 19.9 to 20.5 | N-S road r01 25.9 m | b18 farm 13.7 m | 100.7 m |
| p7 | 5274 | 9947.6 | 32 x 16 | 512 | 22 to 24.8 | E-W road r03 15.9 m | b03 farm-small 90.6 m | 108.2 m |
| p8 | 5374 | 9768.6 | 12 x 38 | 456 | 19.8 to 20.9 | N-S road r01 72.2 m | b17 farm-small 30.3 m | 102.6 m |

plots are open rectangles at least 10 m on the short side, kept 2.5 m off buildings, 4.5 m off a road's centre line, 2 m off a tree, on ground under 12 degrees; axis aligned, not the best use of the land

## Interiors

![fuel, inside (looking 000 deg)](img/interior-b01-in000.jpg)

![fuel, inside (looking 090 deg)](img/interior-b01-in090.jpg)

![fuel, inside (looking 270 deg)](img/interior-b01-in270.jpg)

![fuel, inside (wide)](img/interior-b01-wide.jpg)

![fuel, inside (looking 000 deg)](img/interior-b02-in000.jpg)

![fuel, inside (looking 090 deg)](img/interior-b02-in090.jpg)

![fuel, inside (looking 270 deg)](img/interior-b02-in270.jpg)

![fuel, inside (wide)](img/interior-b02-wide.jpg)

## Limits of this dossier

- buildings (types, footprints, floors, interiors) are read from the Everon catalog, docs/everon/catalog-buildings.json; 10 of 18 matched an entity of the engine dump, which adds the door positions, and the other 8 have no entrances listed

## Prefab references

Every distinct building prefab, as the world file refers to it.

| Prefab | Type | Count |
| --- | --- | --- |
| `{00FD2B4A0BDEC271}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building.et` | fuel | 1 |
| `{627FE49D4DC922AE}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building_small.et` | fuel | 1 |
| `{E005876522A8CE1D}Prefabs/Structures/Agriculture/Henhouse/Henhouse_01/Henhouse_01.et` | farm-small | 2 |
| `{AD57E850A575A54F}Prefabs/Structures/Houses/Village/House_Village_E_1I06/House_Village_E_1I06.et` | house | 1 |
| `{9D6C69116A1DE3B0}Prefabs/Structures/BuildingParts/Doors/Door_HouseAddon_Shed_E_L_EXT_COV_01.et` | shed | 2 |
| `{270728E69E8B2D7E}Prefabs/Structures/BuildingParts/Doors/Door_HouseAddon_Shed_E_R_EXT_COV_01.et` | shed | 2 |
| `{325991D9993FA95B}Prefabs/Structures/Houses/Village/HouseAddon_Shed_E_01/HouseAddon_Shed_E_01.et` | shed | 1 |
| `{4B531C06224FA250}Prefabs/Structures/BuildingParts/Doors/Door_HouseAddon_Shed_E_R_EXT_COV_02.et` | shed | 3 |
| `{8590602F727399F4}Prefabs/Structures/Houses/Shed/Shed_02/Shed_02_small.et` | shed | 1 |
| `{F08D8E78433D713A}Prefabs/Structures/Houses/Shed/Shed_01/Shed_01.et` | shed | 1 |
| `{1F0A3A2A077ED92F}Prefabs/Structures/Houses/Village/House_Village_E_1I02/Furniture/House_Village_E_1I02_Furniture.et` | house | 1 |
| `{F5A2DC0597B53E19}Prefabs/Structures/Agriculture/Barn/Barn_E_02/Barn_E_02.et` | farm | 1 |
| `{99F6E8E52B71B137}Prefabs/Structures/Agriculture/Barn/Barn_E_01/Barn_E_01.et` | farm | 1 |
