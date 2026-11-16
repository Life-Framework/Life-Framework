# Saint Philippe: building survey

Generated 2026-11-16 from the in-engine dump of vanilla Everon ([catalog-buildings.md](catalog-buildings.md) says how). Data: [saint-philippe.json](saint-philippe.json) (one row per building: prefab, x, z, yaw, interior flag, props inside).

**Centre**: the map descriptor `#AR-MapLocation_StPhillipe` at x 4503, z 10772 (the town centre; arrival spawn marker of the CoreOnEveron walk proof is at 4471, 10535). Everything below is within 450 m of it. Other map places nearby: Harbour site, Meaux (village) 1.3 km south at 4520, 9468.

## What is here

| type | count |
|---|---:|
| police | 1 |
| government | 2 |
| fire station | 2 |
| church | 2 |
| chapel | 4 |
| pub | 2 |
| shop | 5 |
| kiosk | 2 |
| fuel station | 2 |
| town house | 10 |
| villa | 3 |
| house | 50 |
| garage | 52 |
| industrial | 36 |
| barn / farm | 19 |
| tank / silo | 6 |
| military | 6 |
| hen house / greenhouse | 11 |
| shed | 51 |
| ruin | 40 |

Sheds and hen houses smaller than 6 m² footprint were not measured and are left out of these counts (the 400 m counts in the planning notes include them: about 100 sheds). Pier and quay pieces (Pier_01, PierConcrete, breakwater and wooden piers): 210 placed pieces, mostly west of the centre.

## Civic and commercial buildings (all of them)

| type | prefab | `{GUID}path` | x | z | yaw | dist m | footprint m | floors | furniture inside |
|---|---|---|---:|---:|---:|---:|---|---:|---:|
| police | Villa_E_2I01_Police | `{151CFDB2968ADB77}Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01_Police.et` | 4552.6 | 10748.3 | 171 | 55 | 15.3×12.7 | 5 | 218 |
| government | MunicipalOffice_E_01_school | `{B514DD5B90A27E78}Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01_school.et` | 4592.3 | 10818.2 | -100 | 101 | 24.1×17.8 | 3 | 297 |
| government | MunicipalOffice_E_01 | `{363A616E6C64BB7F}Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01.et` | 4418.9 | 10647.5 | -8 | 150 | 24.1×17.8 | 3 | 217 |
| fire station | FireStation_E_01 | `{E8C14F26268A4D6D}Prefabs/Structures/Services/Fire/FireStation_E_01/FireStation_E_01.et` | 4572.5 | 10673.6 | 79 | 120 | 15.9×15.7 | 4 | 89 |
| fire station | FireStation_E_01_hose_hanger | `{575A6852D1E5CAA6}Prefabs/Structures/Services/Fire/FireStation_E_01/FireStation_E_01_hose_hanger.et` | 4576.3 | 10676.2 | 79 | 121 | 0.6×2 | 0 | 0 |
| church | Church_01_red | `{D645C54FF0E4D55E}Prefabs/Structures/Cultural/Churches/Church_01/Church_01_red.et` | 4448.4 | 10733.6 | -98 | 67 | 20×33.8 | 2 | 66 |
| church | Church_01_crypt_attic | `{A683E3049A6872A3}Prefabs/Structures/Cultural/Churches/Church_01/Church_01_crypt_attic.et` | 4448.4 | 10733.6 | -98 | 67 | 16.5×28 | 1 | 61 |
| chapel | Chapel_01_pink | `{6C4146212E67FFD4}Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_pink.et` | 4510.1 | 10910.4 | 77 | 139 | 4.7×4.7 | 1 | 0 |
| chapel | Chapel_01_base | `{0E9DBC9B70C345F3}Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_base.et` | 4314.3 | 10710 | -81 | 199 | 4.7×4.7 | 1 | 0 |
| chapel | Chapel_01_pink | `{6C4146212E67FFD4}Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_pink.et` | 4707 | 10740 | -93 | 207 | 4.7×4.7 | 1 | 0 |
| chapel | Chapel_01_yellow | `{FC4C6642B6BF5564}Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_yellow.et` | 4307.9 | 10446.8 | 115 | 379 | 4.7×4.7 | 1 | 0 |
| pub | PubVillage_E_2L01_V8_ProgresBar | `{965AFAD159CB0C8A}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V8_ProgresBar.et` | 4455.4 | 10700.3 | 172 | 86 | 17.9×17 | 3 | 263 |
| pub | PubVillage_E_2L01_V1 | `{1B68F3EE279C97A1}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V1.et` | 4468.5 | 10902.3 | 78 | 135 | 17.9×17 | 3 | 295 |
| shop | ShopHouse_E_2I01t_householdwares | `{F51B9B9BD15BBE64}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_householdwares.et` | 4515.3 | 10746.1 | 171 | 29 | 12.4×16.8 | 3 | 121 |
| shop | ShopHouse_E_2I01t_drugstore | `{FF48B5059EB0AC65}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_drugstore.et` | 4552.9 | 10774.1 | 80 | 50 | 12.4×16.8 | 3 | 110 |
| shop | ShopHouse_E_2I01t_clothesstore | `{92E8F78BCF17D93D}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_clothesstore.et` | 4441.4 | 10788.8 | 153 | 64 | 12.4×16.8 | 3 | 92 |
| shop | ShopHouse_E_2I01t_drugstore | `{FF48B5059EB0AC65}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_drugstore.et` | 4431.4 | 10841.5 | 76 | 100 | 12.4×16.8 | 3 | 110 |
| shop | ShopModern_E_01 | `{CFE8511B2B7E8AAA}Prefabs/Structures/Commercial/Shops/ShopModern_E_01/ShopModern_E_01.et` | 4373.5 | 10681.5 | 82 | 158 | 24.4×19.8 | 2 | 251 |
| kiosk | NewspaperBooth_E_01_redyellow | `{DA33ED8E94FD68DF}Prefabs/Structures/Commercial/Booth/NewspaperBooth_E_01/NewspaperBooth_E_01_redyellow.et` | 4424.9 | 10753.7 | 100 | 80 | 4.1×5 | 2 | 0 |
| kiosk | NewspaperBooth_E_01_redwhite | `{B0C7B3A0C937328B}Prefabs/Structures/Commercial/Booth/NewspaperBooth_E_01/NewspaperBooth_E_01_redwhite.et` | 4437 | 10691.4 | -5 | 104 | 4.1×5 | 2 | 0 |
| fuel station | FuelStation_E_01_building_small | `{627FE49D4DC922AE}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building_small.et` | 4615.5 | 10669.4 | -100 | 152 | 4.8×3.7 | 2 | 20 |
| fuel station | FuelStation_E_01_building | `{00FD2B4A0BDEC271}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building.et` | 4616.6 | 10658.7 | -100 | 160 | 10.9×5.5 | 2 | 50 |

