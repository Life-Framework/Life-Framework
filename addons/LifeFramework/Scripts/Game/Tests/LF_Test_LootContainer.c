// red-proof: observed 2026-11-19 `tools\cli test --no-build --red LF_Test_LootContainer` printed `RED PROOF OK` (the hook skips the first search, so the fill, stock, cooldown and refill assertions fail); `--only` then passed 31 assertions.
//! Loot container (#3343, slice of #1020): the first search fills the storage from the kind's table, a
//! second search while stocked or in the cooldown adds nothing, an emptied container refills once the
//! cooldown passed, and two searches at the same moment fill it once.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LootContainer : LF_Test
{
	protected static const ResourceName CRATE_PREFAB = "{4EC3252A043DB67F}Prefabs/Props/Loot/LF_LootCrate.et";
	protected static const int SEED = 424242;
	protected static const int T0 = 5000;

	override string GetName()
	{
		return "loot/container";
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountItems(LF_LootContainerComponent container)
	{
		BaseInventoryStorageComponent storage = LF_Component<BaseInventoryStorageComponent>.Find(container.GetOwner());
		if (!storage)
			return -1;

		array<IEntity> items = {};
		return storage.GetAll(items);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Empty(LF_LootContainerComponent container)
	{
		BaseInventoryStorageComponent storage = LF_Component<BaseInventoryStorageComponent>.Find(container.GetOwner());
		if (!storage)
			return;

		array<IEntity> items = {};
		storage.GetAll(items);
		foreach (IEntity item : items)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected LF_LootContainerComponent SpawnCrate(LF_TestContext ctx, vector origin)
	{
		IEntity crate = LF_Utils.SpawnEntityPrefab(CRATE_PREFAB, origin);
		ctx.NotNull(crate, "the loot crate spawns");
		if (!crate)
			return null;

		LF_LootContainerComponent container = LF_Component<LF_LootContainerComponent>.Find(crate);
		ctx.NotNull(container, "the crate carries LF_LootContainerComponent");
		ctx.NotNull(LF_Component<InventoryStorageManagerComponent>.Find(crate), "the crate has a storage manager the server fills through");
		ctx.NotNull(LF_Component<BaseInventoryStorageComponent>.Find(crate), "the crate has a storage");
		if (!container)
			SCR_EntityHelper.DeleteEntityAndChildren(crate);

		return container;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity anchor = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(anchor, "Annex_AppleTradeContainer resolves in the DebugWorld");
		if (!anchor)
			return;

		LF_LootConfig config = LF_LootConfig.Get();
		ctx.NotNull(config, "the loot config loads");
		if (!config)
			return;

		LF_LootTableDef table = config.FindTable("crate");
		ctx.NotNull(table, "the crate loot table exists");
		if (!table)
			return;

		array<ref LF_LootDrop> expected = {};
		LF_LootRules.Roll(table, SEED, expected);
		ctx.True(expected.Count() > 0, "the seeded table rolls something");

		LF_LootContainerComponent container = SpawnCrate(ctx, anchor.GetOrigin() + Vector(4, 0.1, 4));
		if (!container)
			return;

		ctx.Equal(0, CountItems(container), "a placed crate starts empty");
		ctx.True(container.CanOfferSearch(), "a never-searched crate offers Search");

		// First search fills it.
		if (!RedProof())
			ctx.True(container.Search(T0, SEED) == LF_ELootSearch.FILLED, "the first search fills the crate");

		int filled = CountItems(container);
		ctx.True(filled > 0, "the crate holds items after the first search");
		ctx.True(filled <= expected.Count() * 4, "the fill is inside what the table can roll");

		// A second search while it is stocked adds nothing, even a long time later.
		ctx.True(container.Search(T0 + 1, SEED + 1) == LF_ELootSearch.STILL_STOCKED, "a second search while stocked adds nothing");
		ctx.Equal(filled, CountItems(container), "the stocked crate holds the same items");
		ctx.True(container.Search(T0 + table.m_iRefillSeconds + 60, SEED + 2) == LF_ELootSearch.STILL_STOCKED, "a search after the cooldown still adds nothing while the crate holds items");
		ctx.Equal(filled, CountItems(container), "still the same items");

		// Emptied inside the cooldown: no refill.
		Empty(container);
		ctx.Equal(0, CountItems(container), "the crate is emptied");
		ctx.True(container.CanOfferSearch(), "an emptied crate offers Search again");
		ctx.True(container.Search(T0 + table.m_iRefillSeconds - 1, SEED) == LF_ELootSearch.COOLDOWN, "a search inside the cooldown adds nothing");
		ctx.Equal(0, CountItems(container), "the crate stays empty inside the cooldown");
		ctx.True(container.SecondsUntilRefill(T0 + table.m_iRefillSeconds - 1) == 1, "one second is left of the cooldown");

		// After the cooldown it refills, from the same seed to the same fill.
		ctx.True(container.Search(T0 + table.m_iRefillSeconds, SEED) == LF_ELootSearch.FILLED, "an emptied crate refills once the cooldown has passed");
		ctx.Equal(filled, CountItems(container), "the same seed refills the same items");

		// Two searches at the same moment fill a fresh crate once.
		LF_LootContainerComponent second = SpawnCrate(ctx, anchor.GetOrigin() + Vector(6, 0.1, 4));
		if (second)
		{
			ctx.True(second.Search(T0, SEED) == LF_ELootSearch.FILLED, "the first of two simultaneous searches fills");
			int once = CountItems(second);
			ctx.True(second.Search(T0, SEED) == LF_ELootSearch.STILL_STOCKED, "the second simultaneous search adds nothing");
			ctx.Equal(once, CountItems(second), "two simultaneous searches filled the crate once");
			SCR_EntityHelper.DeleteEntityAndChildren(second.GetOwner());
		}

		SCR_EntityHelper.DeleteEntityAndChildren(container.GetOwner());
	}
};
