# Loot - Design Requirements

> Context file for container loot (#1020, rules slice #3341). Player docs: `docs/features/loot.md`.

## Intent

A container kind (crate, locker, toolbox) holds a loot table: what it can contain, how full one fill
makes it and how long until it refills. A server tunes the tables in config; no code change.

## Shape

- Config: `Configs/Loot/LF_Loot.conf` read through `LF_LootConfig`. One `LF_LootTableDef` per container kind
  (`m_sKind`): weighted `LF_LootRowDef` rows (prefab, weight, count range), `m_iRolls` (the fill size, at most
  `LF_LootRules.MAX_ROLLS`) and `m_iRefillSeconds` (0 refills at once). A pack adds or tunes tables by
  overlay through the shared `LF_ConfigOverlay` (`LF_LootTableList`): add, replace, patch (fill size and refill
  time only), remove. A new or replaced table that fails `Validate` is refused with the reason.
- Rules: `LF_LootRules` is pure logic with no world, entity or clock. `Roll(table, seed, drops)` picks one row
  per roll by weight and a count inside its range from a seeded Park-Miller generator, so the same seed gives
  the same drops (a test predicts them). `CanRefill` and `SecondsUntilRefill` take the last fill time and the
  current time from the caller; refused before `m_iRefillSeconds` has passed, allowed at it, and allowed when
  the container never filled (negative last fill).
- A bad table (no rows, every weight 0, a row with no prefab, a count range turned round, a fill size outside
  1..`MAX_ROLLS`, a negative refill time) logs `LF_Debug.Error("Loot", ...)` and rolls nothing; `CanRefill` is
  false. It never throws.

## Not built yet

Tagged containers in the world, filling on first open, the saved fill time and the refill tick are later slices
of #1020. Nothing in the world reads this yet.

## Tests

`LF_Test_LootRules` (`loot/rules`, LOGIC).
