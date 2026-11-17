# Everon building catalog

Generated 2026-11-16. Machine-readable twin: [catalog-buildings.json](catalog-buildings.json) (all 281 prefabs, 6514 placed instances with coordinates and place index, 64 named places with buildings plus 104 natural places). Props: [catalog-props.md](catalog-props.md). Saint Philippe in full: [saint-philippe.md](saint-philippe.md).

## How this was made

In-engine dump of the vanilla Everon world (Worlds/CoreOnEveron: a SubScene of worlds/Eden/Eden.ent) on a dedicated server, Arma Reforger build 26266485. Every entity with a prefab under Prefabs/Structures, de-duplicated across the 400 m query tiles. Footprint, floors and cellar from `tools\cli measure --surfaces` (collider bounds and horizontal surfaces).

- **Coordinates**: Enfusion world metres: x east, z north, y up; Everon spans about x -107..12348, z -100..12654.
- **Instances** are every placed copy of a prefab; variants (colour, `_V2`) are separate rows.
- **Floors** are counted from the collider's horizontal surfaces (areas of at least 15 % of the footprint, levels merged within 1 m). A cellar shows in the `cellar` column. It is a measurement of the model, not a walk through it.
- **Interior = yes** means the prefab spawns door entities and has at least one walkable floor. Vanilla Everon interiors are furnished: the `furn` column is the average count of Furniture and Decorations props inside the footprint in the four town dumps.
- Method files: [method/](method/) (the in-engine dump script and the node build scripts).

## Summary

| type | prefabs | instances | with interior (instances) |
|---|---:|---:|---:|
| Town houses (2-3 floor row and apartment-style houses) | 7 | 40 | 40 |
| Village, wooden and prefab houses | 26 | 382 | 382 |
| Villas | 1 | 7 | 7 |
| Farm houses | 3 | 37 | 37 |
| Shops and shop-houses | 5 | 18 | 18 |
| Pubs | 8 | 10 | 10 |
| Kiosks and booths | 2 | 17 | 17 |
| Fuel stations | 2 | 10 | 10 |
| Churches | 7 | 12 | 8 |
| Chapels | 3 | 24 | 24 |
| Government, municipal office, school | 3 | 8 | 8 |
| Police (the police-variant villa) | 1 | 4 | 4 |
| Fire stations | 2 | 7 | 3 |
| Hotels | 2 | 2 | 2 |
| Garages and workshops | 11 | 238 | 230 |
| Sheds | 13 | 1088 | 438 |
| Barns, cow sheds, hay lofts, weigh stations | 12 | 185 | 42 |
| Hen houses and greenhouses | 2 | 222 | 110 |
| Industrial: warehouses, factory, offices, sawmill, cabins, metal sheds | 20 | 169 | 116 |
| Fuel tanks and silos | 14 | 74 | 0 |
| Smokestacks and light towers | 3 | 45 | 0 |
| Military: barracks, bunkers, garages, guard boxes, fortifications | 32 | 386 | 86 |
| Airport buildings | 3 | 3 | 2 |
| Cottages and recreation | 6 | 44 | 34 |
| Port: piers, quays, breakwater, bollards | 50 | 1319 | 86 |
| Montfort Castle pieces | 16 | 135 | 0 |
| House ruins | 9 | 1534 | 0 |
| Waterworks (wells, reservoirs, treatment plant) | 18 | 494 | 13 |

## Town houses (2-3 floor row and apartment-style houses)

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| House_Town_E_2I01 | `{38A5F3E4578087AB}Prefabs/Structures/Houses/Town/House_Town_E_2I01/House_Town_E_2I01.et` | 12.4×16.8 | 15.1 | 3 | 5.5 | 11 | yes | 142 | 12 | Saint Philippe 4, Lamentin 3, Morton 1, Montignac 1 |
| House_Town_E_2I02 | `{54F1C704EB440885}Prefabs/Structures/Houses/Town/House_Town_E_2I02/House_Town_E_2I02.et` | 18.5×13.4 | 16.5 | 3 | 5.7 | 13 | yes | 164 | 10 | Saint Pierre 3, Saint Philippe 2, Montignac 2, Lamentin 1 |
| House_Town_E_2I03 | `{E7EC0E80F0645DFE}Prefabs/Structures/Houses/Town/House_Town_E_2I03/House_Town_E_2I03.et` | 20.3×12.6 | 16.4 | 3 | 6 | 11 | yes | 169 | 8 | Lacourt 1, Meaux 1, Saint Philippe 1, Tyrone 1 |
| House_Town_E_2I02_v2 | `{27CC3110521583D6}Prefabs/Structures/Houses/Town/House_Town_E_2I02/House_Town_E_2I02_v2.et` | 18.5×13.4 | 16.5 | 3 | 5.7 | 13 | yes | 163 | 7 | Lamentin 1, Saint Philippe 1, Montignac 1, Morton 1 |
| House_Town_E_2I01_orange | `{C50CF821412A2E0F}Prefabs/Structures/Houses/Town/House_Town_E_2I01/House_Town_E_2I01_orange.et` | 12.4×16.8 | 15.1 | 3 | 5.5 | 11 | yes | 142 | 1 | Saint Philippe 1 |
| House_Town_E_2I02_yellow | `{7B197693AE45574A}Prefabs/Structures/Houses/Town/House_Town_E_2I02/House_Town_E_2I02_yellow.et` | 18.5×13.4 | 16.5 | 3 | 5.7 | 13 | yes | 168 | 1 | Saint Philippe 1 |
| House_Town_E_2I03_v2 | `{F29BECBA5BAFB1B6}Prefabs/Structures/Houses/Town/House_Town_E_2I03/House_Town_E_2I03_v2.et` | 20.3×12.6 | 16.4 | 3 | 6 | 11 | yes | 177 | 1 | Montignac 1 |

## Village, wooden and prefab houses

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| House_Mountain_E_1I01_short | `{678574044F58E1CA}Prefabs/Structures/Houses/Village/House_Mountain_E_1I01/House_Mountain_E_1I01_short.et` | 17.5×11.6 | 8.9 | 2 | 5.9 | 7 | yes | 73 | 40 | (rural) 17, Chotain 3, Villeneuf 2, Entre-Deux 2 |
| House_Village_E_1I02_Furniture | `{1F0A3A2A077ED92F}Prefabs/Structures/Houses/Village/House_Village_E_1I02/Furniture/House_Village_E_1I02_Furniture.et` | 11×20.5 | 11.2 | 2 | 6 | 7 | yes |  | 38 | Saint Philippe 7, (rural) 4, Le Moule 2, Villeneuf 2 |
| House_Wooden_E_1I01 | `{C212C8AC6001F002}Prefabs/Structures/Houses/Wooden/House_Wooden_E_1I01/House_Wooden_E_1I01.et` | 13.5×7.1 | 8.4 | 2 |  | 3 | yes | 6 | 36 | (rural) 13, Saint Philippe 3, Lamentin 2, Meaux 2 |
| House_Village_E_1L03t | `{B2172573ED15AA82}Prefabs/Structures/Houses/Village/House_Village_E_1L03/House_Village_E_1L03t.et` | 25.9×25.6 | 11.4 | 2 | 6.5 | 11 | yes | 104 | 29 | Saint Philippe 5, Montignac 3, (rural) 2, Meaux 2 |
| House_Village_E_1I01 | `{EDBC0E94793BA9F1}Prefabs/Structures/Houses/Village/House_Village_E_1I01/House_Village_E_1I01.et` | 9.9×12.5 | 10.9 | 2 | 4.4 | 4 | yes | 46 | 29 | Saint Pierre 4, Montignac 2, Meaux 2, Saint Philippe 2 |
| House_Mountain_E_1I01 | `{BEF9077AF9F93AE7}Prefabs/Structures/Houses/Village/House_Mountain_E_1I01/House_Mountain_E_1I01.et` | 24.7×13.5 | 8.9 | 2 | 5.9 | 14 | yes | 38 | 26 | (rural) 10, Levie 3, Morton 2, Provins 2 |
| House_Village_E_1I03_Furniture | `{366759CF520BC4CA}Prefabs/Structures/Houses/Village/House_Village_E_1I03/Furniture/House_Village_E_1I03_Furniture.et` | 15.3×25.5 | 10.3 | 2 | 5.9 | 9 | yes |  | 26 | Saint Philippe 4, Montignac 2, Saint Pierre 2, Redon 1 |
| House_Village_E_1I08t | `{D424A3CD07F7A6F0}Prefabs/Structures/Houses/Village/House_Village_E_1I08/House_Village_E_1I08t.et` | 18.9×10.2 | 10.4 | 3 | 7 | 8 | yes | 75 | 21 | Meaux 4, Montignac 2, Saint Philippe 2, Lamentin 1 |
| House_Village_E_1I04t_red | `{1E6FF35236AFF31C}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04t_red.et` | 20.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 72 | 16 | Meaux 4, Saint Philippe 3, Lamentin 1, Gravette 1 |
| House_Village_E_1I05t | `{6ABF4CB2949CB4DE}Prefabs/Structures/Houses/Village/House_Village_E_1I05/House_Village_E_1I05t.et` | 24.7×11.2 | 10.8 | 2 | 8.9 | 6 | yes | 109 | 16 | Saint Philippe 4, Lamentin 2, Meaux 2, Tyrone 2 |
| House_Village_E_1L02t | `{DE43119351D125AC}Prefabs/Structures/Houses/Village/House_Village_E_1L02/House_Village_E_1L02t.et` | 20.6×25.6 | 11.5 | 2 | 5.1 | 9 | yes | 74 | 16 | Saint Philippe 5, Montignac 3, Lamentin 1, Villeneuf 1 |
| House_Wooden_E_1I02 | `{4459413B98E2C9AD}Prefabs/Structures/Houses/Wooden/House_Wooden_E_1I02/House_Wooden_E_1I02.et` | 14.8×13.1 | 7.4 | 2 | 4.6 | 14 | yes | 59 | 13 | (rural) 3, Levie 2, Harbour 1, Saint Philippe 1 |
| House_Village_E_1I06 | `{AD57E850A575A54F}Prefabs/Structures/Houses/Village/House_Village_E_1I06/House_Village_E_1I06.et` | 18.3×14 | 12.5 | 2 | 4 | 11 | yes | 103 | 13 | (rural) 4, Saint Philippe 2, Gravette 1, Meaux 1 |
| House_Village_E_1I04sr_green | `{5D91774D9139FF87}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04sr_green.et` | 21.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 74 | 9 | Saint Philippe 2, Montignac 2, Lamentin 1, Villeneuf 1 |
| House_Village_E_1I04sr | `{751E011171780C2E}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04sr.et` | 21.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 75 | 7 | Lamentin 3, Montignac 1, Figari 1, Chotain 1 |
| House_Village_E_1I05s | `{60333F74EE12B52F}Prefabs/Structures/Houses/Village/House_Village_E_1I05/House_Village_E_1I05s.et` | 26.7×11.2 | 10.8 | 2 | 8.9 | 6 | yes | 110 | 7 | Saint Pierre 2, Villeneuf 1, Saint Philippe 1, Montignac 1 |
| House_Village_E_1L01_Furniture | `{AAF978B0E605529F}Prefabs/Structures/Houses/Village/House_Village_E_1L01/Furniture/House_Village_E_1L01_Furniture.et` | 20.2×19.3 | 11.9 | 2 | 6 | 7 | yes |  | 6 | Lamentin 1, Saint Philippe 1, Tyrone 1, Figari 1 |
| House_Village_E_1I04t | `{2A0D72C46FDCD368}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04t.et` | 20.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 71 | 6 | Saint Philippe 2, Meaux 1, Montignac 1, Morton 1 |
| House_Village_E_1I04sf_red | `{4942F5A04C472230}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04sf_red.et` | 21.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 76 | 5 | Saint Philippe 1, Montignac 1, Provins 1, Durras 1 |
| House_Village_E_1I04sf_green | `{9EC1D8CA0391BF16}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04sf_green.et` | 21.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 79 | 4 | Lamentin 1, Saint Philippe 1, Figari 1, Levie 1 |
| House_Prefab_2I01_v2 | `{34D88C9132A17F8F}Prefabs/Structures/Houses/Prefabricated/House_Prefab_2I01/House_Prefab_2I01_v2.et` | 16.6×13.4 | 11.5 | 3 | 4.8 | 16 | yes | 74 | 4 | Saint Pierre 2, Saint Philippe 1, Montignac 1 |
| House_Village_E_1I04t_green | `{F874C1A05BB6FE2D}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04t_green.et` | 20.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 71 | 4 | Saint Philippe 2, Morton 2 |
| House_Prefab_2I02t | `{ACCE1E281C0C1DAB}Prefabs/Structures/Houses/Prefabricated/House_Prefab_2I02/House_Prefab_2I02t.et` | 10×15.1 | 11.6 | 2 | 4.2 | 11 | yes | 90 | 4 | Saint Pierre 4 |
| House_Prefab_2I01_v1 | `{A43A9D7D878F6EC0}Prefabs/Structures/Houses/Prefabricated/House_Prefab_2I01/House_Prefab_2I01_v1.et` | 16.6×13.4 | 11.5 | 3 | 4.8 | 16 | yes | 73 | 3 | Saint Pierre 2, Saint Philippe 1 |
| House_Village_E_1I04sr_red | `{9A87AD2E3BA2C7FA}Prefabs/Structures/Houses/Village/House_Village_E_1I04/House_Village_E_1I04sr_red.et` | 21.1×11.1 | 11.3 | 2 | 5.1 | 9 | yes | 73 | 3 | Saint Philippe 1, Morton 1, Montignac 1 |
| House_Village_E_1I08t_white | `{6A4927BB36DFC030}Prefabs/Structures/Houses/Village/House_Village_E_1I08/House_Village_E_1I08t_white.et` | 18.9×10.2 | 10.4 | 3 | 7 | 8 | yes | 85 | 1 | Montignac 1 |

