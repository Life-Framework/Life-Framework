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

## Containers (#3343)

- `LF_LootContainerComponent` on a prop names its table kind (`m_sKind`). The held "Search" action
  (`LF_LootSearchAction`, prefab Duration 3 s) asks the server: `Search(nowSeconds, seed)` fills the prop's
  storage from the table through its `LF_LootStorageManagerComponent`. The items are spawned into the
  storage, never handed to the searcher; the container then opens through the normal "Open storage" action.
- The decision: never filled, fill; holds items, `STILL_STOCKED` (nothing added); empty inside the cooldown,
  `COOLDOWN`; empty after it, refill. The fill time is stamped before anything spawns and the call is one
  synchronous server step, so two searches at once fill once. A replicated `m_bFilled` lets a client hide
  "Search" on a stocked container; the server never trusts it.
- Nothing is saved: after a restart every container starts unsearched. `Prefabs/Props/Loot/LF_LootCrate.et`
  (kind `crate`) is the core example; a pack adds containers by prefab and kinds by table overlay.

## Not built yet

A pack's tagged containers in the world and the zone-tuned tables are later slices of #1020.

## Tests

`LF_Test_LootRules` (`loot/rules`, LOGIC), `LF_Test_LootContainer` (`loot/container`, WORLD).
