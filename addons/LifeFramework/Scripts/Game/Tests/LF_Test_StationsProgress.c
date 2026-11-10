// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_StationsProgress`: a wrong expected value failed `three workers need a third of that (expected 5, got 4)` and `SUMMARY tier=only passed=0 failed=3` (run with its two siblings); restored, green.

// tier: LOGIC
//! Hands-on station progress (#417): progress belongs to the station and adds across workers, so
//! one worker takes N times as long as N workers; a saved batch restores clamped; a work tick that
//! comes faster than a hold can finish is refused; site heat rises with work and fades.
class LF_Test_StationsProgress : LF_Test
{
	override string GetName()
	{
		return "stations/progress";
	}

	//------------------------------------------------------------------------------------------------
	//! Simulates `workers` workers, each doing one unit per round, on one station.
	protected int RoundsToFinish(int needed, int workers)
	{
		LF_StationProgress progress = new LF_StationProgress();
		progress.Load(needed);
		int rounds;
		while (!progress.IsDone() && rounds < 1000)
		{
			rounds++;
			for (int worker = 0; worker < workers; worker++)
				progress.AddWork(1);
		}

		return rounds;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunAdds(ctx);
		RunRestore(ctx);
		RunHeat(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAdds(LF_TestContext ctx)
	{
		LF_StationProgress progress = new LF_StationProgress();
		ctx.Equal(LF_StationProgress.EMPTY, progress.Percent(), "a new station is empty");
		ctx.False(progress.AddWork(1), "an empty station takes no work");

		ctx.True(progress.Load(8), "a batch loads");
		ctx.False(progress.Load(8), "a loaded station refuses a second batch");
		ctx.Equal(0, progress.Percent(), "a fresh batch is at 0");

		// Two workers: their units land on the same number.
		progress.AddWork(1);
		progress.AddWork(1);
		ctx.Equal(2, progress.GetWork(), "work from two workers adds up");
		ctx.Equal(25, progress.Percent(), "2 of 8 is 25 percent");

		progress.AddWork(5);
		ctx.Equal(87, progress.Percent(), "7 of 8 is 87 percent, not ready");
		ctx.False(progress.IsDone(), "7 of 8 is not done");
		ctx.True(progress.AddWork(1), "the last unit finishes the batch");
		ctx.Equal(LF_StationProgress.READY, progress.Percent(), "a finished batch reads ready");
		ctx.False(progress.AddWork(1), "a finished batch takes no more work");
		ctx.Equal(8, progress.GetWork(), "work never passes what the batch needs");

		progress.Clear();
		ctx.Equal(LF_StationProgress.EMPTY, progress.Percent(), "collecting empties the station");

		// One worker takes N times as long as N workers.
		int solo = RoundsToFinish(12, 1);
		int three = RoundsToFinish(12, 3);
		ctx.Equal(12, solo, "one worker needs 12 rounds for 12 units");
		ctx.Equal(4, three, "three workers need a third of that");
		ctx.Equal(solo, three * 3, "one worker takes 3 times as long as 3");
		ctx.Equal(three, LF_StationProgress.Rounds(12, 3), "Rounds agrees with the simulation");
		ctx.Equal(4, LF_StationProgress.Rounds(10, 3), "an uneven split rounds up");
		ctx.Equal(0, LF_StationProgress.Rounds(10, 0), "no workers never finish");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRestore(LF_TestContext ctx)
	{
		LF_StationProgress progress = new LF_StationProgress();
		progress.Restore(true, 3, 8);
		ctx.True(progress.IsLoaded(), "a saved batch restores loaded");
		ctx.Equal(3, progress.GetWork(), "the saved work comes back");
		ctx.Equal(37, progress.Percent(), "37 percent after the restore");

		progress.Restore(true, 99, 8);
		ctx.True(progress.IsDone(), "work past the need clamps to done");
		ctx.Equal(8, progress.GetWork(), "clamped to the need");

		progress.Restore(true, -4, 8);
		ctx.Equal(0, progress.GetWork(), "negative work clamps to 0");

		progress.Restore(false, 3, 8);
		ctx.False(progress.IsLoaded(), "an unloaded row restores empty");
		progress.Restore(true, 3, 0);
		ctx.False(progress.IsLoaded(), "an unsized batch restores empty");

		ctx.True(LF_ChainStationComponent.TickAllowed(10, 11.3, 1200), "a tick a full hold later is allowed");
		ctx.False(LF_ChainStationComponent.TickAllowed(10, 10.5, 1200), "a tick half a hold later is refused");
		ctx.True(LF_ChainStationComponent.TickAllowed(-100000, 0, 1200), "a worker's first tick is allowed");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunHeat(LF_TestContext ctx)
	{
		LF_ChainCatalog.ResetStatics();

		LF_ChainDef chain = new LF_ChainDef();
		chain.m_sChainId = "heat-chain";
		chain.m_iTier = 1;
		chain.m_aSteps = new array<ref LF_ChainStepDef>();
		LF_ChainStepDef step = new LF_ChainStepDef();
		step.m_sStepId = "mix";
		step.m_sStationType = "barrel";
		step.m_iWork = 4;
		step.m_aInputs = new array<ref LF_ProcessingInput>();
		step.m_aOutputs = new array<ref LF_ProcessingOutput>();
		LF_ProcessingInput input = new LF_ProcessingInput();
		input.m_InputPrefab = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
		input.m_iInputAmount = 1;
		step.m_aInputs.Insert(input);
		LF_ProcessingOutput output = new LF_ProcessingOutput();
		output.m_OutputPrefab = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
		output.m_iOutputAmount = 1;
		step.m_aOutputs.Insert(output);
		chain.m_aSteps.Insert(step);
		chain.m_Heat = new LF_ChainHeatDef();
		chain.m_Heat.m_fPerWork = 2;
		chain.m_Heat.m_fDecayPerSecond = 1;
		chain.m_Heat.m_fMax = 10;
		chain.m_Heat.m_fRangeM = 40;

		LF_ResourceChainConfig config = new LF_ResourceChainConfig();
		config.m_aChains = new array<ref LF_ChainDef>();
		config.m_aChains.Insert(chain);
		LF_ChainCatalog.Build(config, false);

		ctx.EqualFloat(0, LF_ChainSites.GetHeat("heat-site", 0), 0.001, "an unknown site is cold");
		LF_ChainSites.Register("heat-site", "heat-chain", Vector(10, 0, 10));
		ctx.EqualFloat(0, LF_ChainSites.GetHeat("heat-site", 0), 0.001, "a new site is cold");

		LF_ChainSites.AddWork("heat-site", 1, 100);
		LF_ChainSites.AddWork("heat-site", 1, 100);
		ctx.EqualFloat(4, LF_ChainSites.GetHeat("heat-site", 100), 0.001, "two units of work heat the site");
		ctx.EqualFloat(16, LF_ChainSites.GetRange("heat-site", 100), 0.001, "4 of 10 heat is 40 percent of the range");
		ctx.EqualFloat(1, LF_ChainSites.GetHeat("heat-site", 103), 0.001, "the site fades 1 a second");
		ctx.EqualFloat(0, LF_ChainSites.GetHeat("heat-site", 200), 0.001, "a long quiet site is cold");

		LF_ChainSites.AddWork("heat-site", 1, 300);
		LF_ChainSites.AddWork("heat-site", 1, 300);
		LF_ChainSites.AddWork("heat-site", 1, 300);
		LF_ChainSites.AddWork("heat-site", 1, 300);
		LF_ChainSites.AddWork("heat-site", 1, 300);
		LF_ChainSites.AddWork("heat-site", 1, 300);
		ctx.EqualFloat(10, LF_ChainSites.GetHeat("heat-site", 300), 0.001, "heat stops at the chain maximum");

		array<LF_ChainSiteState> warm = {};
		LF_ChainSites.GetWarmSites(300, warm);
		ctx.Equal(1, warm.Count(), "the warm list holds the one hot site");
		LF_ChainSites.GetWarmSites(400, warm);
		ctx.Equal(0, warm.Count(), "a faded site leaves the warm list");

		LF_ChainSites.ClearHeat("heat-site");
		ctx.EqualFloat(0, LF_ChainSites.GetHeat("heat-site", 300), 0.001, "ClearHeat makes the site cold");

		LF_ChainSites.Remove("heat-site");
		LF_ChainCatalog.ResetStatics();
	}
}