## Villas

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Villa_E_2I01 | `{5CADC96916FF1CC4}Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01.et` | 15.3×12.7 | 14.7 | 5 | 4.9 | 8 | yes | 139 | 7 | Saint Philippe 3, Lamentin 1, Morton 1, Power Plant 1 |

## Farm houses

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| FarmHouse_E_1L01 | `{CD370ECB385DE67A}Prefabs/Structures/Houses/Farm/FarmHouse_E_1L01/FarmHouse_E_1L01.et` | 16.2×21.2 | 11.3 | 2 | 4.5 | 7 | yes | 0 | 20 | (rural) 8, Lamentin 2, Le Moule 1, Villeneuf 1 |
| FarmHouse_E_1L01_Wood | `{18B2439FBF6CADFD}Prefabs/Structures/Houses/Farm/FarmHouse_E_1L01/FarmHouse_E_1L01_Wood.et` | 16.2×21.2 | 11.3 | 2 | 4.5 | 7 | yes |  | 9 | (rural) 4, Sawmill 1, Regina 1, Durras 1 |
| FarmHouse_E_1L01_Green | `{87D032F62D9FF82F}Prefabs/Structures/Houses/Farm/FarmHouse_E_1L01/FarmHouse_E_1L01_Green.et` | 16.2×21.2 | 11.3 | 2 | 4.5 | 7 | yes | 0 | 8 | (rural) 2, Gravette 1, Morton 1, Etoupe 1 |

## Shops and shop-houses

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| ShopModern_E_01 | `{CFE8511B2B7E8AAA}Prefabs/Structures/Commercial/Shops/ShopModern_E_01/ShopModern_E_01.et` | 24.4×19.8 | 5.8 | 2 | 6 | 8 | yes | 57 | 7 | Lamentin 1, Meaux 1, Saint Philippe 1, Morton 1 |
| ShopHouse_E_2I01t_drugstore | `{FF48B5059EB0AC65}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_drugstore.et` | 12.4×16.8 | 15.1 | 3 | 5.5 | 12 | yes | 65 | 4 | Saint Philippe 2, Lamentin 1, Saint Pierre 1 |
| ShopHouse_E_2I01t_householdwares | `{F51B9B9BD15BBE64}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_householdwares.et` | 12.4×16.8 | 15.1 | 3 | 5.5 | 12 | yes | 67 | 3 | Montignac 1, Saint Philippe 1, Saint Pierre 1 |
| ShopHouse_E_2I01t_clothesstore | `{92E8F78BCF17D93D}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_clothesstore.et` | 12.4×16.8 | 15.1 | 3 | 5.5 | 12 | yes | 66 | 3 | Saint Philippe 1, Morton 1, Saint Pierre 1 |
| ShopHouse_E_2I01t_foodstore | `{2F830B9B3A7D4855}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_foodstore.et` | 12.4×16.8 | 15.1 | 3 | 5.5 | 12 | yes |  | 1 | Figari 1 |