## Housing (every house with an interior, nearest first)

| type | prefab | x | z | yaw | dist m | footprint m | floors | doors | furniture inside | most common props |
|---|---|---:|---:|---:|---:|---|---:|---:|---:|---|
| town house | House_Town_E_2I01 | 4503.5 | 10744.6 | 171 | 27 | 12.4×16.8 | 3 | 11 | 201 | LightWall_01×7, PlantRoom_02×5, Faucet_01×4, Wardrobe_02×3 |
| town house | House_Town_E_2I02 | 4476 | 10765.4 | -100 | 28 | 18.5×13.4 | 3 | 13 | 257 | BoxWooden_01×8, LightWall_01×8, ShelfDecorative_01_barewood×6, Dresser_01×5 |
| town house | House_Town_E_2I01_orange | 4474.5 | 10780.5 | -100 | 30 | 12.4×16.8 | 3 | 11 | 202 | LightWall_01×7, PlantRoom_02×5, Faucet_01×4, Wardrobe_02×3 |
| town house | House_Town_E_2I01 | 4480.3 | 10750.7 | -100 | 31 | 12.4×16.8 | 3 | 11 | 200 | LightWall_01×7, PlantRoom_02×5, Faucet_01×4, Wardrobe_02×3 |
| town house | House_Town_E_2I02 | 4530.5 | 10746.9 | 171 | 37 | 18.5×13.4 | 3 | 13 | 255 | LightWall_01×8, BoxWooden_01×7, ShelfDecorative_01_barewood×6, Dresser_01×5 |
| house | House_Village_E_1I06 | 4471.3 | 10806.1 | -14 | 47 | 18.3×14 | 2 | 11 | 162 | PaintCan_01×6, SackUniversal_01×4, BenchWooden_01×4, LightWall_01×4 |
| house | House_Village_E_1I04t_red | 4552.6 | 10789.7 | 79 | 53 | 20.1×11.1 | 2 | 9 | 100 | CurtainWindow_01_V2_folded_left_V1×4, CurtainWindow_01_V2_folded_right_V1×4, PaintCan_01×4, CurtainsWindow_01_V1_medium_folded_V1×3 |
| house | House_Village_E_1I05t | 4518.5 | 10715.1 | -8 | 59 | 24.7×11.2 | 2 | 6 | 148 | FileOffice_01_Stack×6, Chair_02_red×5, CurtainUnfolded_Center_01_long_V3×4, LightCeiling_03×3 |
| house | House_Village_E_1I04t | 4548.9 | 10809.2 | 80 | 59 | 20.1×11.1 | 2 | 9 | 99 | CurtainWindow_01_V2_folded_left_V1×4, CurtainWindow_01_V2_folded_right_V1×4, PaintCan_01×4, CurtainsWindow_01_V1_medium_folded_V1×3 |
| town house | House_Town_E_2I01 | 4537.5 | 10715.3 | -8 | 66 | 12.4×16.8 | 3 | 11 | 201 | LightWall_01×7, PlantRoom_02×5, Faucet_01×5, CardboardBoxStack_01×3 |
| house | House_Village_E_1L03t | 4488.3 | 10702.3 | 82 | 71 | 25.9×25.6 | 2 | 11 | 209 | BoxWooden_01×8, Wardrobe_01×6, PaintCan_01×4, CabinetMetal_01_Base×4 |
| town house | House_Town_E_2I02_v2 | 4427.8 | 10781.8 | 152 | 76 | 18.5×13.4 | 3 | 13 | 256 | LightWall_01×8, BoxWooden_01×7, ShelfDecorative_01_barewood×6, CrateWooden_01_plain×5 |
| house | House_Village_E_1I08t | 4583.3 | 10776.1 | 80 | 80 | 18.9×10.2 | 3 | 8 | 108 | Chair_02_green×3, Wardrobe_01×2, ShelfDecorative_01_barewood×2, CurtainsWindow_01_V1_medium_folded_V3×2 |
| house | House_Village_E_1I04t_green | 4580 | 10795.7 | -100 | 81 | 20.1×11.1 | 2 | 9 | 98 | CurtainWindow_01_V2_folded_left_V1×4, CurtainWindow_01_V2_folded_right_V1×4, PaintCan_01×4, CurtainsWindow_01_V1_medium_folded_V1×3 |
| house | House_Village_E_1L02t | 4586.9 | 10757.1 | 80 | 85 | 20.6×25.6 | 2 | 9 | 130 | Rack_01_Long×3, Dresser_01×3, Cardboard_Pile_01×3, Crate_01_green×3 |
| house | House_Village_E_1I02_Furniture | 4535.9 | 10852 | 171 | 86 | 11×20.5 | 2 | 7 | 135 | CurtainWindow_01_V2_folded_right_V1×7, CurtainWindow_01_V2_folded_left_V1×7, CurtainUnfolded_Center_01_long_V1×5, CurtainsWindow_01_V1_big_folded_V1×5 |
| house | House_Village_E_1I04t_red | 4456.8 | 10846.9 | -104 | 88 | 20.1×11.1 | 2 | 9 | 99 | CurtainWindow_01_V2_folded_right_V1×4, CurtainWindow_01_V2_folded_left_V1×4, PaintCan_01×4, CurtainsWindow_01_V1_medium_folded_V1×3 |
| house | House_Village_E_1L02t | 4564.8 | 10709.7 | -99 | 88 | 20.6×25.6 | 2 | 9 | 131 | LightWall_01×3, BoxWooden_01×3, Cardboard_Pile_01×3, Rack_01_Long×3 |
| town house | House_Town_E_2I02_yellow | 4411.6 | 10773.4 | 152 | 91 | 18.5×13.4 | 3 | 13 | 254 | LightWall_01×8, BoxWooden_01×7, ShelfDecorative_01_barewood×6, Dresser_01×5 |
| town house | House_Town_E_2I01 | 4397.5 | 10767.8 | 153 | 106 | 12.4×16.8 | 3 | 11 | 202 | LightWall_01×7, PlantRoom_02×5, Faucet_01×4, Wardrobe_02×3 |
| house | House_Village_E_1L03t | 4451 | 10868.7 | 76 | 110 | 25.9×25.6 | 2 | 11 | 201 | BoxWooden_01×8, Wardrobe_01×6, PaintCan_01×4, CabinetMetal_01_Base×4 |
| house | House_Village_E_1I08t | 4391.2 | 10749.1 | -91 | 114 | 18.9×10.2 | 3 | 8 | 112 | Chair_02_green×3, Shelf_01_long×2, PlantRoom_04×2, Wardrobe_01×2 |
| house | House_Village_E_1I05t | 4429 | 10860.2 | 75 | 115 | 24.7×11.2 | 2 | 6 | 149 | FileOffice_01_Stack×6, Chair_02_red×5, CurtainUnfolded_Center_01_long_V3×4, LightCeiling_03×3 |
| house | House_Village_E_1I04sr_green | 4617.6 | 10756 | 176 | 116 | 21.1×11.1 | 2 | 9 | 101 | CurtainWindow_01_V2_folded_left_V1×4, CurtainWindow_01_V2_folded_right_V1×4, PaintCan_01×4, CurtainsWindow_01_V1_medium_folded_V1×3 |
| town house | House_Town_E_2I03 | 4444.3 | 10670.6 | -98 | 117 | 20.3×12.6 | 3 | 11 | 250 | CurtainWindow_01_V2_folded_right_V1×9, CurtainWindow_01_V2_folded_left_V1×8, Dresser_01×7, Chair_02_green×5 |
| house | House_Village_E_1L01_Furniture | 4391.5 | 10732.5 | 179 | 118 | 20.2×19.3 | 2 | 7 | 152 | BoxWooden_01×4, ShelfDecorative_01_base×3, CurtainUnfolded_Center_01_long_V2×3, LightCeiling_01×3 |
| house | House_Village_E_1I02_Furniture | 4473.1 | 10657.8 | 170 | 118 | 11×20.5 | 2 | 7 | 141 | CurtainWindow_01_V2_folded_left_V1×7, CurtainWindow_01_V2_folded_right_V1×7, CurtainsWindow_01_V1_big_folded_V1×5, CurtainUnfolded_Center_01_long_V1×5 |
| house | House_Village_E_1I04t | 4566.8 | 10872.7 | -103 | 119 | 20.1×11.1 | 2 | 9 | 99 | CurtainWindow_01_V2_folded_left_V1×4, PaintCan_01×4, CurtainWindow_01_V2_folded_right_V1×4, LightCeiling_03×3 |
| house | House_Village_E_1I04t_red | 4423.5 | 10880.7 | 75 | 135 | 20.1×11.1 | 2 | 9 | 101 | PaintCan_01×4, CurtainWindow_01_V2_folded_right_V1×4, CurtainWindow_01_V2_folded_left_V1×4, PlantRoom_02×3 |
| house | House_Village_E_1I01 | 4368.8 | 10782.3 | 48 | 135 | 9.9×12.5 | 2 | 4 | 62 | CurtainsWindow_01_V2_big_folded_V3×2, CurtainUnfolded_Center_01_long_V3×2, CurtainsWindow_01_small_V2×2, CurtainUnfolded_Center_01_short_V2×2 |
| house | House_Village_E_1I05t | 4562 | 10893.3 | -103 | 135 | 24.7×11.2 | 2 | 6 | 150 | FileOffice_01_Stack×6, Chair_02_red×5, CurtainUnfolded_Center_01_long_V3×4, CurtainWindow_01_V1_folded_left_V3×3 |
| house | House_Village_E_1I04sr_red | 4638 | 10756.3 | -178 | 136 | 21.1×11.1 | 2 | 9 | 102 | PlantRoom_02×4, CurtainWindow_01_V2_folded_right_V1×4, CurtainWindow_01_V2_folded_left_V1×4, PaintCan_01×4 |
| house | House_Village_E_1I02_Furniture | 4642.5 | 10723.7 | -3 | 148 | 11×20.5 | 2 | 7 | 136 | CurtainWindow_01_V2_folded_right_V1×7, CurtainWindow_01_V2_folded_left_V1×7, CurtainUnfolded_Center_01_long_V1×5, CurtainsWindow_01_V1_big_folded_V1×5 |
| house | House_Village_E_1I06 | 4638.3 | 10831.3 | -17 | 148 | 18.3×14 | 2 | 11 | 159 | PaintCan_01×6, LightWall_01×4, BenchWooden_01×4, SackUniversal_01×4 |
| villa | Villa_E_2I01 | 4359.1 | 10718.2 | 170 | 154 | 15.3×12.7 | 5 | 8 | 199 | LightWall_01×5, PlantRoom_01×5, BoxWooden_01×5, CardboardBoxStack_01×5 |
| house | House_Village_E_1L02t | 4658.6 | 10755.9 | 2 | 156 | 20.6×25.6 | 2 | 9 | 134 | BoxWooden_01×4, LightWall_01×3, Cardboard_Pile_01×3, Rack_01_Long×3 |
| house | House_Village_E_1L03t | 4470.2 | 10612.7 | 172 | 163 | 25.9×25.6 | 2 | 11 | 208 | BoxWooden_01×8, Wardrobe_01×6, PaintCan_01×4, CabinetMetal_01_Base×4 |
| house | House_Village_E_1I02_Furniture | 4338.4 | 10756.5 | 49 | 165 | 11×20.5 | 2 | 7 | 136 | CurtainWindow_01_V2_folded_left_V1×7, CurtainWindow_01_V2_folded_right_V1×7, CurtainsWindow_01_V1_big_folded_V1×5, CurtainUnfolded_Center_01_long_V1×5 |
| house | House_Village_E_1I04t_green | 4554.4 | 10930.8 | -103 | 167 | 20.1×11.1 | 2 | 9 | 100 | CurtainWindow_01_V2_folded_left_V1×4, CurtainWindow_01_V2_folded_right_V1×4, PaintCan_01×4, PlantRoom_02×3 |
| villa | Villa_E_2I01 | 4442.2 | 10938.5 | -101 | 177 | 15.3×12.7 | 5 | 8 | 202 | BoxWooden_01×5, CardboardBoxStack_01×5, LightWall_01×5, PlantRoom_01×5 |
| house | House_Village_E_1I05s | 4547.3 | 10951.6 | -114 | 185 | 26.7×11.2 | 2 | 6 | 145 | FileOffice_01_Stack×6, Chair_02_red×5, CurtainUnfolded_Center_01_long_V3×4, Dresser_01×3 |
| house | House_Village_E_1I03_Furniture | 4501.3 | 10957.6 | 159 | 186 | 15.3×25.5 | 2 | 9 | 224 | Dresser_01×7, Shelf_01_long×6, LightWall_01×6, Cupboard_01_F×5 |
| house | House_Village_E_1L03t | 4680.8 | 10711.6 | 176 | 188 | 25.9×25.6 | 2 | 11 | 204 | BoxWooden_01×8, Wardrobe_01×6, PaintCan_01×4, CabinetMetal_01_Base×4 |
| house | House_Village_E_1L03t | 4400.9 | 10931.2 | -104 | 189 | 25.9×25.6 | 2 | 11 | 200 | BoxWooden_01×8, Wardrobe_01×6, CabinetMetal_01_Base×4, PaintCan_01×4 |
| house | House_Village_E_1I03_Furniture | 4328.2 | 10683.6 | 81 | 196 | 15.3×25.5 | 2 | 9 | 227 | Dresser_01×7, LightWall_01×6, Shelf_01_long×6, Cupboard_01_F×5 |
| house | House_Village_E_1L02t | 4399.1 | 10599.3 | 172 | 202 | 20.6×25.6 | 2 | 9 | 138 | LightWall_01×3, BoxWooden_01×3, Cardboard_Pile_01×3, Rack_01_Long×3 |
| villa | Villa_E_2I01 | 4471.1 | 10974.2 | -26 | 205 | 15.3×12.7 | 5 | 8 | 196 | LightWall_01×5, BoxWooden_01×5, CardboardBoxStack_01×5, PlantRoom_01×5 |
| house | House_Village_E_1I04sf_red | 4379.5 | 10595.7 | -13 | 215 | 21.1×11.1 | 2 | 9 | 108 | Shelf_01×4, PaintCan_01×4, CurtainWindow_01_V2_folded_right_V1×4, CurtainWindow_01_V2_folded_left_V1×4 |
| house | House_Village_E_1I03_Furniture | 4622.9 | 10581.3 | 18 | 225 | 15.3×25.5 | 2 | 9 | 233 | Dresser_01×7, Shelf_01_long×6, LightWall_01×6, PlantRoom_01×5 |
| house | House_Village_E_1L02t | 4295.7 | 10676 | 146 | 228 | 20.6×25.6 | 2 | 9 | 136 | Crate_01_green×4, Cardboard_Pile_01×3, Rack_01_Long×3, LightWall_01×3 |
| house | House_Village_E_1I04sr_green | 4359.4 | 10591.8 | -7 | 230 | 21.1×11.1 | 2 | 9 | 105 | PlantRoom_02×4, CurtainWindow_01_V2_folded_right_V1×4, CurtainWindow_01_V2_folded_left_V1×4, PaintCan_01×4 |
| house | House_Village_E_1I02_Furniture | 4552.4 | 11005 | -32 | 238 | 11×20.5 | 2 | 7 | 136 | CurtainWindow_01_V2_folded_right_V1×7, CurtainWindow_01_V2_folded_left_V1×7, CurtainUnfolded_Center_01_long_V1×5, CurtainsWindow_01_V1_big_folded_V1×5 |
| house | House_Village_E_1I04sf_green | 4279.7 | 10664.1 | -40 | 248 | 21.1×11.1 | 2 | 9 | 107 | CurtainWindow_01_V2_folded_right_V1×4, CurtainWindow_01_V2_folded_left_V1×4, Shelf_01×4, PaintCan_01×4 |
| house | House_Village_E_1I05t | 4336.2 | 10588.5 | -7 | 248 | 24.7×11.2 | 2 | 6 | 150 | FileOffice_01_Stack×6, Chair_02_red×5, CurtainUnfolded_Center_01_long_V3×4, Armchair_01×3 |
| house | House_Wooden_E_1I02 | 4748.2 | 10721 | -96 | 250 | 14.8×13.1 | 2 | 14 | 111 | HookMetal_01×4, BenchWooden_01×4, ChairOld_01×4, SackUniversal_01×4 |
| house | House_Wooden_E_1I01 | 4665.9 | 10577.2 | -154 | 254 | 13.5×7.1 | 2 | 3 | 30 | LightWall_01×3, Crate_01_green×3, Faucet_01×2, Chair_01_base×2 |
| house | House_Village_E_1I01 | 4605.6 | 10528.8 | 6 | 264 | 9.9×12.5 | 2 | 4 | 62 | ConcreteStairs_02_Narrow×2, LightCeiling_01×2, CurtainWindow_01_V1_folded_left_V3×2, CurtainWindow_01_V1_folded_right_V3×2 |
| house | House_Prefab_2I01_v2 | 4296.5 | 10586.7 | -5 | 277 | 16.6×13.4 | 3 | 16 | 125 | Crate_01_green×4, LightWall_01×3, Chair_01_blue×3, LightCeiling_02×3 |
| house | House_Prefab_2I01_v1 | 4267.5 | 10586.7 | 0 | 300 | 16.6×13.4 | 3 | 16 | 112 | LightWall_01×4, PaintCan_01×4, Faucet_01×3, LightCeiling_02×3 |
| house | House_Village_E_1I03_Furniture | 4584.9 | 11061.2 | 50 | 301 | 15.3×25.5 | 2 | 9 | 230 | Dresser_01×7, Shelf_01_long×6, LightWall_01×6, PlantRoom_01×5 |
| house | House_Village_E_1I02_Furniture | 4633.8 | 10482.3 | -7 | 318 | 11×20.5 | 2 | 7 | 139 | CurtainWindow_01_V2_folded_right_V1×7, CurtainWindow_01_V2_folded_left_V1×7, CurtainUnfolded_Center_01_long_V1×5, CurtainsWindow_01_V1_big_folded_V1×5 |
| house | House_Village_E_1I02_Furniture | 4205.8 | 10624.9 | 96 | 332 | 11×20.5 | 2 | 7 | 138 | CurtainWindow_01_V2_folded_right_V1×7, CurtainWindow_01_V2_folded_left_V1×7, CurtainUnfolded_Center_01_long_V1×5, CurtainsWindow_01_V1_big_folded_V1×5 |
| house | House_Village_E_1I01 | 4870.2 | 10584.9 | 90 | 412 | 9.9×12.5 | 2 | 4 | 66 | Crate_01_green×3, ConcreteStairs_02_Narrow×2, CurtainWindow_01_V1_folded_right_V3×2, CurtainUnfolded_Center_01_long_V3×2 |

