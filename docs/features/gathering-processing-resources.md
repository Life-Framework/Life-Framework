# Gathering / Processing / Resources

### `LF_GatherAction` — `Feature/Gathering/LF_GatherAction.c`
- ✅ **Carried tools count (pickaxe-count).** `LF_InventoryUtils.GetAmount`/`RemoveAmount` now also search the equipped-weapon storage (a pickaxe is a weapon-slot item the deposit count skips), each carried item once; `LF_GatherAction.HasToolIn` is the shared tool check (GUID compare). No shipped prefab sets the inventory mode and ore is mined by damage with the pickaxe in hand, so play was not affected. Test `tool/pickaxe-count`.
- ✅ Interact action granting `m_GatherAmount` of `m_GatherItemPrefab` into
  the user inventory; tool requirement (right hand / left gadget / anywhere
  in inventory); per-resource gather counter + cooldown restock.
- ✅ Server re-validates tool, crop stage, cooldown, and malformed item/amount
  configuration in `PerformAction` (verified 2026-08-30). Remaining: timeout
  attribute says "ms" but is compared as seconds-compatible — fragile.
- ✅ The server applies a gather when no peer owns the prop, and a grant that
  adds nothing is an error log (verified 2026-10-10). A sandbag the inventory
  rejects, or a nugget with no usable slot, used to look like a successful use.
