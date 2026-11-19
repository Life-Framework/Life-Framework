# Lumber: design requirements

> Context file for felling real map trees, carrying logs and the lumber buyers. The full design
> and numbers are in `docs/roadmap/lumber.md`; the slice plan is `docs/roadmap/lumber-slices.md`
> (cards #3259 to #3275 under #3169).

## Intent

Lumber happens in the world, on the island's own trees. A player fells a real map tree with an axe,
shoulders a log, hauls it by hand or by truck to a yard, and saws it into planks, boards, posts or
firewood. Premium buyers take a limited demand at a good price; the harbour warehouse takes
everything on a falling curve.

## Current state

- `LF_TreeFelling` (rules, #3259): it reads a base-game tree prefab path as a wood class (softwood,
  hardwood, none) and a size, gives the ordered refusals, and makes the stable tree key (position to
  0.1 m plus prefab).
- `LF_TreeFellingConfig`: the species and yield tables, `Configs/Lumber/LF_TreeSpecies.conf`.
- `LF_TreeFellingLedger`: felled keys and their regrow time, in whole seconds on a caller clock.
- `LF_FellAction` (#3260): the hold action and `TryFell`, the server fell. It re-checks every rule from
  facts it measures (axe in the hands, distance, safe zone, road and building clearance via
  `LF_FelledTrees`, stamina), hides the tree by flags, records the key in the server ledger
  (`LF_FelledTrees`), drops one `WoodLog_01_Small` per log beside the trunk, takes the stamina, pays XP
  (`gather_logging` through `LF_OnGathered` family LOGGING for softwood; `fell_hardwood` directly for
  hardwood), calls `LF_FelledTreesReplication.Publish` so every client hides the tree, and publishes
  `LF_OnTreeFelled` (`LF_LumberEvents.c`). The ledger only grows here.
- `LF_FellPrompt` (#3554): while the local player holds the axe, a client-local helper (`Prefabs/Lumber/LF_FellHelper.et`, no
  `RplComponent`) carries `LF_FellAction` on the nearest standing fellable tree within reach. Completing the hold sends the tree's
  position to the server (`SCR_PlayerController.LF_AskFell`), which finds the tree itself and runs `TryFell`. The stamina cost runs
  on the owner (`LF_SpendFellStamina`, an Owner RPC).
- Not yet: regrow and save (#3262, which also calls `LF_FelledTreesReplication.Retract`).
- `LF_FelledTreesReplication` (#3261): the server table of felled keys, a broadcast RPC per change, the
  join snapshot in the game mode `RplSave`/`RplLoad`, and the client that hides or shows the map tree a key names.

## Dependencies

- `Gathering` (the event shape `LF_OnGathered`), `Jobs` (the `gather_logging` and `fell_hardwood` awards).
- `Moderation` (`LF_Zones.IsSafe`), `Farming` (`LF_PlantRules.IsHardSurface` for roads).
- `Processing` (the sawmill), `Market` (buyers with demand, the price curve).
