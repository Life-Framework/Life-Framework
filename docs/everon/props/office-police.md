# Office, police, government and bank props (vanilla Reforger)

Generated 2026-11-16, factual, from `toolsi measure`, `game_read` and the Everon world dumps in `tmp/everon/raw/`. Data file: `office-police.json` (GUIDs, surfaces, variants, notes).

Size is W x D x H in metres (collider bounds). Count is vanilla Everon instances of that exact prefab. Towns: SP Saint Philippe, SPi Saint Pierre, Mo Morton, Mon Montignac (450 m dumps). Action: yes means a user action was found in the resolved prefab chain. No inventory storage component was found on any listed prefab; the destructible flag per prop is in the JSON.

## Desks and tables

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Office desk (standard) | `Prefabs/Props/Furniture/Desk_01_Base.et` | 1.7 x 0.75 x 0.91 | 0.91, 0.23 | 484 | SP SPi Mo Mon | no |
| Office desk (long) | `Prefabs/Props/Furniture/Desk_01_long.et` | 1.98 x 0.75 x 0.91 | 0.91, 0.23 | 332 | SP SPi Mo Mon | no |
| Office desk (short) | `Prefabs/Props/Furniture/Desk_01_short.et` | 1.7 x 0.75 x 0.91 | 0.91, 0.23 | 224 | SP SPi Mo Mon | no |
| School desk (two-surface) | `Prefabs/Props/Furniture/DeskSchool_01.et` | 1.54 x 0.59 x 0.9 | 0.9, 0.89, 0.75, 0.74 | 68 | SP SPi Mo Mon | no |
| Meeting table | `Prefabs/Props/Furniture/MeetingTable_01.et` | 2.3 x 1 x 0.91 | 0.91 | 13 | SP SPi Mo Mon | no |
| Plain table | `Prefabs/Props/Furniture/Table_01.et` | 1.44 x 1.06 x 0.9 | 0.9 | 544 | SP SPi Mo Mon | no |
| Old table (white) | `Prefabs/Props/Furniture/TableOld_01/TableOld_01_white.et` | 1.71 x 0.91 x 0.9 | 0.9, 0.88 | 114 | SP SPi Mo Mon | no |
| Small side table | `Prefabs/Props/Furniture/TableSmall_01/TableSmall_01_DarkWood.et` | 0.69 x 0.7 x 0.72 | 0.7, 0.49 | 165 | SP SPi Mo Mon | no |
| Recreation table (square) | `Prefabs/Props/Furniture/TableRecreation_01/TableRecreation_01.et` | 1.21 x 1.21 x 0.9 | 0.9, 0.86, 0.19 | 42 | SP SPi Mo Mon | no |

## Seating

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Office swivel chair | `Prefabs/Props/Furniture/ChairOffice_01.et` | 0.7 x 0.7 x 0.96 | - | 259 | SP SPi Mo Mon | no |
| Chair 02 (wood, green/red) | `Prefabs/Props/Furniture/Chair_02/Chair_02_Base.et` | 0.47 x 0.58 x 1 | - | 304 | SP SPi Mo Mon | no |
| Chair 01 (white) | `Prefabs/Props/Furniture/Chair_01/Chair_01_white.et` | 0.47 x 0.56 x 1.02 | - | 174 | SP SPi Mo Mon | no |
| Chair 03 (tan) | `Prefabs/Props/Furniture/Chair_03/Chair_03_tan.et` | 0.5 x 0.61 x 0.99 | - | 10 | SP SPi Mon | no |
| Old wooden chair | `Prefabs/Props/Furniture/ChairOld_01.et` | 0.48 x 0.56 x 1 | - | 477 | SP SPi Mo Mon | no |
| Office waiting bench | `Prefabs/Props/Furniture/BenchOffice_01/BenchOffice_01_Base.et` | 1.36 x 0.61 x 0.97 | - | 55 | SP SPi Mo Mon | no |
| Wooden bench | `Prefabs/Props/Furniture/BenchWooden_01.et` | 1.45 x 0.39 x 0.51 | - | 250 | SP SPi Mo Mon | no |
| Wooden bench (type 3) | `Prefabs/Props/Furniture/BenchWooden_03.et` | 1.51 x 0.43 x 0.5 | - | 165 | SP SPi Mo Mon | no |
| Sofa | `Prefabs/Props/Furniture/Sofa_01.et` | 2.9 x 1.03 x 0.85 | - | 209 | SP SPi Mo Mon | no |
| Armchair | `Prefabs/Props/Furniture/Armchair_02.et` | 1.19 x 1.06 x 0.9 | - | 78 | SP SPi Mo Mon | no |

