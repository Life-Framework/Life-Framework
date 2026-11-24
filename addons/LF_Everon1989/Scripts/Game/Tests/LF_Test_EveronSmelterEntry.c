// red-proof: #4856 2026-11-24, `tools\cli test --no-build --red LF_Test_EveronSmelterEntry`: RED PROOF OK (RedProof() puts the old gate of 8 back on the smelter row: the SMELTING 1 worker is refused, no bar comes out and no SMELTING XP is paid). Green with `--only`.

//! everon/smelter-entry (#4856): under the Everon 1989 header (the pack leg), the iron smelter at the centre-east works
//! is open to a worker at SMELTING 1. A new player reaches it at hour 5 with SMELTING XP of 0 (the trade is only earned
//! by smelting), so a gate at 8 refused them for good. The shipped row has no gate; one real run takes 2 iron nuggets and
//! 1 coal, makes 1 iron bar and pays the worker's first SMELTING XP. The copper refinery keeps its SMELTING 15 gate and
//! refuses in words that name the trade and the level ("Needs Smelting 15"), not the generic "not experienced enough".
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronSmelterEntry : LF_Test
{
	protected static const ResourceName SMELTER = "{CBFF9864374D9542}Prefabs/Processor/LF_IronSmelter.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";
	protected static const ResourceName COAL = "{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et";
	protected static const ResourceName BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	protected static const string MACHINE = "smelter-iron";
	protected static const string GATED = "refinery-copper";
	protected static const string JOB = "SMELTING";
	protected static const string WORKER = "test-smelter-entry";
	protected static const vector SPOT = "96 0 96";
	protected static const float SWEEP_RADIUS = 4;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/smelter-entry";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ProcessAction FindAction(IEntity smelter)
	{
		ActionsManagerComponent actions = LF_Component<ActionsManagerComponent>.Find(smelter);
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
		LF_ConfigService.Reset();
		LF_JobsConfig.Reload();
		LF_ResourceChainCatalog.Reload();

		LF_ResourceMachineDef row = LF_ResourceChainCatalog.FindMachine(MACHINE);
		ctx.NotNull(row, MACHINE + " has a machine row");
		if (!row)
			return;

		int shippedLevel = row.m_iMinLevel;
		int shippedSeconds = row.m_iProcessSeconds;

		// Red proof: the gate of 8 the smelter shipped with before #4856.
		if (RedProof())
			row.m_iMinLevel = 8;

		CheckGate(ctx, row);
		CheckRun(ctx, row);
		CheckNamedRefusal(ctx);

		row.m_iMinLevel = shippedLevel;
		row.m_iProcessSeconds = shippedSeconds;
		LF_ProcessRun.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped row: SMELTING, no level to meet.
	protected void CheckGate(LF_TestContext ctx, LF_ResourceMachineDef row)
	{
		ctx.EqualStr(JOB, row.m_sLevelJob, "the smelter reads the SMELTING track");
		ctx.True(row.m_iMinLevel <= 1, "the smelter wants no more than SMELTING 1");
		ctx.True(LF_ProcessAction.LevelRefusal(row, 1).IsEmpty(), "a SMELTING 1 worker is not refused at the smelter");
		ctx.Equal(2, row.m_aInputs.Count(), "the smelter takes two inputs: nuggets and coal");
		if (row.m_aInputs.Count() == 2)
		{
			ctx.Equal(2, row.m_aInputs[0].m_iInputAmount, "2 iron nuggets a bar");
			ctx.Equal(1, row.m_aInputs[1].m_iInputAmount, "1 coal a bar");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A real worker at SMELTING 1 loads the smelter: 2 nuggets and 1 coal go, one bar and the first XP come out.
	protected void CheckRun(LF_TestContext ctx, LF_ResourceMachineDef row)
	{
		float y = GetGame().GetWorld().GetSurfaceY(SPOT[0], SPOT[2]);
		IEntity worker = LF_TestCharacterFixture.Spawn(ctx);
		IEntity smelter = LF_Utils.SpawnEntityPrefab(SMELTER, Vector(SPOT[0], y, SPOT[2]));
		ctx.NotNull(worker, "the worker spawns");
		ctx.NotNull(smelter, "the iron smelter spawns");
		if (worker && smelter)
		{
			LF_Utils.SetUidForTests(worker, WORKER);
			LF_JobLevels.Forget(WORKER);
			RunWorker(ctx, row, worker, smelter);
		}

		LF_ProcessRun.ResetStatics();
		LF_Utils.SetUidForTests(null, "");
		LF_JobLevels.Forget(WORKER);
		LF_TestCharacterFixture.Delete(worker);
		if (smelter)
		{
			LF_TestLooseSweep sweep = new LF_TestLooseSweep();
			sweep.Delete(smelter.GetOrigin(), SWEEP_RADIUS, BAR);
			SCR_EntityHelper.DeleteEntityAndChildren(smelter);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RunWorker(LF_TestContext ctx, LF_ResourceMachineDef row, IEntity worker, IEntity smelter)
	{
		LF_ProcessAction process = FindAction(smelter);
		ctx.NotNull(process, "the pack smelter offers the " + MACHINE + " action");
		LF_JobXpAwardDef smelt = LF_JobsConfig.Get().FindAward("smelt_batch");
		if (!process || !smelt)
			return;

		smelt.m_fCooldownSeconds = 0;
		ctx.Equal(1, LF_JobLevels.LevelOf(WORKER, JOB), "the worker starts at SMELTING 1");
		ctx.Equal(0, LF_JobLevels.XpOf(WORKER, JOB), "with no SMELTING XP");

		ctx.Equal(2, LF_InventoryUtils.AddAmount(worker, NUGGET, 2, true), "the worker carries two iron nuggets");
		ctx.Equal(1, LF_InventoryUtils.AddAmount(worker, COAL, 1, true), "and one coal lump");
		process.PerformAction(smelter, worker);

		ctx.Equal(0, LF_InventoryUtils.GetAmount(worker, NUGGET), "the run took both nuggets");
		ctx.Equal(0, LF_InventoryUtils.GetAmount(worker, COAL), "the run took the coal");
		ctx.True(LF_ProcessRun.IsBusy(smelter), "the 60 s run is under way");
		ctx.Equal(1, LF_ProcessRun.CompleteAll("test run done"), "the run finishes");

		LF_TestLooseSweep sweep = new LF_TestLooseSweep();
		int bars = LF_InventoryUtils.GetAmount(worker, BAR) + sweep.Delete(smelter.GetOrigin(), SWEEP_RADIUS, BAR);
		ctx.Equal(1, bars, "one iron bar came out");
		ctx.Equal(smelt.m_iXp, LF_JobLevels.XpOf(WORKER, JOB), "the batch paid the first SMELTING XP");
	}

	//------------------------------------------------------------------------------------------------
	//! A gate that stays names the trade and the level, in words.
	protected void CheckNamedRefusal(LF_TestContext ctx)
	{
		LF_ResourceMachineDef refinery = LF_ResourceChainCatalog.FindMachine(GATED);
		ctx.NotNull(refinery, GATED + " has a machine row");
		if (!refinery)
			return;

		ctx.EqualStr(JOB, refinery.m_sLevelJob, "the copper refinery reads SMELTING");
		ctx.Equal(15, refinery.m_iMinLevel, "the copper refinery keeps SMELTING 15");
		string words = LF_ProcessAction.LevelRefusal(refinery, 1);
		ctx.False(words.IsEmpty(), "a SMELTING 1 worker is refused at the refinery");
		ctx.True(words.Contains("Smelting"), "the refusal names the trade: " + words);
		ctx.True(words.Contains("15"), "the refusal names the level: " + words);
		ctx.EqualStr(words, LF_ProcessAction.LevelRefusal(refinery, 14), "one level short reads the same, with the level");
		ctx.True(LF_ProcessAction.LevelRefusal(refinery, 15).IsEmpty(), "SMELTING 15 is let in");
	}
}
