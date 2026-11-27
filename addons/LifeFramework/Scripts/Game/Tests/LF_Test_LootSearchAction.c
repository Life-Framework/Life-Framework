// red-proof: Observed 2026-11-27 in the batched `tools\cli test --no-build --red` run of the six round-5 action tests: `RED PROOF OK (all 6 listed test(s) fail with the hook on)`; here `the crate holds items after the first search`. Green with `--only`.
// tier: WORLD
//! loot/search-action (#3343, coverage round 5): the held "Search" prompt on a loot crate. The perform
//! runs on the server against the crate's own component: the first search fills the crate from its table,
//! a second search while it is stocked adds nothing, an emptied crate inside its cooldown stays empty, and a
//! forged perform with no searcher fills nothing. The prompt reads the crate's own state.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_LootSearchAction : LF_Test
{
	protected static const ResourceName CRATE_PREFAB = "{4EC3252A043DB67F}Prefabs/Props/Loot/LF_LootCrate.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "loot/search-action";
	}

	//------------------------------------------------------------------------------------------------
	protected static int CountItems(IEntity crate)
	{
		BaseInventoryStorageComponent storage = LF_Component<BaseInventoryStorageComponent>.Find(crate);
		if (!storage)
			return -1;

		array<IEntity> items = {};
		return storage.GetAll(items);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Empty(IEntity crate)
	{
		BaseInventoryStorageComponent storage = LF_Component<BaseInventoryStorageComponent>.Find(crate);
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
	override void Run(LF_TestContext ctx)
	{
		IEntity searcher = LF_TestCharacterFixture.Spawn(ctx);
		if (!searcher)
			return;

		IEntity crate = LF_Utils.SpawnEntityPrefab(CRATE_PREFAB, searcher.GetOrigin() + "2 0.1 0");
		ctx.NotNull(crate, "the loot crate spawns");
		LF_LootSearchAction action = LF_LootSearchAction.Cast(LF_TestActionFinder.Find(crate, LF_LootSearchAction));
		ctx.NotNull(action, "the crate carries the search action");
		LF_LootContainerComponent container;
		if (crate)
			container = LF_Component<LF_LootContainerComponent>.Find(crate);

		ctx.NotNull(container, "the crate carries LF_LootContainerComponent");
		if (!action || !container)
		{
			Cleanup(searcher, crate);
			return;
		}

		ctx.Equal(0, CountItems(crate), "a placed crate starts empty");
		ctx.True(action.CanBeShownScript(searcher), "a never-searched crate offers Search");
		ctx.True(action.CanBePerformedScript(searcher), "a searcher may perform it");
		ctx.False(action.CanBePerformedScript(null), "no searcher cannot perform it");

		// A forged perform with no searcher fills nothing.
		action.PerformAction(crate, null);
		ctx.Equal(0, CountItems(crate), "a perform with no searcher fills nothing");

		// The first real search fills the crate.
		if (!RedProof())
			action.PerformAction(crate, searcher);

		int filled = CountItems(crate);
		ctx.True(filled > 0, "the crate holds items after the first search");

		// A second search while it is stocked adds nothing.
		action.PerformAction(crate, searcher);
		ctx.Equal(filled, CountItems(crate), "a second search while stocked adds nothing");

		// Emptied inside the cooldown: the crate offers the prompt again but the perform adds nothing.
		Empty(crate);
		ctx.Equal(0, CountItems(crate), "the crate is emptied");
		ctx.True(action.CanBeShownScript(searcher), "an emptied crate offers Search again");
		action.PerformAction(crate, searcher);
		ctx.Equal(0, CountItems(crate), "a search inside the cooldown adds nothing");

		Cleanup(searcher, crate);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity searcher, IEntity crate)
	{
		if (crate)
			SCR_EntityHelper.DeleteEntityAndChildren(crate);
		if (searcher)
			LF_TestCharacterFixture.Delete(searcher);
	}
}
