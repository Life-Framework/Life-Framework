// red-proof: 2026-11-22, cli test --no-build --red LF_Test_MapRunChains,LF_Test_EveronMapRuns: RED-OK (RedProof() indexes no run: "the pack has the lumber run" fails). Green under the pack header.
//------------------------------------------------------------------------------------------------
//! everon/map-runs: under the Everon 1989 header the run table holds the island's job runs as chains (the
//! maintainer's mining, apple, postal van, lumber and fishing runs), each stop naming a world entity and a
//! label, the mining run in its five steps.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMapRuns : LF_Test
{
	override string GetName()
	{
		return "everon/map-runs";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MapRunConfig.ResetStatics();
		LF_MapRunConfig config = LF_MapRunConfig.Get();
		ctx.NotNull(config, "the run table loads");
		if (!config)
			return;

		array<string> want = {"mining", "apples", "postal", "lumber", "fishing"};
		map<string, LF_MapRun> byId = new map<string, LF_MapRun>();
		foreach (LF_MapRun run : config.m_aRuns)
		{
			if (!RedProof())
				byId.Set(run.m_sId, run);
		}

		foreach (string id : want)
		{
			LF_MapRun run;
			ctx.True(byId.Find(id, run), "the pack has the " + id + " run");
			if (!run)
				continue;

			ctx.True(run.StopCount() >= 2, id + " has at least two stops");
			ctx.False(run.m_sLabel.IsEmpty(), id + " has a name");
			foreach (LF_MapRunStop stop : run.m_aStops)
			{
				ctx.False(stop.m_sEntity.IsEmpty() || stop.m_sLabel.IsEmpty(), id + ": every stop names an entity and a label");
			}
		}

		LF_MapRun mining;
		if (byId.Find("mining", mining))
		{
			ctx.Equal(5, mining.StopCount(), "mining: desk, pickaxe, rocks, furnace, buyer");
			ctx.EqualStr("#LF-MapRunStop_Furnace", mining.m_aStops[3].m_sLabel, "step 4 is the furnace");
		}

		LF_MapRunConfig.ResetStatics();
	}
}
