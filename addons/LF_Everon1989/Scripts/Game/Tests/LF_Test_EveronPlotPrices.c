// red-proof: observed 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronPlotPrices` printed `RED PROOF OK` (RedProof() reads the core catalogue alone): failed `saint-philippe.p4 is for sale at a real price` and `saint-philippe.p4 names the house tier built on it`; green without the hook (pack leg, worlds=LF_Everon1989_Test).
//! #2936: under the Everon 1989 header (the pack leg of `tools\cli test`), the four free plots near
//! Saint-Philippe are priced by the pack's property overlay, so a plot sign there offers Buy. The
//! core catalogue ships two neutral example plots only (#2976 lets the pack add its own).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPlotPrices : LF_Test
{
	static const ResourceName OVERLAY = "{7C3E19A4D5B28F10}Configs/Property/LF_Property_Everon1989.conf";

	override string GetName()
	{
		return "everon/plot-prices";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon property overlay");

		LF_PropertyConfig.Reload();
		if (RedProof())
			LF_PropertyConfig.Reload("{D9770AC8486445C3}Configs/Property/LF_Property.conf");

		array<string> keys = {"saint-philippe.p1", "saint-philippe.p2", "saint-philippe.p3", "saint-philippe.p4"};
		foreach (string key : keys)
		{
			ctx.True(LF_PropertyConfig.PlotPrice(key) > 0, key + " is for sale at a real price");
			ctx.True(!LF_PropertyConfig.PlotTierId(key).IsEmpty(), key + " names the house tier built on it");
		}

		ctx.True(LF_PropertyConfig.PlotPrice("plot-a") > 0, "the core example plot stays in the catalogue");

		LF_PropertyConfig.Reload();
	}
}
