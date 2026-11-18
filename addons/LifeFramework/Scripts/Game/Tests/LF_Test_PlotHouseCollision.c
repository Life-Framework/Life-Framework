// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_PlotHouseCollision` (property/plot-house-collision): with the hook on the refusals are skipped and the test failed (`a second pass refuses nothing (expected 0, got 1)`, `a house registered as plot-a drops the loaded plot`, `the dropped plot has no price (expected 0, got 5000)`), `RED PROOF OK`; without the hook `--only` passed.
//! A plot key equal to a house identifier is refused (#2783): LF_Properties keys houses and plots in one table
//! and a house wins, so buying such a plot would sell the house at the plot price. The colliding plot leaves the
//! catalogue (and logs an error naming the key), the other plots and the house stay; a house that registers
//! after the config loaded drops the loaded plot the same way.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PlotHouseCollision : LF_Test
{
	protected LF_PropertyPlotDef MakePlot(string id, int price)
	{
		LF_PropertyPlotDef plot = new LF_PropertyPlotDef();
		plot.m_sPlotId = id;
		plot.m_sTierId = "cottage";
		plot.m_iPrice = price;
		return plot;
	}

	override string GetName()
	{
		return "property/plot-house-collision";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_Houses.ResetStatics();
		LF_PropertyConfig.Reload();

		// At config load: a known house with the key of a plot.
		LF_PropertyConfig config = new LF_PropertyConfig();
		config.m_aPlots = new array<ref LF_PropertyPlotDef>();
		config.m_aPlots.Insert(MakePlot("plot-a", 5000));
		config.m_aPlots.Insert(MakePlot("house-x", 9000));
		config.m_aPlots.Insert(MakePlot("plot-b", 7500));
		ctx.True(LF_Houses.Remember(LF_HouseLockRecord.Create("house-x", false)), "the house row is stored");

		int refused = 0;
		// Red-proof hook: with --red the check is skipped, so the colliding plot stays for sale.
		if (!RedProof())
			refused = config.RefuseHouseCollisions();

		ctx.Equal(1, refused, "exactly one plot collides with a house");
		ctx.True(config.FindPlot("house-x") == null, "the plot named like a house is not for sale");
		ctx.NotNull(config.FindPlot("plot-a"), "an ordinary plot stays");
		ctx.NotNull(config.FindPlot("plot-b"), "the plot after the refused one stays");
		ctx.True(LF_Houses.Get("house-x") != null, "the house is untouched");
		ctx.Equal(0, config.RefuseHouseCollisions(), "a second pass refuses nothing");

		// A house that registers after the config loaded: the shipped example plot-a.
		LF_Houses.ResetStatics();
		LF_PropertyConfig.Reload();
		ctx.Equal(5000, LF_PropertyConfig.PlotPrice("plot-a"), "the shipped plot-a sells for 5000");
		bool dropped = false;
		if (!RedProof())
			dropped = LF_PropertyConfig.RefuseHouseId("plot-a");

		ctx.True(dropped, "a house registered as plot-a drops the loaded plot");
		ctx.Equal(0, LF_PropertyConfig.PlotPrice("plot-a"), "the dropped plot has no price");
		ctx.Equal(7500, LF_PropertyConfig.PlotPrice("plot-b"), "the other loaded plot keeps its price");
		ctx.False(LF_PropertyConfig.RefuseHouseId("not-a-plot"), "a house with no plot of its key refuses nothing");

		LF_PropertyConfig.Reload();
		LF_Houses.ResetStatics();
	}
}
