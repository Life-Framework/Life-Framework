// red-proof: RED-OK processing/recycler (observed 2026-11-19 `tools/cli test --no-build --red LF_Test_Recycler`: with the collect skipped the juice never arrives: `half of the four juice come back, once (expected 2, got 0)`)
//! The recycler bench (#3349), driven through the server methods its three actions call: a bench with a
//! recipe for an apple (4 juice at 50 %). An item with no recipe is refused in words and nothing is taken;
//! a recipe item that is not carried is refused; loading takes the apple; a second load is refused while
//! the bench is busy; collecting early is refused; two work calls from one worker faster than a hold are
//! refused; four spaced work calls finish it; collecting gives 2 juice once; collecting twice is refused
//! and gives nothing more. The test spawns its own bench and worker and deletes both.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_Recycler : LF_Test
{
	protected static const ResourceName BENCH = "{F1984B044B2AF24B}Prefabs/Props/Processing/LF_Recycler.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";

	override string GetName()
	{
		return "processing/recycler";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ItemCatalogueConfig Catalogue()
	{
		LF_ItemCatalogueConfig catalogue = new LF_ItemCatalogueConfig();
		catalogue.m_aItems = {};
		catalogue.m_aItems.Insert(LF_CatalogueItem.Make("test_apple", APPLE, "equipment", 40, 20, "legal"));
		catalogue.m_aItems.Insert(LF_CatalogueItem.Make("test_juice", JUICE, "equipment", 10, 5, "legal"));
		return catalogue;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_RecycleConfig Recipes()
	{
		array<ref LF_RecyclePart> parts = {};
		parts.Insert(LF_RecyclePart.Make("test_juice", 4));
		LF_RecycleConfig config = new LF_RecycleConfig();
		config.m_aRecipes = {};
		config.m_aRecipes.Insert(LF_RecycleRecipe.Make("test_apple", 50, parts));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		if (!worker)
			return;

		IEntity bench = LF_Utils.SpawnEntityPrefab(BENCH, worker.GetOrigin());
		LF_RecyclerComponent recycler;
		if (bench)
			recycler = LF_Component<LF_RecyclerComponent>.Find(bench);

		ctx.NotNull(recycler, "the bench prefab spawns with LF_RecyclerComponent");
		if (!recycler)
		{
			if (bench)
				SCR_EntityHelper.DeleteEntityAndChildren(bench);

			LF_TestCharacterFixture.Delete(worker);
			return;
		}

		LF_ItemCatalogueConfig catalogue = Catalogue();
		LF_RecycleConfig config = Recipes();
		ctx.Equal(0, config.Sanitize(catalogue), "the test recipe passes the load check");
		LF_ItemCatalogueConfig.UseForTest(catalogue);
		LF_RecycleConfig.UseForTest(config);

		RunBench(ctx, recycler, worker);

		LF_RecycleConfig.Reload();
		LF_ItemCatalogueConfig.Reload();
		SCR_EntityHelper.DeleteEntityAndChildren(bench);
		LF_TestCharacterFixture.Delete(worker);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBench(LF_TestContext ctx, LF_RecyclerComponent recycler, IEntity worker)
	{
		ctx.Equal(LF_StationProgress.EMPTY, recycler.GetPercent(), "the bench starts empty");

		// An item with no recipe is refused in words, and nothing is taken.
		LF_InventoryUtils.AddAmount(worker, APPLE, 1);
		int applesBefore = LF_InventoryUtils.GetAmount(worker, APPLE);
		ctx.True(applesBefore >= 1, "the worker carries an apple");
		ctx.Equal(LF_ERecycleCheck.NO_RECIPE, recycler.ServerLoadItem(worker, "test_juice", 100), "an item with no recipe is refused");
		ctx.EqualStr("#LF-Recycler_NoRecipe", LF_RecyclerRules.RefusalKey(LF_ERecycleCheck.NO_RECIPE), "the no-recipe refusal has its words");
		ctx.Equal(applesBefore, LF_InventoryUtils.GetAmount(worker, APPLE), "a refused load takes nothing");
		ctx.Equal(LF_StationProgress.EMPTY, recycler.GetPercent(), "a refused load leaves the bench empty");

		// Nobody at the bench: refused.
		ctx.Equal(LF_ERecycleCheck.TOO_FAR, recycler.ServerLoadItem(null, "test_apple", 100), "a load with nobody at the bench is refused");

		// Load takes the apple.
		int juiceBefore = LF_InventoryUtils.GetAmount(worker, JUICE);
		ctx.Equal(LF_ERecycleCheck.NONE, recycler.ServerLoadItem(worker, "test_apple", 100), "a carried recipe item loads");
		ctx.Equal(applesBefore - 1, LF_InventoryUtils.GetAmount(worker, APPLE), "the loaded apple is gone from the inventory");
		ctx.Equal(0, recycler.GetPercent(), "the bench is at the start of its job");
		ctx.EqualStr("test_apple", recycler.GetLoadedItem(), "the bench holds the apple");

		// One job at a time; early collect refused.
		ctx.Equal(LF_ERecycleCheck.BUSY, recycler.ServerLoadItem(worker, "test_apple", 101), "a second load is refused while busy");
		ctx.Equal(LF_ERecycleCheck.WRONG_STATE, recycler.ServerCollect(worker, 101), "collect before the work is done is refused");
		ctx.Equal(juiceBefore, LF_InventoryUtils.GetAmount(worker, JUICE), "an early collect hands over nothing");

		// Work: a hold repeats no faster than the bench's tick; four spaced units finish the job.
		ctx.Equal(LF_ERecycleCheck.NONE, recycler.ServerWork(worker, 200), "the first work lands");
		ctx.Equal(LF_ERecycleCheck.TOO_FAST, recycler.ServerWork(worker, 200), "a work call faster than a hold is refused");
		ctx.Equal(LF_ERecycleCheck.NONE, recycler.ServerWork(worker, 210), "the second work lands");
		ctx.Equal(LF_ERecycleCheck.NONE, recycler.ServerWork(worker, 220), "the third work lands");
		ctx.True(recycler.GetPercent() < LF_StationProgress.READY, "three of four units is not ready");
		ctx.Equal(LF_ERecycleCheck.NONE, recycler.ServerWork(worker, 230), "the fourth work lands");
		ctx.Equal(LF_StationProgress.READY, recycler.GetPercent(), "the job is ready to collect");
		ctx.Equal(LF_ERecycleCheck.WRONG_STATE, recycler.ServerWork(worker, 240), "work on a finished job is refused");

		// Collect once: 2 juice (half of 4); collect twice is refused and gives nothing.
		// Red-proof hook: with --red the collect is skipped, so the assertions on the parts must fail.
		if (!RedProof())
			ctx.Equal(LF_ERecycleCheck.NONE, recycler.ServerCollect(worker, 250), "collect hands over the parts");

		ctx.Equal(juiceBefore + 2, LF_InventoryUtils.GetAmount(worker, JUICE), "half of the four juice come back, once");
		ctx.Equal(LF_StationProgress.EMPTY, recycler.GetPercent(), "the bench is empty after the collect");
		ctx.Equal(LF_ERecycleCheck.WRONG_STATE, recycler.ServerCollect(worker, 251), "a second collect is refused");
		ctx.EqualStr("#LF-Recycler_WrongState", LF_RecyclerRules.RefusalKey(LF_ERecycleCheck.WRONG_STATE), "the refusal has its words");
		ctx.Equal(juiceBefore + 2, LF_InventoryUtils.GetAmount(worker, JUICE), "the second collect gives nothing more");
		ctx.Equal(applesBefore - 1, LF_InventoryUtils.GetAmount(worker, APPLE), "the apple is not given back");

		// Not carried: refused (the apple is gone).
		if (applesBefore - 1 == 0)
			ctx.Equal(LF_ERecycleCheck.NO_ITEM, recycler.ServerLoadItem(worker, "test_apple", 300), "a recipe item that is not carried is refused");
	}
}