- ✅ A node id copies tool, yield, and cooldown from `ResourceChains.conf`.
- ✅ **Crop growth runs on the server only** (2026-11-08, #157): `LF_BaseCrop`
  starts its growth clock only on the server (`Replication.IsServer()` and
  `RplSession.Mode()` is not `Client`), not on
  `RplComponent.IsOwner()`, which is still true on a client while the world
  loads. Every stage read goes through `LF_BaseCrop.StageAt`, which returns
  null for an index outside the list. Before this each game client logged a
  `NULL pointer to instance` VME in `LF_BaseCrop.EOnFrame` for every crop.
  Each crop logs `crop <name> init: grows here <bool> (rpl mode <mode>)` once.
  A missing row or a prefab that does not resolve logs `LF_Debug.Error` and
  grants nothing (verified 2026-10-11). The action stays on the node.

### `LF_FarmPlotComponent` + `LF_FarmPlotAction` + `LF_FarmPlots` — `Feature/Farming/`
- ✅ **Seed to harvest in dirt plots (#614).** `Prefabs/Vegetation/Crops/FarmPlot_Cabbage.et` is a vanilla small vegetable bed with `LF_FarmPlotComponent`: an empty plot offers *Plant Cabbage seeds* (`LF_FarmPlotAction` PLANT, needs a `Prefabs/Items/Seeds/CabbageSeeds.et` packet, one is used), the crop grows on the server through the reused `LF_CropStage` list (sprouts, half grown, ripe; one minute a stage in the prefab, so DebugWorld ripens in two minutes; the last stage holds, a ripe plot waits), the prompt reads *Growing: <stage>* until it is ripe, then *Harvest Cabbage* (HARVEST) gives `m_iHarvestAmount` (3) cabbages, the rest drops at the farmer's feet when the pockets are full, and the plot is empty again. The harvest reports `LF_JobManager.OnGatherCompleted` like a gather. Every machine but a dedicated server draws the crop as local, physics-free `FarmPlot_CabbageVisual.et` copies on `m_sSlots` (six on a bed), scaled per stage from the replicated stage index (`RplProp`, bumped only on a stage change). The server checks reach (3 m), the packet and the stage; refusals toast `#LF-Farm_*` and log `[LFDebug:Farming] plot <key> refused`. The growth clock is one 1 s `LF_FarmPlots.Tick` over the live plots (world-time delta), stopped with the last plot. Seeds come from `Prefabs/Props/Commercial/LF_SeedCrate_Cabbage.et` (`LF_GatherAction` *Take Cabbage seeds*, 4 then a 2 min refill; the gather action gained `m_sActionFormat` for the prompt). DebugWorld: `FarmPlot_1..3` at (145/148.5/152, 299.5), `Farm_SeedCrate`, and the `farm_plots` chalkboard `Sign_FarmPlots` replace the freestanding cabbages (`CabbageCrop_Debug`, `CabbageCrop_Bed`), which stopped being gatherable for good 1.4 min after boot. Tests `farm/plot-rules` (LOGIC), `farm/plot-loop` (WORLD), `persist/farm-plots-roundtrip`; proof `session.mjs farm-proof`.
- ✅ **Growth survives a restart** (save version 23, payload `farmplots`, `LF_FarmPlotRow`: plot key, crop id, whole seconds grown). Only planted plots are saved; a row whose plot is not in the world waits and is saved again; a row for a plot that now grows another crop is dropped with a warning. Growth counts only while a server runs. `wt e2e` pass 2 reads `plot farm_plot_3 restored: cabbage grown 81 s, stage 1`.
- ⚠️ Follow-ups: one crop (cabbage); seeds are free from the crate (no seed shop line yet); the seed packet wears the personal-papers mesh (placeholder in `docs/asset-inventory.md`); a client that joins late relies on the stage `RplProp` callback to draw the crop (manual check).

### `LF_ProcessAction` — `Feature/Processing/LF_ProcessAction.c`
- ✅ Recipe: consume all `m_aProcessingInputs`, produce all
  `m_aProcessingOutputs` (into inventory or force-dropped).
- ✅ **Free-output exploit closed** (verified 2026-08-30): malformed recipes,
  empty input/output lists, missing inputs, and non-positive amounts are
  rejected before inventory mutation. Regression coverage is in
  `LF_Test_ActionGuards`.
- ✅ A machine id copies inputs and outputs from `ResourceChains.conf`. An
  empty recipe or a prefab that does not resolve logs `LF_Debug.Error` and
  returns before inventory changes (verified 2026-10-11).
- ✅ Cocaine, jam, and beer are one-input, one-output recipe prefabs under
  `Prefabs/Processing/Chains/`, each parented to `ItemProcessor_Base`
  (verified 2026-10-11). Cocaine consumes coca leaves and produces a cocaine
  pouch. Jam consumes rowan berries and produces berry jam. Beer consumes an
  empty beer bottle and produces a beer bottle. `LF_Test_RecipeChains` checks
  that each input prefab and output prefab is set and distinct.
  ⚠️ The beer output is the real `Drinks/BeerBottle.et` (#2001, `drinks/alcohol`), but
  `Items/Resources/EmptyBeerBottle.et`, the input, does not exist yet, so the beer processor cannot run.

### `LF_ResourceChainCatalog` — `Feature/Resources/LF_ResourceChainCatalog.c`
- ✅ `Configs/Resources/ResourceChains.conf` names each gather node's tool,
  yield, and cooldown, and each machine's inputs and outputs (verified
  2026-10-11). Yields are item prefabs. Gather and process do not mint cash.
  The playable iron, copper, and gold rows name the pickaxe. The log row does
  not: the axe stays on `CuttableTree_Base`, and `LogPile_Debug` grants a log
  with no held tool so the sawmill still runs before the axe pickup.
- ✅ **Tomato chain removed.** The tomato crop, item, seed, `TomatoChain.layer` and its trader are gone (poor custom asset). The DebugWorld farm keeps apples and plums plus a cabbage bed (`CabbageCrop_Debug`). `EHarvestJobType` keeps stable values (1 was TOMATOES); license value 1 is now `FARMER_FIELD`. Saved tomato items load skipped with a `restore skipped missing prefab` error, no VME (test `persist/removed-tomato-skipped`).
- ✅ MainWorld's St Philippe farm (`Area_StPhillipe_AppleFarm.layer`) places
  the apple trees, the juicer, and a juice trader (15 cash, matching the
  DebugWorld juice crate), plus iron ore, a pickaxe, the furnace, and an
  iron-bar trader (45 cash, the ShopPrices iron-bar sell price). That iron
  node requires the pickaxe in hand. DebugWorld keeps Apple, Plum,
  Logging, Mining, Cement, Gravel, and Sand. `IronOre_Debug` uses
  `iron-debug` (4 nuggets per piece) so one piece still feeds both furnaces.
  The furnace (`Prefabs/Processor/Furnace.et`) wears the base-game `BoilerLarge_02` mesh instead of the sawmill chipper (#593); its recipes are unchanged.
  `GoldOre_Debug` and `CopperOre_Debug` use the debug rows with the prefab
  yields. Every ore rock still needs the pickaxe (the hit zone's tool list).

### `LF_DestructibleResourceComponent` + `LF_DestructibleResourceHitZone` — `Feature/Resources/`
- ✅ Damage gated to configured tools; tool → per-hit damage; FX on hit. The
  tool match compares prefab GUIDs, so path casing no longer matters. A melee
  hit from a tool the resource does not list logs `[LFDebug:Resources]` and
  deals 0 (2026-11-08).

### `LF_OreNodeComponent` + `LF_OreYield` — `Feature/Mining/` (2026-11-08)
- ✅ You mine ore by swinging the pickaxe at the rock; there is no gather
  prompt. The hit goes through `LF_DestructibleResourceHitZone` (pickaxe 20
  damage, the axe 0). Every 60 damage (three swings) a piece breaks off and the
  node row's yield drops as real nugget items beside the rock, toward the
  miner. After the row's `m_iMaxGathers` pieces the rock crumbles (hidden, no
  collision) and grows back after the row's `m_fCooldownMs`. Server-
  authoritative; clients get the hit dust, a shake and the vanilla rock-break
  sound by broadcast, and the spent state by RplProp (JIP-safe). The spent
  state is not saved. Ported from thebonbon `LF_ToolHitZone` and
  `LF_RandomPrefabSpawnable` with permission. Tests: LOGIC `mining/ore-yield`,
  WORLD `mining/pickaxe-hits`.
  2026-11-10 (#592): a rock shrinks one equal step per piece (`m_fMinScale`, default 0.45, replicated float
  `m_fRockScale` so a late joiner sees a half-mined rock), vanishes when spent and grows back after the cooldown.
  Dropped nuggets are placed by a trace straight down (`FindGroundY`) and rest on the ground. Nugget and pickaxe
  inventory preview cameras were refit (nuggets match the bars; pickaxe distance scaled from the axe by mesh length).
  `LF_MiningArea` no longer logs `spawned 0/0 ores` every minute when the area is full.
  Proof (#596, #598): `agent-box/session.mjs mining-proof [--regrow]` mines `IronOre_Debug` to nothing with real pickaxe swings, then smelts four nuggets (the harness `nuggets` verb) at the furnace.
- ✅ Nuggets and bars render textured: `LF_QuantityComponent.UpdateStackModel`
  no longer re-sets the base mesh, which dropped the prefab's material
  overrides and rendered the items white. Cash, pickaxe, axe, ore and bar
  inventory previews use the Front preset with a narrow FOV.
- ✅ Each ore and metal reads at a glance (2026-11-14): one cast ingot model for the three bars
  (`Assets/Resources/Mining/Ingot/`), one fractured lump for the three nuggets
  (`Assets/Resources/Mining/Nugget/`), each with a material per metal, and the iron, copper and gold
  rocks show their ore as veins on the granite (`Assets/Resources/Mining/OreVein/`). Made in Blender
  from CC0 textures by `tools\cli asset` (`tools/assets/metal-ingot`, `ore-nugget`, `ore-veins`);
  prefab and emat GUIDs are unchanged, so mining, smelting and selling are untouched.
  Follow-up: the gold nugget and gold rock now read as dull ochre-yellow ore with gold flecks (they were white quartz and previewed pinkish-white); the copper nugget stays dark rock with malachite green. Close-up of each rock: `tools\cli look 182,175 --radius 8`.
  Natural pass (2026-11-14): the camo-paint look is gone. The host is darkened, desaturated Rock 058, and the ore sits only in the rock's own crevices and thin veins: rust-brown staining on dark rock (iron), a muted green malachite crust with a few azurite specks (copper), thin cream quartz veins with yellow glints (gold).
- ⚠️ The pickaxe uses the pistol rig and is gripped at the handle end;
  `HandAdditiveTranslation` has no effect on a weapon, so moving the grip also
  needs a re-exported mesh with the pivot up the handle.

### `LF_MiningSpawn` + `LF_MiningForge` — `Feature/Mining/`
- ✅ Weighted ore spawn. A roll in `[0, total)` maps onto the first bin, including
  a roll of 0. The spawn count is the shortfall to the configured amount, and
  an over-full area spawns nothing. An empty list logs `LF_Debug.Error`.
- ✅ Timed forge. One iron nugget
  (`{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et`) becomes one iron
  bar (`{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et`) after 5 seconds
  (5000 ms). The nugget stays until the delay elapses. A missing nugget, a
  second start, or a nugget removed before the end logs `LF_Debug.Error` and
  yields no bar. A zero delay finishes inside the start.

