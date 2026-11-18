// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronPlots` printed `RED-OK everon/plots`: RedProof() reads the core file alone, so `the overlay adds the four Saint-Philippe plots` failed (expected 6, got 2); green without the hook (pack leg).
//! #2918: the 1989 plot catalogue (core plots plus the pack overlay `Configs/Property/LF_Property_Everon1989.conf`,
//! #2976) is sound as a whole: the overlay loads, every plot key is unique and not a house id, each plot names a core
//! tier whose build stages exist, and the prices sit inside the core catalogue's range. Pack leg of `tools\cli test`.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPlots : LF_Test
{
	static const ResourceName OVERLAY = "{7C3E19A4D5B28F10}Configs/Property/LF_Property_Everon1989.conf";
	static const int PACK_PLOTS = 4;
	static const int PRICE_CEILING_FACTOR = 10;

	override string GetName()
	{
		return "everon/plots";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon property overlay");

		// The core file alone: the plots and the price range the pack has to stay inside.
		LF_PropertyConfig core = LF_PropertyConfig.LoadFrom(LF_PropertyConfig.CONFIG_PATH);
		ctx.NotNull(core, "the core property config loads");
		if (!core || !core.m_aPlots)
			return;

		int corePlots = core.m_aPlots.Count();
		int coreMax = 0;
		foreach (LF_PropertyPlotDef corePlot : core.m_aPlots)
		{
			if (corePlot)
				coreMax = LF_Utils.MaxInt(coreMax, corePlot.m_iPrice);
		}

		LF_PropertyConfig.Reload();
		if (RedProof())
			LF_PropertyConfig.Reload(LF_PropertyConfig.CONFIG_PATH);

		LF_PropertyConfig config = LF_PropertyConfig.Get();
		ctx.NotNull(config, "the merged property config loads");
		if (!config || !config.m_aPlots)
		{
			LF_PropertyConfig.Reload();
			return;
		}

		ctx.Equal(corePlots + PACK_PLOTS, config.m_aPlots.Count(), "the overlay adds the four Saint-Philippe plots to the core catalogue");

		array<string> seen = {};
		foreach (LF_PropertyPlotDef plot : config.m_aPlots)
		{
			if (!plot)
			{
				ctx.Fail("the catalogue holds a null plot");
				continue;
			}

			string key = plot.m_sPlotId;
			ctx.True(!key.IsEmpty(), "a plot has a key");
			ctx.True(seen.Find(key) == -1, key + " is listed once");
			seen.Insert(key);
			ctx.True(!LF_Houses.Get(key) && !LF_Houses.Find(key), key + " is not a house identifier");

			ctx.True(plot.m_iPrice > 0, key + " is for sale");
			ctx.True(plot.m_iPrice <= coreMax * PRICE_CEILING_FACTOR, key + " costs no more than ten times the priciest core plot");

			LF_PropertyTierDef tier = config.FindTier(plot.m_sTierId);
			ctx.NotNull(tier, key + " names a tier the core defines");
			if (!tier)
				continue;

			int stages = LF_PropertyConfig.StageCount(plot.m_sTierId);
			ctx.True(stages > 0, key + " has build stages");
			for (int stage = 1; stage <= stages; stage++)
			{
				ctx.True(LF_PropertyConfig.StageWork(plot.m_sTierId, stage) > 0, string.Format("%1 stage %2 has hands-on work", key, stage));
				int reqs = LF_PropertyConfig.RequirementCount(plot.m_sTierId, stage);
				for (int r = 0; r < reqs; r++)
				{
					LF_PropertyStageReqDef req = LF_PropertyConfig.Requirement(plot.m_sTierId, stage, r);
					ctx.True(req && !req.m_Prefab.IsEmpty() && req.m_iCount > 0, string.Format("%1 stage %2 requirement %3 names an item and a count", key, stage, r));
				}
			}
		}

		LF_PropertyConfig.Reload();
	}
}
