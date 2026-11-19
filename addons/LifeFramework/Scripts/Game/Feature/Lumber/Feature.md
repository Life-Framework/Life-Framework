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
- `LF_FelledTreesReplication` (#3261): the server table of felled keys, a broadcast RPC per change, the
  join snapshot in the game mode `RplSave`/`RplLoad`, and the client that hides or shows the map tree a key names.
- Nothing in the world fells a tree yet. The axe action is #3260, replication #3261, regrow and save
  #3262.

## Dependencies

- `Gathering` (the event shape `LF_OnGathered`), `Jobs` (the `gather_logging` and `fell_hardwood` awards).
- `Moderation` (`LF_Zones.IsSafe`), `Farming` (`LF_PlantRules.IsHardSurface` for roads).
- `Processing` (the sawmill), `Market` (buyers with demand, the price curve).