## Computers and typewriters

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Computer (screen on) | `Prefabs/Props/Civilian/Computer_E_01/Computer_E_01_on.et` | 0.44 x 0.42 x 0.12 | - | 0 | - | no |
| Computer (screen off) | `Prefabs/Props/Civilian/Computer_E_01/Computer_E_01_off.et` | 0.44 x 0.42 x 0.12 | - | 0 | - | no |
| Typewriter | `Prefabs/Props/Civilian/Typewriter_01/Typewriter_01_Base.et` | 0.34 x 0.33 x 0.14 | - | 102 | SP SPi Mo Mon | no |

## Phones and radios

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Rotary desk phone | `Prefabs/Props/Furniture/PhoneTable_01_Base.et` | 0.28 x 0.24 x 0.12 | - | 49 | SP SPi Mo Mon | no |
| Desk phone (type 2) | `Prefabs/Props/Furniture/PhoneTable_02.et` | 0.26 x 0.19 x 0.11 | - | 9 | SP SPi Mon | no |
| Wall phone (grey) | `Prefabs/Props/Services/PhoneWall_01/PhoneWall_01_grey.et` | 0.32 x 0.19 x 0.4 | - | 24 | SP SPi Mo Mon | no |
| Small radio | `Prefabs/Props/Civilian/RadioSmall_01_Base.et` | 0.35 x 0.08 x 0.22 | - | 77 | SP SPi Mo Mon | yes: SCR_TurnOnAction |

## Cabinets, cupboards, shelves

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Metal cabinet | `Prefabs/Props/Furniture/CabinetMetal_01_Base.et` | 0.75 x 0.51 x 1.98 | 1.98 | 256 | SP SPi Mo Mon | no |
| Wide metal cabinet | `Prefabs/Props/Furniture/CabinetMetal_02.et` | 1.5 x 0.61 x 2.07 | 2.07, 2.06 | 96 | SP SPi Mon | no |
| Card-file cabinet | `Prefabs/Props/Commercial/CabinetCardFile_01/CabinetCardFile_01_Base.et` | 0.47 x 0.72 x 1.44 | 1.44, 1.39, 1.09, 1.06, 0.76 | 184 | SP SPi Mo Mon | no |
| Cupboard F (narrow) | `Prefabs/Props/Furniture/Cupboard_01/Cupboard_01_F.et` | 0.76 x 0.75 x 2 | 2, 1.53, 0.76, 0.46, 0.13 | 560 | SP SPi Mo Mon | no |
| Cupboard D (wide, glass) | `Prefabs/Props/Furniture/Cupboard_01/Cupboard_01_D.et` | 1.5 x 0.75 x 2 | 2, 1.53, 1.27, 1.01, 0.76 | 415 | SP SPi Mo Mon | no |
| Bookshelf | `Prefabs/Props/Furniture/Bookshelf_01/Bookshelf_01.et` | 1.5 x 0.5 x 1.85 | 1.85, 1.5, 1.17, 0.85, 0.53 | 521 | SP SPi Mo Mon | no |

## Safes

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Safe | `Prefabs/Props/Furniture/Safe_01.et` | 1.01 x 0.65 x 1.8 | 1.8 | 21 | SP SPi Mo Mon | no |