## Pubs

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| PubVillage_E_2L01_V1 | `{1B68F3EE279C97A1}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V1.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes | 191 | 2 | Gravette 1, Saint Philippe 1 |
| PubVillage_E_2L01_V2 | `{C7B11EF2ABFDF49F}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V2.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes | 191 | 2 | Montignac 1, Laruns 1 |
| PubVillage_E_2L01_V7_Kamzik | `{F5F4CF162914FB95}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V7_Kamzik.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes |  | 1 | (rural) 1 |
| PubVillage_E_2L01_V3_UCeperu | `{7BFECEE28B046E72}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V3_UCeperu.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes |  | 1 | Meaux 1 |
| PubVillage_E_2L01_V8_ProgresBar | `{965AFAD159CB0C8A}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V8_ProgresBar.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes | 192 | 1 | Saint Philippe 1 |
| PubVillage_E_2L01_V5_Mortonka | `{D13E0BC1C8B07819}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V5_Mortonka.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes | 191 | 1 | Morton 1 |
| PubVillage_E_2L01_V4_UGrocha | `{2727C76FA6C96B21}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V4_UGrocha.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes |  | 1 | Durras 1 |
| PubVillage_E_2L01_V6_Upirata | `{DA1AA1BD5CBCD864}Prefabs/Structures/Commercial/Pubs/PubVillage_E_2L01/PubVillage_E_2L01_V6_Upirata.et` | 17.9×17 | 11.1 | 3 | 6.2 | 10 | yes | 182 | 1 | Saint Pierre 1 |

## Kiosks and booths

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| NewspaperBooth_E_01_redyellow | `{DA33ED8E94FD68DF}Prefabs/Structures/Commercial/Booth/NewspaperBooth_E_01/NewspaperBooth_E_01_redyellow.et` | 4.1×5 | 3.1 | 2 |  | 1 | yes | 0 | 9 | (rural) 3, Lamentin 1, Villeneuf 1, Saint Philippe 1 |
| NewspaperBooth_E_01_redwhite | `{B0C7B3A0C937328B}Prefabs/Structures/Commercial/Booth/NewspaperBooth_E_01/NewspaperBooth_E_01_redwhite.et` | 4.1×5 | 3.1 | 2 |  | 1 | yes | 0 | 8 | Lamentin 1, Meaux 1, Saint Philippe 1, Montignac 1 |

## Fuel stations

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| FuelStation_E_01_building | `{00FD2B4A0BDEC271}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building.et` | 10.9×5.5 | 3.5 | 2 |  | 2 | yes | 5 | 5 | Lamentin 1, Saint Philippe 1, Provins 1, Etoupe 1 |
| FuelStation_E_01_building_small | `{627FE49D4DC922AE}Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/HouseAddons/FuelStation_E_01_building_small.et` | 4.8×3.7 | 3.5 | 2 |  | 1 | yes | 1 | 5 | Industrial 1, Saint Philippe 1, Military 1, Provins 1 |

## Churches

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| ChurchSmall_E_01 | `{0FD764569422DBA7}Prefabs/Structures/Cultural/Churches/ChurchSmall_E_01/ChurchSmall_E_01.et` | 15.5×22.3 | 24 | 2 |  | 2 | yes | 14 | 3 | Meaux 1, Chotain 1, Saint Pierre 1 |
| Church_01_crypt_attic | `{A683E3049A6872A3}Prefabs/Structures/Cultural/Churches/Church_01/Church_01_crypt_attic.et` | 16.5×28 | 19.1 | 1 | 4 | 0 | no | 31 | 3 | Saint Philippe 1, Montignac 1, Morton 1 |
| ChurchSmall_E_01_weathered | `{F63BC48BE582FF20}Prefabs/Structures/Cultural/Churches/ChurchSmall_E_01/ChurchSmall_E_01_weathered.et` | 15.5×22.3 | 24 | 2 |  | 2 | yes |  | 2 | Lamentin 1, Le Moule 1 |
| Church_01_red | `{D645C54FF0E4D55E}Prefabs/Structures/Cultural/Churches/Church_01/Church_01_red.et` | 20×33.8 | 41.4 | 2 | 6 | 11 | yes | 35 | 1 | Saint Philippe 1 |
| Church_01_yellow | `{22D7D4C89F6AEDDF}Prefabs/Structures/Cultural/Churches/Church_01/Church_01_yellow.et` | 20×33.8 | 41.4 | 2 | 6 | 11 | yes | 35 | 1 | Montignac 1 |
| Church_01_blue_nocellar | `{2BF82F9EBAC834D6}Prefabs/Structures/Cultural/Churches/Church_01/Church_01_blue_nocellar.et` | 20×33.8 | 41.4 | 2 | 6 | 10 | yes | 35 | 1 | Morton 1 |
| Church_01_addon_02 | `{8F6395A72C546A04}Prefabs/Structures/Cultural/Churches/Church_01/BuildingAddons/Church_01_addon_02.et` | 1.5×0.1 | 2.5 | ? |  | 0 | no |  | 1 | Morton 1 |

## Chapels

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Chapel_01_base | `{0E9DBC9B70C345F3}Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_base.et` | 4.7×4.7 | 8.8 | 1 |  | 1 | yes | 0 | 15 | Saint Pierre 3, Saint Philippe 2, (rural) 2, Lamentin 1 |
| Chapel_01_pink | `{6C4146212E67FFD4}Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_pink.et` | 4.7×4.7 | 8.8 | 1 |  | 1 | yes | 0 | 5 | (rural) 3, Saint Philippe 2 |
| Chapel_01_yellow | `{FC4C6642B6BF5564}Prefabs/Structures/Cultural/Chapels/Chapel_01/Chapel_01_yellow.et` | 4.7×4.7 | 8.8 | 1 |  | 1 | yes | 0 | 4 | Saint Philippe 1, Entre-Deux 1, (rural) 1, Lancre 1 |

## Government, municipal office, school

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| MunicipalOffice_E_01 | `{363A616E6C64BB7F}Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01.et` | 24.1×17.8 | 14.8 | 3 | 5.8 | 14 | yes | 146 | 6 | Lamentin 1, Meaux 1, Saint Philippe 1, Montignac 1 |
| MunicipalOffice_E_01_school | `{B514DD5B90A27E78}Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01_school.et` | 24.1×17.8 | 14.8 | 3 | 5.8 | 14 | yes | 221 | 1 | Saint Philippe 1 |
| MunicipalOffice_E_01_Military | `{6AB811A185D78D2A}Prefabs/Structures/Services/Government/MunicipalOffice_E_01/MunicipalOffice_E_01_Military.et` | 24.1×17.8 | 14.8 | 3 | 5.8 | 14 | yes |  | 1 | Military 1 |

## Police (the police-variant villa)

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Villa_E_2I01_Police | `{151CFDB2968ADB77}Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01_Police.et` | 15.3×12.7 | 14.7 | 5 | 4.9 | 8 | yes | 124 | 4 | Meaux 1, Saint Philippe 1, Montignac 1, Saint Pierre 1 |

## Fire stations

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| FireStation_E_01_hose_hanger | `{575A6852D1E5CAA6}Prefabs/Structures/Services/Fire/FireStation_E_01/FireStation_E_01_hose_hanger.et` | 0.6×2 | 9 | 0 |  | 0 | no | 0 | 4 | Lamentin 1, Saint Philippe 1, Montignac 1, Saint Pierre 1 |
| FireStation_E_01 | `{E8C14F26268A4D6D}Prefabs/Structures/Services/Fire/FireStation_E_01/FireStation_E_01.et` | 15.9×15.7 | 9.7 | 4 | 4.4 | 9 | yes | 58 | 3 | Saint Philippe 1, Montignac 1, Saint Pierre 1 |

## Hotels

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| MountainHotel_01_red | `{FC4BFDB06DC30F3F}Prefabs/Structures/Services/Hotels/MountainHotel_01/MountainHotel_01_red.et` | 33.1×18.9 | 8.8 | 2 | 6 | 17 | yes |  | 1 | Pennants Pass 1 |
| MountainHotel_01_green | `{39FC44E47347DF5D}Prefabs/Structures/Services/Hotels/MountainHotel_01/MountainHotel_01_green.et` | 33.1×18.9 | 8.8 | 2 | 6 | 17 | yes |  | 1 | (rural) 1 |

## Garages and workshops

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Garage_E_01 | `{A548C1C5370D0734}Prefabs/Structures/Houses/Garage/Garage_E_01/Garage_E_01.et` | 4.9×9.9 | 3.3 | 2 |  | 2 | yes | 0 | 96 | Saint Philippe 19, Harbour 14, Saint Pierre 12, Meaux 9 |
| Workshop_01_Garage | `{5F00FD9BDAD8C1A5}Prefabs/Structures/Industrial/Houses/Workshop_01/Workshop_01_Garage.et` | 8.3×5.8 | 4.1 | 1 | 2.4 | 3 | yes | 3 | 32 | Lamentin 4, Villeneuf 3, Military 3, Factory 2 |
| Workshop_01_Side | `{F9F2F6D2BD65B719}Prefabs/Structures/Industrial/Houses/Workshop_01/Workshop_01_Side.et` | 5.4×5.8 | 5 | 1 | 2.4 | 1 | yes | 7 | 26 | Saint Philippe 4, Military 4, Saint Pierre 3, Lamentin 2 |
| Workshop_01_Main | `{CFF7F6F7042AFFEF}Prefabs/Structures/Industrial/Houses/Workshop_01/Workshop_01_Main.et` | 12.2×7.6 | 4.1 | 1 | 2.4 | 4 | yes | 11 | 23 | Saint Philippe 7, Military 3, Sawmill 2, Villeneuf 2 |
| Workshop_02_grey | `{CF7E43120709D9BE}Prefabs/Structures/Industrial/Houses/Workshop_02/Workshop_02_grey.et` | 8.9×15.5 | 6.2 | 1 | 3 | 4 | yes | 21 | 16 | Harbour 3, Military 3, Saint Philippe 2, (rural) 1 |
| Workshop_02 | `{4DC0293EF161C898}Prefabs/Structures/Industrial/Houses/Workshop_02/Workshop_02.et` | 8.9×15.5 | 6.2 | 1 | 3 | 4 | yes | 21 | 11 | Harbour 3, Saint Philippe 2, Military 2, Factory 1 |
| HouseAddon_Garage_E_01 | `{448D2BD96AA205E4}Prefabs/Structures/Houses/Village/HouseAddon_Garage_E_01/HouseAddon_Garage_E_01.et` | 7×8.2 | 4.6 | 1 | 2.6 | 3 | yes | 1 | 11 | Saint Philippe 4, Erquy 2, Lamentin 1, Industrial 1 |
| Garage_E_02 | `{80A5B37A1472B084}Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et` | 28.1×18.9 | 8 | 3 | 5.9 | 6 | yes | 22 | 10 | Harbour 3, Military 3, Industrial 1, Montignac 1 |
| Warehouse_01_Workshop | `{BE70E3BA6C1911A9}Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Workshop.et` | 9.7×10.4 | 5.5 | 1 | 4 | 3 | yes | 5 | 5 | Industrial 1, Saint Philippe 1, Military 1, Farm 1 |
| RampVehicle_01_concrete | `{FFDC9C8026E15E29}Prefabs/Structures/Industrial/Repair/RampVehicle_01/RampVehicle_01_concrete.et` | 5.4×12.7 | 1 | 1 |  | 0 | no | 0 | 5 | Saint Philippe 1, Montignac 1, Industrial 1, Provins 1 |
| RampVehicle_01_metal | `{74FAABE8512145EF}Prefabs/Structures/Industrial/Repair/RampVehicle_01/RampVehicle_01_metal.et` | 5.1×11.1 | 1 | 1 |  | 0 | no | 0 | 3 | Harbour 2, Power Plant 1 |

## Sheds

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Door_HouseAddon_Shed_E_R_EXT_COV_02 | `{4B531C06224FA250}Prefabs/Structures/BuildingParts/Doors/Door_HouseAddon_Shed_E_R_EXT_COV_02.et` | 1.6×0.1 | 2.5 | ? |  | 0 | no |  | 223 | (rural) 70, Saint Philippe 21, Vernon 10, Erquy 10 |
| Door_HouseAddon_Shed_E_R_EXT_COV_01 | `{270728E69E8B2D7E}Prefabs/Structures/BuildingParts/Doors/Door_HouseAddon_Shed_E_R_EXT_COV_01.et` | 1.7×0.1 | 2.7 | ? |  | 0 | no |  | 146 | (rural) 42, Saint Philippe 16, Montignac 6, Erquy 6 |
| Door_HouseAddon_Shed_E_L_EXT_COV_01 | `{9D6C69116A1DE3B0}Prefabs/Structures/BuildingParts/Doors/Door_HouseAddon_Shed_E_L_EXT_COV_01.et` | 1.9×0.1 | 2.7 | ? |  | 0 | no |  | 146 | (rural) 42, Saint Philippe 16, Montignac 6, Erquy 6 |
| Shed_01 | `{F08D8E78433D713A}Prefabs/Structures/Houses/Shed/Shed_01/Shed_01.et` | 3.6×2.7 | 2.7 | 1 |  | 1 | yes | 0 | 91 | (rural) 18, Meaux 13, Saint Pierre 8, Lamentin 5 |
| Shed_02_square | `{D61A7F2891D457C6}Prefabs/Structures/Houses/Shed/Shed_02/Shed_02_square.et` | 4.4×3.7 | 2.7 | 2 |  | 1 | yes | 0 | 84 | (rural) 23, Montignac 7, Morton 6, Meaux 4 |
| Shed_02_big_V1 | `{6FA58B435F69B179}Prefabs/Structures/Houses/Shed/Shed_02/Shed_02_big_V1.et` | 6.5×3.7 | 2.7 | 2 |  | 2 | yes | 0 | 83 | (rural) 22, Saint Philippe 5, Morton 5, Regina 4 |
| Shed_03 | `{8597237EA3C59701}Prefabs/Structures/Houses/Shed/Shed_03/Shed_03.et` | 6.8×3.9 | 3.7 | 0 |  | 0 | no | 0 | 77 | (rural) 15, Saint Philippe 11, Montignac 10, Levie 6 |
| Shed_02_line | `{24BA2E136E8815C2}Prefabs/Structures/Houses/Shed/Shed_02/Shed_02_line.et` | 6.6×2.8 | 2.7 | 1 |  | 1 | yes | 0 | 65 | Saint Philippe 12, (rural) 8, Montignac 5, Provins 4 |
| Shed_02_small | `{8590602F727399F4}Prefabs/Structures/Houses/Shed/Shed_02/Shed_02_small.et` | 3.5×2.6 | 2.7 | 1 |  | 1 | yes | 0 | 49 | (rural) 17, Saint Philippe 4, Chotain 4, Figari 3 |
| Shed_02_big_V2 | `{03F1BFA3E3AD3E57}Prefabs/Structures/Houses/Shed/Shed_02/Shed_02_big_V2.et` | 7×3.7 | 2.8 | 2 |  | 1 | yes | 0 | 48 | (rural) 13, Saint Philippe 5, Morton 4, Figari 4 |
| Shed_03_wide | `{EF96889E35217E1E}Prefabs/Structures/Houses/Shed/Shed_03/Shed_03_wide.et` | 12.3×3.9 | 3.7 | 0 |  | 0 | no | 0 | 33 | (rural) 10, Saint Philippe 4, Harbour 3, Sawmill 3 |
| HouseAddon_Shed_E_01 | `{325991D9993FA95B}Prefabs/Structures/Houses/Village/HouseAddon_Shed_E_01/HouseAddon_Shed_E_01.et` | 4.9×8.5 | 5.1 | 0 | 2.9 | 3 | no | 0 | 25 | Saint Philippe 4, Villeneuf 3, (rural) 3, Gravette 2 |
| HouseAddon_Workshop_E_01 | `{DC1CA32A90006916}Prefabs/Structures/Houses/Village/HouseAddon_Workshop_E_01/HouseAddon_Workshop_E_01.et` | 8.1×7 | 4.7 | 1 | 2.6 | 2 | yes | 2 | 18 | Saint Philippe 3, Harbour 2, Lamentin 2, Villeneuf 1 |

## Barns, cow sheds, hay lofts, weigh stations

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Barn_E_02 | `{F5A2DC0597B53E19}Prefabs/Structures/Agriculture/Barn/Barn_E_02/Barn_E_02.et` | 9.5×8.5 | 5.1 | 0 | 3.2 | 4 | no | 0 | 87 | (rural) 31, Saint Philippe 7, Vernon 5, Erquy 4 |
| Barn_E_01 | `{99F6E8E52B71B137}Prefabs/Structures/Agriculture/Barn/Barn_E_01/Barn_E_01.et` | 15.1×10.8 | 12.1 | 0 | 4 | 4 | no | 0 | 46 | (rural) 9, Saint Philippe 6, Montignac 3, Figari 3 |
| Barn_E_03_closed | `{C363B659675041BA}Prefabs/Structures/Agriculture/Barn/Barn_E_03/Barn_E_03_closed.et` | 17.8×7.3 | 7.1 | 2 | 2.8 | 4 | yes | 0 | 14 | (rural) 4, Regina 2, Lamentin 1, Sawmill 1 |
| Barn_E_03_open | `{66F1A0049CC2F5BD}Prefabs/Structures/Agriculture/Barn/Barn_E_03/Barn_E_03_open.et` | 17.8×7.3 | 7.1 | 2 | 2.8 | 5 | yes | 0 | 10 | Saint Philippe 2, (rural) 2, Factory 1, Montignac 1 |
| Barn_E_01_white | `{10297A86E459E9D4}Prefabs/Structures/Agriculture/Barn/Barn_E_01/Barn_E_01_white.et` | 15.1×10.8 | 12.1 | 0 | 4 | 4 | no |  | 4 | Le Moule 1, Villeneuf 1, Meaux 1, (rural) 1 |
| WeightStation_01 | `{B461AC17AE9B29EF}Prefabs/Structures/Agriculture/WeightStation/WeightStation_01/WeightStation_01.et` | 6.6×11 | 5.1 | 2 |  | 4 | yes | 12 | 4 | Farm 2, Saint Philippe 1, Industrial 1 |
| Hayloft_E_01_metal | `{EC63F328F15AD3AF}Prefabs/Structures/Agriculture/Hayloft/Hayloft_E_01/Hayloft_E_01_metal.et` | 25.1×18.7 | 12.3 | 2 | 6 | 4 | yes | 2 | 4 | Farm 3, (rural) 1 |
| CowShed_E_01_long | `{8ED47AC94995D71A}Prefabs/Structures/Agriculture/CowShed/CowShed_E_01/CowShed_E_01_long.et` | 19.7×70.6 | 7.6 | 1 | 6 | 12 | yes | 10 | 4 | Farm 4 |
| CowShed_E_01_short | `{4E0845B85F4535D8}Prefabs/Structures/Agriculture/CowShed/CowShed_E_01/CowShed_E_01_short.et` | 19.6×46.7 | 7.6 | 1 | 6 | 13 | yes | 15 | 3 | Farm 2, Saint Philippe 1 |
| Hayloft_E_01_wood | `{0C55DFF2B0385FFE}Prefabs/Structures/Agriculture/Hayloft/Hayloft_E_01/Hayloft_E_01_wood.et` | 24.9×18.7 | 12.2 | 2 | 6 | 5 | yes |  | 3 | Villeneuf 1, (rural) 1, Erquy 1 |
| WeightStation_01_scale | `{82992F7B47B6191F}Prefabs/Structures/Agriculture/WeightStation/WeightStation_01/BuildingAddons/WeightStation_01_scale.et` | 0.3×1.7 | 1.7 | ? |  | 0 | no |  | 3 | Saint Philippe 1, Farm 1, Industrial 1 |
| Barn_E_01_old | `{5943A8F87263D906}Prefabs/Structures/Agriculture/Barn/Barn_E_01/Barn_E_01_old.et` | 15.1×10.8 | 12.1 | 0 | 4 | 4 | no |  | 3 | (rural) 1, Regina 1, Levie 1 |

## Hen houses and greenhouses

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Henhouse_01 | `{E005876522A8CE1D}Prefabs/Structures/Agriculture/Henhouse/Henhouse_01/Henhouse_01.et` | 3.3×1.2 | 1.9 | ? |  | 0 | no |  | 112 | Saint Philippe 16, (rural) 12, Le Moule 8, Meaux 8 |
| Greenhouse_01 | `{3299E6F34CE40185}Prefabs/Structures/Agriculture/Greenhouse/Greenhouse_01/Greenhouse_01.et` | 3.8×4.2 | 3 | 1 |  | 1 | yes | 0 | 110 | (rural) 19, Saint Philippe 11, Montignac 10, Saint Pierre 10 |

## Industrial: warehouses, factory, offices, sawmill, cabins, metal sheds

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| PortableCabin_E_01_beige | `{E30E1250FD927736}Prefabs/Structures/Industrial/Houses/PortableCabin_E_01/PortableCabin_E_01_beige.et` | 3.8×6.4 | 3.5 | 2 | 3.2 | 1 | yes | 4 | 30 | (rural) 8, Saint Philippe 7, Provins 2, Industrial 2 |
| ShedMetal_02_long_nowalls | `{137E98DD5A116B68}Prefabs/Structures/Industrial/Sheds/ShedMetal_02/ShedMetal_02_long_nowalls.et` | 31×11.2 | 5.2 | 1 |  | 0 | no | 1 | 19 | Military 9, Harbour 4, Industrial 3, (rural) 1 |
| ShedMetal_02_short_nowalls | `{DEBC2FB8DA61B83E}Prefabs/Structures/Industrial/Sheds/ShedMetal_02/ShedMetal_02_short_nowalls.et` | 16.3×11.2 | 5.2 | 1 |  | 0 | no | 0 | 19 | Saint Philippe 6, Harbour 5, Military 3, Sawmill 1 |
| PortableCabin_E_01_yellow | `{B9F18808252E22C3}Prefabs/Structures/Industrial/Houses/PortableCabin_E_01/PortableCabin_E_01_yellow.et` | 3.8×6.4 | 3.5 | 2 | 3.2 | 1 | yes | 5 | 17 | (rural) 5, Harbour 3, Saint Philippe 3, Gravette 2 |
| PortableCabin_E_01_white | `{D445D04331B6809E}Prefabs/Structures/Industrial/Houses/PortableCabin_E_01/PortableCabin_E_01_white.et` | 3.8×6.4 | 3.5 | 2 | 3.2 | 1 | yes | 3 | 16 | Saint Philippe 4, Industrial 3, Harbour 2, (rural) 2 |
| Warehouse_01 | `{12310677867A85D4}Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01.et` | 41.3×19.2 | 7.1 | 1 | 4 | 2 | yes | 10 | 14 | Military 7, Meaux 2, Saint Philippe 2, Harbour 1 |
| PortableCabin_E_02 | `{A42182DF49D50A3B}Prefabs/Structures/Industrial/Houses/PortableCabin_E_02/PortableCabin_E_02.et` | 3.7×2.9 | 3.6 | 2 | 2.9 | 1 | yes | 0 | 12 | Saint Philippe 2, Harbour 1, Briars 1, Villeneuf 1 |
| HouseAddon_ShedMetal_01_Long | `{EBD26252635EDDCE}Prefabs/Structures/Industrial/Houses/HouseAddon_ShedMetal_01/HouseAddon_ShedMetal_01_Long.et` | 19×4.8 | 5.4 | 1 | 4 | 0 | no | 0 | 8 | Saint Philippe 4, Harbour 2, Industrial 1, Military 1 |
| Office_E_01 | `{51F233A0BA73532A}Prefabs/Structures/Industrial/Houses/Office_E_01/Office_E_01.et` | 17×15 | 10.6 | 3 | 6 | 13 | yes | 91 | 6 | Military 2, Montignac 1, Saint Philippe 1, Airport 1 |
| HouseAddon_ShedMetal_01_Short | `{9753007229A082C4}Prefabs/Structures/Industrial/Houses/HouseAddon_ShedMetal_01/HouseAddon_ShedMetal_01_Short.et` | 9.6×4.7 | 5.4 | 1 | 4 | 0 | no | 0 | 5 | Harbour 3, Industrial 1, Saint Philippe 1 |
| Warehouse_01_Office | `{BBCEDBF2C4E3E896}Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Office.et` | 38.3×19.2 | 7.1 | 1 | 4 | 6 | yes | 31 | 5 | Harbour 5 |
| Warehouse_01_V3 | `{1350F724723AAC87}Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_V3.et` | 41.3×19.2 | 7.1 | 1 | 4 | 2 | yes | 25 | 4 | Saint Philippe 1, Power Plant 1, Military 1, Harbour 1 |
| Office_E_01_ENP | `{A33B68467443EBAF}Prefabs/Structures/Industrial/Houses/Office_E_01/Office_E_01_ENP.et` | 17×15 | 10.6 | 3 | 6 | 13 | yes | 91 | 3 | Harbour 3 |
| PortableCabin_E_01_stripes | `{28309952A194BEDE}Prefabs/Structures/Industrial/Houses/PortableCabin_E_01/PortableCabin_E_01_stripes.et` | 3.8×6.4 | 3.5 | 2 | 3.2 | 1 | yes | 0 | 3 | Airport 2, Saint Philippe 1 |
| Sawmill_E_01 | `{1A18A755842DCA36}Prefabs/Structures/Industrial/Houses/Sawmill_E_01/Sawmill_E_01.et` | 27.1×31.9 | 8.6 | 2 | 6 | 8 | yes |  | 2 | Sawmill 2 |
| PortableCabin_E_02_stripes | `{922C7672F36270F1}Prefabs/Structures/Industrial/Houses/PortableCabin_E_02/PortableCabin_E_02_stripes.et` | 3.7×2.9 | 3.6 | 2 | 2.9 | 1 | yes |  | 2 | (rural) 1, Airport 1 |
| FactoryHall_E_01 | `{B1F1B1BEB21102A7}Prefabs/Structures/Industrial/Houses/FactoryHall_E_01/FactoryHall_E_01.et` | 39.3×26.2 | 19.6 | 3 | 3.9 | 20 | yes | 134 | 1 | Factory 1 |
| ConcretePlant_E_01 | `{A92A5F3B79FF56BE}Prefabs/Structures/Industrial/Houses/ConcretePlant_E_01.et` | 12.8×20.6 | 8.8 | 2 |  | 2 | yes | 0 | 1 | Industrial 1 |
| ShedMetal_03_v2 | `{2EC358E97E66CEB2}Prefabs/Structures/Industrial/Sheds/ShedMetal_03/ShedMetal_03_v2.et` | 11.2×5.5 | 3.4 | 2 | 3.6 | 0 | no |  | 1 | Military 1 |
| ShedMetal_03_v1 | `{C4FD5AA3368F166C}Prefabs/Structures/Industrial/Sheds/ShedMetal_03/ShedMetal_03_v1.et` | 11.2×5.5 | 3.4 | 2 | 3.6 | 0 | no |  | 1 | Military 1 |

## Fuel tanks and silos

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| FuelTank_01_grey | `{389D6DF9941FF22D}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_01/FuelTank_01_grey.et` | 5.2×4.6 | 8.5 | 1 |  | 0 | no | 0 | 14 | Harbour 6, Military 4, Saint Philippe 3, Power Plant 1 |
| Silo_01 | `{4D100F180B3EFEC1}Prefabs/Structures/Industrial/Containers/Silos/Silo_01/Silo_01.et` | 2.5×3.7 | 8.4 | 0 |  | 0 | no | 0 | 11 | Farm 9, Saint Philippe 2 |
| FuelTank_02_Base | `{C7A59DCCFF7BF19F}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_Base.et` | 9.1×2.8 | 3.2 | 2 | 4.5 | 0 | no | 0 | 9 | Harbour 2, Lamentin 1, Sawmill 1, Saint Philippe 1 |
| FuelTank_03_grey | `{AAB10C30EE97D12D}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_03/FuelTank_03_grey.et` | 16×17.5 | 10.5 | 0 | 2.1 | 0 | no | 0 | 7 | Power Plant 4, Harbour 3 |
| FuelTank_02_grey | `{AF44CFF705C52B9B}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_grey.et` | 9.1×2.8 | 3.2 | 2 | 4.5 | 0 | no | 0 | 6 | Military 2, Harbour 1, Lamentin 1, Industrial 1 |
| FuelTank_02_green | `{2D92D7E09B3424BC}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_green.et` | 9.1×2.8 | 3.2 | 2 | 4.5 | 0 | no | 0 | 6 | Military 5, Saint Philippe 1 |
| FuelTank_01_green | `{D44E66C875C41446}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_01/FuelTank_01_green.et` | 5.2×4.6 | 8.5 | 1 |  | 0 | no | 0 | 5 | Farm 3, Military 2 |
| Silo_02_red_yellow | `{0DB364ED28333396}Prefabs/Structures/Industrial/Containers/Silos/Silo_02/Silo_02_red_yellow.et` | 3.5×4.4 | 15.1 | 1 |  | 0 | no | 0 | 4 | Industrial 2, (rural) 2 |
| FuelTank_02_Pump_Base | `{AB9840CD694B453E}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_Pump_Base.et` | 11.2×2.8 | 3.2 | 2 | 4.5 | 0 | no | 0 | 3 | Meaux 1, Harbour 1, Military 1 |
| FuelTank_01_rust | `{AD207D3DE43922D8}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_01/FuelTank_01_rust.et` | 5.2×4.6 | 8.5 | 1 |  | 0 | no | 0 | 3 | Farm 3 |
| FuelTank_02_Pump_green | `{51453500F5727B57}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_Pump_green.et` | 11.2×2.8 | 3.2 | 2 | 4.5 | 0 | no | 0 | 2 | Harbour 1, Military 1 |
| Silo_02_white | `{ECC9D2866789526D}Prefabs/Structures/Industrial/Containers/Silos/Silo_02/Silo_02_white.et` | 3.5×4.4 | 15.1 | 1 |  | 0 | no |  | 2 | Industrial 2 |
| FuelTank_02_Pump_grey | `{2D4B0333F2858BDD}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_Pump_grey.et` | 11.2×2.8 | 3.2 | 2 | 4.5 | 0 | no | 0 | 1 | Farm 1 |
| FuelTank_03_green | `{D39297A56E6648F0}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_03/FuelTank_03_green.et` | 16×17.5 | 10.5 | 0 | 2.1 | 0 | no | 0 | 1 | Harbour 1 |

## Smokestacks and light towers

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| LightTower_01 | `{80DF3BD810D032AF}Prefabs/Structures/Industrial/Towers/LightTower_01/LightTower_01.et` | 2.3×1.9 | 16.2 | ? |  | 0 | no |  | 43 | Harbour 18, Military 10, Saint Philippe 5, Meaux 3 |
| SmokestackBrick_E_01 | `{12C531A3B1CE3D56}Prefabs/Structures/Industrial/Smokestacks/SmokestackBrick_E_01/SmokestackBrick_E_01.et` | 6.7×3.9 | 34.2 | ? |  | 0 | no |  | 1 | Factory 1 |
| SmokestackMetal_E_01 | `{6FC492B696FF2DFC}Prefabs/Structures/Industrial/Smokestacks/SmokestackMetal_E_01/SmokestackMetal_E_01.et` | 4.6×4.6 | 48.7 | ? |  | 0 | no |  | 1 | Power Plant 1 |

## Military: barracks, bunkers, garages, guard boxes, fortifications

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Dragonsteeth_01_old_V1 | `{5608B9078BF2AC5A}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragonsteeth_01_old_V1.et` | 1.2×1.4 | 1.2 | ? |  | 0 | no |  | 110 | Military 72, Peters Pass 19, (rural) 16, Perelle 3 |
| Bunker_01_3fp | `{EE5219B38424AB12}Prefabs/Structures/Military/Bunkers/Bunker_01/Bunker_01_3fp.et` | 5.9×6.3 | 3.9 | 2 | 2.3 | 4 | yes | 0 | 48 | (rural) 43, Military 1, Harbour 1, Morton 1 |
| TrenchOld_02_450cm_metal | `{D57988FC629F723F}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_02/TrenchOld_02_450cm_metal.et` | 5.7×7 | 1.3 | 0 |  | 0 | no |  | 30 | (rural) 30 |
| Dragontooth_01_V1 | `{C16C62041E2AAAB2}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_V1.et` | 1.3×1.5 | 1.3 | ? |  | 0 | no |  | 22 | Military 16, Montignac 4, Harbour 2 |
| Dragontooth_01_old_V2 | `{9639C7B3CF572320}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_old_V2.et` | 1.2×1.4 | 1.2 | ? |  | 0 | no |  | 19 | Military 12, (rural) 7 |
| Dragontooth_01_V3 | `{DA1B42FB9AEC02BE}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_V3.et` | 1.3×1.5 | 1.3 | ? |  | 0 | no |  | 16 | Benac 14, (rural) 2 |
| Dragontooth_01_V2 | `{8B7B5323534404A7}Prefabs/Structures/Military/Fortifications/Dragontooth_01/Dragontooth_01_V2.et` | 1.3×1.5 | 1.3 | ? |  | 0 | no |  | 16 | Military 11, Harbour 3, (rural) 2 |
| GuardTower_01 | `{FBCE1861B987EA68}Prefabs/Structures/Military/Houses/GuardTower_01/GuardTower_01.et` | 7.5×8.7 | 9.6 | 0 |  | 0 | no | 0 | 14 | Military 12, Harbour 1, Peters Pass 1 |
| TrenchOld_01_6m_wood_forest | `{AF8D04AE2FDC3351}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_6m_wood_forest.et` | 7.2×7.1 | 1.4 | 0 |  | 0 | no |  | 14 | Peters Pass 9, Tyrone 5 |
| TrenchOld_01_poles_wood_forest | `{17BF019EC515B651}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_poles_wood_forest.et` | 0.2×2.2 | 1.2 | ? |  | 0 | no |  | 14 | Peters Pass 12, Tyrone 2 |
| FlagPole_01_empty | `{1A8791C2BC2B8B75}Prefabs/Structures/Military/Flags/FlagPole_01/FlagPole_01_empty.et` | 0.1×0.3 | 6 | 0 |  | 0 | no | 0 | 13 | Harbour 5, Military 5, Kermovan 2, (rural) 1 |
| TrenchOld_02_450cm_wood_forest | `{885E76819956A5CE}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_02/TrenchOld_02_450cm_wood_forest.et` | 6×7.1 | 1.4 | 0 |  | 0 | no |  | 9 | Peters Pass 7, Tyrone 2 |
| GuardHouse_01_beige | `{CCE6A451A226AF4E}Prefabs/Structures/Military/Houses/GuardHouse_01/GuardHouse_01_beige.et` | 7.6×8.9 | 5.7 | 2 | 2.4 | 3 | yes | 14 | 8 | Military 4, Saint Philippe 1, Farm 1, Power Plant 1 |
| Bunker_01_1fp | `{44B2776836744D69}Prefabs/Structures/Military/Bunkers/Bunker_01/Bunker_01_1fp.et` | 5.5×6.2 | 3.9 | 2 | 2.3 | 2 | yes | 0 | 7 | (rural) 4, Saint Philippe 1, Pennants Pass 1, Durras 1 |
| GuardHouse_01 | `{3F91DEEC9C78E473}Prefabs/Structures/Military/Houses/GuardHouse_01/GuardHouse_01.et` | 7.6×8.9 | 5.7 | 2 | 2.4 | 3 | yes | 14 | 6 | Harbour 4, Meaux 1, Factory 1 |
| Barracks_01_military_yellow | `{88044D117268E628}Prefabs/Structures/Military/Houses/Barracks_01/Barracks_01_military_yellow.et` | 15.9×30.1 | 5.8 | 1 | 5.9 | 11 | yes | 57 | 6 | Military 3, Saint Philippe 1, Harbour 1, (rural) 1 |
| GuardBox_01_beige | `{F50905235FBAA094}Prefabs/Structures/Military/Houses/GuardBox_01/GuardBox_01_beige.et` | 2.2×2.3 | 3.4 | 0 |  | 0 | no |  | 6 | (rural) 4, Military 2 |
| TrenchOld_01_roof_2m_forest | `{B922D2F65ABF6D3B}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_roof_2m_forest.et` | 2×2.5 | 0.5 | ? |  | 0 | no |  | 6 | Peters Pass 4, (rural) 2 |
| Barracks_01_medical_base | `{9E1B1BC411CE2CBC}Prefabs/Structures/Military/Houses/Barracks_01/Barracks_01_medical_base.et` | 15.9×30.1 | 5.8 | 1 | 5.9 | 11 | yes |  | 3 | Military 3 |
| TrenchOld_01_6m_wood_end_forest | `{D9BA0985430B7981}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_6m_wood_end_forest.et` | 7.2×7.1 | 1.4 | 0 |  | 0 | no |  | 2 | Tyrone 2 |
| TrenchOld_01_6m_wood_end | `{349214338129EA51}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_6m_wood_end.et` | 7.1×7 | 1.3 | 0 |  | 0 | no |  | 2 | Tyrone 2 |
| TrenchOld_01_poles_wood | `{DC4BECDC0675ACEC}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_poles_wood.et` | 0.2×2.2 | 1.2 | ? |  | 0 | no |  | 2 | Tyrone 1, (rural) 1 |
| Barracks_01_military_camo | `{4D62E2F38A755750}Prefabs/Structures/Military/Houses/Barracks_01/Barracks_01_military_camo.et` | 15.9×30.1 | 5.8 | 1 | 5.9 | 11 | yes |  | 2 | Military 2 |
| Barracks_01_military_white | `{56073EA03FCF2043}Prefabs/Structures/Military/Houses/Barracks_01/Barracks_01_military_white.et` | 15.9×30.1 | 5.8 | 1 | 5.9 | 11 | yes |  | 2 | (rural) 2 |
| Bunker_01_3fp_Camo | `{501A020C34E2FB63}Prefabs/Structures/Military/Bunkers/Bunker_01/Bunker_01_3fp_Camo.et` | 5.9×6.3 | 3.9 | 2 | 2.3 | 4 | yes |  | 2 | (rural) 2 |
| GuardHouse_02_civilian | `{9606669975C97DC9}Prefabs/Structures/Military/Houses/GuardHouse_02/GuardHouse_02_civilian.et` | 12.8×14.1 | 6.3 | 3 |  | 6 | yes |  | 1 | Airport 1 |
| GuardHouse_03_white | `{E0D823883B9C3E26}Prefabs/Structures/Military/Houses/GuardHouse_03/GuardHouse_03_white.et` | 25.7×17 | 9.8 | 4 | 5.4 | 15 | yes |  | 1 | Military 1 |
| GarageMilitary_E_01_V1 | `{26D21E6AD994B0D2}Prefabs/Structures/Military/Houses/GarageMilitary_E_01/GarageMilitary_E_01_V1.et` | 49.8×25.5 | 12.4 | 1 | 4.6 | 0 | no |  | 1 | Military 1 |
| GarageMilitary_E_01_V2 | `{4A862A8A65503FFC}Prefabs/Structures/Military/Houses/GarageMilitary_E_01/GarageMilitary_E_01_V2.et` | 49.8×25.5 | 12.4 | 1 | 4.6 | 0 | no |  | 1 | Military 1 |
| TrenchOld_01_roof_1m | `{5ABAFD2D08C25E59}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_roof_1m.et` | 1×2.5 | 0.5 | ? |  | 0 | no |  | 1 | (rural) 1 |
| TrenchOld_01_roof_4m | `{B369D66D87013A7A}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_roof_4m.et` | 3.9×2.5 | 0.5 | 0 |  | 0 | no |  | 1 | (rural) 1 |
| TrenchOld_01_roof_1m_forest | `{46F88FA2DA4BFD7D}Prefabs/Structures/Military/Fortifications/Trenches/TrenchOld_01/TrenchOld_01_roof_1m_forest.et` | 1×2.5 | 0.5 | ? |  | 0 | no |  | 1 | (rural) 1 |

## Airport buildings

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| AntennaAirport_01 | `{53251E86B44EF4F2}Prefabs/Structures/Airport/AntennaAirport_01/AntennaAirport_01.et` | 8.7×8.7 | 14.5 | 0 |  | 0 | no | 0 | 1 | Saint Philippe 1 |
| ControlTower_01 | `{9CEC8BB63429FF28}Prefabs/Structures/Airport/ControlTower_01/ControlTower_01.et` | 18.8×17 | 24.1 | 9 | 4.9 | 12 | yes |  | 1 | Airport 1 |
| Hangar_01 | `{CEB1F71D46592F3C}Prefabs/Structures/Airport/Hangar_01/Hangar_01.et` | 44.4×38.3 | 15 | 2 | 3.3 | 2 | yes |  | 1 | Airport 1 |

## Cottages and recreation

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Cottage_E_01 | `{9562159FDE627B33}Prefabs/Structures/Recreation/Houses/Cottage_E_01/Cottage_E_01.et` | 6×9.4 | 5.1 | 1 | 2.3 | 1 | yes |  | 34 | (rural) 24, Saint Philippe 3, Kermovan 2, Redon 1 |
| LatrineBeach_01_brown | `{645EFF51289096E0}Prefabs/Structures/Recreation/Beach/LatrineBeach_01/LatrineBeach_01_brown.et` | 3×1.9 | 2.7 | ? |  | 2 | no |  | 3 | (rural) 2, Lancre 1 |
| DivingBoard_01 | `{9F66FE542D8770B3}Prefabs/Structures/Recreation/Beach/DivingBoard_01.et` | 0.8×7.2 | 0.6 | ? |  | 0 | no |  | 3 | (rural) 2, Lancre 1 |
| ChangingCabin_01 | `{4D15B58E8889BE61}Prefabs/Structures/Recreation/Beach/ChangingCabin_01/ChangingCabin_01.et` | 1.6×1.6 | 2.8 | ? |  | 1 | no |  | 2 | (rural) 2 |
| WaterSlide_01 | `{CE65A38CDE9653A8}Prefabs/Structures/Recreation/Beach/WaterSlide_01.et` | 1.4×9 | 7.1 | 0 |  | 0 | no |  | 1 | (rural) 1 |
| LatrineBeach_01_bluewhite | `{AF349C6A58068CAF}Prefabs/Structures/Recreation/Beach/LatrineBeach_01/LatrineBeach_01_bluewhite.et` | 3×1.9 | 2.7 | ? |  | 2 | no |  | 1 | Lancre 1 |

## Port: piers, quays, breakwater, bollards

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| BreakwaterStone_01_stack_wall | `{F281250F8F8E3755}Prefabs/Structures/Infrastructure/Naval/Breakwater/BreakwaterStone_01_stack_wall.et` | 39.6×22 | 8.6 | ? |  | 0 | no |  | 170 | (rural) 73, Harbour 43, Saint Pierre 20, Meaux 16 |
| Pier_01 | `{F6AA92F7B23D8A71}Prefabs/Structures/Infrastructure/Naval/Piers/Pier_01.et` | 6×6 | 6 | ? |  | 0 | no |  | 154 | (rural) 76, Lancre 22, Morton 14, Harbour 10 |
| PierConcrete_01_20m | `{DB3F36604A282237}Prefabs/Structures/Infrastructure/Naval/Piers/PierConcrete_01_20m.et` | 20×8.2 | 4.1 | ? |  | 0 | no |  | 139 | Harbour 79, Saint Philippe 33, Military 19, Meaux 6 |
| PierWooden_01_25m_abandoned | `{6C60A5750A40FC63}Prefabs/Structures/Infrastructure/Naval/Piers/PierWooden_01_25m_abandoned.et` | 1.7×22.1 | 1.3 | ? |  | 0 | no |  | 107 | (rural) 68, Saint Philippe 11, Morton 11, Harbour 8 |
| Pier_02 | `{9AFEA6170EF9055F}Prefabs/Structures/Infrastructure/Naval/Piers/Pier_02.et` | 6×3 | 6 | ? |  | 0 | no |  | 83 | (rural) 18, Lancre 13, Kermovan 10, Saint Philippe 9 |
| PierConcrete_01_wall_10m | `{A7DE92CA47404D69}Prefabs/Structures/Infrastructure/Naval/Piers/PierConcrete_01_wall_10m.et` | 10×1.2 | 6 | ? |  | 0 | no |  | 68 | Military 19, Meaux 18, (rural) 11, Morton 9 |
| PierWooden_01_10m | `{48023B0C1DCC8F0D}Prefabs/Structures/Infrastructure/Naval/Piers/PierWooden_01_10m.et` | 1.6×10.5 | 1.3 | ? |  | 0 | no |  | 42 | (rural) 25, Harbour 5, Meaux 2, Saint Pierre 2 |
| PierRail_01_V1 | `{49B2F2E9ED2D3D8F}Prefabs/Structures/Infrastructure/Naval/Piers/PierRail_01_V1.et` | 6×0.1 | 1 | ? |  | 0 | no |  | 39 | Harbour 22, Saint Philippe 17 |
| Pier_Tires_01 | `{5BA952DAADCB9C5B}Prefabs/Structures/Infrastructure/Naval/Piers/Pier_Tires_01.et` | 0.9×1.6 | 0.1 | ? |  | 0 | no |  | 33 | Harbour 18, Meaux 6, Saint Philippe 5, Military 4 |
| PierConcrete_01_flat | `{686B4C83AC940895}Prefabs/Structures/Infrastructure/Naval/Piers/PierConcrete_01_flat.et` | 10×8 | 4 | ? |  | 0 | no |  | 32 | Harbour 20, Meaux 9, Saint Philippe 3 |
| Pier_Tires_02 | `{37FD663A110F1375}Prefabs/Structures/Infrastructure/Naval/Piers/Pier_Tires_02.et` | 5.3×1.5 | 0.1 | ? |  | 0 | no |  | 27 | Harbour 9, Military 7, Power Plant 6, Saint Philippe 3 |
| PierWooden_01_25m | `{FF39390F2D826521}Prefabs/Structures/Infrastructure/Naval/Piers/PierWooden_01_25m.et` | 1.6×23.6 | 1.3 | ? |  | 0 | no |  | 27 | (rural) 13, Saint Philippe 3, Morton 3, Kermovan 2 |
| PierConcrete_01_wall_5m | `{88024E0E7D9FF438}Prefabs/Structures/Infrastructure/Naval/Piers/PierConcrete_01_wall_5m.et` | 5.1×1.1 | 6 | ? |  | 0 | no |  | 25 | Harbour 10, Meaux 8, Military 5, Morton 2 |
| PierConcrete_01_corner_30_inner | `{34B1ABE52532D51A}Prefabs/Structures/Infrastructure/Naval/Piers/PierConcrete_01_corner_30_inner.et` | 5×8.3 | 4.1 | ? |  | 0 | no |  | 23 | Harbour 18, Meaux 3, Saint Philippe 1, Saint Pierre 1 |
| PierWooden_01_25m_destroyed | `{9C489B3BE514B0C2}Prefabs/Structures/Infrastructure/Naval/Piers/PierWooden_01_25m_destroyed.et` | 1.7×23.6 | 1.4 | ? |  | 0 | no |  | 23 | (rural) 12, Kermovan 4, Saint Philippe 3, Harbour 2 |
| PierConcrete_01_wall_20m | `{D988B6959B31AC3E}Prefabs/Structures/Infrastructure/Naval/Piers/PierConcrete_01_wall_20m.et` | 20×1.1 | 6 | ? |  | 0 | no |  | 23 | Saint Philippe 23 |
| PierConcrete_01_slipway | `{A8731EFCC2382173}Prefabs/Structures/Infrastructure/Naval/Piers/PierConcrete_01_slipway.et` | 10×8.1 | 4 | ? |  | 0 | no |  | 22 | Harbour 9, Meaux 7, Saint Philippe 6 |
| BoatShed_01_floor | `{832EA6ECA170FB2A}Prefabs/Structures/Infrastructure/Naval/Sheds/BoatShed_01/HouseAddons/BoatShed_01_floor.et` | 6×8 | 1.1 | ? |  | 0 | no |  | 20 | (rural) 5, Kermovan 4, Lancre 4, Saint Philippe 3 |
| BoatShed_01 | `{2B165B5BF9FE270D}Prefabs/Structures/Infrastructure/Naval/Sheds/BoatShed_01/BoatShed_01.et` | 7×8.7 | 5.7 | ? |  | 3 | yes |  | 20 | (rural) 5, Kermovan 4, Lancre 4, Saint Philippe 3 |
| Pier_Stairs02 | `{008ECFE8693C0A1D}Prefabs/Structures/Infrastructure/Naval/Piers/Pier_Stairs02.et` | 4×2.3 | 0 | ? |  | 0 | no |  | 17 | Harbour 12, Saint Philippe 2, Lancre 2, Montignac 1 |
| PierWooden_02_pier | `{401E516FD6CA886B}Prefabs/Structures/Infrastructure/Naval/Piers/PierWooden_02/PierWooden_02_pier.et` | 38.6×12.8 | 3 | ? |  | 0 | no |  | 17 | Harbour 12, Saint Philippe 4, Power Plant 1 |
| PierShed_01 | `{3961F81016AAE7CD}Prefabs/Structures/Infrastructure/Naval/Sheds/PierShed_01/PierShed_01.et` | 13.6×14.8 | 6.4 | ? |  | 10 | yes |  | 17 | Harbour 5, (rural) 3, Saint Philippe 2, Kervel 1 |
| Pier_Stairs01 | `{6CDAFB08D5F88533}Prefabs/Structures/Infrastructure/Naval/Piers/Pier_Stairs01.et` | 2.4×2.4 | 0 | ? |  | 0 | no |  | 16 | Harbour 7, Saint Philippe 2, Montignac 2, Airport 2 |
| BreakwaterStone_01_stack_slope | `{4B04E992D0E9413E}Prefabs/Structures/Infrastructure/Naval/Breakwater/BreakwaterStone_01_stack_slope.et` | 8.3×12.5 | 2.1 | ? |  | 0 | no |  | 15 | Harbour 7, Camurac 7, Morton 1 |
| Davit_02 | `{2777C2591D7B6788}Prefabs/Structures/Infrastructure/Naval/Davits/Davit_02.et` | 1.6×6.9 | 6.4 | ? |  | 0 | no |  | 13 | Harbour 6, Saint Philippe 4, Military 1, Meaux 1 |
| BreakwaterStone_01_stack_wall_half | `{0B31DF40DF62F895}Prefabs/Structures/Infrastructure/Naval/Breakwater/BreakwaterStone_01_stack_wall_half.et` | 38.6×11.8 | 8.1 | ? |  | 0 | no |  | 13 | Military 4, (rural) 3, Meaux 3, Morton 2 |
| LightBeacon_01 | `{4890FA03952D692B}Prefabs/Structures/Infrastructure/Naval/Lighthouse/LightBeacon_01/LightBeacon_01.et` | 4.2×5.1 | 10.1 | ? |  | 2 | no |  | 12 | (rural) 7, Harbour 3, Military 2 |
| FishermanShed_02_V1 | `{73AEA0148D58F723}Prefabs/Structures/Infrastructure/Naval/Sheds/FishermanShed_02/FishermanShed_02_V1.et` | 4.3×6.1 | 5.6 | ? |  | 1 | yes |  | 11 | (rural) 4, Kermovan 3, Lancre 2, Gravette 1 |
| Pier_Wood_Broken_01 | `{F708B6D672C79833}Prefabs/Structures/Infrastructure/Naval/Piers/Pier_Wood_Broken_01.et` | 4.3×0.7 | 0.5 | ? |  | 0 | no |  | 10 | Morton 6, Harbour 4 |
| Davit_01 | `{4B23F6B9A1BFE8A6}Prefabs/Structures/Infrastructure/Naval/Davits/Davit_01.et` | 0.5×3.4 | 3.2 | ? |  | 0 | no |  | 8 | Harbour 5, (rural) 1, Meaux 1, Power Plant 1 |

(20 more port pieces in the JSON)

## Montfort Castle pieces

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Castle_01_InnerWall_01 | `{6BE382F85F1A7491}Prefabs/Structures/Cultural/Castles/Castle_01_InnerWall_01.et` | 16×2.3 | 9.2 | ? |  | 0 | no |  | 27 | Lamentin 19, Harbour 5, Montfort Castle 3 |
| Castle_01_InnerWall_Small_01 | `{2C97FDC04C9E39C0}Prefabs/Structures/Cultural/Castles/Castle_01_InnerWall_Small_01.et` | 7.4×0.9 | 3.1 | ? |  | 0 | no |  | 23 | Montfort Castle 17, Lamentin 6 |
| Castle_01_Bridge | `{ADDA5472F867C249}Prefabs/Structures/Cultural/Castles/Castle_01_Bridge.et` | 4.7×18.6 | 1.6 | ? |  | 0 | no |  | 21 | (rural) 13, Saint Pierre 3, Saint Philippe 1, Regina 1 |
| Castle_01_OuterWall_03_d | `{1321E90FE0CF8C3C}Prefabs/Structures/Cultural/Castles/Castle_01_OuterWall_03_d.et` | 21.2×2.9 | 6.4 | ? |  | 0 | no |  | 14 | Montfort Castle 9, (rural) 5 |
| Castle_01_Bridge_Pillar | `{70231867B54741BB}Prefabs/Structures/Cultural/Castles/Castle_01_Bridge_Pillar.et` | 4×19.4 | 0.3 | ? |  | 0 | no |  | 10 | (rural) 7, Saint Pierre 2, Montfort Castle 1 |
| Castle_01_OuterWall_01 | `{92F8DAE5E56CA22E}Prefabs/Structures/Cultural/Castles/Castle_01_OuterWall_01.et` | 21.2×3.1 | 8.8 | ? |  | 0 | no |  | 8 | Montfort Castle 8 |
| Castle_01_Stairs_01 | `{31CAD6AEE7E7E4CF}Prefabs/Structures/Cultural/Castles/Castle_01_Stairs_01.et` | 16.5×3 | 6 | ? |  | 0 | no |  | 6 | (rural) 3, Montfort Castle 3 |
| Castle_01_InnerWall_02 | `{07B7B618E3DEFBBF}Prefabs/Structures/Cultural/Castles/Castle_01_InnerWall_02.et` | 16×1.2 | 9.4 | ? |  | 0 | no |  | 5 | Lamentin 3, Montfort Castle 2 |
| Castle_01_InnerWall_Small_02 | `{85D22B7ED8C8024C}Prefabs/Structures/Cultural/Castles/Castle_01_InnerWall_Small_02.et` | 6.2×0.9 | 3.2 | ? |  | 0 | no |  | 5 | Montfort Castle 4, (rural) 1 |
| Castle_01_Pillar | `{47DA19EDF2754DE0}Prefabs/Structures/Cultural/Castles/Castle_01_Pillar.et` | 2.1×2.7 | 5.5 | ? |  | 0 | no |  | 5 | Montfort Castle 5 |
| Castle_01_OuterWall_01_Round | `{6E10F2B9F8C8BB2C}Prefabs/Structures/Cultural/Castles/Castle_01_OuterWall_01_Round.et` | 20.4×8 | 8.8 | ? |  | 0 | no |  | 3 | Montfort Castle 3 |
| Castle_01_OuterWall_01_Corner | `{034A45B0D41025B9}Prefabs/Structures/Cultural/Castles/Castle_01_OuterWall_01_Corner.et` | 4.6×4.5 | 8.7 | ? |  | 0 | no |  | 2 | Montfort Castle 2 |
| Castle_01_OuterWall_01_Skewed_02 | `{C8214079EF7F307B}Prefabs/Structures/Cultural/Castles/Castle_01_OuterWall_01_Skewed_02.et` | 15.5×3 | 14.8 | ? |  | 0 | no |  | 2 | Montfort Castle 2 |
| Castle_01_OuterWall_01_Skewed_01 | `{BD66CD1F3562CBE6}Prefabs/Structures/Cultural/Castles/Castle_01_OuterWall_01_Skewed_01.et` | 15.5×3 | 14.8 | ? |  | 0 | no |  | 2 | Montfort Castle 2 |
| Castle_01_OuterWall_01_Hole | `{8D5BD6AF6FBD7A62}Prefabs/Structures/Cultural/Castles/Castle_01_OuterWall_01_Hole.et` | 21.1×3.1 | 8.8 | ? |  | 0 | no |  | 1 | Montfort Castle 1 |
| Castle_01_Tower | `{DECF116069DCCC2B}Prefabs/Structures/Cultural/Castles/Castle_01_Tower.et` | 16.1×20.7 | 35.2 | ? |  | 2 | no |  | 1 | Montfort Castle 1 |

## House ruins

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| HouseRuin_01_BrickPile_Big | `{5A81BD9171FC3B07}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_BrickPile_Big.et` | 5×4.7 | 0.7 | 0 |  | 0 | no | 0 | 532 | (rural) 378, Kermovan 25, Saint Philippe 20, Laruns 17 |
| HouseRuin_01_BrickPile_Small | `{F7919E01B9B17B3E}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_BrickPile_Small.et` | 3×2.9 | 0.5 | 0 |  | 0 | no | 0 | 431 | (rural) 118, Saint Philippe 64, Military 58, Richemont 53 |
| HouseRuin_01_Corner | `{AAB9EC1130878E02}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_Corner.et` | 6.3×5.1 | 2.3 | 0 |  | 0 | no | 0 | 155 | (rural) 101, Saint Philippe 16, Laruns 8, Lancre 6 |
| HouseRuin_01_Wall | `{9CFA6E8952B4E00D}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_Wall.et` | 11.6×0.7 | 2.3 | 0 |  | 0 | no | 0 | 139 | (rural) 87, Laruns 9, Saint Philippe 8, Lamentin 7 |
| HouseRuin_01_WoodPile | `{5CDDDFB2D698DCC3}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_WoodPile.et` | 5.3×4.8 | 0.5 | 0 |  | 0 | no | 0 | 122 | (rural) 62, Saint Philippe 9, Landfill 7, Saint Pierre 7 |
| HouseRuin_01_Entry | `{8C3EF42812208AB4}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_Entry.et` | 10.9×0.7 | 3.4 | 0 |  | 0 | no | 0 | 68 | (rural) 52, Saint Philippe 4, Lancre 4, Kermovan 2 |
| HouseRuin_01_Windows | `{3E7CF4C5E180F5D1}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_Windows.et` | 10.7×0.7 | 3.9 | 0 |  | 0 | no | 0 | 43 | (rural) 29, Saint Philippe 4, Kermovan 2, Lancre 2 |
| HouseRuin_01_Chimney | `{4B2072D6C07CC8C1}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_Chimney.et` | 1.4×1.3 | 6 | 0 |  | 0 | no | 0 | 35 | (rural) 21, Saint Philippe 3, Lancre 2, Lamentin 1 |
| HouseRuin_01_Corner_Stairs | `{22759E6CD7F7BFF8}Prefabs/Structures/Ruins/HouseRuins/HouseRuin_01/HouseRuin_01_Corner_Stairs.et` | 6.5×6.4 | 2.9 | 1 |  | 0 | no |  | 9 | (rural) 9 |

## Waterworks (wells, reservoirs, treatment plant)

| prefab | `{GUID}path` | footprint m | height | floors | cellar | doors | interior | furn | count | used in |
|---|---|---|---:|---:|---:|---:|---|---:|---:|---|
| Well_02_blue | `{09277B76FF010FCB}Prefabs/Structures/Infrastructure/Waterworks/Well_02/Well_02_blue.et` | 1.6×1.6 | 0.4 | ? |  | 0 | no |  | 148 | (rural) 20, Morton 14, Lamentin 10, Figari 10 |
| WellPump_01_blue | `{FA66F415264D1555}Prefabs/Structures/Infrastructure/Waterworks/WellPump_01/WellPump_01_blue.et` | 0.7×0.6 | 1.4 | ? |  | 0 | no |  | 148 | (rural) 21, Morton 14, Lamentin 10, Figari 10 |
| Well_02_red | `{08E07DC45E4E7E24}Prefabs/Structures/Infrastructure/Waterworks/Well_02/Well_02_red.et` | 1.6×1.6 | 0.4 | ? |  | 0 | no |  | 34 | Saint Philippe 11, (rural) 4, Montignac 4, Meaux 2 |
| WellPump_01_red | `{1FEA777E6B9BD50E}Prefabs/Structures/Infrastructure/Waterworks/WellPump_01/WellPump_01_red.et` | 0.7×0.6 | 1.4 | ? |  | 0 | no |  | 34 | Saint Philippe 11, (rural) 4, Montignac 4, Meaux 2 |
| WellPump_01_green | `{2E64D491F4B08FA5}Prefabs/Structures/Infrastructure/Waterworks/WellPump_01/WellPump_01_green.et` | 0.7×0.6 | 1.4 | ? |  | 0 | no |  | 31 | Saint Philippe 9, (rural) 5, Meaux 2, Regina 2 |
| Well_02_green | `{5E86B2DD27E139BF}Prefabs/Structures/Infrastructure/Waterworks/Well_02/Well_02_green.et` | 1.6×1.6 | 0.4 | ? |  | 0 | no |  | 30 | Saint Philippe 8, (rural) 5, Meaux 2, Regina 2 |
| HydrantStreet_01 | `{2D4B21B9086F6AFB}Prefabs/Structures/Infrastructure/Waterworks/HydrantStreet_01/HydrantStreet_01.et` | 0.4×0.2 | 1.1 | ? |  | 0 | no |  | 24 | Saint Philippe 12, Saint Pierre 5, Lamentin 1, Villeneuf 1 |
| WaterStation_01_Uncovered | `{42959A29753486F5}Prefabs/Structures/Infrastructure/Waterworks/WaterStation_01/WaterStation_01_Uncovered.et` | 4.5×5.6 | 6.3 | ? |  | 1 | yes |  | 13 | Farm 2, Lamentin 1, Meaux 1, Saint Philippe 1 |
| Well_02_base | `{4267BB536FCEF37F}Prefabs/Structures/Infrastructure/Waterworks/Well_02/Well_02_base.et` | 1.6×1.6 | 0.4 | ? |  | 0 | no |  | 11 | (rural) 5, Provins 3, Entre-Deux 2, Regina 1 |
| FloodGate_01_gate | `{10CD2AE2D3555690}Prefabs/Structures/Infrastructure/Waterworks/FloodGate_01/FloodGate_01_gate.et` | 1.1×0.1 | 3.1 | ? |  | 0 | no |  | 5 | (rural) 4, Laruns 1 |
| Well_01 | `{52F1B4468BB8AE09}Prefabs/Structures/Infrastructure/Waterworks/Well_01/Well_01.et` | 1.8×1.8 | 1.2 | ? |  | 0 | no |  | 4 | Lamentin 1, Huntsmans Heath 1, Pennants Pass 1, Montfort Castle 1 |
| FloodGate_01_stone_open | `{0DA90E5B79E762EF}Prefabs/Structures/Infrastructure/Waterworks/FloodGate_01/FloodGate_01_stone_open.et` | 3.9×2.2 | 3 | ? |  | 0 | no |  | 4 | (rural) 4 |
| GravelDam_01 | `{64E482C4598F3E45}Prefabs/Structures/Infrastructure/Waterworks/GravelDam_01/GravelDam_01.et` | 19×1.7 | 2.7 | ? |  | 0 | no |  | 2 | (rural) 1, Figari 1 |
| WaterReservoir_01 | `{728F2D7CD93D3D1D}Prefabs/Structures/Infrastructure/Waterworks/WaterReservoir_01/WaterReservoir_01.et` | 17.8×20.8 | 0.2 | ? |  | 0 | no |  | 2 | Montignac 1, Military 1 |
| WellPump_01_base | `{C9FD2230AF5AC2D8}Prefabs/Structures/Infrastructure/Waterworks/WellPump_01/WellPump_01_base.et` | 0.7×0.6 | 1.4 | ? |  | 0 | no |  | 1 | Montignac 1 |
| WaterTreatmentPlant_01_main | `{8DF7F603987CC4A7}Prefabs/Structures/Infrastructure/Waterworks/WaterTreatmentPlant_01/WaterTreatmentPlant_01_main.et` | 16.4×28.3 | 3.3 | ? |  | 0 | no |  | 1 | Provins 1 |
| WaterTreatmentPlant_01_small | `{11219C5E9D717E14}Prefabs/Structures/Infrastructure/Waterworks/WaterTreatmentPlant_01/WaterTreatmentPlant_01_small.et` | 4.1×6.6 | 1 | ? |  | 0 | no |  | 1 | Provins 1 |
| FloodGate_01_stone_closed | `{566B991892F0BC5E}Prefabs/Structures/Infrastructure/Waterworks/FloodGate_01/FloodGate_01_stone_closed.et` | 3.9×2.2 | 3 | ? |  | 0 | no |  | 1 | Laruns 1 |

## Named places

Counts include every building type inside the place's radius (city 800 m, town 550 m, village 380 m, hamlet 230 m, site 300 m; each building goes to the place it is nearest to in radius units). Names are the map's own descriptors (`#AR-MapLocation_<key>`). Saint Philippe's map key is `StPhillipe`.

