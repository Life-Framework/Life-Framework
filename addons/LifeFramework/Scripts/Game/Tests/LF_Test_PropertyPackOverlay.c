// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_PropertyPackOverlay` printed `RED PROOF OK`; with the hook skipping the pack source it failed `- add: the pack's new plot is priced (expected 3300, got 0)` and `- remove: the pack takes plot-b out of the catalogue (expected 0, got 7500)`. `--only` then passed=2 failed=0 with PropertyConfig.
//! #2976: the property config reads pack overlays like LF_SitesConfig. The plot catalogue is a list keyed
//! by plot id: a pack row with an id the core has patches it (the price), a new id adds a plot, and the
//! remove verb takes one out. Without a pack the core file answers alone. The test overlay
//! (Configs/Tests/LF_Test_PropertyOverlay.conf) is reached through a mission header built in code.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PropertyPackOverlay : LF_Test
{
	static const ResourceName OVERLAY = "{5E2A9C41D07B3F67}Configs/Tests/LF_Test_PropertyOverlay.conf";

	override string GetName()
	{
		return "property/pack-overlay";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_PropertyConfig.Reload();

		// No pack: the core catalogue alone.
		ctx.Equal(5000, LF_PropertyConfig.PlotPrice("plot-a"), "without a pack, plot-a costs the core price");
		ctx.Equal(0, LF_PropertyConfig.PlotPrice("test-pack-plot"), "without a pack, the pack's plot is not for sale");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_PropertyConfig.Reload();
		ctx.Equal(6100, LF_PropertyConfig.PlotPrice("plot-a"), "patch: the pack's price replaces the core price");
		ctx.EqualStr("cottage", LF_PropertyConfig.PlotTierId("plot-a"), "patch: a field the pack leaves alone stays core");
		ctx.Equal(3300, LF_PropertyConfig.PlotPrice("test-pack-plot"), "add: the pack's new plot is priced");
		ctx.EqualStr("cottage", LF_PropertyConfig.PlotTierId("test-pack-plot"), "add: the new plot has its tier");
		ctx.Equal(0, LF_PropertyConfig.PlotPrice("plot-b"), "remove: the pack takes plot-b out of the catalogue");
		ctx.Equal(2, LF_PropertyConfig.PlotCount(), "plot-a patched, plot-b removed, one added");

		// A path override (tests) replaces the shipped file whole and takes no pack layer.
		LF_PropertyConfig.Reload("{D9770AC8486445C3}Configs/Property/LF_Property.conf");
		ctx.Equal(5000, LF_PropertyConfig.PlotPrice("plot-a"), "a path override reads the file alone");

		LF_ConfigService.Reset();
		LF_PropertyConfig.Reload();
	}
}
