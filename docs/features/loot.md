# Loot

### `LF_LootRules`, `LF_LootConfig` — `Feature/Loot/` (2026-11-19, #3341, framework #1020)
- 🟡 A loot table per container kind lives in `Configs/Loot/LF_Loot.conf`: weighted rows (prefab, weight, count
  range), the fill size (`m_iRolls`, 1 to 64) and the refill time (`m_iRefillSeconds`, 0 refills at once). The core
  ships one neutral `crate` example. A pack adds, replaces, patches (fill size, refill time) or removes tables by
  overlay (`LF_LootTableList`, the shared `LF_ConfigOverlay`).
- 🟡 `LF_LootRules.Roll(table, seed, drops)` is pure: the same seed gives the same drops, a zero-weight row is
  never picked, and over many rolls the weights set the counts. `CanRefill` / `SecondsUntilRefill` take the last
  fill time and now from the caller: refused before the cooldown, allowed at it and for a container that never
  filled. An empty, zero-weight or otherwise bad table logs `LF_Debug.Error("Loot", ...)` and rolls nothing.
- Nothing in the world reads it yet: tagged containers, fill on first open and the saved fill time are the next
  slices of #1020. Proof: `LF_Test_LootRules` (`loot/rules`).