### citys

| place | key | x | z | townhouse | house | villa | farmh. | shop | pub | church | chapel | kiosk | fuel | govt | police | fire | hotel | garage | shed | farm | indust. | military | port | ruin | total |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Saint Philippe | StPhillipe | 4503 | 10772 | 10 | 51 | 3 |  | 5 | 2 | 2 | 5 | 2 | 2 | 2 | 1 | 2 |  | 42 | 103 | 19 | 32 | 3 | 172 | 128 | 698 |

### towns

| place | key | x | z | townhouse | house | villa | farmh. | shop | pub | church | chapel | kiosk | fuel | govt | police | fire | hotel | garage | shed | farm | indust. | military | port | ruin | total |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Montignac | Montignac | 4773 | 7095 | 5 | 26 |  |  | 2 | 1 | 2 | 1 | 1 |  | 1 | 1 | 2 |  | 13 | 51 | 6 | 3 | 4 | 12 | 34 | 197 |
| Lamentin | Lamentin | 1280 | 5941 | 5 | 16 | 1 | 2 | 2 |  | 1 | 1 | 2 | 1 | 1 |  | 1 |  | 9 | 36 | 4 | 1 |  | 4 | 36 | 191 |
| Saint Pierre | StPierre | 9689 | 1558 | 5 | 27 |  |  | 4 | 1 | 1 | 3 | 1 | 1 | 1 | 1 | 2 |  | 18 | 30 | 2 | 1 |  | 28 | 16 | 190 |
| Morton | Morton | 5135 | 4012 | 3 | 16 | 1 | 1 | 2 | 1 | 3 | 1 | 1 |  | 1 |  |  |  | 9 | 35 | 4 |  | 1 | 64 | 9 | 183 |

