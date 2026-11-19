// red-proof: `tools\cli test --no-build --red LF_Test_ProcessRunTimer`: RedProof() leaves the sawmill row
// instant, so the posts are already out before the delay and no run is ever busy: the "absent before the
// delay", "is heard running" and "second start is refused" checks fail.

//! A machine row with m_iProcessSeconds takes its time (#3229): the log goes in at the start, the
//! posts are absent until the time is up and then drop at the machine, a second worker is refused
//! while it runs, the machine is heard running, and a world end hands the owed outputs over at once.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_ProcessRunTimer : LF_Test
{
	protected static const ResourceName SAWMILL = "{61418A72BA63D059}Prefabs/Processor/Sawmill.et";
	protected static const ResourceName SOFT_LOG = "{32F5A906D8A573BC}Prefabs/Resources/WoodCutting/WoodLog_01_Small.et";
	protected static const ResourceName POST = "{7EB79C84E76D667D}Prefabs/Resources/WoodCutting/FencePost.et";
	protected static const string MACHINE = "sawmill-posts";
	protected static const vector SPOT = "80 0 80";
	protected static const float SWEEP_RADIUS = 4;
	protected static const int RUN_SECONDS = 30;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "processing/run-timer";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ProcessAction FindAction(IEntity sawmill, string machineId)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(sawmill);
		if (!actions)
			return null;

		array<BaseUserAction> list = {};
		actions.GetActionsList(list);
		foreach (BaseUserAction base : list)
		{
			LF_ProcessAction process = LF_ProcessAction.Cast(base);
			if (process && process.m_sMachineId == machineId)
				return process;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Loose posts within the sweep radius of the sawmill; deletes them so the next check counts alone.
	protected int Sweep(IEntity sawmill)
	{
		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		return sweep.Delete(sawmill.GetOrigin(), SWEEP_RADIUS, POST);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource loaded = Resource.Load(POST);
		ctx.True(loaded && loaded.IsValid(), "the post prefab loads");

		LF_ResourceMachineDef row = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(row, "the sawmill posts machine row exists");
		if (!row)
			return;

		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		IEntity first = LF_TestCharacterFixture.Spawn(ctx);
		IEntity second = LF_TestCharacterFixture.Spawn(ctx);
		IEntity sawmill = LF_Utils.SpawnEntityPrefab(SAWMILL, Vector(SPOT[0], y, SPOT[2]));
		ctx.NotNull(first, "the first worker spawns");
		ctx.NotNull(second, "the second worker spawns");
		ctx.NotNull(sawmill, "the sawmill spawns");
		if (first && second && sawmill)
			RunChecks(ctx, row, first, second, sawmill);

		// The row goes back to instant for every later test.
		row.m_iProcessSeconds = 0;
		LF_ProcessRun.ResetStatics();
		LF_TestCharacterFixture.Delete(first);
		LF_TestCharacterFixture.Delete(second);
		SCR_EntityHelper.DeleteEntityAndChildren(sawmill);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, LF_ResourceMachineDef row, IEntity first, IEntity second, IEntity sawmill)
	{
		LF_ProcessAction process = FindAction(sawmill, MACHINE);
		ctx.NotNull(process, "the sawmill offers the posts action");
		LF_AmbientSwitchComponent hum = LF_AmbientSwitchComponent.Cast(sawmill.FindComponent(LF_AmbientSwitchComponent));
		ctx.NotNull(hum, "the sawmill has a switch to hear it by");
		if (!process || !hum)
			return;

		Sweep(sawmill);
		ctx.True(row.m_iProcessSeconds == 0, "a row with no m_iProcessSeconds is instant");

		// Red proof: the row stays instant, as every machine was before #3229.
		if (!RedProof())
			row.m_iProcessSeconds = RUN_SECONDS;

		ctx.Equal(1, LF_InventoryUtils.AddAmount(first, SOFT_LOG, 1, true), "the first worker carries a log");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(second, SOFT_LOG, 1, true), "the second worker carries a log");
		ctx.False(hum.IsOn(), "the sawmill is quiet before the run");

		process.PerformAction(sawmill, first);
		ctx.Equal(0, LF_InventoryUtils.GetAmount(first, SOFT_LOG), "the log goes in when the run starts");
		ctx.True(LF_ProcessRun.IsBusy(sawmill), "the machine is busy while it runs");
		ctx.True(hum.IsOn(), "the machine is heard running");
		ctx.Equal(0, Sweep(sawmill), "the posts are absent before the delay");

		// A second worker is refused while it runs: their log stays and nothing is queued.
		process.PerformAction(sawmill, second);
		ctx.Equal(1, LF_InventoryUtils.GetAmount(second, SOFT_LOG), "a second start is refused while busy: the log stays");
		ctx.Equal(1, LF_ProcessRun.Count(), "the refused start queued nothing");
		ctx.Equal(0, Sweep(sawmill), "still no posts for the refused start");

		// Part way through nothing is due; the run is still on.
		ctx.Equal(0, LF_ProcessRun.CompleteDue(LF_ProcessRun.Now() + RUN_SECONDS / 2), "nothing is due half way");
		ctx.True(LF_ProcessRun.IsBusy(sawmill), "still busy half way");
		ctx.Equal(0, Sweep(sawmill), "no posts half way");

		// The time is up: the posts drop at the machine and it takes the next run.
		ctx.Equal(1, LF_ProcessRun.CompleteDue(LF_ProcessRun.Now() + RUN_SECONDS + 1), "the run is due after the delay");
		ctx.False(LF_ProcessRun.IsBusy(sawmill), "the machine is free again");
		ctx.Equal(2, Sweep(sawmill), "the posts are present after the delay");
		ctx.Equal(0, LF_ProcessRun.CompleteDue(LF_ProcessRun.Now() + 1000), "a run is delivered once");

		// A world end mid-run drops the owed outputs at once and loses nothing.
		process.PerformAction(sawmill, second);
		ctx.Equal(0, LF_InventoryUtils.GetAmount(second, SOFT_LOG), "the second worker starts the next run");
		ctx.True(LF_ProcessRun.IsBusy(sawmill), "the next run is on");
		ctx.Equal(0, Sweep(sawmill), "no posts while it runs");
		ctx.Equal(1, LF_ProcessRun.CompleteAll("test world end"), "a world end hands the run over");
		ctx.Equal(2, Sweep(sawmill), "the owed posts drop at once");
		ctx.Equal(0, LF_ProcessRun.Count(), "no run is left owed");
	}
}
