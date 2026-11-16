# Everon props: medical, emergency and services

Generated 2026-11-16 from the installed game (pak data), `toolsi measure` and the Everon world dumps in `tmp/everon/raw`. Machine-readable twin: `medical-services.json` (same rows, with GUIDs, `{GUID}path` refs, notes). Size is width x depth x height in metres from model bounds. Count is instances on vanilla Everon (world dump, non-structure entities plus the building parts listed). Towns are limited to the 450 m dumps (Saint Philippe, Saint Pierre, Morton, Montignac). Action column: `yes` means an ActionsManagerComponent action or a support-station component was found on the prefab or its inheritance chain; `-` means none.

## What is available (verified facts only)

- Clinic: bed, double and single medical cabinets, bin, scale and exam lamp exist in the world (inside each town's MunicipalOffice_E_01 corner); IV stand, surgical table and lamp, monitor, sterilizer, medical tables and a full dental set exist in the game data but are placed 0 times on Everon. All of them are mesh-only (no heal, sit or lie action, no storage); LF already reuses the bed model in LF_HospitalBed and LF_FirstSpawnBed.
- Ambulances: UAZ-452 ambulance (plus FIA paint) and the M997 Maxi ambulance both carry a heal support station and load/remove-casualty actions; there is no fire truck, police car, stretcher or wheelchair prefab in the vanilla data (searched by name).
- Fire station: only FireStation_E_01 (with its 8.96 m hose-hanger tower and siren part) plus extinguishers (350 on the map), extinguisher boxes and hydrant boxes (49); none has a use action.
- Church and school: pews, small and large altars, organ and piano (both have SCR_PlayInstrument), baptismal font, coffin; school desk, school chair, cork notice board, globe; chalkboard and flipchart exist but are unplaced. No pulpit, lectern, hotel reception desk or key rack was found; a shop counter and coat rack are the nearest stand-ins.
- Storage actions exist only on the medical equipment crate (open storage) and the medical arsenal crate (arsenal, resupply, loadout); both are vanilla systems and need LF routing before use on the server.

## Medical / clinic

| Prop | Path | Size m (w x d x h) | Everon count | Action |
|---|---|---|---|---|
| Bed, hospital | `Prefabs/Props/Services/Healthcare/BedMedical_01.et` | 1.16 x 0.66 x 2.5 | 55 | - |
| Medical waste bin | `Prefabs/Props/Services/Healthcare/BinMedical_01.et` | 0.41 x 0.52 x 0.47 | 20 | - |
| Medical cabinet (double) | `Prefabs/Props/Services/Healthcare/CabinetMedical_01.et` | 0.9 x 1.77 x 0.55 | 12 | - |
| Medical cabinet (single) | `Prefabs/Props/Services/Healthcare/CabinetMedical_01_single.et` | 0.6 x 1.77 x 0.55 | 16 | - |
| Medical exam lamp | `Prefabs/Props/Services/Healthcare/LampMedical_01.et` | 0.48 x 2.05 x 0.81 | 10 | - |
| Medical scale | `Prefabs/Props/Services/Healthcare/ScaleMedical_01.et` | 0.61 x 1.5 x 0.52 | 9 | - |
| IV / infusion stand | `Prefabs/Props/Medical/InfusionStand_01/InfusionStand_01.et` | 0.54 x 1.89 x 0.52 | 0 | - |
| Surgical lamp | `Prefabs/Props/Medical/LampSurgical_01.et` | 0.63 x 2.09 x 1.36 | 0 | - |
| Medical monitor screen | `Prefabs/Props/Medical/MedicalScreen_01.et` | 1.07 x 2.1 x 0.43 | 0 | - |
| Instrument sterilizer | `Prefabs/Props/Medical/SterilizerMedical_01/SterilizerMedical_01.et` | 0.75 x 0.52 x 0.63 | 0 | - |
| Medical table | `Prefabs/Props/Medical/TableMedical_01/TableMedical_01.et` | 1.14 x 1.25 x 0.58 | 0 | - |
| Medical table (small) | `Prefabs/Props/Medical/TableMedical_01/TableMedical_01_small.et` | 0.49 x 1.07 x 0.57 | 0 | - |
| Surgical table | `Prefabs/Props/Medical/TableSurgical_01/TableSurgical_01.et` | 1 x 1.01 x 2.04 | 0 | - |
| Autoclave drum on stand | `Prefabs/Props/Medical/ToolsSurgical_01_autoclavedrum_stand_V1.et` | 0.44 x 0.78 x 0.4 | 0 | - |
| First aid box (wall) | `Prefabs/Props/Furniture/FirstAidBox_01.et` | 0.39 x 0.52 x 0.17 | 386 | - |
| Dental chair | `Prefabs/Props/Furniture/ChairDental_01/ChairDental_01.et` | 0.62 x 1.18 x 1.6 | 0 | - |
| Dental lamp | `Prefabs/Props/Furniture/LampDental_01/LampDental_01.et` | 1.16 x 1.56 x 1.39 | 0 | - |
| Dental stool | `Prefabs/Props/Furniture/StoolDental_01/StoolDental_01.et` | 0.46 x 0.86 x 0.46 | 0 | - |
| Medical equipment box (US) | `Prefabs/Props/Military/AmmoBoxes/US/EquipmentBoxWooden_Medical_01_US.et` | 0.93 x 0.49 x 0.49 | 0 | yes |
| Medical arsenal box (US) | `Prefabs/Props/Military/Arsenal/AmmoBoxes/US/AmmoBoxArsenal_Medical_US.et` | 0.93 x 0.49 x 0.49 | 0 | yes |

## Fire service

| Prop | Path | Size m (w x d x h) | Everon count | Action |
|---|---|---|---|---|
| Fire extinguisher | `Prefabs/Props/Civilian/FireExtinguisher_01.et` | 0.31 x 0.66 x 0.21 | 350 | - |
| Fire extinguisher wall box | `Prefabs/Props/Services/Fire/FireExtinguisherBox_01.et` | 0.87 x 1.82 x 0.74 | 8 | - |
| Fire extinguisher box with shovel | `Prefabs/Props/Services/Fire/FireExtinguisherBox_01_Shovel.et` | 0.16 x 0.84 x 0.05 | 8 | - |
| Hydrant box | `Prefabs/Props/Services/Fire/HydrantBox_01.et` | 0.86 x 0.86 x 0.39 | 49 | - |
| Fire station hose hanger | `Prefabs/Structures/Services/Fire/FireStation_E_01/FireStation_E_01_hose_hanger.et` | 0.58 x 8.96 x 2.03 | 4 | - |
| Fire station siren | `Prefabs/Structures/BuildingParts/Eletrical/Firestation_siren_02.et` | 2.06 x 0.85 x 1.33 | 1 | - |

## School

| Prop | Path | Size m (w x d x h) | Everon count | Action |
|---|---|---|---|---|
| School desk | `Prefabs/Props/Furniture/DeskSchool_01.et` | 1.53 x 0.9 x 0.59 | 68 | - |
| School chair | `Prefabs/Props/Furniture/ChairSchool_01.et` | 0.48 x 1 x 0.57 | 67 | - |
| Chalkboard | `Prefabs/Props/Services/ChalkBoard_01/ChalkBoard_01.et` | 2.22 x 2.12 x 0.36 | 0 | - |
| Flipchart | `Prefabs/Props/Furniture/Flipchart_01/Flipchart_01.et` | 0.74 x 1.79 x 0.61 | 0 | - |
| Globe | `Prefabs/Props/Civilian/Globe_01.et` | 0.28 x 0.34 x 0.26 | 143 | - |
| Cork notice board | `Prefabs/Props/Civilian/NoticeBoardCork_01.et` | 1.8 x 1.1 x 0.02 | 227 | - |

## Church

| Prop | Path | Size m (w x d x h) | Everon count | Action |
|---|---|---|---|---|
| Church bench long | `Prefabs/Props/Furniture/BenchChurch_02/BenchChurch_02_long.et` | 4.81 x 1.06 x 0.7 | 18 | - |
| Church bench short | `Prefabs/Props/Furniture/BenchChurch_02/BenchChurch_02_short.et` | 2.13 x 1.06 x 0.7 | 36 | - |
| Church bench small | `Prefabs/Props/Furniture/BenchChurch_Small_01.et` | 2.27 x 1.06 x 0.69 | 60 | - |
| Candle holder | `Prefabs/Props/Civilian/CandleHolderDecorative_01/CandleHolderDecorative_01.et` | 0.15 x 0.37 x 0.15 | 0 | - |
| Church altar | `Prefabs/Structures/BuildingParts/Cultural/AltarChurch_01.et` | 8.3 x 8.03 x 1.3 | 3 | - |
| Church altar (small) | `Prefabs/Structures/BuildingParts/Cultural/AltarChurch_01_small.et` | 1.88 x 1.01 x 0.81 | 8 | - |
| Church organ | `Prefabs/Structures/BuildingParts/Cultural/OrganChurch_01.et` | 4.73 x 5.23 x 4.32 | 3 | yes |
| Baptismal font | `Prefabs/Structures/BuildingParts/Cultural/ChurchFont_01_decor.et` | 0.42 x 0.76 x 0.31 | 5 | - |
| Piano (brown) | `Prefabs/Props/Furniture/Piano_01_brown.et` | 2.13 x 1.53 x 0.91 | 113 | yes |
| Coffin | `Prefabs/Props/Civilian/Coffin_01.et` | 0.74 x 0.59 x 1.98 | 51 | - |

## Hotel / reception / lodging

| Prop | Path | Size m (w x d x h) | Everon count | Action |
|---|---|---|---|---|
| Suitcase | `Prefabs/Props/Civilian/Suitcase_01.et` | 0.71 x 0.48 x 0.25 | 569 | - |
| Shop counter | `Prefabs/Props/Commercial/CounterShop_01.et` | 1.5 x 1.04 x 1.12 | 28 | - |
| Coat rack | `Prefabs/Props/Furniture/CoatRack_01.et` | 0.56 x 1.97 x 0.57 | 524 | - |
| Single bed | `Prefabs/Props/Furniture/Bed_01.et` | 1.13 x 0.51 x 2.3 | 443 | - |
| Double bed | `Prefabs/Props/Furniture/BedDouble_01.et` | 2.26 x 0.9 x 2.41 | 267 | - |

## Other services

| Prop | Path | Size m (w x d x h) | Everon count | Action |
|---|---|---|---|---|
| Police control panel | `Prefabs/Props/Services/Police/PoliceControlPanel_01.et` | 1.53 x 1.13 x 0.82 | 4 | - |

## Emergency vehicles

| Prop | Path | Size m (w x d x h) | Everon count | Action |
|---|---|---|---|---|
| Ambulance UAZ-452 | `Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_ambulance.et` | 2.29 x 1.89 x 4.53 | 0 | yes |
| Ambulance UAZ-452 (FIA) | `Prefabs/Vehicles/Wheeled/UAZ452/UAZ452_ambulance_FIA.et` | 2.29 x 1.89 x 4.53 | 0 | yes |
| Ambulance M997 Maxi | `Prefabs/Vehicles/Wheeled/M998/M997_maxi_ambulance.et` | 2.22 x 1.57 x 4.8 | 0 | yes |

Notes per prop, GUIDs and towns are in `medical-services.json`. Gaps: no hospital or clinic building exists as a structure in the world dump (only the MunicipalOffice_E_01 corner); Flipchart_01 has no resolvable GUID from the pak data (guid null).