## Boards and maps

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Cork noticeboard | `Prefabs/Props/Civilian/NoticeBoardCork_01.et` | 1.8 x 0.02 x 1.1 | - | 227 | SP SPi Mo Mon | no |
| Wall map | `Prefabs/Props/Civilian/WallMap_01.et` | 1.44 x 0.03 x 1.12 | - | 73 | SP SPi Mo Mon | no |
| Drafting board | `Prefabs/Props/Civilian/BoardDrafting_01.et` | 1.96 x 0.74 x 2.43 | - | 3 | SP | no |

## Paper and documents

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| A4 notice sheet | `Prefabs/Props/Commercial/Posters/Notice_A4_Horizontal_V1.et` | 0.21 x 0 x 0.3 | - | 14 | SP SPi | no |
| Paper stack | `Prefabs/Props/Civilian/FileOffice_01_Stack.et` | 0.26 x 0.29 x 0.31 | - | 422 | SP SPi Mo Mon | no |
| Open file folder | `Prefabs/Props/Civilian/FileOffice_01_Open.et` | 0.62 x 0.3 x 0.05 | - | 43 | SP SPi Mo Mon | no |
| Notebook stack | `Prefabs/Props/Civilian/Notebook_01_Stack.et` | 0.2 x 0.3 x 0.07 | - | 296 | SP SPi Mo Mon | no |
| Intelligence folder (item) | `Prefabs/Items/Misc/IntelligenceFolder_E_01/IntelligenceFolder_E_01.et` | 0.26 x 0.35 x 0.02 | - | 0 | - | yes: SCR_PickUpItemAction |

## Cells

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Prison cell door set | `Prefabs/Structures/BuildingParts/Doors/DoorSet_PrisonCell_L_EXT_B.et` | 1.45 x 0.08 x 2.48 | - | 4 | SP SPi Mon | yes: SCR_DoorUserAction |
| Window bars | `Prefabs/Structures/BuildingParts/Windows/Win_Bars_A/Win_Bars_A_130x182_B.et` | 1.52 x 0.15 x 2.06 | - | 20 | SP SPi Mon | no |
| Single bed | `Prefabs/Props/Furniture/Bed_02.et` | 1.1 x 2.38 x 0.85 | - | 82 | SP SPi Mo Mon | no |
| Bunk bed | `Prefabs/Props/Furniture/BunkBed_02/BunkBed_02.et` | 0.86 x 2.4 x 1.71 | - | 19 | - | no |

## Racks

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Shelf rack (long) | `Prefabs/Props/Furniture/Rack_01_Long.et` | 2.45 x 0.75 x 2.19 | 1.88, 1.4, 0.9, 0.43 | 370 | SP SPi Mo Mon | no |
| Coat rack | `Prefabs/Props/Furniture/CoatRack_01.et` | 0.56 x 0.57 x 1.97 | 1.91 | 524 | SP SPi Mo Mon | no |
| Weapon rack cabinet (camp) | `Prefabs/Props/Military/Camps/WeaponRackCabinet_01.et` | 2.23 x 0.43 x 1.16 | 1.16, 1.15, 0.93, 0.81, 0.45 | 0 | - | no |
| Arsenal weapon cabinet (FIA) | `Prefabs/Props/Military/Arsenal/WeaponRacks/FIA/WeaponRackCabinetArsenal_FIA.et` | 2.23 x 0.43 x 1.16 | 1.16, 1.15, 0.93, 0.81, 0.45 | 0 | - | yes: Arsenal display |

## Flags

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Flagpole (empty) | `Prefabs/Structures/Military/Flags/FlagPole_01/FlagPole_01_empty.et` | 0.16 x 0.29 x 6.8 | - | 13 | SP | no |
| Flagpole with flag (US) | `Prefabs/Structures/Military/Flags/FlagPole_02/FlagPole_02_V1_US.et` | 0.08 x 0.08 x 5.58 | - | 0 | - | no |
| Desk flag set | `Prefabs/Props/Military/DeskFlags_01/DeskFlags_01_base.et` | 0.09 x 0.07 x 0.31 | - | 0 | - | no |

