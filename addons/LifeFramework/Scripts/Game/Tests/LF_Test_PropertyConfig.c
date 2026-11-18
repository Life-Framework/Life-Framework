// red-proof: observed 2026-11-17 `tools\cli test --no-build --red LF_Test_PropertyConfig` printed `RED-OK property/config (fails with the hook on, 2 assertions)` (the hook points Get() at a missing config); `--only` then passed=1 failed=0.
//! Property config (#2647, slice P2 of #747): Configs/Property/LF_Property.conf describes plots (id,
//! tier, price), house tiers with four build stages and what each stage needs (prefab and count, work
//! units). A missing config logs LF_Debug.Error and every getter returns a safe default.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PropertyConfig : LF_Test
{
	protected static const ResourceName MISSING = "Configs/Property/LF_NoSuchProperty.conf";

	override string GetName()
	{
		return "property/config";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_PropertyConfig.Reload();
		if (RedProof())
			LF_PropertyConfig.Reload(MISSING);
		LF_PropertyConfig config = LF_PropertyConfig.Get();
		ctx.NotNull(config, "the shipped Configs/Property/LF_Property.conf resolves");
		if (!config)
			return;

		// A plot: id, tier, price.
		LF_PropertyPlotDef plot = config.FindPlot("plot-a");
		ctx.NotNull(plot, "plot-a is in the catalogue");
		if (plot)
		{
			ctx.EqualStr("plot-a", plot.m_sPlotId, "the plot keeps its id");
			ctx.EqualStr("cottage", plot.m_sTierId, "the plot names its tier");
			ctx.True(plot.m_iPrice > 0, "the plot has a price");
		}
		ctx.Equal(5000, LF_PropertyConfig.PlotPrice("plot-a"), "PlotPrice reads the plot price");
		ctx.EqualStr("cottage", LF_PropertyConfig.PlotTierId("plot-a"), "PlotTierId reads the plot tier");
		ctx.True(LF_PropertyConfig.PlotCount() >= 2, "the example catalogue holds at least two plots");

		// A tier with four stages; shell prefab per stage; owned outright (no rent).
		LF_PropertyTierDef tier = config.FindTier("cottage");
		ctx.NotNull(tier, "the cottage tier is defined");
		ctx.Equal(4, LF_PropertyConfig.StageCount("cottage"), "a tier has four build stages");
		ctx.Equal(4, LF_PropertyConfig.ShellPrefabCount("cottage"), "one shell prefab per stage");
		ctx.False(LF_PropertyConfig.ShellPrefab("cottage", 3).IsEmpty(), "stage 3 has a shell prefab");
		ctx.Equal(0, LF_PropertyConfig.RentIntervalHours("cottage"), "rentIntervalHours 0 means owned outright");
		ctx.Equal(0, LF_PropertyConfig.GraceHours("cottage"), "graceHours 0 means owned outright");

		// Stage 1 is excavation: work only. Stage 2 is the foundation: gravel and cement bags plus work.
		ctx.Equal(0, LF_PropertyConfig.RequirementCount("cottage", 1), "stage 1 (excavation) needs no items");
		ctx.True(LF_PropertyConfig.StageWork("cottage", 1) > 0, "stage 1 needs work units");
		ctx.Equal(2, LF_PropertyConfig.RequirementCount("cottage", 2), "stage 2 (foundation) needs two kinds of item");
		ctx.True(LF_PropertyConfig.StageWork("cottage", 2) > 0, "stage 2 needs work units");

		LF_PropertyStageReqDef gravel = LF_PropertyConfig.Requirement("cottage", 2, 0);
		ctx.NotNull(gravel, "stage 2 requirement 0 resolves");
		if (gravel)
		{
			ctx.False(gravel.m_Prefab.IsEmpty(), "the requirement names a prefab");
			ctx.True(gravel.m_iCount > 0, "the requirement has a count");
		}

		// Out-of-range lookups are safe.
		ctx.Equal(0, LF_PropertyConfig.RequirementCount("cottage", 9), "an unknown stage needs nothing");
		ctx.Equal(0, LF_PropertyConfig.StageWork("cottage", 0), "stage 0 has no work");
		ctx.True(LF_PropertyConfig.Requirement("cottage", 2, 99) == null, "an unknown requirement index is null");
		ctx.Equal(0, LF_PropertyConfig.StageCount("no-such-tier"), "an unknown tier has no stages");
		ctx.Equal(0, LF_PropertyConfig.PlotPrice("no-such-plot"), "an unknown plot has price 0");
		ctx.True(LF_PropertyConfig.PlotTierId("no-such-plot").IsEmpty(), "an unknown plot has no tier");

		// A missing config: Error logged, null instance, safe defaults from every getter.
		LF_PropertyConfig.Reload(MISSING);
		ctx.True(LF_PropertyConfig.Get() == null, "a missing config gives no instance");
		ctx.Equal(0, LF_PropertyConfig.PlotCount(), "missing config: no plots");
		ctx.Equal(0, LF_PropertyConfig.StageCount("cottage"), "missing config: no stages");
		ctx.Equal(0, LF_PropertyConfig.PlotPrice("plot-a"), "missing config: price 0");
		ctx.Equal(0, LF_PropertyConfig.RequirementCount("cottage", 2), "missing config: no requirements");
		ctx.True(LF_PropertyConfig.ShellPrefab("cottage", 1).IsEmpty(), "missing config: empty shell prefab");

		LF_PropertyConfig.Reload();
		ctx.NotNull(LF_PropertyConfig.Get(), "Reload() restores the shipped config");
	}
}
