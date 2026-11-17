// red-proof: red on main before EmptyBeerBottle.et existed (#2097): the input prefab did not load
// (`the empty beer bottle loads`), so no bottle could be given to the character and the processor step
// failed `one empty bottle gives one beer`.

// tier: WORLD
//! The beer chain runs (#2097): the processor's input, the empty beer bottle, is a real item that
//! loads, and a character holding one empty bottle at the beer processor ends up with one beer.
class LF_Test_BeerChainRuns : LF_Test
{
	protected static const ResourceName EMPTY_BOTTLE = "{2B0213949BE91F38}Prefabs/Items/Resources/EmptyBeerBottle.et";
	protected static const ResourceName BEER = "{4CBB1F9A04D15772}Prefabs/Items/Drinks/BeerBottle.et";
	protected static const ResourceName BEER_PROCESSOR = "{1303AC26D8473E33}Prefabs/Processing/Chains/Beer.et";
	protected static const vector SPOT = "52 0 52";

	override string GetName()
	{
		return "chains/beer-runs";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource bottle = Resource.Load(EMPTY_BOTTLE);
		ctx.True(bottle && bottle.IsValid(), "the empty beer bottle loads");
		if (ctx.FailureCount() > 0)
			return;

		IEntity user = LF_TestCharacterFixture.Spawn(ctx);
		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		IEntity processor = LF_Utils.SpawnEntityPrefab(BEER_PROCESSOR, Vector(SPOT[0], y, SPOT[2]));
		ctx.NotNull(user, "a character spawns");
		ctx.NotNull(processor, "the beer processor spawns");
		if (user && processor)
			RunChecks(ctx, user, processor);

		LF_TestCharacterFixture.Delete(user);
		SCR_EntityHelper.DeleteEntityAndChildren(processor);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ProcessAction FindProcess(IEntity processor)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(processor);
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_ProcessAction process = LF_ProcessAction.Cast(base);
			if (process)
				return process;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, IEntity user, IEntity processor)
	{
		LF_ProcessAction process = FindProcess(processor);
		ctx.NotNull(process, "the beer processor carries a process action");
		if (!process)
			return;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(user, EMPTY_BOTTLE, 1), "one empty bottle enters the inventory");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(user, EMPTY_BOTTLE), "the character holds one empty bottle");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(user, BEER), "no beer yet");
		if (ctx.FailureCount() > 0)
			return;

		ctx.True(process.CanBePerformedScript(user), "an empty bottle can be made into beer");
		process.PerformAction(processor, user);

		ctx.Equal(0, LF_InventoryUtils.GetAmount(user, EMPTY_BOTTLE), "the empty bottle is used up");
		ctx.Equal(1, LF_InventoryUtils.GetAmount(user, BEER), "one empty bottle gives one beer");
	}
}