### villages

| place | key | x | z | townhouse | house | villa | farmh. | shop | pub | church | chapel | kiosk | fuel | govt | police | fire | hotel | garage | shed | farm | indust. | military | port | ruin | total |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Meaux | Meaux | 4520 | 9468 | 1 | 20 |  |  | 1 | 1 | 1 |  | 1 |  | 1 | 1 |  |  | 11 | 33 | 5 | 4 | 1 | 84 | 10 | 207 |
| Levie | Levie | 7464 | 4739 | 1 | 13 |  | 1 |  |  |  |  |  |  |  |  |  |  | 2 | 35 | 5 |  |  |  | 11 | 98 |
| Laruns | Laruns | 7558 | 5541 | 1 | 8 |  |  |  | 1 |  | 1 |  |  |  |  |  |  | 8 | 19 | 1 |  |  |  | 46 | 98 |
| Vernon | Vernon | 9243 | 2077 |  | 8 |  | 2 |  |  |  |  |  |  |  |  |  |  |  | 31 | 7 |  |  | 4 | 18 | 96 |
| Erquy | Erquy | 10919 | 11706 |  | 7 | 1 | 3 |  |  |  | 1 |  |  |  |  |  |  | 6 | 35 | 7 |  |  | 5 |  | 91 |
| Figari | Figari | 5251 | 5338 | 1 | 17 |  |  | 1 |  |  | 1 | 1 |  |  |  |  |  | 4 | 34 | 6 |  |  |  |  | 90 |
| Durras | Durras | 8827 | 2746 | 2 | 9 |  | 1 |  | 1 |  |  |  |  |  |  |  |  | 1 | 27 | 4 |  | 1 | 2 | 9 | 78 |
| Tyrone | Tyrone | 4928 | 9093 | 1 | 8 |  |  |  |  |  |  |  |  |  |  |  |  | 2 | 19 | 2 |  | 14 | 2 | 15 | 75 |
| Villeneuf | Villeneuf | 2839 | 6340 |  | 12 |  | 1 |  |  |  | 1 | 1 |  |  |  |  |  | 8 | 26 | 3 | 2 |  |  |  | 74 |
| Gravette | Gravette | 4121 | 7796 |  | 9 |  | 1 |  | 1 |  |  |  |  |  |  |  |  | 3 | 18 | 2 | 3 |  | 19 | 5 | 71 |
| Provins | Provins | 5486 | 6087 | 1 | 9 |  |  |  |  |  |  |  | 2 |  |  |  |  | 7 | 16 | 2 | 3 |  |  |  | 65 |
| Regina | Regina | 7205 | 2324 | 1 | 5 |  | 3 |  |  |  | 1 |  |  |  |  |  |  | 1 | 27 | 4 | 1 |  |  | 7 | 65 |
| Chotain | Chotain | 7086 | 6012 | 2 | 12 |  |  | 1 |  | 1 |  |  |  |  |  |  |  | 5 | 23 |  |  |  | 2 |  | 59 |
| Entre-Deux | EntreDeux | 5767 | 7036 |  | 12 |  |  |  |  |  | 1 |  |  |  |  |  |  | 3 | 21 | 3 | 2 |  |  |  | 58 |
| Le Moule | LeMoule | 2616 | 5379 |  | 7 |  | 1 |  |  | 1 |  |  |  |  |  |  |  | 1 | 14 | 3 |  |  |  |  | 53 |

