# Bank robbery

Phase: Release
Extends: [forced entry](forced-entry.md), `Money`, `Crime`, hand-carry

## Player loop

Civilians enter a bank, breach the vault, and carry cash out in bags. The haul is physical cash, not a bank-balance credit. Police are already alerted by the charge.

## In scope

- A bank is a building override: teller floor plus a vault target that uses forced entry.
- On open, the vault spawns a config number of money bags.
- A bag is a container of MoneyStack items. The player picks it up with the existing hand-carry. Dropping it leaves the cash in the world.
- Bag count and cash per bag are config. The server spawns them. A client cannot ask for a larger haul.
- Minimum police on duty, cooldown, and civilian-only use the same gates as `EL_RobAction`.
- Empty vault until the cooldown refills it.

## Out of scope

- Silent drilling minigames, laser halls, or a custom vault mesh.
- Paying the robber by depositing into their ATM account.
- Hostage AI.

## Assets

There is no bank building in the base game. The shell is `Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/` or `ShopModern_E_01/`, with `EL_Door_Base` as the vault. Cash is `{5439738849229352}Prefabs/Items/Currencies/MoneyStack.et`. The bag is `Prefabs/Items/Core/HandCarryItem_Base.et` (`EL_HandCarryComponent`) filled with those stacks. The black-market crate mesh `Assets/Props/Crates/Crate_01.xob` is the world prop if the bag is dropped as a container.

Payout still goes through `EL_MoneyUtils`, under `EL_RobAction` gates. LNGC has no vault robbery. See [sources](../../sources.md).

## First slice

One DebugWorld bank. Breach, take one bag, cash is in inventory, bank account balance does not change, wanted rises once.

## Depends on

- [Forced entry](forced-entry.md)
- `Money` (`EL_MoneyUtils`)
- `Crime` gates and wanted
- Character hand-carry

## Proof

- Logic: payout never touches `EL_BankAccount`; over-capacity spawn is capped by config.
- World: bag in hand contains the configured cash; a second breach before cooldown spawns nothing.
- Persistence: a bag left on the ground is world loot, not a second bank balance.