## Counters, barriers, lamps, other

| Name | Path | Size | Surfaces | Count | Towns | Action |
|---|---|---|---|---|---|---|
| Table light (LightTable_01) | `Prefabs/Props/Furniture/LightTable_01.et` | 0.23 x 0.32 x 0.48 | - | 363 | SP SPi Mo Mon | no |
| Punch clock | `Prefabs/Props/Civilian/ClockPunch_01.et` | 0.64 x 0.36 x 0.45 | - | 3 | SP | no |
| Teller window counter | `Prefabs/Props/Services/Post/CounterPostOffice_01_window.et` | 1.51 x 0.48 x 1.86 | 0.91, 0.74, 0.06 | 12 | SP SPi Mo Mon | no |
| Shop counter | `Prefabs/Props/Commercial/CounterShop_01.et` | 1.5 x 1.12 x 1.04 | 1.04, 0.78, 0.7, 0.2 | 28 | SP SPi Mo Mon | no |
| Cash register | `Prefabs/Props/Commercial/CashierShop_01.et` | 0.46 x 0.51 x 0.5 | - | 30 | SP SPi Mo Mon | no |
| Barrier arm gate | `Prefabs/Structures/Infrastructure/Barriers/BarGate_01/BarGate_01.et` | 8.74 x 0.47 x 4.59 | - | 29 | SP Mon | no |
| Globe | `Prefabs/Props/Civilian/Globe_01.et` | 0.27 x 0.27 x 0.35 | - | 143 | SP SPi Mo Mon | no |
| Table clock | `Prefabs/Props/Furniture/ClockTable_01.et` | 0.59 x 0.23 x 0.31 | - | 88 | SP SPi Mo Mon | no |
| Interior floor lamp | `Prefabs/Props/Furniture/LampInterior_01.et` | 0.51 x 0.51 x 1.53 | - | 327 | SP SPi Mo Mon | no |
| Kerosene lamp | `Prefabs/Props/Civilian/LampKerosene_01/LampKerosene_01_base.et` | 0.22 x 0.22 x 0.35 | - | 8 | SP | yes: SCR_SwitchLightUserAction |
| Post-office box | `Prefabs/Props/Services/Post/PostOfficeBox_01.et` | 0.78 x 0.34 x 0.5 | - | 1 | - | no |

## How to fit out a police station, office or bank

1. Office: Desk_01 (0.91 m top; use `measure --surfaces` heights to place paper stacks, phone, typewriter or computer on it), ChairOffice_01, CabinetCardFile_01, Cupboard_01 and NoticeBoardCork_01 are the props vanilla towns already use, so they match the existing buildings.
2. Police station: vanilla ships exactly one cell prefab, the barred door set DoorSet_PrisonCell_L_EXT_B (one in each of Saint Philippe, Saint Pierre and Montignac) plus Win_Bars windows; add Bed_02 or BunkBed_02 and BenchWooden for the cell, Table_01 or MeetingTable_01 for interviews. There is no cage, turnstile, locker or wanted-board prop.
3. Guns and evidence: WeaponRackCabinet_01 is decorative with no action and no storage; the Arsenal variant adds an arsenal display and armory service point (faction driven), not a Life Framework inventory. Safe_01 is purely visual (no open, lock or storage), so any locking, loot or cash behaviour must be built in Life Framework.
4. Bank: no bank props exist. Use CounterPostOffice_01_window (teller window with Glass_CounterPostOffice panes; 2 counters per town), CounterShop_01, CashierShop_01 and Safe_01 behind it; BarGate_01 is not interactive (its ActionsManagerComponent is disabled).
5. Interactivity: the only user actions found are on RadioSmall_01 (turn on), the cell door (open/close), LampKerosene_01 (light switch) and IntelligenceFolder_E_01 (pick up); no chair has a sit action and no phone or computer has an action. Computer_E_01 is not placed anywhere on vanilla Everon. Flags exist only as FIA/US/USSR poles and a USSR desk flag.
