# Third-party code

Code in this repository that came from another project, and the terms we use it
under. Assets are listed in `assets-source/LICENSES.md`. The import policy is
section 5 of `docs/ecosystem.md`. Paths below are under
`addons/LifeFramework/Scripts/Game/`.

## Overthrow

- Source: https://github.com/ArmaOverthrow/Overthrow.Arma4
- Licence in that repo: Arma Public License Share Alike (APL-SA).
- Our terms: the Overthrow owner gave Life Framework explicit permission to use
  its code and relicense it under our MIT licence (APL on the Workshop build),
  with credit. The maintainer holds the record of that permission. The grant
  covers Life Framework only (`docs/decisions.md`, 2026-11-08).
- Every copied or derived file starts with:
  `// Derived from Overthrow (https://github.com/ArmaOverthrow/Overthrow.Arma4), used with the author's permission, relicensed MIT.`
- Files:
  - `Feature/Money/LF_MoneyDeltaTracker.c` from `Scripts/Game/Data/OVT_MoneyDeltaTracker.c`
  - `Feature/Money/LF_MoneyFormat.c` from `Scripts/Game/Data/OVT_MoneyFormat.c`
  - `Feature/Shop/LF_ShopRules.c` from `Scripts/Game/Data/OVT_ShopSellRules.c`
  - `Feature/Property/LF_HouseSpawnComponent.c` from `Scripts/Game/Components/OVT_SpawnPointComponent.c`
  - `Feature/Fuel/LF_FuelStockRules.c` from `Scripts/Game/Data/OVT_FuelPricing.c` and `OVT_FuelChargeLedger.c` (shape only)

A PR that copies more from Overthrow adds its files here.

## LNGCLifeRP_Public

- Source: https://github.com/SouLGenetic/LNGCLifeRP_Public
- Licence: MIT (2026 LNGCGenetic).
- Files: `Feature/Police/LF_Jail*.c` (jail, arrest, escort, put in and take out
  of a vehicle) and `Feature/Vehicles/LF_VehicleGarageComponent.c`,
  `LF_VehicleGarageDialog.c`, `LF_VehicleSpawnPointComponent.c`. Each keeps a
  `// Ported from` line.

## EveronLife forks

Our upstream is [EveronLife/EveronLife](https://github.com/EveronLife/EveronLife)
(MIT). Code ported from MIT forks of it keeps a `// Ported from` line naming the
fork and branch:

- thebonbon/EveronLife: `Feature/Chat/LF_RoleplayChat.c`, `UI/Chat/LF_ChatPanel.c`,
  `Feature/Mining/LF_Mining*.c`, `Feature/Mining/LF_OreNodeComponent.c` (pickaxe-hit
  mining from branch `enhancement/Main_World`, ported with the author's permission
  given to the maintainer, 2026-11-08), `Feature/Toys/LF_JumpPad.c`, `Feature/Police/LF_SpikeStrip.c`
- rekterakathom/EveronLife: parts of
  `Components/InventorySystem/EL_InventoryStorageManagerComponent.c` and
  `Feature/Shop/LF_ShopRules.c`
- Jacob-Mango/EveronLife: parts of `Feature/Quantity/LF_QuantityComponent.c`

`docs/everonlife-forks.md` surveys the forks.

## Reforger Mapper (format only)

- Source: https://github.com/benjibossk/mappingenfusiontest
- Licence: MIT (2026 Benji). The notice is `tools/test/fixtures/mapper/LICENSE-Mapper.txt`.
- Use: its project JSON ("mapgen.json", version 0) is read by `tools\cli mapper import`
  (`tools/mapper/convert.mjs`, `tools/commands/mapper.mjs`), reimplemented from its published format docs. No code of
  Mapper's is copied. `docs/architecture/reforger-mapper-import.md` describes the format and the mapping.
