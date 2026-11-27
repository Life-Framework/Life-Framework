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

### `LF_LootContainerComponent`, `LF_LootSearchAction` — `Feature/Loot/` (2026-11-19, #3343, framework #1020)
- 🟡 A prop with `LF_LootContainerComponent` (`m_sKind` names its table) is searched with a short held "Search"
  (3 s). The first search fills its storage on the server from the table; it then opens as normal storage through
  "Open crate". The core ships `Prefabs/Props/Loot/LF_LootCrate.et` (kind `crate`).
- 🟡 An emptied container refills on the next search once the table's refill time has passed; while it still holds
  items, or inside the cooldown, a search adds nothing (a toast says so, with the minutes left). Two searches at
  once fill it once. Nothing is saved: a restart leaves every container unsearched. Proof: `LF_Test_LootContainer`
  (`loot/container`); picture: `look grid:loot`.

### Everon 1989 loot tables — `LF_Everon1989/Configs/Loot/LF_Loot_Everon1989.conf` (2026-11-19, #3344, framework #1020)
- 🟡 The pack adds four tables over the core `crate` one (listed in the mission header, merged by `LF_LootConfig.ApplyLayers`): `home`,
  `backroom` (a shop's back room), `shed` and `dump`. Period goods only: tinned food, jars, chocolate, bread, soda, a flashlight, brick,
  an entrenching tool and iron nuggets as scrap (no copper nugget: the island has no copper). The dump holds the most scrap, the shed the tools; a find above a few dollars
  (flashlight, entrenching tool, iron nugget) is a one-in-twenty roll. Every table refills after 3600 s.
- 🟡 Pay: an hour of searching, at the rate the cooldowns allow, sells for under half of the parcel job's hour at the shop's sell prices
  (the dump about $150 an hour, a home about $125). No coin item exists yet, so no table pays small cash; that wants a coin prefab (follow-up).
  Proof (pack leg): `LF_Test_EveronLootTables` (`everon/loot-tables`). Containers placed in the world are later slices.
- 🟡 Scrap drops (#3844): the five scrap items of #3823 (scrap metal, copper wire coil, rubber scrap, worn hand tool, old radio) are rows in
  the tables. The dump holds the most scrap metal, rubber and old radios, sheds the worn hand tools and wire, and a home an occasional old
  radio (one weight in 22). To keep the pay rule the dump now refills every 5400 s (the other tables stay 3600). The pay guard values the
  five items at their catalogue sell price (5, 15, 5, 20, 40), since the shop manager prices an unlisted prefab at the config default of 25.
  Proof: `LF_Test_EveronLootTables` asserts each item drops in the expected tables and the pay guard still passes.

### Everon 1989 loot props — `LF_Everon1989/Prefabs/Props/Loot/` (2026-11-19, #3345, framework #1020)
- 🟡 Three searchable props over vanilla models, each with `LF_LootContainerComponent`, storage and the Search and Open actions of the core crate:
  `LF_LootToolChest` (a red toolbox, kind `shed`), `LF_LootKitchenCupboard` (the old cupboard, kind `home`) and `LF_LootScrapPile` (the large junk pile, kind `dump`).
  Names are pack strings (`LF-Loot_ToolChestName`, `LF-Loot_KitchenCupboardName`, `LF-Loot_ScrapPileName`). Not placed anywhere yet: the island fit cards place them.
  The toolbox and the pile carry an explicit `RplComponent` (their vanilla chain has none at runtime, so the storage component refused the prefab).
  Proof (pack leg): `LF_Test_EveronLootTables` spawns each, checks the kind names a pack table and one search fills it.
