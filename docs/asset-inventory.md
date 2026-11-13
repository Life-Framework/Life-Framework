# Asset inventory

Every visible thing the mod adds, and whether its look is final or a stand-in.
Placeholder rows are the to-model list for the 3D asset pipeline (#339, Development
track).

## How to keep this up to date

- **The area polish pass** (`docs/agent-area-polish.md`) adds or updates one row per
  mod prefab it checks.
- **Any PR that adds a visible prefab** adds its row in the same change.
- **When a model replaces a placeholder,** set the row to `final` and name the new asset.

## Status

- `final`: a real base-game or mod asset that is the thing.
- `placeholder`: a stand-in mesh that sort of works (for example, a radio as a pager).
  Needs a model.
- `missing`: no mesh or an invisible fallback. Needs a model before it can ship.

| Item | Prefab | Status | Uses now | What it should be | Area / issue | Priority |
|---|---|---|---|---|---|---|
| Pager | `LF_Pager.et` | placeholder | ANPRC68 radio mesh | 1980s belt pager with a small screen | Comms #249 | medium |
| Taser | `Weapons/Handguns/Taser/Handgun_Taser.et` | placeholder | `Assets/Weapons/Handguns/Taser/Taser.xob`: a Meshy text-to-3D model (v2, 2026-11-13, model `latest`, prompt in `tools/assets/taser/spec.json`, generation ids in `tools/assets/taser/meshy.json`; use signed off under #664) cleaned by `toolscli asset`. Reads as a boxy angular yellow and black stun gun with a slim straight grip the hand closes around; the dart panel sits mid-body, not at the muzzle, and it still looks like a toy pistol | a boxy 1980s dart stun gun with the cartridge block at the front and a grip the vanilla pistol hand pose closes around | Police #341 | medium |
| Handcuffs | `Roleplay/Handcuffs.et`, `Roleplay/Handcuffs_Worn.et` | check | `Assets/Items/Roleplay/Handcuffs/Handcuffs.xob`: a Meshy text-to-3D model (2026-11-13, model `latest`, generated with Meshy, ids in `tools/assets/handcuffs/meshy.json`) of two steel cuffs and a short chain, cleaned by `tools\cli asset` (3,000 triangles, box collider) with its maps darkened (`tools/assets/handcuffs/tone-maps.py`). Reads as chain handcuffs on the desk and as a steel cuff on a cuffed wrist; held, the hand takes the vanilla gauze pose and the model does not show | the cuffs visible in the left hand (a Workbench pass on the hand offsets) |  Police #664 | low |
| Taser cartridge | `Weapons/Handguns/Taser/Magazine_Taser.et` | placeholder | the vanilla M9 magazine mesh (inherited) | a square two-dart cartridge | Police #341 | low |
| Moonshine still | `LF_ChainStill.et` | placeholder | barrel station with the upright `BoilerTank_03` metal tank mesh (was the horizontal `BoilerTank_02`, which read as lying on its side) (was the white `Boiler_01`, #497); a dark `BoilerLarge_02` furnace with a chimney pipe stands beside it | copper pot still with coil | Moonshine #420 | high: in website shots |
| Smelter furnace | `Processor/Furnace.et` | placeholder | base-game `BoilerLarge_02` heating boiler (was the `Sawmill_E_01_SmallSaw` chipper, #593); the base game has no named furnace or foundry asset | brick or cast furnace with a fire door and chimney | Mining #593 | medium |
| Fence drill | `LF_Drill.et` | placeholder | `BenchDrill_01` model at 0.4 (0.47 m, reads as a magnetic safe drill), upright when left | handheld power drill or safe-cracking drill | Fence #271 | medium |
| House key | `HouseKeyItem.et` | final | `Assets/Items/Keys/HouseKey.xob`: brass key with a round bow and toothed blade, made in Blender (`tools/assets/keys/build.py`; no base-game key mesh exists, #613). Materials tint shared base-game plastic maps | (already what it is) | Housing #613 | none |
| Vehicle key | `Roleplay/VehicleKey.et`, `LF_PoliceCarKey.et` | final | `Assets/Items/Keys/VehicleKey.xob`: black remote fob with two red buttons and a steel blade (#613), so a car key reads differently from the brass house key | (already what it is) | Vehicles #613 | none |
| Fence keycard | `LF_Keycard.et` | placeholder | stand-in mesh (#271) | plastic access card | Fence #271 | low |
| Fence charge | `LF_Charge.et` | placeholder | stand-in mesh (#271) | improvised breaching charge | Fence #271 | low |
| Cabbage seed packet | `Items/Seeds/CabbageSeeds.et` | placeholder | vanilla `Papers_Personal` folded papers | small printed paper seed packet | Farm plots #614 | low |
| Farm plot | `Vegetation/Crops/FarmPlot_Cabbage.et`, `FarmPlot_CabbageVisual.et`, `Props/Commercial/LF_SeedCrate_Cabbage.et` | final | vanilla `VegetableBed_01_small` bed, `CabbageCrop_01` heads scaled per stage, `Crate_01` | (already a bed, cabbages and a crate) | Farm plots #614 | none |
| Police scanner | `LF_Scanner.et` | check | (#253) | handheld 1980s scanner | Comms #253 | medium |
| Police car | `LF_UAZ469_Police.et`, `LF_UAZ469_LEDLightBar.et` | final | vanilla `UAZ469_covered_CIV_blue` with the mod bar mount `M151A2_LightBarMount.xob` raised 0.25 m to the UAZ roof (#595; the open M151A2 jeep left the bar floating) | (already a car) | Police station car, jail escort car | none |

The polish pass fills in the rest, area by area. The `check` rows haven't been
looked at yet.

## Area polish checks

| Item | Prefab | Status | Uses now | What it should be | Area / issue | Priority |
|---|---|---|---|---|---|---|
| Payphone | `LF_Payphone.et` | final | vanilla `PhoneBooth_E_01` | (already a phone booth) | Square, fuel, hospital, police: Comms #249 | none |
| Town signs (town map, safe zone) | `LF_Sign_Posts.et`, `LF_Sign_PolePlate.et` | final | vanilla sign bodies with a rendered face | (already signs) | Spawn square | none |
| Clerk desk and chair | `Desk_01_short.et`, `Chair_02_green.et` | final | vanilla furniture | (already a desk and chair) | Spawn square | none |
| Fence yard props | `Workbench_01.et`, `BoxWooden_01.et`, `Crate_01_green.et`, `BarrelMetal_01_blue.et`, `Tire_M151A2_pile.et`, `EL_S105_beige.et` | final | vanilla props and a parked Skoda | (already what they are); no tarp prefab found, so none placed. Drill, charge and keycard stay placeholders (rows above) | Fence yard #276 | none |
| Cash van box | `LF_CashVanBox.et` | placeholder | vanilla `Chest_01` (dark steel trunk, 0.8 m) on the ground at the van's kerb-side door | a locked steel cash-in-transit box | Cash van #274 | low |
| Bank teller window (#590, replaces the ATM) | `LF_BankTeller.et`, `Bank_Teller`, `Bank_Counter_West/East` (DebugWorld `ATM.layer`) | placeholder | vanilla `Npc_Default_01` clerk standing behind the vanilla post-office counter `CounterPostOffice_01_window`; the old ATM entity stays hidden (no mesh) inside the bank | a 1989 bank teller cage with brass grille | Bank, ATM | low |
| Money bag | `LF_MoneyBag.et` | placeholder | vanilla `SackUniversal_01` sack mesh at 0.4 scale (0.54 m) | a canvas cash bag with a drawstring | Bank, cash van #273 #274 | low |
| Hospital bed | `LF_HospitalBed.et` | final | vanilla `BedMedical_01` | (already a medical bed) | Hospital, area 7 | none |
| Ambulance | `LF_M997_Ambulance.et` | final | vanilla `M997_maxi_ambulance` | (already an ambulance) | Hospital, area 7 | none |
| Hospital till | `Roadmap_Hospital_Till` | final | vanilla `CounterPostOffice_01` | (a reception counter) | Hospital, area 7 | none |
| Medicine cabinet | `Roadmap_Hospital_Cabinet` | placeholder | vanilla `CupboardOld_01_medium` wooden cupboard | white medical cabinet | Hospital, area 7 | low |
| Stretcher | none | missing | no base-game stretcher found | stretcher by the ambulance | Hospital, area 7 | low |
| Store counter | `Shop_GeneralStore_Debug` | placeholder | vanilla `CounterShop_01` (a yellow market cart) | a shop counter with a till | General store, area 3 | low |
| Market stall | umbrella stands plus `TableRecreation_01` | placeholder | vanilla `MarketStand_01_Umbrella` over recreation tables | a market stall with a awning | General store and market, area 3 | low |
| Site alarm beacon | `LF_SiteAlarm.et` | final | vanilla `HazardBeacon_01` at 0.4 (0.21 m), no collision, while the alarm runs; on an alarm mount (bank bell box, cash van roof) or the surface under the site | (a small rotating beacon) | Alarm at store, bank, till, area 3 | none |
| Bank alarm bell box | `Roadmap_Bank_AlarmBox` (layer, `Street_Bank.layer`) | final | vanilla `ElectricityBox_small_01` on the bank's east wall above the vault door, carries `LF_AlarmMountComponent` | (a wall alarm box) | Bank vault, area 2 | none |
| Hideout machine | `Roadmap_Hideout_Machine` | placeholder | vanilla `CoffeeGrinderShop_01` | contraband press | Hideout, area 13 | medium |
| Hideout stash | `LF_HideoutStash.et` | final | vanilla chest | (a trunk) | Hideout, area 13 | none |
| Hideout van | `Roadmap_Hideout_Van` | final | vanilla `UAZ452_base` (the van-bodied car) | (a parked van, scenery) | Hideout, area 13 | none |
| Hideout workbench, crates, barrels, tyres | `Roadmap_Hideout_*` | final | vanilla `Workbench_01` (under the pergola), `Crate_01`, `BoxWooden_01`, `WoodenPalletBox_01`, `Pallet_01_Pile`, `Cart_01`, `Jerrycan_01`, `CardboardBox_03`, `BarrelMetal_01_blue`, `Tire_M151A2_pile` | (same) | Hideout, area 13 | none |
| Fuel till counter and register | `EL_FuelTill.et` on vanilla `CounterShop_01` (Village_Dressing) | placeholder | vanilla shop counter with a cash register on top | a petrol-station kiosk counter inside the shop | Fuel till #236 | low |
| Lock-up garage door | `Roadmap_Lockpick_Door` (`EL_Door_Base.et`) | placeholder | plain wooden house door in a brick wall line | roll-up or double-leaf garage door | Lock-up #411 | low |
| Apple juicer | `AppleJuicer.et` (`AppleProcessor`) | placeholder | vanilla `CoffeeGrinderShop_01` on a `TableRecreation_01` press table in the orchard | a hand fruit press | Farm and trader chains, area 12 | low |
| Farm field: beds, greenhouse, crops | `VegetableBed_01_big/small`, `Greenhouse_01`, `CabbageCrop.et` | final | vanilla farm structures and the mod crop prefab | (same) | Farm, area 12 | none |
| Farm yard props | `Cart_01`, `Wheelbarrow_01`, `Bucket_01`, `Crate_01_*`, `HayBale_01_square_big/small`, `WoodenFence_01_4m`, `TableRecreation_01` | final | vanilla props (no 1989-specific farm crate exists; Crate_01 stands in for a produce crate) | (same) | Farm, area 12 | none |
| Quarry lane barrier | `WoodenFence_01_4m` rails + `ConeTraffic_01` | placeholder | two fence rails each side of a 6 m gap | red-white boom barrier on a post | Quarry polish, area 11 | low |
| Quarry gatehouse | `Shed_03.et` | placeholder | a village garden shed | site hut or trailer | Quarry polish, area 11 | low |
| Quarry spoil heap | `DirtPile_01_base.et` | final | base-game dirt pile | | Quarry polish, area 11 | |
| Chop shop yard props | `Street_ChopShop.layer`: `BrickWallOld_01_4m_A`, `M151A2_wreck`, `Workbench_01` + `Workbench_Vice_01`, `ToolBox_01_base`, `Tire_M151A2` (+ pile), `BarrelMetal_01_*`, `Crate_01_brown`, M998 door and car jack meshes | placeholder (shell, parts) | the stripped car shell is a vanilla M151A2 jeep wreck; the engine block is a vanilla `VehicleGarbage_01_pile_medium` scrap pile; spare parts are M998 doors and M151A2 tyres; the work strip is vanilla `AsphaltPavement_01_Wide_8m` kerbed asphalt (a pad stand-in) | a stripped 1980s saloon shell and an engine block | Chop shop #415, area 15 | low |
| Car lot price board | `Sign_LotPrice_*` (`LF_Sign_Chalkboard`) | final | vanilla `ChalkBoard_01` | (a price board) | Car lot, area 4 | none |
| Lot display cars | `Lot_Display_*`, `Roadmap_Vehicle_S105` | final | the lot offers base vehicles | (display cars) | Car lot, area 4 | none |
| Quarry rock walls and stones | `GraniteCliff_01..03.xob`, `Granite_Cluster_Large_01_V2`, `GraniteStone_01/02_V2` | final | base-game granite cliffs form a three-sided pit; small stones scattered on the floor | (same) | Quarry polish, area 11 | none |
| Crime site (test annex) | `Crimes.layer` safe `Safe_01`, weapon rack `WeaponRackStand_01`, tyre pile `Tire_M151A2_pile`; `Police.layer` `PoliceControlPanel_01` on a `TableRecreation_01` | placeholder | the safe, rack and tyre pile are real props, the "police desk" is a base-game control panel with an ID card and key on a table, and the suspect is a default NPC; stand-ins for the village's real armory and duty desk | a bank safe, an armory rack, a getaway car | Fenced crime site polish (#608) | low: test annex |
| Crime site boards | `Sign_crime_safe`, `_rack`, `_car`, `_police` (`LF_Sign_Chalkboard`) | final | vanilla `ChalkBoard_01`, one per step, read for what it is and what you need | (a chalk board) | Fenced crime site polish (#608) | none |
| Quarry info board | `Sign_Quarry_Info` (`LF_Sign_Board`, sign id `quarry_info`) | final | the shared sign board with a rendered face | (same) | Quarry regroup, #618 | none |
| Quarry materials yard | `ConcretePlant_E_01` (cement plant, moved off the east cliff), the cement, gravel and sand builder stands, sawmill and cuttable pines | final | base-game plant, piles and pines regrouped inside the pit | (same) | Quarry regroup, #618 | none |
| Cabbage item | `Items/Food/Cabbage.et` | missing | inherits the `CabbageCrop_01` vegetation `Tree` prefab, which has no mesh object, so its inventory preview is empty (#612) | a loose cabbage head `.xob` with a normal item prefab | Farm #612 | medium |

Inventory previews (#612): `tools\cli shots previews` draws every mod item's inventory preview on two contact sheets. The preview camera ignores a prefab's `scale`, so scale a big source mesh down with `CameraDistanceToItem`, not `scale`. Several items share the ID-card placeholder mesh (evidence bag and marker, keycard, lockpick, house key, vehicle key).
| Vehicle lock sound | `SOUND_PICK_UP` (base inventory click, played on the user at the car) | placeholder | no door-latch event found offline; swap `LF_VehicleLockComponent.LOCK_SOUND` for a real latch or chirp event | (none) | Car polish (lock) | needs a real latch/chirp event |
| Garage counter | `CounterShop_01.xob` on `Roadmap_Garage` | final | gives the garage use point a visible, targetable body beside the doors | (same as lot desk) | Garage fix | none |
