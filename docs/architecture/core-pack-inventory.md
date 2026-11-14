# Core and pack inventory

Every folder and file group in `addons/LifeFramework/`, classified with the red
line in [`core-vs-pack.md`](core-vs-pack.md). Taken on 2026-11-08 from `main`
at `b0f5b8e`. **Batch 1 moved (#323, 2026-11-14):** the rows marked *moved* below now live in
`addons/LF_Everon1989/` at the same relative path and GUID, except the village layers, which sit in
the pack world `Worlds/Everon1989Town/Everon1989Town_Layers/` (a SubScene of the core DebugWorld,
`pack-move-spike.md` section 1). **Batch 2 moved (#323, 2026-11-14):** the supermarket prefabs and model. The other pack rows each have a core user, so they move with the batch that splits it: the bank vault kit is spawned by the core tests `LF_Test_SystemsEconomy` and `LF_Test_SystemsWorld` and placed by the core `Street_Bank.layer` (a core example vault first, with the layers); the clerk dialogue is listed by the core `LF_DialogueIndex.conf` (configs); the atmosphere sounds are placed by core layers and `LF_ChopBench.et` (layers); `medic_flow` is named by the core `LF_Signs.conf` (configs). The move is an M3 freeze under epic #223,
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
   first. **Done (#323 batch 0, 2026-11-12):** `Annex.layer` holds an `Annex_*`
   twin of every fixture a test finds by name, `LF_TestAnnex` names them, and
   `validate-annex-fixtures` keeps an empty baseline (it reads nested
   `$grp` and child entities too). **Done (#323 batch 0d, 2026-11-12):** core
   Scripts no longer quote a village entity name: convoy markers carry
   `m_bDrop`/`m_bNearDrop`, the test car lot is the lot flagged
   `m_bTestCarLot`, `LF_Jail.SyncPlacedCell` re-applies the placed
   `LF_JailCellComponent`, the cash van route names its stops and van on the
   placed box, and the harness finds the stash and garage through components.
   `validate-core-village-names` keeps that list empty. The hideout the desk
   leases and contraband starts from is the door with `m_bOrganizationHideout`
   (LF_Places `OrgHideout`); its id (`Roadmap_Hideout`) is a layer value and the saved key (#1055). `validate-map-independence` (#1046) now holds the
   rest of the map knowledge in core Scripts (find-by-name, world coordinates, place names)
   to a shrink-only baseline; the harness folder is exempt as dev tooling.
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
| `Feature/Toys` | core | Jump pad |
| `Feature/UIKit` | split | The kit is core; `LF_UITheme` and `LF_UISkin` hard-code the 1989 look |
| `Feature/VehicleLock` | core | System |
| `Feature/Vehicles` | split | Lot, garage, and impound are core; `LF_VehicleCatalog` is the flagship catalogue |
| `Feature/Whitelist` | core | System; faction names come from `LF_EFactionType` (see below) |

## Prefabs

| Path | Class | Reason |
| --- | --- | --- |
| `Building/` | core | Building preview |
| `Buildings/Supermarket/` | pack, moved (batch 2) | The 1989 Everon supermarket from `tools\cli building` (ArchBuilder, `tools/buildings/supermarket/spec.json`, which now sets `"addon": "LF_Everon1989"`): the building is this server's look. Its placement left the core `Annex.layer` (`Annex_Supermarket`) for the pack layer `Village_Buildings` as `Village_Supermarket`, same coordinates; `building-proof` defaults to the new name |
| `Characters/Core/` | core | Player and NPC bases |
| `Characters/Presets/` (3) | core | Neutral spawn bodies the game mode lists |
| `Characters/Npcs/Npc_Default_01.et` | split | Base of every NPC, dressed in the M88 uniform; core needs a neutral base |
| `Characters/Npcs/PoliceDutyNPC.et` | split | Desk NPC slot is core; "Police Sergeant" and the uniform are pack |
| `Characters/Quests/QuestGiver_Start.et` | core | Test quests |
| `Characters/Quests/QuestGiver_Clerk.et` | split | Carries the arrival story; `LF_Test_ArrivalChain` needs a neutral example |
| `Crafting/` | core | Station prefabs a system depends on |
| `Weapons/Handguns/Taser/` | split | The stun weapon `LF_Test_Stun` loads is the core's example; the police-issue taser and its strings are pack (#341) |
| `Comms/` | core | Payphone, station telephone, phone line and dispatch boards: interactables the Comms system depends on (#147). Which props are phones in the village is layer data |
| `Debug/` | core | Test fixtures |
| `Items/Core/` | core | Item bases |
| `Items/Currencies/`, `Items/Roleplay/` | core | Cash, ID card, ticket, slip, keys, handcuffs (the restraint's requirement, #664) |
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
| `Props/Signs/LF_BoardFace.et` | core | The 16:9 render-target face a board draws on (wanted posters, #758) |
| `Props/Signs/EL_WhitelistSign.et` | core | Action sign |
| `Props/Signs/EL_MiningSign.et` | pack, moved (batch 1) | Soviet undermined-area warning sign; its placement left the core `MiningChain.layer` for the pack layer `Village_Signs` |
| `Props/Roleplay/SpikeStrip/` | split | The deployed strip and the folded item are the core's working deployable that `LF_Test_SpikeStrip` loads (#646); the 1989 stinger look, its place in the rank kits and the DebugWorld display strip are pack (#647) |
| `Props/Roleplay/BankVault/` | pack | The 1989 bank vault kit: the round door in its frame and wall, the steel room, the deposit box wall, the cash trolley, and the assembled vault closed and open. The assembled vault carries the rob, breach and swing components and is the town bank vault (#925); `LF_BankVault_Light` is its two ceiling lights (each with a vanilla caged bulkhead fixture); `LF_BankVault_SmallRoof` is the door hood (the vanilla `SmallRoof_01` mesh); the vault room and front use a vanilla concrete material in place of the dark generated one. The gameplay components (`LF_BankVaultComponent`, `LF_BreachTargetComponent`) are core |
| `Assets/Props/Roleplay/BankVault/` | pack | Meshes and materials of the vault kit (door generated with Meshy, `docs/credits.md`; the rest built by `tools/assets/bank-vault/build.py`) |
| `Assets/Props/Roleplay/SpikeStrip/` | split | Mesh of the strip above (generated with Meshy, `docs/credits.md`): example in core, the police stinger in pack |
| `Resources/` | core | Ore, trees, logs |
| `Structures/` | core | Doors, jail door, bank, concrete plant |
| `Tools/` | core | Axe, pickaxe |
| `Trader/` | core | Trader bases |
| `Vegetation/` | core | Crops and fruit trees |
| `Vehicles/Core/`, `Vehicles/Test/` | core | Vehicle base and test fixtures |
| `Vehicles/LicensePlate/`, `Vehicles/SirenLights/` | core | System parts |
| `Vehicles/Wheeled/{S105,S1203,UAZ452,UAZ469,M923A1,V2101,T601}/LF_*.et` | split | Locked variants: the catalogue and liveries; core keeps one example car |
| `Vehicles/Wheeled/M151A2/` | split | The police car; core needs a police-car slot, the model is a catalogue pick |
| `Weapons/Handguns/Taser/` | split | Mesh of the stun weapon above: example in core, police-issue item in pack |
| `Configs/Vehicles/LF_VehiclePrices.conf` | split | The multiplier shape is core; the seven vehicle rows are the flagship catalogue's prices (#688, #981) |

## Configs

| Path | Class | Reason |
| --- | --- | --- |
| `Chat/` | core | Channels |
| `Core/EditorManagerCore.conf` | core | Engine setup |
| `UI/LF_UITheme.conf` | split | The theme shape (`LF_UIThemeRecord`, the reader) is core; the 1989 paper, brass and phosphor values are the flagship's look, kept as the core default until the M3 move. A pack overlay patches a theme by id (#966) |
| `Dialogue/LF_DialogueIndex.conf` | core | The dialogue registry (#957): id to file; a pack overlay adds, replaces or removes an id |
| `Dialogue/LF_Dialogue_ClerkEveron.conf` (in `LF_Everon1989`) | pack | The Everon clerk as a pack patch of the core clerk (`tools/assets/dialogue/clerk_everon/`); a text-only line, listed in the pack's mission header |
| `Dialogue/LF_Dialogue_Clerk.conf` | pack | The town clerk's conversation (generated from `tools/assets/dialogue/clerk/`); the scene system in `Feature/Dialogue/` is core |
| `Weapons/Ammo/` | core | Ammunition list override the engine needs for any mod projectile, and the stun weapon's magazine config |
| `Inventory/` | core | Navigation bar |
| `NameTags/` | core | Tag rules |
| `System/` | core | Inputs, menus, key binding menu |
| `Systems/` | core | Systems and persistence configs |
| `Spawning/LF_Respawn.conf` | core | Neutral timers |
| `Survival/LF_Survival.conf` | core | Neutral rates |
| `Vehicles/VehicleSettings.conf` | core | Plate generator choice |
| `Jobs/LF_Jobs.conf` | split | Holds the flagship duty pay and benefit (decision 2026-11-08) |
| `Police/LF_Police.conf` | split | Holds the flagship ticket deadlines and caps |
| `Police/LF_WantedPosters.conf` | core | Neutral budget: 4 posters, 30 m draw radius (#758) |
| `Shop/ShopPrices.conf` | split | Flagship prices; path-matched rules |
| `Resources/ResourceChains.conf` | split | Chain shape is core; yields and prices are pack |
| `Signs/LF_Signs.conf` | split | Sign defs tests use; the titles are the village's |
| `Vehicles/LF_Garage.conf` | split | Flagship impound fee |
| `Account/LF_Characters.conf` | split | Characters per account (#331): the core supports any number and defaults to 1; the flagship's one-life rule is the pack's value |

## Language

| Path | Class | Reason |
| --- | --- | --- |
| `everonlife_localization.st` (1468 keys after #325) | split, done | Functional and neutral keys stay. 195 per-sign `LF-Sign_*` rows and the Everon-named, arrival and clerk copy moved to `LF_Everon1989/Language/lf_everon1989_localization.st`; the core keeps a neutral row under the same key (the pack's text wins when it loads) |
| `everonlife_localization.*.conf` (7) | split | Generated from the `.st`; regenerated per table |

The flavour keys that name the setting today: `LF-Sign_TownMap_*` ("Welcome to
Everon"), the registration form ("Civil Registry of Everon, Form R-89"), the ID
card ("Republic of Everon, Identity Card"), the police station line ("the
Everon police"), the org ledger ("Organization Register - 1989 ledger"), the
arrival quest's welcome text, and `EL-Keybinds_Title` ("Everon Life").

The photo identity card (#902) is core: the registry, the card sheet, the UI kit `PORTRAIT`
part, the neutral registry stamp (`Assets/UI/Illustrations/LF_IdStamp.*`, "CIVIL REGISTRY,
OFFICIAL, R-89") and `Configs/Identity/LF_PhotoIdCard.conf` (fee and show time). One piece is
flagship: the card number prefix `EV ` (`LF_PhotoIdCard.NUMBER_PREFIX`), a split. Saved card
numbers carry the prefix, so the move makes it a pack value that a server keeps for the life of
its save.

## Missions

| Path | Class | Reason |
| --- | --- | --- |
| `EL_DebugTest.conf` | core | The suite's header |
| `LF_PlaytestTown.conf` | pack, moved (batch 1) | The flagship playtest header; its world is now the pack world. The core suite reads the neutral fixture `LF_HostedTest.conf` (core) instead |
| `LF_PlaytestTown_Preview.{edds,jpg}` | pack, moved (batch 1) | The Workshop scenario picture of the Everon town (2026-11-10); moves with `LF_PlaytestTown.conf` |
| `EveronLifeGameMode.conf` | pack, moved (batch 1) | The MainWorld header |
| `LF_HostedTest.conf` | core | Test fixture (#323 batch 1): a hosted header on DebugWorld with dev tools closed, read by `host/playtest-mission` and `harness/dev-gate-dedicated` |
| `addons/LF_Everon1989/Missions/LF_Everon1989.conf` | pack (in the pack) | The pack's own playtest header (2026-11-14, #322): a copy of `LF_PlaytestTown.conf` that `host`, `playtest` and `session --mission playtest` boot with both addons. Since batch 1 it loads the pack world `Everon1989Town.ent` and uses the moved preview art. A host whose save was made on `LF_PlaytestTown.conf` keeps that header until the save is cleared |
| `EveronLife_TitleImage.{edds,jpg}` | split | Every header uses it; the core needs neutral art |

## Worlds

| Path | Class | Reason |
| --- | --- | --- |
| `DebugWorld/DebugWorld.ent`, `DebugTerrain/` | core | The flat test terrain |
| Annex layers: `Annex`, `Crimes`, `HousesKeys`, `Jobs`, `NPCs`, `PRFeatures`, `Police`, `Tests`, `WhitelistChat` | core | Every top-level entity sits inside the annex fence |
| `Factions`, `Shops` | core | Empty layers |
| `Quests` | split | One giver in the annex, the clerk on the square |
| `Village_Buildings`, `Village_Dressing`, `Village_Roads` | pack, moved (batch 1) | Vanilla buildings, props, and roads; no test names them. Now in the pack world `Everon1989Town` (SubScene of DebugWorld), with `Village_Signs` |
| `Street_*` (13, with `Street_SpawnHub`) | split | Village sites that hold test-named fixtures |
| `Signs` | split | Village signs; `LF_Test_SafeZone` finds two |
| Chain layers: `AppleChain`, `CementChain`, `CrimeChain`, `FarmingChain`, `GravelChain`, `LoggingChain`, `MiningChain`, `PlumChain`, `SandChain`, `TraderChain` | split | On the village outskirts; tests find some of their nodes |
| `ATM`, `Gamemode`, `Roadmap`, `Survival`, `default` | split | Village placement of entities tests use; the game mode entity is core |
| `MainWorld/` | pack, moved (batch 1) | The Everon town, St Philippe apple farm |

## Assets

| Path | Class | Reason |
| --- | --- | --- |
| `Buildings/Supermarket/` | pack, moved (batch 2) | Model and `.xob.meta` of the supermarket above; the `.xob` keeps its core `Buildings/Materials/` assignments (a pack may reference the core) |
| `Buildings/Materials/` | split | ArchBuilder's `AB_*` materials over vanilla `_SharedData` textures, shared by every generated building: neutral, but only pack buildings use them today; they move with the first pack building or stay as a core material set |
| `Crafting/` | core | Station meshes |
| `Items/` | core | Cash, ID card, food, roleplay meshes |
| `Props/CementMound/`, `Props/Military/Sandbags/` | core | Chain item textures |
| `Props/GreenScreen/` | core | Filming tooling |
| `Props/TitlePoster/` | split, mesh moved (batch 1) | The poster mesh (`titleposter.{fbx,txo,xob}`) moved; `Data/` (the Everon title texture and `.emat`) stays in the core because the core `Signs/LF_BoardFace.xob` bakes that material. A neutral board material frees it |
| `Resources/` | core | Ore and wood meshes |
| `Signs/` | core | Sign face material; `LF_BoardFace.xob`, a copy of the title poster's 16:9 quad with its own GUID (the title poster itself is pack) |
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
| `Sounds/Atmosphere/` | pack | The Everon town's two radio tracks and fifteen background loops (`docs/architecture/atmosphere.md`); the emitter scripts and `Prefabs/Atmosphere/` that play them are core |
| `Sounds/Weapons/Handguns/Taser/` | split | Sounds of the stun weapon (shot, handling, stun buzz): they follow the taser, example in core, police-issue item in pack |
| `Assets/Items/Roleplay/Drill/`, `Anims/LF/Drill/`, `Anims/Workspaces/Work/LF_DrillPush.*`, `Particles/LF/Drill_fx_bit.ptc`, `Sounds/Items/Drill/` | core | The vault drill and its scene (#340): model, push clips, sparks, motor and grinding loops. They follow `Prefabs/Items/Fence/LF_Drill.et`, the robbery's tool; the component that plays them (`Feature/AssetAnim/`) is core |
| `Assets/Items/Resources/CementBag/` | core | The cement bag item, a CC0 Poly Haven model through the source stage (`docs/asset-pipeline/sources.md`): the quarry's cement chain is core. |
| `Sounds/Alarm/Robbery/`, `Prefabs/Police/LF_SiteAlarm_{Bank,Fuel,Store,Van}*.et` | core | The site alarm of each robbery type (#740): bank bell, fuel station intruder alarm, store buzzer, van horn, each with a loud twin. They play through `LF_SiteAlarmComponent` and are picked by the `m_aAlarmKinds` rows of `LF_Police.conf`; the kinds' ids and offences are data, so a pack changes which robbery sounds which alarm without code |
| `Prefabs/Jobs/LF_Workplace.et` | core | The generic workplace (#1111): a desk, a punch clock with the clock-in action, `LF_WorkplaceComponent` (job id, place registration, optional NPC slot). The police and medic duty desks inherit it; a pack inherits it for its own job (`pack new --job`) |
| `Sounds/Dialogue/clerk/` | pack | The town clerk's recorded lines and audio project (`tools\cli asset dialogue clerk`) |
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
| `Feature/Vehicles/LF_VehicleCatalog.c` | seven cars and seven `LF_` variants | S105, UAZ-469, S1203, UAZ-452, M923A1, Dana 2101, Sputnik T601 | vehicles config |
| `Feature/Vehicles/LF_Garage.c` | `FALLBACK_FEE` | 250 | `LF_Garage.conf` (fallback only) |
| `Feature/Mining/LF_MiningForge.c` | `IRON_BAR`, `IRON_SECONDS` | iron bar, 5 s | resource chains |
| `Feature/UIKit/LF_UITheme.c`, `LF_UISkin.c` | palette, paper and phosphor skins | the 1989 look | a skin config |
| `Feature/Police/LF_WantedReports.c` | `GridPlace()` | "grid X-Z", described as how a 1989 map reads | fixed: neutral grid text |
| `Feature/Whitelist/LF_WhitelistManager.c` | `LF_EFactionType` | `CIVILIAN`, `POLICE`, `MILITARY`, `MAFIA` | fixed enum; display names are pack strings |
| `Prefabs/Characters/Npcs/PoliceDutyNPC.et` | `Name` | "Police Sergeant" (literal) | a pack string key |

Script holds no place names. Every user-facing string in `Feature/` is already a
`#` key; the flavour sits in the string table.
