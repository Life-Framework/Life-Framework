// red-proof: #4341 2026-11-23, cement-works added to the plants that pay smelt_batch: `--only LF_Test_SmeltingXp` on the old conf FAILED (cement-works pays smelt_batch), green after. Earlier: 2026-11-23, `tools\cli test --no-build --red LF_Test_SmeltingXp`: RED PROOF OK (RedProof() clears the furnace row's m_sXpAction: a finished furnace batch raises SMELTING expected 14, got 0; the timed run pays 0 as well). Green with the rows via `--only`.

//! jobs/smelting-xp (#4337, W2 of the 0.3.0 work scope, decision N3): a finished furnace batch raises SMELTING and
//! never MINER. The jobs config ships the SMELTING and SALVAGE tracks (30 levels, the MINER table) and the
//! `smelt_batch` and `salvage_work` rows; the furnace, smelter and refinery machine rows name `smelt_batch`; a machine
//! with no `m_sXpAction` pays nothing. The real path runs here: a worker loads a furnace and its batch pays at once,
//! then a timed run pays when the bar drops.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_SmeltingXp : LF_Test
{
	protected static const ResourceName FURNACE = "{B63FA2F04B5A5B00}Prefabs/Processor/Furnace.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const ResourceName BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected static const string MACHINE = "furnace-iron";
	protected static const string SMELTER = "test-smelting-xp";
	protected static const vector SPOT = "96 0 96";
	protected static const float SWEEP_RADIUS = 4;
	protected static const int RUN_SECONDS = 30;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/smelting-xp";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ProcessAction FindAction(IEntity furnace, string machineId)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(furnace);
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
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_JobsConfig.Reload();
		CheckConfig(ctx);

		LF_ResourceMachineDef row = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(row, MACHINE + " has a machine row");
		if (!row)
			return;

		string shippedAction = row.m_sXpAction;
		int shippedSeconds = row.m_iProcessSeconds;

		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		IEntity furnace = LF_Utils.SpawnEntityPrefab(FURNACE, Vector(SPOT[0], y, SPOT[2]));
		ctx.NotNull(worker, "the worker spawns");
		ctx.NotNull(furnace, "the furnace spawns");
		if (worker && furnace)
		{
			LF_Utils.SetUidForTests(worker, SMELTER);
			LF_JobLevels.Forget(SMELTER);
			RunChecks(ctx, row, worker, furnace);
		}

		// Leave the row, the uid and the store as every later test expects them.
		row.m_sXpAction = shippedAction;
		row.m_iProcessSeconds = shippedSeconds;
		LF_ProcessRun.ResetStatics();
		LF_Utils.SetUidForTests(null, "");
		LF_JobLevels.Forget(SMELTER);
		LF_TestCharacterFixture.Delete(worker);
		if (furnace)
		{
			LF_TestLooseSweep sweep = new LF_TestLooseSweep();
			sweep.Delete(furnace.GetOrigin(), SWEEP_RADIUS, BAR);
			SCR_EntityHelper.DeleteEntityAndChildren(furnace);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped rows: both tracks on the MINER table, both award rows, and which machines pay.
	protected void CheckConfig(LF_TestContext ctx)
	{
		LF_JobsConfig config = LF_JobsConfig.Get();
		ctx.NotNull(config, "the shipped jobs config loads");
		if (!config)
			return;

		LF_JobTrackDef miner = config.FindTrack("MINER");
		array<string> trades = {"SMELTING", "SALVAGE"};
		foreach (string trade : trades)
		{
			LF_JobTrackDef track = config.FindTrack(trade);
			ctx.NotNull(track, trade + " has a track");
			if (!track || !track.m_aLevelThresholds || !miner)
				continue;

			ctx.Equal(29, track.m_aLevelThresholds.Count(), trade + " has 29 thresholds (levels 2 to 30)");
			ctx.Equal(miner.XpForLevel(5), track.XpForLevel(5), trade + " level 5 costs what MINER's does");
			ctx.Equal(miner.XpForLevel(30), track.XpForLevel(30), trade + " level 30 costs what MINER's does");
			ctx.True(track.m_iDailyXpCap > 0, trade + " has a daily cap");
		}

		LF_JobXpAwardDef smelt = config.FindAward("smelt_batch");
		ctx.NotNull(smelt, "smelt_batch has an award row");
		if (smelt)
		{
			ctx.EqualStr("SMELTING", smelt.m_sJob, "smelt_batch feeds SMELTING");
			ctx.True(smelt.m_iXp > 0, "smelt_batch is worth some xp");
		}

		LF_JobXpAwardDef salvage = config.FindAward("salvage_work");
		ctx.NotNull(salvage, "salvage_work has an award row");
		if (salvage)
			ctx.EqualStr("SALVAGE", salvage.m_sJob, "salvage_work feeds SALVAGE");

		array<string> smelters = {"furnace-iron", "furnace-gold", "furnace-copper", "smelter-iron", "refinery-copper", "cement-works"};
		foreach (string id : smelters)
		{
			LF_ResourceMachineDef machine = LF_ResourceChainCatalog.FindMachine(id);
			ctx.NotNull(machine, id + " has a machine row");
			if (machine)
				ctx.EqualStr("smelt_batch", machine.m_sXpAction, id + " pays smelt_batch");
		}

		LF_ResourceMachineDef sawmill = LF_ResourceChainCatalog.FindMachine("sawmill-posts");
		if (sawmill)
			ctx.True(sawmill.m_sXpAction.IsEmpty(), "the sawmill pays no smelting xp");

		LF_JobLevels.Forget(SMELTER);
		ctx.Equal(0, LF_ProcessXp.PayBatches(SMELTER, "sawmill-posts", 1), "a machine with no xp action pays nothing");
		ctx.Equal(0, LF_ProcessXp.PayBatches(SMELTER, "no-such-machine", 1), "an unknown machine pays nothing");
		ctx.Equal(0, LF_ProcessXp.PayBatches("", "furnace-iron", 1), "a worker with no player id earns nothing");
		ctx.Equal(0, LF_ProcessXp.PayBatches(SMELTER, "furnace-iron", 0), "no batches pay nothing");
		LF_JobLevels.Forget(SMELTER);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunChecks(LF_TestContext ctx, LF_ResourceMachineDef row, IEntity worker, IEntity furnace)
	{
		LF_ProcessAction process = FindAction(furnace, MACHINE);
		ctx.NotNull(process, "the furnace offers the iron action");
		LF_JobXpAwardDef smelt = LF_JobsConfig.Get().FindAward("smelt_batch");
		if (!process || !smelt)
			return;

		smelt.m_fCooldownSeconds = 0;
		row.m_iProcessSeconds = 0;

		// Red proof: the furnace row names no xp action, as before #4337.
		if (RedProof())
			row.m_sXpAction = "";

		// An instant batch pays when the bar comes out.
		ctx.Equal(2, LF_InventoryUtils.AddAmount(worker, NUGGET, 2, true), "the worker carries two nuggets");
		process.PerformAction(furnace, worker);
		ctx.Equal(1, LF_InventoryUtils.GetAmount(worker, BAR) + CountLoose(furnace), "the batch made a bar");
		ctx.Equal(smelt.m_iXp, LF_JobLevels.XpOf(SMELTER, "SMELTING"), "a finished furnace batch raises SMELTING");
		ctx.Equal(0, LF_JobLevels.XpOf(SMELTER, "MINER"), "a finished furnace batch never raises MINER");

		// A timed run pays when the bar drops, not when the nuggets go in.
		row.m_iProcessSeconds = RUN_SECONDS;
		ctx.Equal(2, LF_InventoryUtils.AddAmount(worker, NUGGET, 2, true), "the worker carries two more nuggets");
		process.PerformAction(furnace, worker);
		ctx.True(LF_ProcessRun.IsBusy(furnace), "the timed run is under way");
		ctx.Equal(smelt.m_iXp, LF_JobLevels.XpOf(SMELTER, "SMELTING"), "a run in progress has paid nothing yet");
		ctx.Equal(1, LF_ProcessRun.CompleteAll("test run done"), "the run finishes");
		ctx.Equal(smelt.m_iXp * 2, LF_JobLevels.XpOf(SMELTER, "SMELTING"), "a finished timed batch raises SMELTING again");
		ctx.Equal(0, LF_JobLevels.XpOf(SMELTER, "MINER"), "the timed batch never raises MINER either");
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLoose(IEntity furnace)
	{
		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		return sweep.Delete(furnace.GetOrigin(), SWEEP_RADIUS, BAR);
	}
}
