# Contraband

Phase: Release
Extends: `Gathering`, `Processing`, `Trader`, [hideouts](../ownership/hideouts.md), `Police` confiscation

## Player loop

An organization gathers a configured illegal good, processes it on the machine in their hideout, and runs it by vehicle to a buyer. Police who confiscate a wanted player take those items.

## In scope

- One example good: a base-game item marked contraband in config. The example is enough; servers swap the prefab.
- Gather node and process recipe use the existing gather and process actions. The process action only succeeds at a hideout the player's organization owns.
- Sell uses the existing trader path. The black-market trader already rejects non-civilian factions.
- First organization job: a convoy. Load the goods into a base-game vehicle at the hideout, drive to a placed drop, get paid in cash on delivery. Leaving the vehicle or dying fails the run and logs it.
- Confiscation, today limited to weapons, also removes items flagged contraband from a wanted player in range.

## Out of scope

- A dedicated island, a custom drug mesh, or a chemistry recipe.
- Real-world production steps. The recipe is "input prefab, output prefab" in config, the same shape as apple to juice.
- Turf wars, lab defense AI, or a second illegal job track beside the organization.

## Assets

Process on `Prefabs/Processor/ItemProcessor_Base.et` (the juicer, furnace, and sawmill are the working examples). Sell at `Prefabs/Trader/BlackMarketTrader.et`. Confiscation extends `EL_ConfiscateAction`.

The example recipe is moonshine-shaped, taken from LNGC `Configs/Economy/CraftingRecipes.conf`: configured input prefabs in, one output prefab out, at a hideout machine. The stash prop is LNGC `Prefabs/Props/Crates/ShineStash.et`. Search uses the request shape of `LNGC_SearchAction` (police, server validates) but checks inventory prefabs, not LNGC virtual item ids.

Do not port the marijuana license or the hidden drug-dealer shop as this example. See [sources](../../sources.md).

## First slice

One gather node, one hideout machine, one drop point. A member processes a stack and completes one convoy for cash. A non-member using the machine is refused and logged. An on-duty officer confiscates the flagged item.

## Depends on

- [Organizations](../ownership/organizations.md) and [hideouts](../ownership/hideouts.md)
- `Gathering`, `Processing`, `Money`
- `EL_ConfiscateAction`
- [Vehicles](../ownership/vehicles.md) only for the lock and keys if the convoy vehicle is owned; the first slice can use a job vehicle spawned for the run

## Proof

- Logic: non-member process refuses; delivery pays cash once; confiscation removes only flagged items.
- World: gather, process, drive, deliver on DebugWorld.
- Persistence: a half-processed stack in the hideout survives a restart.