### hamlets

| place | key | x | z | townhouse | house | villa | farmh. | shop | pub | church | chapel | kiosk | fuel | govt | police | fire | hotel | garage | shed | farm | indust. | military | port | ruin | total |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Lancre | Lancre | 11685 | 2235 |  | 1 |  | 1 |  |  |  | 1 | 1 |  |  |  |  |  |  | 10 | 1 |  |  | 61 | 47 | 131 |
| Kermovan | Kermovan | 6367 | 9664 |  | 2 |  | 1 |  |  |  |  |  |  |  |  |  |  | 1 | 14 | 2 | 1 | 2 | 43 | 47 | 118 |
| Richemont | Richemont | 3541 | 5062 |  |  |  |  |  |  |  |  |  |  |  |  |  |  | 2 | 7 | 1 | 1 |  | 1 | 56 | 71 |
| Etoupe | Etoupe | 5390 | 10009 |  | 4 |  | 2 |  |  |  |  |  | 2 |  |  |  |  |  | 22 | 4 |  |  | 3 | 12 | 62 |
| Perelle | Perelle | 9354 | 5073 |  | 3 |  |  |  |  |  |  |  |  |  |  |  |  |  | 12 | 2 |  | 3 | 15 | 12 | 47 |
| Les Creux | LesCreux | 5334 | 11373 |  | 2 |  |  |  |  |  |  |  |  |  |  |  |  |  | 9 | 3 |  |  |  |  | 21 |
| Benac | Benac | 6579 | 7103 |  | 1 |  |  |  |  |  |  |  |  |  |  |  |  | 1 | 4 | 1 |  | 14 |  |  | 21 |
| Le Bosc | LeBosc | 7939 | 1470 |  | 1 |  |  |  |  |  |  |  |  |  |  |  |  |  | 4 | 1 |  | 1 | 7 |  | 14 |
| Lacourt | Lacourt | 3884 | 6286 | 1 | 1 |  |  |  |  |  |  |  |  |  |  |  |  | 1 | 6 | 1 |  |  |  |  | 12 |
| Redon | Redon | 2223 | 7570 |  | 1 |  |  |  |  |  |  |  |  |  |  |  |  | 1 | 4 | 2 |  |  |  |  | 11 |
| Gorey | Gorey | 4840 | 8094 |  | 2 |  |  |  |  |  |  |  |  |  |  |  |  |  | 5 | 1 |  |  |  |  | 11 |
| Camurac | Camurac | 6589 | 3114 |  |  |  | 1 |  |  |  |  |  |  |  |  |  |  |  | 2 |  |  |  | 8 |  | 11 |
| La Valette | LaValette | 6737 | 5650 |  | 2 |  |  |  |  |  |  |  |  |  |  |  |  |  | 5 | 2 |  |  | 1 |  | 11 |
| Kervel | Kervel | 2521 | 4290 |  | 1 |  |  |  |  |  |  |  |  |  |  |  |  |  | 3 |  |  |  | 1 |  | 7 |

