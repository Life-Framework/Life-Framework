// red-proof: 2026-11-24 `tools\cli test --no-build --only LF_Test_PlankBundle` with WoodPlank.et still at the base
// stack of 1: `- a plank stacks to a bundle of 4 (expected 4, got 1)` and `- the 4 planks drop as one bundle (one trip
// to the buyer) (expected 1, got 4)`; with LF_ProcessAction.c unchanged (the instant drop): `- the instant saw drops the
// 4 planks as one bundle (expected 1, got 4)`. Both restored: green.

//! Planks are a carried bundle (#4875): a plank stacks to 4 in the hands, and a timed sawmill run drops its
//! 4 planks as one bundle a player carries in one trip (the kindling, a pocket item, still drops on its own).
//! One plank per trip to the timber buyer held the hand-haul hour at about $262.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PlankBundle : LF_Test
{
	protected static const ResourceName SAWMILL = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";
	protected static const ResourceName SOFT_LOG = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	protected static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	protected static const ResourceName KINDLING = "{6697C9B8CEB5234C}Prefabs/Resources/WoodCutting/WoodKindling.et";
	protected static const string MACHINE = "sawmill";
	protected static const vector SPOT = "84 0 76";
	protected static const float SWEEP_RADIUS = 4;
	protected static const int RUN_SECONDS = 30;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "lumber/plank-bundle";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ProcessAction FindAction(IEntity sawmill)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(sawmill);
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_ProcessAction process = LF_ProcessAction.Cast(base);
			if (process && process.m_sMachineId == MACHINE)
				return process;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The pure rule: a stack holds what is owed up to its limit; no limit takes it all.
		ctx.Equal(4, LF_ProcessRun.BundleSize(5, 4), "five owed, a stack of four takes four");
		ctx.Equal(3, LF_ProcessRun.BundleSize(3, 4), "three owed fit one stack");
		ctx.Equal(1, LF_ProcessRun.BundleSize(4, 1), "an item that does not stack holds one");
		ctx.Equal(7, LF_ProcessRun.BundleSize(7, 0), "no limit takes everything owed");
		ctx.Equal(0, LF_ProcessRun.BundleSize(0, 4), "nothing owed, nothing held");

		IEntity loose = LF_Utils.SpawnEntityPrefab(PLANK, "84 0 70");
		ctx.NotNull(loose, "a plank spawns");
		if (loose)
		{
			LF_QuantityComponent quantity = LF_Component<LF_QuantityComponent>.Find(loose);
			ctx.NotNull(quantity, "a plank carries LF_QuantityComponent");
			if (quantity)
				ctx.Equal(4, quantity.GetMaxQuantity(), "a plank stacks to a bundle of 4");
			ctx.NotNull(loose.FindComponent(LF_HandCarryComponent), "a plank is still a hand-carry item");
			SCR_EntityHelper.DeleteEntityAndChildren(loose);
		}

		LF_ResourceMachineDef row = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(row, "the sawmill planks row exists");
		if (!row)
			return;

		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		IEntity sawmill = LF_Utils.SpawnEntityPrefab(SAWMILL, Vector(SPOT[0], y, SPOT[2]));
		ctx.NotNull(worker, "the worker spawns");
		ctx.NotNull(sawmill, "the sawmill spawns");
		int seconds = row.m_iProcessSeconds;
		if (worker && sawmill)
			RunChecks(ctx, row, worker, sawmill);

		row.m_iProcessSeconds = seconds;
		LF_ProcessRun.ResetStatics();
		LF_TestCharacterFixture.Delete(worker);
		SCR_EntityHelper.DeleteEntityAndChildren(sawmill);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, LF_ResourceMachineDef row, IEntity worker, IEntity sawmill)
	{
		LF_ProcessAction process = FindAction(sawmill);
		ctx.NotNull(process, "the sawmill offers the planks action");
		if (!process)
			return;

		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, PLANK);
		sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, KINDLING);

		// The sawmill row is instant: the outputs drop at the saw at once (the island's woodlot saw).
		ctx.Equal(1, LF_InventoryUtils.AddAmount(worker, SOFT_LOG, 1, true), "the worker carries a log for the instant saw");
		process.PerformAction(sawmill, worker);
		ctx.Equal(0, LF_InventoryUtils.GetAmount(worker, SOFT_LOG), "the instant saw takes the log");
		int instantBundles = sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, PLANK);
		ctx.Equal(4, sweep.m_iUnits, "the instant saw makes 4 planks");
		ctx.Equal(1, instantBundles, "the instant saw drops the 4 planks as one bundle");
		ctx.Equal(1, sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, KINDLING), "the instant saw drops the kindling on its own");

		// A timed row: the outputs drop at the machine when the run ends, bundled the same way.
		row.m_iProcessSeconds = RUN_SECONDS;
		ctx.Equal(1, LF_InventoryUtils.AddAmount(worker, SOFT_LOG, 1, true), "the worker carries a log");
		process.PerformAction(sawmill, worker);
		ctx.Equal(0, LF_InventoryUtils.GetAmount(worker, SOFT_LOG), "the log goes in");
		ctx.Equal(1, LF_ProcessRun.CompleteDue(LF_ProcessRun.Now() + RUN_SECONDS + 1), "the run ends");

		int bundles = sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, PLANK);
		int planks = sweep.m_iUnits;
		ctx.Equal(4, planks, "the run makes 4 planks");
		ctx.Equal(1, bundles, "the 4 planks drop as one bundle (one trip to the buyer)");
		ctx.Equal(1, sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, KINDLING), "the kindling drops on its own");
	}
}