## Garages, industrial, farm and military buildings

| type | prefab | x | z | yaw | dist m | footprint m | interior |
|---|---|---:|---:|---:|---:|---|---|
| garage | Garage_E_01 | 4540.4 | 10762.9 | 80 | 39 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4430.6 | 10827.9 | 77 | 91 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4429.5 | 10832.7 | 77 | 95 | 4.9×9.9 | yes |
| garage | Workshop_01_Main | 4546.2 | 10670 | -11 | 111 | 12.2×7.6 | yes |
| garage | Workshop_01_Side | 4544.2 | 10663.2 | -101 | 116 | 5.4×5.8 | yes |
| garage | Garage_E_01 | 4575 | 10659.8 | -99 | 133 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4461.8 | 10641.4 | 170 | 137 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4457.1 | 10640.6 | 170 | 139 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4452.4 | 10639.8 | 170 | 142 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4447.9 | 10639 | 170 | 144 | 4.9×9.9 | yes |
| garage | Workshop_02 | 4535.4 | 10629.6 | -99 | 146 | 8.9×15.5 | yes |
| garage | Garage_E_01 | 4443.3 | 10638.1 | 170 | 147 | 4.9×9.9 | yes |
| garage | Workshop_01_Main | 4469.7 | 10920.8 | 80 | 152 | 12.2×7.6 | yes |
| garage | Garage_E_01 | 4414.9 | 10898.6 | -14 | 154 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4410.3 | 10897.5 | -14 | 156 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4405.7 | 10896.3 | -14 | 158 | 4.9×9.9 | yes |
| garage | Workshop_01_Main | 4398.9 | 10898.6 | -104 | 164 | 12.2×7.6 | yes |
| garage | Garage_E_01 | 4671.1 | 10759.5 | -179 | 169 | 4.9×9.9 | yes |
| garage | RampVehicle_01_concrete | 4673 | 10768.8 | 92 | 170 | 5.4×12.7 | no |
| garage | Garage_E_01 | 4675.9 | 10759.4 | -179 | 173 | 4.9×9.9 | yes |
| garage | Workshop_01_Side | 4399.3 | 10632.3 | -9 | 174 | 5.4×5.8 | yes |
| garage | HouseAddon_Garage_E_01 | 4496.3 | 10945.8 | 159 | 174 | 7×8.2 | yes |
| garage | HouseAddon_Garage_E_01 | 4333.1 | 10724.2 | 172 | 177 | 7×8.2 | yes |
| garage | Garage_E_01 | 4680.6 | 10759.3 | -179 | 178 | 4.9×9.9 | yes |
| garage | Workshop_01_Main | 4391 | 10629.9 | -9 | 181 | 12.2×7.6 | yes |
| garage | Garage_E_01 | 4323.6 | 10740 | 22 | 182 | 4.9×9.9 | yes |
| garage | Workshop_01_Side | 4690.5 | 10758.7 | 3 | 188 | 5.4×5.8 | yes |
| garage | HouseAddon_Garage_E_01 | 4412.2 | 10602.1 | -7 | 193 | 7×8.2 | yes |
| garage | Garage_E_01 | 4459.7 | 10961.6 | -24 | 194 | 4.9×9.9 | yes |
| garage | Workshop_01_Main | 4696.6 | 10755.1 | 93 | 194 | 12.2×7.6 | yes |
| garage | Garage_E_01 | 4430.9 | 10958.2 | 168 | 200 | 4.9×9.9 | yes |
| garage | Workshop_01_Side | 4390.7 | 10959.2 | -108 | 218 | 5.4×5.8 | yes |
| garage | Workshop_01_Garage | 4400.9 | 10970.3 | -18 | 223 | 8.3×5.8 | yes |
| garage | Workshop_01_Main | 4391.9 | 10966.2 | -18 | 224 | 12.2×7.6 | yes |
| garage | Workshop_01_Garage | 4615 | 10545.6 | -162 | 253 | 8.3×5.8 | yes |
| garage | Workshop_01_Main | 4257.4 | 10693.4 | 144 | 258 | 12.2×7.6 | yes |
| garage | Garage_E_01 | 4448.5 | 11029.6 | -60 | 263 | 4.9×9.9 | yes |
| garage | Garage_E_01 | 4450.9 | 11033.8 | -60 | 267 | 4.9×9.9 | yes |
| garage | Workshop_02_grey | 4243.9 | 10687.7 | 144 | 272 | 8.9×15.5 | yes |
| garage | Workshop_02_grey | 4419.6 | 11053.1 | -150 | 293 | 8.9×15.5 | yes |
| garage | Garage_E_01 | 4557.6 | 11063.2 | -35 | 296 | 4.9×9.9 | yes |
| garage | Workshop_02 | 4538.1 | 10460 | 141 | 314 | 8.9×15.5 | yes |
| garage | Workshop_01_Side | 4572.5 | 10464.8 | -7 | 315 | 5.4×5.8 | yes |
| garage | Workshop_01_Main | 4579.1 | 10462.3 | 83 | 319 | 12.2×7.6 | yes |
| garage | Workshop_01_Side | 4581.1 | 10454 | 83 | 327 | 5.4×5.8 | yes |
| garage | Workshop_01_Garage | 4581.9 | 10447.4 | 83 | 334 | 8.3×5.8 | yes |
| garage | Garage_E_02 | 4655.5 | 11105.6 | 1 | 367 | 28.1×18.9 | yes |
| garage | Workshop_02 | 4635.1 | 11152 | -179 | 402 | 8.9×15.5 | yes |
| garage | RampVehicle_01_metal | 4459.9 | 11174.4 | -91 | 405 | 5.1×11.1 | no |
| garage | Warehouse_01_Workshop | 4654 | 11150.8 | -88 | 408 | 9.7×10.4 | yes |
| garage | Garage_E_02 | 4472.1 | 11194.1 | -93 | 423 | 28.1×18.9 | yes |
| garage | HouseAddon_Garage_E_01 | 4865.9 | 11028.6 | -90 | 444 | 7×8.2 | yes |
| industrial | PortableCabin_E_01_white | 4501.5 | 10623.6 | 87 | 148 | 3.8×6.4 | yes |
| industrial | Office_E_01 | 4368.8 | 10655.9 | 173 | 177 | 17×15 | yes |
| industrial | PortableCabin_E_01_beige | 4384.9 | 10904.3 | -110 | 177 | 3.8×6.4 | yes |
| industrial | ShedMetal_02_long_nowalls | 4560.2 | 10567.9 | 172 | 212 | 31×11.2 | no |
| industrial | ShedMetal_02_short_nowalls | 4326.9 | 10649.5 | 86 | 215 | 16.3×11.2 | no |
| industrial | Warehouse_01 | 4343.7 | 10623.2 | -11 | 218 | 41.3×19.2 | yes |
| industrial | HouseAddon_ShedMetal_01_Long | 4335.8 | 10630.8 | -11 | 219 | 19×4.8 | no |
| industrial | Warehouse_01 | 4523 | 10545.9 | 84 | 227 | 41.3×19.2 | yes |
| industrial | ShedMetal_02_short_nowalls | 4314.6 | 10912.5 | -9 | 235 | 16.3×11.2 | no |
| industrial | PortableCabin_E_01_beige | 4272.6 | 10716.8 | -126 | 237 | 3.8×6.4 | yes |
| industrial | HouseAddon_ShedMetal_01_Short | 4355.1 | 10974.5 | -122 | 251 | 9.6×4.7 | no |
| industrial | Warehouse_01_V3 | 4373 | 10988.5 | -32 | 253 | 41.3×19.2 | yes |
| industrial | PortableCabin_E_01_white | 4293.7 | 10915.7 | 80 | 254 | 3.8×6.4 | yes |
| industrial | ShedMetal_02_long_nowalls | 4403.9 | 11009 | -122 | 257 | 31×11.2 | no |
| industrial | HouseAddon_ShedMetal_01_Long | 4374.4 | 11000.3 | -32 | 262 | 19×4.8 | no |
| industrial | Office_E_01_ENP | 4430.3 | 11024.3 | -60 | 263 | 17×15 | yes |
| industrial | PortableCabin_E_01_beige | 4724.3 | 10923.3 | -127 | 268 | 3.8×6.4 | yes |
| industrial | FactoryHall_E_01 | 4566.4 | 10502.9 | -98 | 276 | 39.3×26.2 | yes |
| industrial | ShedMetal_02_short_nowalls | 4325.6 | 10984.1 | 57 | 277 | 16.3×11.2 | no |
| industrial | Warehouse_01_Office | 4464.6 | 11080.2 | -60 | 311 | 38.3×19.2 | yes |
| industrial | HouseAddon_ShedMetal_01_Long | 4453.5 | 11079.3 | -60 | 311 | 19×4.8 | no |
| industrial | PortableCabin_E_01_beige | 4567.9 | 10464.4 | 175 | 314 | 3.8×6.4 | yes |
| industrial | PortableCabin_E_01_yellow | 4488.2 | 10402 | 27 | 370 | 3.8×6.4 | yes |
| industrial | PortableCabin_E_01_yellow | 4491.7 | 10399.8 | 22 | 372 | 3.8×6.4 | yes |
| industrial | PortableCabin_E_01_yellow | 4471.1 | 11143.9 | 122 | 373 | 3.8×6.4 | yes |
| industrial | PortableCabin_E_02 | 4146.9 | 10599.4 | -178 | 396 | 3.7×2.9 | yes |
| industrial | PortableCabin_E_01_beige | 4597.5 | 11161.1 | 88 | 400 | 3.8×6.4 | yes |
| industrial | PortableCabin_E_01_beige | 4597.4 | 11164.9 | 88 | 404 | 3.8×6.4 | yes |
| industrial | PortableCabin_E_01_beige | 4597.3 | 11168.7 | 88 | 408 | 3.8×6.4 | yes |
| industrial | ShedMetal_02_long_nowalls | 4434.7 | 11182.1 | 87 | 416 | 31×11.2 | no |
| industrial | ShedMetal_02_short_nowalls | 4849.4 | 11007.1 | 3 | 419 | 16.3×11.2 | no |
| industrial | Warehouse_01 | 4666.6 | 11162.4 | 92 | 423 | 41.3×19.2 | yes |
| industrial | HouseAddon_ShedMetal_01_Long | 4675.8 | 11159.2 | 92 | 424 | 19×4.8 | no |
| industrial | HouseAddon_ShedMetal_01_Long | 4657.6 | 11169.2 | -88 | 426 | 19×4.8 | no |
| industrial | ShedMetal_02_short_nowalls | 4433.5 | 11205.2 | 87 | 439 | 16.3×11.2 | no |
| industrial | PortableCabin_E_01_beige | 4596.4 | 11210.7 | 161 | 449 | 3.8×6.4 | yes |
| barn / farm | Barn_E_02 | 4607.7 | 10779.5 | -9 | 105 | 9.5×8.5 | no |
| barn / farm | Barn_E_03_closed | 4522.5 | 10653.2 | 171 | 120 | 17.8×7.3 | yes |
| barn / farm | Barn_E_01 | 4501.3 | 10648.5 | -8 | 124 | 15.1×10.8 | no |
| barn / farm | Barn_E_01 | 4557.4 | 10913 | -103 | 151 | 15.1×10.8 | no |
| barn / farm | Barn_E_01 | 4623.6 | 10884.4 | 60 | 165 | 15.1×10.8 | no |
| barn / farm | Barn_E_02 | 4426.9 | 10609 | 176 | 180 | 9.5×8.5 | no |
| barn / farm | Barn_E_01 | 4446.4 | 10576.1 | -7 | 204 | 15.1×10.8 | no |
| barn / farm | Barn_E_03_open | 4576.2 | 10969.4 | -120 | 211 | 17.8×7.3 | yes |
| barn / farm | Barn_E_02 | 4362.5 | 10936.4 | -86 | 216 | 9.5×8.5 | no |
| barn / farm | Barn_E_01 | 4690.6 | 10657.7 | -5 | 220 | 15.1×10.8 | no |
| barn / farm | Barn_E_03_open | 4385.9 | 10560.2 | 177 | 242 | 17.8×7.3 | yes |
| barn / farm | Barn_E_02 | 4645.4 | 10561.3 | -156 | 254 | 9.5×8.5 | no |
| barn / farm | Barn_E_02 | 4256.9 | 10702.4 | 144 | 256 | 9.5×8.5 | no |
| barn / farm | Barn_E_03_open | 4601.2 | 10499.5 | 80 | 290 | 17.8×7.3 | yes |
| barn / farm | Barn_E_02 | 4173.7 | 10696.6 | -175 | 338 | 9.5×8.5 | no |
| barn / farm | Barn_E_02 | 4618 | 10437.8 | -2 | 353 | 9.5×8.5 | no |
| barn / farm | WeightStation_01 | 4552.4 | 10403.1 | 119 | 372 | 6.6×11 | yes |
| barn / farm | CowShed_E_01_short | 4522 | 10379.3 | 119 | 393 | 19.6×46.7 | yes |
| barn / farm | Barn_E_01 | 4867.2 | 10617.5 | 0 | 396 | 15.1×10.8 | no |
| tank / silo | FuelTank_02_Base | 4616.5 | 10684.8 | 81 | 143 | 9.1×2.8 | no |
| tank / silo | FuelTank_01_grey | 4373.8 | 10638.8 | 20 | 186 | 5.2×4.6 | no |
| tank / silo | FuelTank_02_green | 4630.4 | 11106.3 | -89 | 358 | 9.1×2.8 | no |
| tank / silo | FuelTank_02_Pump_green | 4431.4 | 11127.5 | -2 | 363 | 11.2×2.8 | no |
| tank / silo | Silo_01 | 4527.3 | 10388.3 | 28 | 384 | 2.5×3.7 | no |
| tank / silo | Silo_01 | 4537.6 | 10382.7 | 28 | 391 | 2.5×3.7 | no |
| military | GuardHouse_01 | 4571.8 | 10533.6 | -8 | 248 | 7.6×8.9 | yes |
| military | GuardHouse_01 | 4447.3 | 11045.6 | 30 | 279 | 7.6×8.9 | yes |
| military | GuardHouse_01_beige | 4596.6 | 11144.2 | 180 | 384 | 7.6×8.9 | yes |
| military | GuardTower_01 | 4453.1 | 11164.1 | 175 | 395 | 7.5×8.7 | no |
| military | Barracks_01_military_yellow | 4616.8 | 11191.1 | -178 | 434 | 15.9×30.1 | yes |
| military | Bunker_01_1fp | 4091.7 | 10616.7 | 23 | 440 | 5.5×6.2 | yes |

