// red-proof: observed with `tools\cli test --no-build --only LF_Test_DeliveryLevelTen` on the unfixed table (DELIVERY thresholds `500 1500 3500 7500 15500 31500`, level 7 the top): `- the DELIVERY ladder has 30 levels (expected 30, got 7)`, `- delivery runs reach level 10 within 60 runs (expected 10, got 4)` and `- a level 10 hauler takes the order` failed; with the 30-level table it is green.
//! DELIVERY reaches the export order gate (#4596): the shipped track has 30 levels like every other trade,
//! the 100 XP delivery run carries a hauler to level 10 in about 50 runs (a few hours), a level 9 hauler is
//! refused an export order and a level 10 hauler takes one.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_DeliveryLevelTen : LF_Test
{
	protected static const string HAULER = "test-delivery-ten-hauler";
	protected static const int MAX_RUNS = 60;	// lf-fixed: "a few hours of hauling" at a run every three to five minutes
	protected static const ResourceName GOODS = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "jobs/delivery-level-ten";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_ExportOrderConfig MakeConfig()
	{
		LF_ExportOrderConfig config = LF_ExportOrderConfig.CreateDefault();
		LF_ExportOrderTemplate template = new LF_ExportOrderTemplate();
		template.m_sId = "iron";
		template.m_Goods = GOODS;
		template.m_sGoodsName = "iron bars";
		template.m_sDestination = "the steamer";
		template.m_iCount = 20;
		template.m_iWholesale = 40;
		template.m_iGameMinutes = 180;
		config.m_aTemplates.Insert(template);
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_JobsConfig.Reload();
		LF_JobLevels.Forget(HAULER);
		LF_JobsConfig jobs = LF_JobsConfig.Get();
		ctx.NotNull(jobs, "the shipped jobs config loads");
		if (!jobs)
			return;

		LF_JobTrackDef track = jobs.FindTrack(LF_JobLevels.JOB_DELIVERY);
		ctx.NotNull(track, "the shipped config has the DELIVERY track");
		if (!track)
			return;

		ctx.Equal(30, track.LevelFor(2000000000), "the DELIVERY ladder has 30 levels");
		int previous = 0;
		bool rising = true;
		foreach (int threshold : track.m_aLevelThresholds)
		{
			if (threshold <= previous)
				rising = false;

			previous = threshold;
		}

		ctx.True(rising, "every DELIVERY threshold is above the one before");

		// Haul with the shipped delivery_run award until level 10.
		int runs = 0;
		int level = 1;
		while (runs < MAX_RUNS && level < 10)
		{
			if (RedProof())
				break;

			LF_JobLevels.Award(HAULER, LF_JobLevels.ACTION_DELIVERY_RUN);
			runs++;
			level = LF_JobLevels.LevelOf(HAULER, LF_JobLevels.JOB_DELIVERY);
		}

		ctx.Equal(10, level, string.Format("delivery runs reach level 10 within %1 runs (took %2)", MAX_RUNS, runs));
		ctx.True(runs >= 20, string.Format("level 10 is a real climb, not a handful of runs (took %1)", runs));

		// The board: one open order; level 9 is refused, level 10 takes it.
		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
		LF_ExportOrderConfig config = MakeConfig();
		LF_ExportOrderConfig.UseForTest(config);
		LF_ExportOrderBook.Tick(600);
		array<ref LF_ExportOrder> orders = LF_ExportOrderBook.List();
		ctx.True(orders.Count() > 0, "an order is open on the board");
		if (orders.Count() > 0)
		{
			int orderId = orders[0].m_iId;
			ctx.EqualStr(config.m_sJob, LF_JobLevels.JOB_DELIVERY, "the board gates on the DELIVERY track");
			ctx.EqualStr(LF_ExportOrderBook.REFUSE_LEVEL, LF_ExportOrderBook.Take(HAULER, 9, orderId), "a level 9 hauler is refused");
			ctx.EqualStr(string.Empty, LF_ExportOrderBook.Take(HAULER, LF_JobLevels.LevelOf(HAULER, config.m_sJob), orderId), "a level 10 hauler takes the order");
		}

		LF_ExportOrderBook.ResetStatics();
		LF_ExportOrderConfig.Reload();
		LF_JobLevels.Forget(HAULER);
	}
}
