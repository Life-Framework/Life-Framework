// red-proof: 2026-11-16, `tools\cli test --no-build --red LF_Test_EveronDrinksImpair` (RedProof skips the overlay apply): RED PROOF OK, failed "the moonshine row applies" and "the beer row applies"; green without the hook (13 assertions, pack leg worlds=LF_Everon1989_Test).
// tier: WORLD
//! #2003 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the pack's drink
//! overlay applies over the neutral core table. Beer and moonshine get their rows, and moonshine is the
//! strongest drink. This replaced the node selftest that read the overlay's text
//! (tools/validation/pack-drinks-impair-selftest.mjs), which could not show the row applies.
class LF_Test_EveronDrinksImpair : LF_Test
{
	static const ResourceName OVERLAY = "{E721EE6C88C0A808}Configs/Survival/LF_DrinkImpairment_Everon1989.conf";
	static const ResourceName MOONSHINE = "{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et";
	static const ResourceName BEER = "{4CBB1F9A04D15772}Prefabs/Items/Drinks/BeerBottle.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/drinks-impair";
	}

	//------------------------------------------------------------------------------------------------
	//! The core table as shipped, before any overlay.
	protected LF_DrinkImpairmentConfig LoadCore()
	{
		Resource container = BaseContainerTools.LoadContainer(LF_DrinkImpairmentConfig.CONFIG_PATH);
		if (!container || !container.IsValid() || !container.GetResource())
			return null;

		LF_DrinkImpairmentConfig config = LF_DrinkImpairmentConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (config && !config.m_aDrinks)
			config.m_aDrinks = new array<ref LF_DrinkImpairmentRow>();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The leg boots under the pack header, and that header lists the drink overlay.
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the Everon drink overlay");

		LF_DrinkImpairmentConfig table = LoadCore();
		ctx.NotNull(table, "the core drink table loads");
		if (!table)
			return;

		ctx.True(table.Find(MOONSHINE) == null, "the core table alone has no moonshine row (the pack decides)");
		// The act: the running header's overlays layered over the core table.
		if (!RedProof())
			table.ApplyLayers();

		LF_DrinkImpairmentRow moonshine = table.Find(MOONSHINE);
		LF_DrinkImpairmentRow beer = table.Find(BEER);
		ctx.NotNull(moonshine, "the moonshine row applies");
		ctx.NotNull(beer, "the beer row applies");
		if (!moonshine || !beer)
			return;

		ctx.EqualFloat(40, moonshine.m_fStrength, 0.001, "moonshine strength");
		ctx.EqualFloat(1200, moonshine.m_fDurationSeconds, 0.001, "moonshine duration (s)");
		ctx.EqualFloat(90, moonshine.m_fCap, 0.001, "moonshine cap");
		ctx.True(moonshine.m_eCurve == LF_EImpairmentCurve.EXPONENTIAL, "moonshine wears off on the exponential curve");
		ctx.EqualFloat(12, beer.m_fStrength, 0.001, "beer strength");
		ctx.True(moonshine.m_fStrength > beer.m_fStrength && moonshine.m_fCap > beer.m_fCap, "moonshine is the strongest drink");

		// The live table the drink effect reads gives the same row.
		LF_DrinkImpairmentTable.ResetStatics();
		LF_DrinkImpairmentRow live = LF_DrinkImpairmentTable.Find(MOONSHINE);
		ctx.True(live && live.m_fStrength == moonshine.m_fStrength, "LF_DrinkImpairmentTable reads the overlaid moonshine row");
	}
}