### sites

| place | key | x | z | townhouse | house | villa | farmh. | shop | pub | church | chapel | kiosk | fuel | govt | police | fire | hotel | garage | shed | farm | indust. | military | port | ruin | total |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Harbour | Harbour | 1024 | 6043 |  | 5 |  |  |  |  |  |  |  |  |  |  |  |  | 27 | 9 |  | 31 | 18 | 345 | 10 | 484 |
| Harbour | Harbour | 4397 | 11098 |  | 5 |  |  |  |  |  |  |  |  |  |  |  |  | 27 | 9 |  | 31 | 18 | 345 | 10 | 484 |
| Harbour | Harbour | 4957 | 3876 |  | 5 |  |  |  |  |  |  |  |  |  |  |  |  | 27 | 9 |  | 31 | 18 | 345 | 10 | 484 |
| Harbour | Harbour | 9925 | 1525 |  | 5 |  |  |  |  |  |  |  |  |  |  |  |  | 27 | 9 |  | 31 | 18 | 345 | 10 | 484 |
| Harbour | Harbour | 10663 | 11791 |  | 5 |  |  |  |  |  |  |  |  |  |  |  |  | 27 | 9 |  | 31 | 18 | 345 | 10 | 484 |
| Military | Military | 3902 | 8451 |  |  |  |  |  |  |  |  |  | 1 | 1 |  |  |  | 21 | 1 |  | 27 | 146 | 72 | 59 | 356 |
| Military | Military | 4937 | 11922 |  |  |  |  |  |  |  |  |  | 1 | 1 |  |  |  | 21 | 1 |  | 27 | 146 | 72 | 59 | 356 |
| Military | Military | 5355 | 10543 |  |  |  |  |  |  |  |  |  | 1 | 1 |  |  |  | 21 | 1 |  | 27 | 146 | 72 | 59 | 356 |
| Military | Military | 7447 | 6705 |  |  |  |  |  |  |  |  |  | 1 | 1 |  |  |  | 21 | 1 |  | 27 | 146 | 72 | 59 | 356 |
| Military | Military | 7505 | 4283 |  |  |  |  |  |  |  |  |  | 1 | 1 |  |  |  | 21 | 1 |  | 27 | 146 | 72 | 59 | 356 |
| Military | Military | 7632 | 8141 |  |  |  |  |  |  |  |  |  | 1 | 1 |  |  |  | 21 | 1 |  | 27 | 146 | 72 | 59 | 356 |
| Farm | Farm | 3032 | 5497 |  | 1 |  | 1 |  |  |  |  | 1 |  |  |  |  |  | 3 | 15 | 17 | 3 | 1 |  | 19 | 85 |
| Farm | Farm | 4961 | 9994 |  | 1 |  | 1 |  |  |  |  | 1 |  |  |  |  |  | 3 | 15 | 17 | 3 | 1 |  | 19 | 85 |
| Farm | Farm | 5134 | 7044 |  | 1 |  | 1 |  |  |  |  | 1 |  |  |  |  |  | 3 | 15 | 17 | 3 | 1 |  | 19 | 85 |
| Farm | Farm | 6622 | 5892 |  | 1 |  | 1 |  |  |  |  | 1 |  |  |  |  |  | 3 | 15 | 17 | 3 | 1 |  | 19 | 85 |
| Peters Pass | PetersPass | 8021 | 4217 |  | 2 |  |  |  |  |  |  |  |  |  |  |  |  |  | 5 | 1 | 1 | 53 |  |  | 62 |
| Landfill | Landfill | 5330 | 10904 |  |  |  |  |  |  |  |  |  |  |  |  |  |  | 1 |  |  | 1 |  |  | 51 | 53 |
| Industrial | Industrial | 4538 | 6844 |  | 2 |  |  |  |  |  |  |  | 1 |  |  |  |  | 7 | 11 | 4 | 13 |  |  |  | 50 |
| Industrial | Industrial | 5043 | 10868 |  | 2 |  |  |  |  |  |  |  | 1 |  |  |  |  | 7 | 11 | 4 | 13 |  |  |  | 50 |
| Industrial | Industrial | 6464 | 6498 |  | 2 |  |  |  |  |  |  |  | 1 |  |  |  |  | 7 | 11 | 4 | 13 |  |  |  | 50 |
| Power Plant | PowerPlant | 5834 | 9786 |  |  | 1 |  |  |  |  |  |  |  |  |  |  |  | 2 | 1 |  | 5 | 1 | 11 | 10 | 45 |
| Sawmill | Sawmill | 3069 | 5195 |  | 2 |  | 1 |  |  |  |  |  |  |  |  |  |  | 3 | 12 | 2 | 4 |  |  | 10 | 38 |
| Sawmill | Sawmill | 7272 | 2541 |  | 2 |  | 1 |  |  |  |  |  |  |  |  |  |  | 3 | 12 | 2 | 4 |  |  | 10 | 38 |
| Factory | Factory | 4537 | 10561 |  | 1 |  |  |  |  |  |  |  |  |  |  |  |  | 6 | 9 | 1 | 4 | 1 |  |  | 30 |
| Airport | Airport | 4901 | 11787 |  |  |  |  |  |  |  |  | 1 |  |  |  |  |  | 2 |  |  | 6 | 1 | 2 |  | 16 |
| Briars | Briars | 1797 | 5096 |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | 1 |  | 1 |  | 1 |  | 5 |
| Pennants Pass | PennantsPass | 8331 | 2415 |  |  |  |  |  |  |  |  |  |  |  |  |  | 1 |  |  |  |  | 1 | 1 |  | 4 |
| Huntsmans Heath | HuntsmansHeath | 2714 | 4397 |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | 2 |
| Ramtop Meadows | RamtopMeadows | 5626 | 7804 |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | 0 |

