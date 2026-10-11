# Resources

Phase: Release
Extends: `Gathering`, `Processing`, `Resources`, `Trader`

## Player loop

A civilian gathers a raw good, processes it at a machine, and sells the product for cash. This is the legal work loop and it already exists in code. Release makes the chains real on the playable world.

## In scope

- Keep the chains DebugWorld already has: apples and other crops, logging, mining, and the cement, gravel, and sand lines.
- Each node names its tool, yield, and cooldown in config. Each machine names inputs and outputs in config.
- Sell remains the trader. Gather and process do not mint cash by themselves (`EL_JobManager` rewards stay zero unless a job scope says otherwise).
- A broken node (missing item prefab, empty recipe) logs and that node does nothing.

## Out of scope

- New ore meshes, new crop models, or a crafting bench. The bench is [crafting](../later/crafting.md).
- Contraband nodes. Those are [contraband](../crime/contraband.md), using this same action code.
- Dynamic market prices.

## Assets

Stay on the nodes and machines already in this repo:

| Chain | Where |
| --- | --- |
| Apple → juice | `t_malus_domestica_2s.et`, `AppleJuicer.et` (3 apples → `{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et`) |
| Plum, tomato, cabbage | `t_prunus_domestica_2s.et`, `TomatoCrop.et`, `CabbageCrop.et` |
| Log → plank + kindling | `CuttableTree_Pinus_Sylvestris_2f.et`, axe `{9CAC68BA551417A4}Prefabs/Tools/Axe/Axe.et`, `Sawmill.et` |
| Ore → bar | `MiningArea.et`, pickaxe `{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et`, `Furnace.et` (iron nugget `{6853E2E2AE878611}` → iron bar `{BCD1E8CD7F975074}`, same for copper and gold) |
| Cement, gravel, sand | piles under `Prefabs/Props/Construction/`, bags under `Prefabs/Items/Resources/` |

Prices for these paths are already in `Configs/Shop/ShopPrices.conf`. LNGC gather zones are a radius grant; do not replace `EL_GatherAction`. See [sources](../../sources.md).

## First slice

The playable world, not only DebugWorld, has one farm chain and one industrial chain a new player can finish: gather, process, sell, cash in inventory.

## Depends on

- `EL_GatherAction`, `EL_ProcessAction`, destructible nodes
- `Trader` cash payout
- [Civilian jobs](civilian-jobs.md) only if a job requires the chain; the chain itself works while unemployed

## Proof

- The existing gather and process guard tests stay green.
- World: one farm chain and one industrial chain on the playable world pay cash at the trader.
- A malformed recipe still refuses before it changes inventory.
