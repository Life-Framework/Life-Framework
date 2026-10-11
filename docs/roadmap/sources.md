# What the scopes reuse

Three sources, in this order:

1. **This repo** when the system already exists. Extend it. Do not replace it with a second copy.
2. **Arma Reforger** for meshes, vehicles, weapons, medical items, buildings, and the explosive charge.
3. **[LNGCLifeRP_Public](https://github.com/SouLGenetic/LNGCLifeRP_Public)** (MIT, author LNGCGenetic) for life-RP scripts we do not have yet. Permission to take what we need is explicit. Keep the MIT copyright notice on ported files.

Paths below were read from this tree or from `asset_search` / `game_browse` against the installed base game. A `{GUID}` is listed only when this repo already references it. Other vanilla paths are real files; copy the GUID from the prefab meta when the reference is written.

## Do not port

- LNGC virtual cash (`LNGC_cash`) and the virtual-item catalog. Cash here is `{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et` through `EL_MoneyUtils`. The ATM stays the only cash-to-account boundary.
- `LNGC_WantedComponent`. It is in-memory and clears on restart. Wanted here lives on `EL_PlayerAccount` and is persisted.
- The phone (`LNGC_Phone*`). It is unfinished in LNGC, and it is not a 1989 interface.
- The marijuana license and the hidden drug-dealer shop as the base contraband example. The period example in that repo is moonshine. See [contraband](scopes/crime/contraband.md).
- `Scripts/WorkbenchGame/EnfusionMCP/`. That is editor tooling, not gameplay.
- The downed-state map close path that LNGC documents as a softlock (`LNGC_LifeStateListener`, `CloseMapIfOpen`). Read those comments before touching revive.

## Already in this repo

| Need | Use |
| --- | --- |
| Cash | `EL_MoneyUtils`, MoneyStack prefab above |
| Bank | `EL_ATMManager`, `EL_BankAccount`, `Feature/ATM/` |
| Robbery gates | `EL_RobAction`, `EL_RobWeaponAction`, `EL_RobVehicleAction`. Tests: `EL_Test_RobReward`, `EL_Test_RobRewardWantedClamp` |
| Police sentence math | `EL_PoliceUtils` in `EL_PoliceMenu.c`. Fine cap 5000. Jail teleport is the literal `"127 1 137"` (DebugWorld desk), not a placed cell |
| Arrest / fine RPC | `RpcAsk_EL_PoliceArrest`, `RpcAsk_EL_PoliceFine` |
| Confiscate | `EL_ConfiscateAction` (weapons only today) |
| Door lock | `EL_LockComponent`, `EL_DoorUserAction`, `EL_KeyComponent`, `EL_HouseManagerComponent`. Door prefab `{99D61E9EDBDF3CC8}Prefabs/Structures/BuildingParts/Doors/EL_Door_Base.et` (parent vanilla `Door_Base.et`). Key `{3735389521255DBE}Prefabs/Items/HouseKeyItem.et`. Lock state is not in the save serializer yet (`EL_LockComponent.c` says so) |
| Vehicle lock and key | `EL_VehicleLockComponent`, `EL_VehicleKeyComponent`, `{9D94B834AF514B4A}Prefabs/Items/Roleplay/VehicleKey.et`. Plates: `EL_LicensePlateManager` |
| Police vehicle | `{C03B70B228FC49BA}Prefabs/Vehicles/Wheeled/M151A2/M151A2_Police_Base.et`, parent `{F649585ABB3706C4}Prefabs/Vehicles/Wheeled/M151A2/M151A2.et` |
| Hand carry | `EL_HandCarryComponent` on `Prefabs/Items/Core/HandCarryItem_Base.et` |
| Gather / process | `EL_GatherAction`, `EL_ProcessAction`, `EL_DestructibleResourceComponent` |
| Process machines | `Prefabs/Processor/AppleJuicer.et` (3 apples → apple juice), `Furnace.et` (nuggets → bars), `Sawmill.et` (log → plank + kindling) |
| Gather nodes | Apple and plum trees, tomato and cabbage crops, pine cuttable tree, mining area, cement / gravel / sand piles. Shop prices already list the item prefabs in `Configs/Shop/ShopPrices.conf` |
| Jobs | `EL_PlayerJobComponent.InitializeJobConfigs`: unemployed, farmer, miner, lumberjack, medic, police. Paycheck is cash. `AddExperience` is still a no-op |
| Sell illegal goods | `Prefabs/Trader/BlackMarketTrader.et` (civilian only). Mesh is vanilla `Assets/Props/Crates/Crate_01.xob`. Listed goods today: M9 and `{FA5C25BF66A53DCF}Prefabs/Weapons/Rifles/AK74/Rifle_AK74.et` |
| Quests | `LF_QuestTracker`, `LF_QuestGiverComponent`, `LF_QuestAction` |
| Sidearm already priced | `{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et` |

## Base game (no extra mod)

| Need | Prefab |
| --- | --- |
| US demolition block | `Prefabs/Weapons/Explosives/DemoBlock_M112/DemoBlock_M112.et` |
| Soviet demolition block | `Prefabs/Weapons/Explosives/DemoBlock_TSh400g/DemoBlock_TSh400g.et` |
| Charge actions | `SCR_ArmExplosiveChargeWithTimedFuzeAction`, `SCR_ArmExplosiveChargeWithRemoteFuzeAction`, `SCR_DisarmExplosiveChargeAction` |
| Field dressing | `Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et` (USSR twin beside it) |
| Morphine | `Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et` |
| Saline | `Prefabs/Items/Medicine/SalineBag_01/SalineBag_US_01.et` |
| Tourniquet | `Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et` |
| Fuel can | `Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et` |
| Civilian car | `Prefabs/Vehicles/Wheeled/S105/S105_beige.et` and the other S105 colors |
| Civilian off-road | `Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_covered_CIV_blue.et` (and the uncovered CIV set) |
| Van | `Prefabs/Vehicles/Wheeled/UAZ452/` and `Prefabs/Vehicles/Wheeled/S1203/S1203_transport_beige.et` |
| Cargo truck | `Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et` and `M923A1_tanker.et` |
| Rifle for a senior police kit | `Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et` |
| Fuel station | `Prefabs/Structures/Commercial/FuelStations/FuelStation_E_01/` |
| Hospital | `Prefabs/Structures/Military/Houses/HospitalMilitary_E_01/` |
| Police building | `Prefabs/Structures/Houses/Villa/Villa_E_2I01/Villa_E_2I01_Police.et` |
| Shop shell (also the bank stand-in; there is no bank building) | `Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/`, `ShopModern_E_01/` |
| Prison door | `Prefabs/Structures/BuildingParts/Doors/Door_PrisonCell_E_A_Left_INT_ST_B.et` |
| Garage shell | `Prefabs/Structures/Houses/Garage/Garage_E_01/` |
| Post counter (parcel depot) | `Prefabs/Props/Services/Post/CounterPostOffice_01.et` |
| Crate mesh already used by the black market | `Assets/Props/Crates/Crate_01.xob` |

`Prefabs/Items/Medical/Bandage.et` appears only as a string in `EL_Test_ShopPrices`. It is not a base-game file. Medicine lives under `Prefabs/Items/Medicine/`.

There is no shotgun prefab in the base game index used here.

## LNGC scripts worth taking

Repo root: `https://github.com/SouLGenetic/LNGCLifeRP_Public` (`master`). Gameplay scripts are `Scripts/Game/LNGC_*.c`.

| Scope | Take | Leave |
| --- | --- | --- |
| [Jailing](scopes/police/jailing.md) | `LNGC_JailCellComponent` (placed cell, zone radius), `LNGC_JailComponent` (timer, confine, release), `LNGC_SendToJailAction`, `LNGC_JailDurationDialog`. Prefab `Prefabs/LNGC_JailCell.et` | Virtual-item seizure, the hard-coded `Civ_Morton` release spawn, in-memory wanted |
| Jailing, vehicle escort | `LNGC_ArrestAction`, `LNGC_ArrestableComponent`, `LNGC_EscortAction`, `LNGC_PoliceComponent` escort tick, `LNGC_PutInVehicleAction`, `LNGC_TakeOutOfVehicleAction`, `LNGC_UncuffAction` | Seat-picker dialog as a requirement. Nearest free non-driver seat is enough for the first escort slice |
| [Forced entry](scopes/crime/forced-entry.md) | Nothing. LNGC has no breach. Use the vanilla demo block and charge actions on `EL_LockComponent` | |
| Robberies | Nothing for the till or the vault. Keep `EL_RobAction` | |
| [Contraband](scopes/crime/contraband.md) | `Configs/Economy/CraftingRecipes.conf` moonshine recipe as the shape of a hideout process. `Prefabs/Props/Crates/ShineStash.et` as the stash prop. `LNGC_SearchAction` for the police search request (server validates, then results) | Virtual item ids, the marijuana recipe, dirty-cash item (nothing pays it) |
| [Ranks](scopes/police/ranks.md) | Uniform prefabs that recolor vanilla clothes: `Prefabs/Characters/HeadGear/Hat_PeakedCap_USSR_01/Hat_PeakedCap_police.et`, `Shirt_Turtleneck_01_police.et`, `Pants_Suit_01_police.et`. `Prefabs/Vehicles/Wheeled/S105/S105_police.et` | No rank ladder exists there. Do not invent one by copying faction select |
| [Vehicles](scopes/ownership/vehicles.md) | Garage store/retrieve and restart persistence: `LNGC_GarageComponent`, `LNGC_GarageDialog`, `LNGC_VehicleSpawnPointComponent`, locked vehicle prefabs under `Prefabs/Vehicles/Wheeled/*/`. Key share and revoke | `LNGC_VehicleKeyLockComponent` is marked prototype and keys a virtual item. Our `EL_VehicleLockComponent` and `VehicleKey.et` stay. Read LNGC's `BumpMe` comments as a checklist, not as a replacement class |
| [Resources](scopes/work/resources.md) | `LNGC_GatherZonesCatalog` only as a config-id example | Do not replace `EL_GatherAction` on the node with a 20 m radius grant |
| [Civilian jobs](scopes/work/civilian-jobs.md) | Nothing. LNGC lists job-specific income as not built. The paycheck there is an unemployed stipend | |
| [Organizations](scopes/ownership/organizations.md), [hideouts](scopes/ownership/hideouts.md) | Stash prop only (`ShineStash.et`) | No gang, firm, or shared-fund scripts |
| [Crafting](scopes/later/crafting.md) | `LNGC_CraftingRecipeDef` fields: inputs, counts, seconds, license, hidden, bulk. `Prefabs/Compositions/Misc/SubCompositions/CraftingBench.et` and `ProcessBench.et` | Virtual output item ids. Outputs stay prefabs, as `EL_ProcessAction` already does |
| [Revive](scopes/later/revive.md) | `LNGC_ReviveAction`, the downed state and force-respawn wait on the player-data manager (`m_iForceRespawnWaitSeconds` default 75) | The map-softlock path. Revive here may use a medical item; LNGC restricts the action to the MEDIC faction |
| [Police missions](scopes/later/police-missions.md) | `LNGC_DispatchCallsDialog`, `LNGC_IssueFineAction`, `LNGC_FineReasonsCatalog`, `LNGC_MarkWantedAction` | The phone as the way a civilian opens a call |
| [Housing](scopes/later/housing.md) | A home-ownership license id exists in their license shop. No door or key scripts | Use our house lock, not a license flag alone |
| [Tasks](scopes/later/tasks.md), [events](scopes/later/events.md) | Dispatch markers are the closest callout. No event scheduler | |

## How to take a file

Copy the behavior into an `EL_` or `LF_` class under `Scripts/Game/Feature/`. Point it at MoneyStack, `EL_PlayerAccount`, and the existing lock components. Add the LNGC copyright lines to the new file. Do not drop `LNGC_` types into this addon and call both economies.