### castles

| place | key | x | z | townhouse | house | villa | farmh. | shop | pub | church | chapel | kiosk | fuel | govt | police | fire | hotel | garage | shed | farm | indust. | military | port | ruin | total |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Montfort Castle | MontfortCastle | 9315 | 1145 |  |  |  |  |  |  |  |  | 1 |  |  |  |  |  |  | 1 |  |  |  |  |  | 67 |

### Natural places (named, no buildings counted)

Map type: 63 hill, 64 headland or location, 65 island, 66 pond or stream, 67 lake, 68 bay, 69 strait or gulf, 70 forest or ridge, 71 valley.

| place | map type | x | z |
|---|---:|---:|---:|
| Gibbet Cape | 64 | 1089 | 6369 |
| Blackrock Bay | 68 | 1450 | 6702 |
| Pins | 65 | 1165 | 11481 |
| Long Hill | 63 | 2254 | 5558 |
| Humboldt Hill | 63 | 2453 | 7188 |
| Driftwood Sands | 64 | 2303 | 7711 |
| Lechucs Shelf | 68 | 2943 | 1031 |
| Thollevast | 64 | 3057 | 2131 |
| Saules | 65 | 2919 | 1932 |
| Halcyon Strait | 69 | 2807 | 3256 |
| Gillaumes Dip | 66 | 2952 | 6457 |
| Meadow Stream | 66 | 2771 | 6886 |
| Spaniards Bay | 68 | 3015 | 7920 |
| Estrapade | 65 | 3397 | 1541 |
| New Wood | 70 | 3408 | 6204 |
| Wolfstone | 64 | 3793 | 4180 |
| Wolf Hill | 63 | 3854 | 4344 |
| Old Wood | 70 | 3658 | 4328 |
| Shepherds Pond | 66 | 3544 | 4818 |
| Calvary Hill | 63 | 3507 | 5816 |
| Tillers Find | 64 | 3597 | 6971 |
| Crabapple Bay | 68 | 4002 | 9619 |
| Charlet Bay | 68 | 4005 | 10905 |
| Birchwood Bay | 68 | 4290 | 11459 |
| Morton Valley | 71 | 4536 | 4658 |
| Morton Stream | 66 | 4524 | 4415 |
| Moonstone Pond | 66 | 4608 | 5430 |
| Pinewood Lake | 67 | 4344 | 6042 |
| Harbour | 64 | 4307 | 9526 |
| Coalmans Brow | 64 | 4317 | 10281 |
| Philips Creek | 66 | 4652 | 10172 |
| Spring Pond | 66 | 4671 | 10504 |
| Juniper Point | 64 | 4336 | 12136 |
| Southern Gulf | 69 | 5047 | 2736 |
| Greenwater | 66 | 4888 | 5569 |
| Raccoon Rock | 64 | 5081 | 7732 |
| Highstone | 63 | 5014 | 8475 |
| Tyrone Ridge | 70 | 4931 | 8759 |
| Hedgehog Hill | 63 | 5030 | 9477 |
| Gallows Hill | 63 | 4982 | 10370 |
| Morton Bay | 68 | 5214 | 3397 |
| Bonfire Hill | 63 | 5356 | 4599 |
| Cowbell Pond | 66 | 5329 | 6409 |
| Hornbeam Valley | 71 | 5267 | 7527 |
| Old Man Pond | 66 | 5119 | 11343 |
| Northern Shoals | 68 | 5423 | 12025 |
| Helmsmans Sip | 64 | 5160 | 12407 |
| Alder Pond | 66 | 5882 | 5714 |
| Upper Fields | 64 | 5705 | 5626 |
| Poachers Hill | 63 | 5825 | 7346 |
| Lacans Head | 63 | 5603 | 9150 |
| Seagull Point | 64 | 5969 | 3377 |
| Simons Wood | 64 | 5992 | 5198 |
| East Rise | 70 | 6221 | 7286 |
| Aluette Pond | 66 | 6103 | 7698 |
| Tyrone Bay | 68 | 6093 | 9121 |
| Maidens Bay | 68 | 5967 | 10836 |
| Stubwood Point | 64 | 6314 | 1966 |
| Goat Bay | 68 | 6295 | 3957 |
| Reed Pond | 66 | 6536 | 5577 |
| Rushes | 66 | 6466 | 8043 |
| Periwinkle Pond | 66 | 6719 | 6793 |
| Gillnet Pond | 66 | 7078 | 8169 |
| Andres Beacon | 64 | 7074 | 8392 |
| Boulder Cape | 64 | 6709 | 9661 |
| Six Bells | 63 | 7266 | 5637 |
| Scythe | 64 | 7487 | 9460 |
| Martins Watch | 64 | 7891 | 1839 |
| Regina Stream | 66 | 7541 | 2045 |
| Western Heights | 70 | 7744 | 3341 |
| Fishermans Bay | 69 | 7867 | 6764 |
| Schooners End | 64 | 7891 | 8178 |
| La Chalette | 64 | 8248 | 2816 |
| Marys Mount | 63 | 8087 | 2752 |
| Courbet | 64 | 8419 | 792 |
| Castle Ridge | 70 | 8575 | 1838 |
| Prow Peak | 63 | 8499 | 2303 |
| Green Valley | 71 | 8642 | 3545 |
| La Roue | 64 | 8956 | 558 |
| Swell Mountain | 63 | 8725 | 1750 |
| Buhrstone Creek | 66 | 9069 | 2261 |
| Mill Pond | 66 | 8858 | 2790 |
| Spruce Hill | 63 | 8736 | 4299 |
| Black Lake | 66 | 9482 | 3048 |
| Margarets Mount | 63 | 9467 | 3907 |
| Beacon Ridge | 70 | 9375 | 4436 |
| Perelle Bay | 68 | 9336 | 5490 |
| Shallows | 68 | 9867 | 5726 |
| Pick Mountain | 63 | 9953 | 2297 |
| Bell Pit Lake | 66 | 10212 | 2590 |
| Cheval Gin Lodge | 64 | 9923 | 2585 |
| Rockweed Cape | 64 | 10410 | 343 |
| Fleet Bay | 68 | 10366 | 1154 |
| Pick Creek | 66 | 10403 | 2048 |
| Cascades | 64 | 10386 | 2956 |
| Whitewater | 66 | 10943 | 1988 |
| Ore Ridge | 70 | 10741 | 2515 |
| Smugglers Hole | 64 | 10743 | 2790 |
| Argent | 65 | 10921 | 8668 |
| Anchor Bay | 68 | 11415 | 2599 |
| Erquy Island | 65 | 11229 | 11451 |
| Crag Point | 64 | 11719 | 1394 |
| Corsairs Cove | 68 | 11722 | 1716 |
| Anchor Peninsula | 64 | 11940 | 1880 |
