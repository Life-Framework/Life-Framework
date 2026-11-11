# Core and pack inventory

Every folder and file group in `addons/LifeFramework/`, classified with the red
line in [`core-vs-pack.md`](core-vs-pack.md). Taken on 2026-11-08 from `main`
at `b0f5b8e`. **Nothing has moved.** The move is an M3 freeze under epic #223,
after the maintainer signs off on M1: load the pack (#322), move in batches
(#323), split the string table (#325), prove the core boots alone (#326), and
load a pre-move save (#327).

- **core:** stays in `addons/LifeFramework/`.
- **pack:** moves to `addons/LF_Everon1989/` as is, `.meta` included.
- **split:** two things in one file or group. The core keeps a neutral part,
  the pack gets the flagship part. "How a split works" in the rule doc has the
  seams.

## Headline

| Class | Rows | Files (excluding `.meta`) |
| --- | --- | --- |
| core | 89 | 932 |
| pack | 6 | 16 |
| split | 22 | 71 |

Files are the 1,019 tracked files under `addons/LifeFramework/`, `.meta` files
not counted.

Most of the tree is core already: the systems, the tests, the UI kit, base
prefabs, and DebugWorld's terrain. The flagship is concentrated in three places
that have to be split, not moved: the string table, the value `.conf` files,
and the DebugWorld village, which the tests depend on.

## What blocks the move

1. **21 test files find 23 named entities in the village.** `Roadmap_Police_Desk`,
   `Roadmap_Bank_Vault`, `Roadmap_Vehicle_S105`, `Sign_SafeZone_Spawn` and others
   live in the `Street_*`, `Signs`, and chain layers, not in the annex. Moving
   the village takes those tests with it, which breaks test 1 (delete the pack,
   every test still passes). Each fixture needs a neutral twin in the annex
   first.
2. **Every config loads one hard-coded core path.** `LF_JobsConfig`,
   `LF_PoliceConfig`, `LF_SignCatalog`, `LF_GarageConfig`, `LF_SurvivalConfig`,
   `LF_RespawnConfig`, and `LF_ResourceChainCatalog` each hold a
   `static const ResourceName CONFIG_PATH = "{GUID}Configs/..."`, and
   `LF_LicensePlateManagerComponent` loads `VehicleSettings.conf` by literal. A pack
   has no way to override a value until the config layer
   (`m3-outline.md` section 2) gives them one lookup.
3. **Shop prices match by path.** `ShopPrices.conf` rules use substrings such as
   `Prefabs/Items/Roleplay/` and `Prefabs/Items/Seeds/`. A prefab that moves to
   the pack keeps its GUID but changes its path, so its price silently falls
   back to the default. Rules must match the moved paths, or match by GUID.
4. **The vehicle catalogue is script.** `LF_VehicleCatalog.c` names five vanilla
   cars and their five locked `LF_` variants as constants. The catalogue is a
   pack choice, so it needs a config list before the variants can move.
5. **The 1989 look is script.** `LF_UITheme` ("the one 1989 sheet") and
   `LF_UISkin` (paper forms, a green-phosphor ATM) hold the period palette as
   constants. Another setting cannot reskin the UI without a skin config.
6. **The playtest mission is the test world.** `LF_PlaytestTown.conf` (the
   flagship header) and `EL_DebugTest.conf` (the suite) both load
   `DebugWorld.ent`. The pack needs its own world that holds the village; the
   core keeps DebugWorld with the annex.

Smaller surprises: `Npc_Default_01`, the base of the duty NPC, both quest
givers, and the NPCs placed in the chain layers, wears the Soviet M88 uniform; `PoliceDutyNPC.et` has the name "Police Sergeant"
as a literal, not a string key; all three mission headers share the
`EveronLife_TitleImage` art; and several GUIDs were typed by hand in a pattern
(`E1A105BE1A000001` for `EL_S105_beige.et`, `A11CE00100000001` for the
resource-chain nodes). They are unique today, but golden rule 2 says GUIDs come
from the tool.

## Scripts (`Scripts/Game/`)

Script is core by the rule. The values and strings it hard-codes are listed
under "Hard-coded flagship values in script".

| Path | Class | Reason |
| --- | --- | --- |
| `Core/` | core | Game mode, NPC base, debug |
| `Components/` | core | Building, farming, inventory, plates |
| `Configs/` | core | Config classes |
| `Persistence/` | core | The save layer |
| `HUD/`, `UI/` | core | Generic HUD, chat panel, toasts |
| `Vehicle/`, `World/` | core | System glue |
| `Tests/` (161 files) | core | The suite |
| `Feature/ATM` | core | System |
| `Feature/Account` | core | System |
| `Feature/AgentHarness` | core | Tooling |
| `Feature/Banking` | core | Docs only |
| `Feature/Character` | core | Spawning, hand carry, inventory |
| `Feature/CharacterCreation` | core | System; the form title is a pack string |
| `Feature/Chat` | core | System |
| `Feature/Crime` | core | System; payouts move to config |
| `Feature/Debug`, `Feature/DebugWorld` | core | Test fixtures |
| `Feature/Farming` | core | Farm plots: plant, grow, harvest, save (#614) |
| `Feature/Film`, `Feature/ScreenshotCapture` | core | Tooling |
| `Feature/Gathering`, `Feature/Mining`, `Feature/Processing`, `Feature/Resources` | core | Systems; drop tables are config |
| `Feature/Hideouts` | core | System |
| `Feature/Houses` | core | System |
| `Feature/Interaction` | core | System |
| `Feature/Jobs` | core | System; pay and the taxi car move to config |
| `Feature/Level`, `Feature/License` | core | Progression systems |
| `Feature/LicensePlate` | core | System; the plate format is a generator class |
| `Feature/Money`, `Feature/Quantity` | core | System |
| `Feature/Notifications` | core | System |
| `Feature/Organizations` | core | System; prices move to config |
| `Feature/Police` | core | System and rank mechanics; caps move to config |
| `Feature/Quests` | core | Quest engine; quest content is prefab data |
| `Feature/Shop`, `Feature/Trader` | core | System |
| `Feature/Signs` | core | Sign system; titles are pack data |
| `Feature/SirenLights` | core | System |
| `Feature/Survival` | core | System |
| `Feature/Toys` | core | Jump pad, spike strip |
| `Feature/UIKit` | split | The kit is core; `LF_UITheme` and `LF_UISkin` hard-code the 1989 look |
| `Feature/VehicleLock` | core | System |
| `Feature/Vehicles` | split | Lot, garage, and impound are core; `LF_VehicleCatalog` is the flagship catalogue |
| `Feature/Whitelist` | core | System; faction names come from `LF_EFactionType` (see below) |

## Prefabs

| Path | Class | Reason |
| --- | --- | --- |
| `Building/` | core | Building preview |
| `Characters/Core/` | core | Player and NPC bases |
| `Characters/Presets/` (3) | core | Neutral spawn bodies the game mode lists |
| `Characters/Npcs/Npc_Default_01.et` | split | Base of every NPC, dressed in the M88 uniform; core needs a neutral base |
| `Characters/Npcs/PoliceDutyNPC.et` | split | Desk NPC slot is core; "Police Sergeant" and the uniform are pack |
| `Characters/Quests/QuestGiver_Start.et` | core | Test quests |
| `Characters/Quests/QuestGiver_Clerk.et` | split | Carries the arrival story; `LF_Test_ArrivalChain` needs a neutral example |
| `Crafting/` | core | Station prefabs a system depends on |
| `Debug/` | core | Test fixtures |
| `Items/Core/` | core | Item bases |
| `Items/Currencies/`, `Items/Roleplay/` | core | Cash, ID card, ticket, slip, keys |
| `Items/Contraband/`, `Items/HouseKeyItem.et` | core | System items |
| `Items/Food/`, `Items/Drinks/`, `Items/Seeds/` | core | Gather and survival items; neutral produce |
| `Items/Resources/` | core | Chain items |
| `MP/` | core | Game mode, chat, spawn point |
| `Particles/`, `VFX/` | core | Gather effects |
| `Police/` | core | Duty desk, jail cell, alarms, wanted board |
| `Processing/`, `Processor/` | core | Example chains and processors |
| `Props/Commercial/`, `Props/Construction/`, `Props/Hideouts/` | core | Interactables |
| `Props/Debug/` | core | Playtest-only stands (bank robbery kit, #605), gated by the debug flag |
| `Props/Signs/LF_Sign*.et` (10) | core | Generic boards and faces |
| `Props/Signs/EL_WhitelistSign.et` | core | Action sign |
| `Props/Signs/EL_MiningSign.et` | pack | Soviet undermined-area warning sign |
| `Resources/` | core | Ore, trees, logs |
| `Structures/` | core | Doors, jail door, bank, concrete plant |
| `Tools/` | core | Axe, pickaxe |
| `Trader/` | core | Trader bases |
| `Vegetation/` | core | Crops and fruit trees |
| `Vehicles/Core/`, `Vehicles/Test/` | core | Vehicle base and test fixtures |
| `Vehicles/LicensePlate/`, `Vehicles/SirenLights/` | core | System parts |
| `Vehicles/Wheeled/{S105,S1203,UAZ452,UAZ469,M923A1}/LF_*.et` | split | Locked variants: the catalogue and liveries; core keeps one example car |
| `Vehicles/Wheeled/M151A2/` | split | The police car; core needs a police-car slot, the model is a catalogue pick |

## Configs

| Path | Class | Reason |
| --- | --- | --- |
| `Chat/` | core | Channels |
| `Core/EditorManagerCore.conf` | core | Engine setup |
| `Inventory/` | core | Navigation bar |
| `NameTags/` | core | Tag rules |
| `System/` | core | Inputs, menus, key binding menu |
| `Systems/` | core | Systems and persistence configs |
| `Spawning/LF_Respawn.conf` | core | Neutral timers |
| `Survival/LF_Survival.conf` | core | Neutral rates |
| `Vehicles/VehicleSettings.conf` | core | Plate generator choice |
| `Jobs/LF_Jobs.conf` | split | Holds the flagship duty pay and benefit (decision 2026-11-08) |
| `Police/LF_Police.conf` | split | Holds the flagship ticket deadlines and caps |
| `Shop/ShopPrices.conf` | split | Flagship prices; path-matched rules |
| `Resources/ResourceChains.conf` | split | Chain shape is core; yields and prices are pack |
| `Signs/LF_Signs.conf` | split | Sign defs tests use; the titles are the village's |
| `Vehicles/LF_Garage.conf` | split | Flagship impound fee |

## Language

| Path | Class | Reason |
| --- | --- | --- |
| `everonlife_localization.st` (862 keys) | split | Functional keys stay; about 138 `LF-Sign_*` titles and texts, the arrival and intro quest copy, and the Everon-named forms move to a pack table |
| `everonlife_localization.*.conf` (7) | split | Generated from the `.st`; regenerated per table |

The flavour keys that name the setting today: `LF-Sign_TownMap_*` ("Welcome to
Everon"), the registration form ("Civil Registry of Everon, Form R-89"), the ID
card ("Republic of Everon, Identity Card"), the police station line ("the
Everon police"), the org ledger ("Organization Register - 1989 ledger"), the
arrival quest's welcome text, and `EL-Keybinds_Title` ("Everon Life").

## Missions

| Path | Class | Reason |
| --- | --- | --- |
| `EL_DebugTest.conf` | core | The suite's header |
| `LF_PlaytestTown.conf` | pack | The flagship playtest header |
| `LF_PlaytestTown_Preview.{edds,jpg}` | pack | The Workshop scenario picture of the Everon town (2026-11-10); moves with `LF_PlaytestTown.conf` |
| `EveronLifeGameMode.conf` | pack | The MainWorld header |
| `EveronLife_TitleImage.{edds,jpg}` | split | Every header uses it; the core needs neutral art |

## Worlds

| Path | Class | Reason |
| --- | --- | --- |
| `DebugWorld/DebugWorld.ent`, `DebugTerrain/` | core | The flat test terrain |
| Annex layers: `Annex`, `Crimes`, `DebugWorldActions`, `HousesKeys`, `Jobs`, `NPCs`, `PRFeatures`, `Police`, `Tests`, `WhitelistChat` | core | Every top-level entity sits inside the annex fence |
| `Factions`, `Shops` | core | Empty layers |
| `Quests` | split | One giver in the annex, the clerk on the square |
| `Village_Buildings`, `Village_Dressing`, `Village_Roads` | pack | Vanilla buildings, props, and roads; no test names them |
| `Street_*` (12) | split | Village sites that hold test-named fixtures |
| `Signs` | split | Village signs; `LF_Test_SafeZone` finds two |
| Chain layers: `AppleChain`, `CementChain`, `CrimeChain`, `FarmingChain`, `GravelChain`, `LoggingChain`, `MiningChain`, `PlumChain`, `SandChain`, `TraderChain` | split | On the village outskirts; tests find some of their nodes |
| `ATM`, `Gamemode`, `Roadmap`, `Survival`, `default` | split | Village placement of entities tests use; the game mode entity is core |
| `MainWorld/` | pack | The Everon town, St Philippe apple farm |

## Assets

| Path | Class | Reason |
| --- | --- | --- |
| `Crafting/` | core | Station meshes |
| `Items/` | core | Cash, ID card, food, roleplay meshes |
| `Props/CementMound/`, `Props/Military/Sandbags/` | core | Chain item textures |
| `Props/GreenScreen/` | core | Filming tooling |
| `Props/TitlePoster/` | pack | Everon title poster |
| `Resources/` | core | Ore and wood meshes |
| `Signs/` | core | Sign face material |
| `Tools/` | core | Axe, pickaxe |
| `UI/` | core | Stats bars, chat icons |
| `UI/Illustrations/medic_flow.*` | pack | The hospital's CALL, TREAT, BED diagram; the `tools\cli ui-image` route that made it is core (#579) |
| `Vehicles/Attachments/`, `Vehicles/EmergencyLightsKnob/`, `Vehicles/LicensePlate/` | core | Siren and plate parts |
| `Vehicles/Wheeled/M151A2/` | split | Light-bar mount for the flagship police car |
| `World/` | core | Terrain surface |

## Other folders

| Path | Class | Reason |
| --- | --- | --- |
| `Anims/` | core | Generic carry and NPC graphs |
| `Particles/` | core | Gather effects |
| `Sounds/` | core | Siren |
| `UI/Layouts/` | core | Generic kit and screens; colours come from `LF_UITheme` |
| `LifeFramework.gproj`, `thumbnail.png` | core | Project |

## Hard-coded flagship values in script

Items to move to config, or to list as deliberately fixed, before the pack can
set them. File and constant, then the value.

| File | Constant or literal | Value | Moves to |
| --- | --- | --- | --- |
| `Feature/Jobs/LF_CivilianJob.c` | `PAY_PARCEL`, `PAY_FUEL`, `PAY_TAXI` | 40, 60, 80 | `LF_Jobs.conf` |
| `Feature/Jobs/LF_CivilianJob.c` | `TAXI_PREFAB` | vanilla `S105_beige.et` | jobs config (catalogue pick) |
| `Feature/Jobs/LF_CivilianJob.c` | `DEPOT_PREFAB` | vanilla post office counter | jobs config |
| `Feature/Jobs/LF_JobGuards.c` | `FruitPrefab()` | apple, tomato, plum, rowan berries | jobs config |
| `Feature/Crime/LF_Contraband.c` | `PAYOUT` | 200 | crime config |
| `Feature/Crime/LF_Hospital.c` | `DEFAULT_PAYOUT`, `DEFAULT_COOLDOWN` | 200, 60 s | crime config |
| `Feature/Organizations/LF_Organization.c` | `PRICE_EXTRA_STORAGE`, `PRICE_PROCESSING_SLOT`, `PRICE_PARKED_VEHICLE` | 500, 250, 750 | org config |
| `Feature/Organizations/LF_OrgRules.c` | `FOUNDING_FEE` | 250 | org config |
| `Feature/Police/LF_PoliceRanks.c` | `PATROL_FINE_CAP`, `DUTY_SECONDS` | 1000, 3600 s | `LF_Police.conf` (it already has a cap default) |
| `Feature/Police/LF_PoliceMenu.c` | `MAX_FINE` | 5000 | `LF_Police.conf` |
| `Feature/Police/LF_FineTicket.c` | `MAX_OPEN_PER_HOLDER` | 3 | `LF_Police.conf` |
| `Feature/Vehicles/LF_VehicleCatalog.c` | five cars and five `LF_` variants | S105, UAZ-469, S1203, UAZ-452, M923A1 | vehicles config |
| `Feature/Vehicles/LF_Garage.c` | `FALLBACK_FEE` | 250 | `LF_Garage.conf` (fallback only) |
| `Feature/Mining/LF_MiningForge.c` | `IRON_BAR`, `IRON_SECONDS` | iron bar, 5 s | resource chains |
| `Feature/UIKit/LF_UITheme.c`, `LF_UISkin.c` | palette, paper and phosphor skins | the 1989 look | a skin config |
| `Feature/Police/LF_WantedReports.c` | `GridPlace()` | "grid X-Z", described as how a 1989 map reads | fixed: neutral grid text |
| `Feature/Whitelist/LF_WhitelistManager.c` | `LF_EFactionType` | `CIVILIAN`, `POLICE`, `MILITARY`, `MAFIA` | fixed enum; display names are pack strings |
| `Prefabs/Characters/Npcs/PoliceDutyNPC.et` | `Name` | "Police Sergeant" (literal) | a pack string key |

Script holds no place names. Every user-facing string in `Feature/` is already a
`#` key; the flavour sits in the string table.