Sheds (51), hen houses and greenhouses (11) and house ruins (40) are in [saint-philippe.json](saint-philippe.json); they are yard clutter around the houses.

## Layout map

Each character is a 30 m cell, north up, 900 m square centred on the town centre (cell column 15, row 15 is the centre). Where several buildings share a cell the more civic one is shown.

Legend: T town house, H house, S shop, P pub, C church, c chapel, F fuel station, G government, X police, R fire station, K kiosk, B barn / farm, b hen house / greenhouse, I industrial, i tank / silo, g garage, s shed, M military, V villa, r ruin, o cottage, L hotel, A airport, ~ pier/quay piece, . nothing.

```
....~.....~~Ig....I.I.........
....~.....~~Ig....I.I......A..
....~......~.I....IgI.........
....~......~i......ig.........
...~~.....~~.I................
.~~.......~.gM..gHs...s.......
~~.......~~.Ig..rs.........g..
......~~~~II....H.........I...
.......~~rIggVH.sB............
.......~~~BHVbg.H.....I.......
......~~~..IgP.cH.b...........
...........bHHbs.H.B..........
............SHs.H.s...........
........~~..gH.sHG.Hb.........
..........HTS.TbSHBs..........
.....~.~~HsTK.TSXHHHHg........
.~.~.~~~.gVH.C.HT..H.c.H......
.~.~~~~scH..KPH..Hi.H.........
.......HHbS..THBgRF..B........
H....Hs.sIigGgB.g.............
.M.I......HHBH............sBs.
.......HHHH..B..I.H.H......H..
.......s...B...I.MgB..........
.................IHs..........
..............s...BH..........
........c.......gI............
..................B.........I.
..............IiB.............
...............B..............
..............................
```

