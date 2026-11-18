// red-proof: new test, red first: with no LF_PriceBoard the file does not compile (all asserts fail); the RedProof() hook skips the sale so the "lower after a sale" assertion fails.
// red-proof (review #2887): on the unfixed code the answer carried the raw base (expected 0, got 40) and the shared board read had no rate limit (AllowReadAt did not exist).
//! The chalk price board (#2847): the server's answer lists every curved good with the book's price in
//! config order; a sale lowers the next read; a disabled market answers an empty board painted as
//! "no prices today".
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PriceBoard : LF_Test
{
	protected static const ResourceName COPPER = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	protected static const ResourceName IRON = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";

	override string GetName()
	{
		return "market/price-board";
	}

	protected LF_MarketConfig MakeConfig(bool enabled)
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = enabled;
		config.m_iMaxRecoveryHours = 168;
		config.m_aGoods = {};
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(COPPER, 40, 20, 60, 5, 10));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(IRON, 30, 15, 45, 5, 10));
		config.Sanitize();
		return config;
	}

	protected static string LineText(array<ref LF_UIPart> parts, string name)
	{
		foreach (LF_UIPart part : parts)
		{
			if (part.GetName() == name)
				return part.GetText();
		}

		return "<no part " + name + ">";
	}

	override void Run(LF_TestContext ctx)
	{
		LF_MarketBook.ResetStatics();
		int now = 1800000000;

		LF_MarketConfig.UseForTest(MakeConfig(true));
		LF_BoardAnswer first = LF_PriceBoard.BuildAnswerAt(now);
		ctx.Equal(2, first.m_aTexts.Count(), "the board lists every curved good");
		ctx.Equal(4, first.m_aNumbers.Count(), "two numbers per good");
		ctx.Equal(40, first.m_aNumbers[0], "copper first, at its base");
		ctx.Equal(0, first.m_aNumbers[1], "copper at base: the below-base flag is off; the base itself never crosses");
		ctx.Equal(30, first.m_aNumbers[2], "iron second, at its base");
		array<ref LF_UIPart> parts = LF_PriceBoard.BuildParts(first);
		ctx.False(LineText(parts, "Price0").EndsWith(LF_PriceBoard.ARROW_DOWN), "no arrow at the base");

		// A sale lowers the next read.
		int oldPrice;
		if (!RedProof())
			LF_MarketBook.ApplySaleAt(COPPER, 4, now, oldPrice);
		LF_BoardAnswer second = LF_PriceBoard.BuildAnswerAt(now);
		ctx.True(second.m_aNumbers[0] < 40, "after a sale the next read shows the lower price");
		ctx.Equal(LF_MarketBook.CurrentPriceAt(COPPER, now), second.m_aNumbers[0], "the board shows the book's price");
		ctx.Equal(30, second.m_aNumbers[2], "the other good is untouched");
		ctx.Equal(1, second.m_aNumbers[1], "below base the flag is on (the base number is never sent)");
		parts = LF_PriceBoard.BuildParts(second);
		ctx.True(LineText(parts, "Price0").EndsWith(LF_PriceBoard.ARROW_DOWN), "a price below base carries the arrow");
		ctx.False(LineText(parts, "Price1").EndsWith(LF_PriceBoard.ARROW_DOWN), "a price at base carries none");

		// Market off: empty, never stale prices.
		LF_MarketConfig.UseForTest(MakeConfig(false));
		LF_BoardAnswer off = LF_PriceBoard.BuildAnswerAt(now);
		ctx.Equal(0, off.m_aTexts.Count(), "a disabled market answers no goods");
		parts = LF_PriceBoard.BuildParts(off);
		ctx.False(LineText(parts, "None").StartsWith("<no part"), "the empty board paints the no-prices line");
		ctx.True(LineText(parts, "Price0").StartsWith("<no part"), "no price lines when disabled");

		// The provider is registered and answers like the helper.
		LF_BoardProviders.ResetStatics();
		ctx.True(LF_BoardProviders.Find(LF_PriceBoard.KIND) != null, "the prices kind is registered");

		// A client spamming the shared read RPC is refused after the burst, then admitted again (#2887 review).
		int spammer = 984701;
		int t0 = 2000000000;
		for (int i = 0; i < LF_RateLimit.COMMS_BURST; i++)
		{
			ctx.True(LF_BoardProviders.AllowReadAt(spammer, t0 + i, false), "reads within the burst are admitted");
		}

		ctx.False(LF_BoardProviders.AllowReadAt(spammer, t0 + 10, false), "a read past the burst is refused");
		ctx.True(LF_BoardProviders.AllowReadAt(spammer, t0 + LF_RateLimit.COMMS_WINDOW_MS + 1, false), "the next window admits again");

		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();
	}
}
