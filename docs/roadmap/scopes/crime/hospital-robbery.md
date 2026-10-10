# Hospital robbery

Phase: Release
Extends: [forced entry](forced-entry.md), `Crime`, `Money`

## Player loop

Civilians rob the hospital till for cash, and breach a locked cabinet for medical supplies. The supplies are ordinary items. They become illegal only when config marks them contraband.

## In scope

- Till: the existing cash-robbery gates (civilian, cooldown, minimum police on duty), placed at the hospital, paying MoneyStack cash.
- Cabinet: a forced-entry container. On open it spawns a config list of base-game medical items.
- If those items are flagged contraband, police confiscation can take them (see [contraband](contraband.md)).
- One alert per site action, not per menu refresh.

## Out of scope

- A chemistry set, precursor items, or a lab process inside the hospital.
- Custom medical meshes.
- Healing as a side effect of the theft. Using the items stays on the survival and medical systems.

## Assets

Building: `Prefabs/Structures/Military/Houses/HospitalMilitary_E_01/`. Till uses `EL_RobAction` and MoneyStack. Cabinet loot, config, defaults:

- `Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et`
- `Prefabs/Items/Medicine/MorphineInjection_01/MorphineInjection_01.et`
- `Prefabs/Items/Medicine/SalineBag_01/SalineBag_US_01.et`
- `Prefabs/Items/Medicine/Tourniquet_01/Tourniquet_US_01.et`

`Prefabs/Items/Medical/Bandage.et` is not a base-game file. It is only a matcher string in `EL_Test_ShopPrices`. LNGC has no hospital robbery. See [sources](../../sources.md).

## First slice

One DebugWorld clinic: till pays cash, cabinet yields the configured medical prefabs, wanted rises, bank balance unchanged.

## Depends on

- [Forced entry](forced-entry.md) for the cabinet
- `EL_RobAction` gates for the till
- `Money`

## Proof

- Logic: cabinet loot is exactly the config list; a missing medical prefab logs and skips that line.
- World: till and cabinet both work once per cooldown.
