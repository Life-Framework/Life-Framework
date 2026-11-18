// red-proof: new test with no failing state on main (no notice existed). Batched hook run:
// `tools\cli test --no-build --red LF_Test_MarketSaleNotice` skips the band-crossing sale in RedProof() so
// the down-key assertions fail.
//! The trader's line after a sale moved the price (#2923): once when a band is crossed down, nothing
//! inside a band, an up line once the good recovered, nothing at all with the market off.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_MarketSaleNotice : LF_Test
{
	protected static const ResourceName COPPER = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";

	//! Base 40, 5 percent of it (2) off per unit, a band every 8 (four units).
	protected static const int BASE = 40;

	override string GetName()
	{
		return "market/sale-notice";
	}

	protected LF_MarketConfig MakeConfig(bool enabled)
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = enabled;
		config.m_iMaxRecoveryHours = 168;
		config.m_aGoods = {};
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(COPPER, BASE, 10, 60, 5, 10));
		config.Sanitize();
		return config;
	}

	//! Books a sale of `units` at `at` and returns the notice key it earns.
	protected string Sell(int units, int at)
	{
		int oldPrice;
		int newPrice = LF_MarketBook.ApplySaleAt(COPPER, units, at, oldPrice);
		return LF_MarketSaleNotice.Evaluate(COPPER, oldPrice, newPrice);
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MarketBook.ResetStatics();
		LF_MarketSaleNotice.ResetStatics();
		int now = 1800000000;
		LF_MarketConfig.UseForTest(MakeConfig(true));

		ctx.Equal(0, LF_MarketSaleNotice.Band(BASE, BASE), "the base is band 0");
		ctx.Equal(0, LF_MarketSaleNotice.Band(BASE, 33), "a little under the base is still band 0");
		ctx.Equal(1, LF_MarketSaleNotice.Band(BASE, 32), "a fifth under the base is band 1");
		ctx.Equal(0, LF_MarketSaleNotice.Band(BASE, BASE + 5), "above the base counts as band 0");

		ctx.EqualStr("", Sell(1, now), "a first sale inside the top band says nothing");

		string crossed = "<skipped>";
		if (!RedProof())
			crossed = Sell(3, now);
		ctx.EqualStr(LF_MarketSaleNotice.KEY_DOWN, crossed, "a sale crossing a band says the down line");
		ctx.EqualStr("", Sell(1, now), "a second sale inside the same band says nothing");

		// A day later the price crept back to the base; the next seller hears it.
		int later = now + 24 * LF_PriceCurve.SECONDS_PER_HOUR;
		ctx.Equal(BASE, LF_MarketBook.CurrentPriceAt(COPPER, later), "a day of recovery is back at the base");
		ctx.EqualStr(LF_MarketSaleNotice.KEY_UP, Sell(1, later), "the recovered good says the up line to the next seller");
		ctx.EqualStr("", Sell(1, later), "and only once");

		// Market off: silence, and the old band is not remembered.
		LF_MarketConfig.UseForTest(MakeConfig(false));
		ctx.EqualStr("", LF_MarketSaleNotice.Evaluate(COPPER, 40, 20), "market off says nothing");

		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();
		LF_MarketSaleNotice.ResetStatics();
	}
};