Cell (col, row) to world: x = 4053 + col × 30, z = 11222 − row × 30.

## Props placed around the town (non-vegetation, non-structure)

The 40 most common prop prefabs inside the 450 m dump, with counts. Interiors are furnished with these in the vanilla world; the full prop catalog is [catalog-props.md](catalog-props.md).

| `{GUID}path` | count |
|---|---:|
| `{2B1D0128ED660BE9}Prefabs/Props/Crates/BoxWooden_01.et` | 363 |
| `{5152F05C17652ADB}Prefabs/Props/Civilian/PaintCan_01.et` | 344 |
| `{34F74A7DBAD46974}Prefabs/Props/Furniture/LightWall_01.et` | 313 |
| `{C76C3AF337109066}Prefabs/Props/Crates/CardboardBoxStack_01.et` | 219 |
| `{AA3D2C0BB0D2B56B}Prefabs/Props/Crates/Crate_01_green.et` | 208 |
| `{D0BD7A55306FC46B}Prefabs/Props/Furniture/Faucet_01/Faucet_01.et` | 205 |
| `{4549BE7A1D414EDA}Prefabs/Props/Furniture/PlantRoom_01.et` | 169 |
| `{F6D1E024E82511F9}Prefabs/Props/Crates/Crate_01_base.et` | 161 |
| `{A319C11D446D88B3}Prefabs/Props/Furniture/Dresser_01.et` | 161 |
| `{CC26426052C1D1E6}Prefabs/Props/Furniture/PlantRoom_02.et` | 159 |
| `{49FF07D895A26DBE}Prefabs/Props/Furniture/LightCeiling_03.et` | 157 |
| `{FD47FF9699A52E82}Prefabs/Props/Furniture/Wardrobe_01.et` | 147 |
| `{97CCD43778D034CF}Prefabs/Props/Furniture/LightCeiling_01.et` | 143 |
| `{BCE07A58672955B9}Prefabs/Props/Civilian/Bucket_01.et` | 141 |
| `{6FD4268EA745B585}Prefabs/Structures/Airport/LightRunway_01/LightRunway_01_light.et` | 137 |
| `{7251C9DFEC27E1AB}Prefabs/Props/Furniture/Shelf_01_long.et` | 132 |
| `{AC98FF880BD0F2C2}Prefabs/Props/Furniture/Chair_01/Chair_01_base.et` | 127 |
| `{B3CF0487BB36323B}Prefabs/Props/Furniture/LampFluorescent/LampFluorescent_01.et` | 126 |
| `{49012FE1DEF653EE}Prefabs/Props/Furniture/Sink_01.et` | 123 |
| `{29BA0E995E24468D}Prefabs/Props/Furniture/PlantRoom_03.et` | 122 |
| `{D7F69BF7D2DCA0A8}Prefabs/Props/Industrial/Pallets/Pallet_01.et` | 120 |
| `{04B7C811ABEA13D5}Prefabs/Props/Furniture/CurtainsWindow_01/CurtainWindow_01_V2_folded_right_V1.et` | 116 |
| `{EFE1F11444E6D34F}Prefabs/Props/Furniture/CurtainsWindow_01/CurtainWindow_01_V2_folded_left_V1.et` | 115 |
| `{7030822F7A54F479}Prefabs/Props/Crates/CrateWooden_01_base.et` | 115 |
| `{C8C36ADC9DFC5FBB}Prefabs/Props/Crates/Crate_01_red.et` | 112 |
| `{2ED930A7BA4156F5}Prefabs/Props/Furniture/MirrorBathroom_01.et` | 109 |
| `{B3283803059E2750}Prefabs/Props/Crates/CrateWooden_01_plain.et` | 109 |
| `{0DC4F830876C6671}Prefabs/Props/Furniture/Chair_02/Chair_02_green.et` | 104 |
| `{208028C4307E3772}Prefabs/Props/Furniture/ShelfDecorative_01_barewood.et` | 102 |
| `{62B2ADC4243454F9}Prefabs/Props/Civilian/Jerrycan_01.et` | 100 |
| `{1112756E8A13445B}Prefabs/Props/Industrial/Pallets/Pallet_01_Pile.et` | 100 |
| `{24DE47095BD6BE7B}Prefabs/Props/Agriculture/Farm/HayBale_01_square_big.et` | 100 |
| `{910424AC5CABE800}Prefabs/Props/Garbage/Bins/BinInterior_01_red.et` | 99 |
| `{F77F7F847CC6455F}Prefabs/Props/Furniture/Bookshelf_01/Bookshelf_01.et` | 97 |
| `{48F1F3A9B178CEA9}Prefabs/Props/Furniture/Bookshelf_01/Parts/Bookshelf_01_glass_02.et` | 97 |
| `{24A5C7490DBC4187}Prefabs/Props/Furniture/Bookshelf_01/Parts/Bookshelf_01_glass_01.et` | 97 |
| `{23EDC2B8CDEE124F}Prefabs/Props/Furniture/Cupboard_01/Cupboard_01_F.et` | 97 |
| `{17F3181D0DAC27FC}Prefabs/Props/Furniture/CabinetMetal_01_grey_V1.et` | 96 |
| `{D21DAFB24F46D64C}Prefabs/Props/Furniture/Nightstand_02.et` | 96 |
| `{A84A73E9D9D4B20E}Prefabs/Props/Furniture/Shelf_01.et` | 95 |
