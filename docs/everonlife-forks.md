# EveronLife fork survey

Survey date: 2026-10-11.

EveronLife and these forks are MIT licensed (Copyright 2022 EveronLife).

This is a reading list. Life Framework already has bank, shop, processing, quantity visuals, gathering, jobs, jail, organizations, vehicles, garage (ported from `thebonbon/EveronLife` `feature/garage_and_vehicle_shop`), whitelist, UI kit, and sirens. Cash is a physical MoneyStack. Phone was refused. Do not port systems we already have.

## What to read

### thebonbon/EveronLife `dedicated_server`

211 commits ahead of that fork's main. The garage, item shop, bank, and file whitelist repeat systems we have. The part that is wired and new is OOC and admin chat styles on `SCR_ChatPanel`, channel configs, VoN name suppression, and an admin-only ArmaVision flag. Kill log, item log, and map icons are commented out.

### thebonbon/EveronLife `enhancement/Main_World`

Phased ore damage (`EL_DestructionMultiPhaseSingleComponent`, `EL_ToolHitZone`) and a mining area that weighted-spawns ores, plus a timed forge from nuggets to bars. Concrete, jam, hay, beer, and cocaine chains are more recipes for a system we have. Ignore jump pads, spikes, and toys.

### jasper-one/EveronLife-Plus `feature/basic_crafting_foundation`

No new scripts. Prefabs worth listing for a later crafting pass: lathe, sewing machine, boiler, fireplace, vice, and items Charcoal, Sulfur, Gunpowder, Linen, plus DebugWorld layers `Mining.layer`, `Smelting.layer`, and `Apple.layer`.

### Arkensor/EveronLife `enhancement/persistence_v2`

120 commits ahead. Account, character, vehicle, and hitzone saves, then a move onto EnfusionPersistenceFramework and EnfusionDatabaseFramework. Read it for damaged-hitzone saves only. Do not replace our versioned serializer with those frameworks.

### Jacob-Mango/EveronLife `feature/item-stacking`

June 2022, 126 commits behind. Split and combine live in `EL_InventoryQuantityComponent` and `EL_QuantityDialog`. Worth a look only if our quantity feature cannot split a stack. The code is old.

### rekterakathom/EveronLife `main`

One commit. A stacked trade input returns one output item per `EL_QuantityComponent` quantity, in `EL_InventoryStorageManagerComponent.c`. Worth a short read if our shop pays one unit regardless of stack size.

## What to ignore

- thebonbon `feature/bankSystem` duplicates the bank we have.
- thebonbon `feature/item-shop` duplicates the shop we have.
- thebonbon `fix/item-shop` duplicates the shop we have.
- thebonbon `feature/processing` duplicates the processing we have.
- thebonbon `feature/quantityVisuals` duplicates the quantity visuals we have.
- thebonbon `farming_system` duplicates farming we have.
- thebonbon `testing/stuff` is toys (lightsabers).
- thebonbon `feature/garage_and_vehicle_shop` is already ported.
- jasper-one `pr-195` is bank, which we already have.
- jasper-one `pr-188` is garage, which we already have.
- jasper-one `pr-186` is an item shop, which we already have.
- jasper-one `pr-142` is sirens, which we already have.
- jasper-one `pr-71` is vehicle lock, which we already have.
- jasper-one `feature/FirstDrugChain` repeats a system we already have.
- jasper-one `pr-201` is a money mesh and is art only.
- jasper-one `feature/bus-stops` is a teleport.
- Arkensor `feature/sirensystem` is the same sirens we have.
- Arkensor `rework/drugchain_poc` is prefabs only.
- Arkensor `feature/object_manager` stays off this reading list.
- Arkensor `poc/persistent_component_guid` stays off this reading list.
- borknbeans Phones is a 1.3 KB `EL_PhoneUI` and a radio prop, not calls. Phone stays out.
- Projectlost-org/EveronLife is archived: Character-Creator vendors EPF and a WIP character screen, the Server branch tip deletes the game, and the ID, admin, and siren commits are not on the tip.
- xCrowe/EveronLife is an Anizay mission shell, with no life systems.
- Pyth3rEx money rework is a cash-stack art pass.
- Trippinboyqc420 `patch-1` only changes a HUD watermark.
- zalexki `patch-1` is a one-line license-plate parent fix.
- benjibossk `main` is an axe prefab visibility fix.

## Mirrors

These forks have no unique commits.

| Fork |
| --- |
| lazarvlade/TakiLife |
| SouLGenetic/GensLife |
| Sparknutz/SparksEveronLife |
| OFFSETSYNT4X/EveronLife_ARC |
| DroDaMan/EveronLife |
| Heidelbergg/GenesisEveronLife |
| TomLorenzi/AltasiaLife |
| KatataFish808/KatataFish |
| mcoffin/EveronLife |
| XMortX/EveronLife |
| Slep-wt/EveronLife |
| 4upmes/EveronLife |
| Ezcoo/EveronLife |
| Tw1zT3d2four7/EveronLife |
| FSoulja/EveronLife |
| alexis-opolka/EveronLife |
