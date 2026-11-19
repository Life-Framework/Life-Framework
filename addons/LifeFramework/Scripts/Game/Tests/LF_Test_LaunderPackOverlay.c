// red-proof: with the `RedProof()` hook skipping the pack source, the pack's overlays never load, so `launder: the pack's cut replaces the core cut` (expected 41, got 30), `buyer prices: the pack's moonshine price replaces the core price` (expected 91, got 60) and the added plum row fail; run `tools\cli test --no-build --red LF_Test_LaunderPackOverlay`.
//! #2922: the laundering config and the buyer price list read pack overlays, like LF_PropertyConfig (#2977).
//! An overlay sets only the fields it writes (the launder config) and each price row it names replaces the
//! core row of the same prefab or adds one (the buyer prices). Without a pack the core file answers alone. The
//! test overlays (Configs/Tests/LF_Test_LaunderOverlay.conf, LF_Test_BuyerPricesOverlay.conf) are reached
//! through a mission header built in code.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_LaunderPackOverlay : LF_Test
{
	static const ResourceName LAUNDER_OVERLAY = "{4A61003C47D9661E}Configs/Tests/LF_Test_LaunderOverlay.conf";
	static const ResourceName PRICES_OVERLAY = "{0E71FE5BB05EA645}Configs/Tests/LF_Test_BuyerPricesOverlay.conf";
	static const ResourceName MOONSHINE = "{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et";
	static const ResourceName PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "laundering/pack-overlay";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ConfigService.Reset();
		LF_LaunderConfig.ResetStatics();
		LF_BuyerPrices.ResetStatics();

		// No pack: the core files alone.
		LF_LaunderConfig core = LF_LaunderConfig.Get();
		ctx.NotNull(core, "the core launder config loads");
		if (core)
		{
			ctx.False(core.m_bCrimePaysMarked, "without a pack, crime pays plain cash");
			ctx.Equal(30, core.m_iCutPct, "without a pack, the cut is the core cut");
		}

		ctx.Equal(60, LF_BuyerPrices.PriceOf(MOONSHINE), "without a pack, the buyer pays the core moonshine price");
		ctx.Equal(0, LF_BuyerPrices.PriceOf(PLUM), "without a pack, the plum is not on the list");

		LF_MissionHeader header = new LF_MissionHeader();
		header.m_aLF_ConfigOverlays = {};
		header.m_aLF_ConfigOverlays.Insert(LAUNDER_OVERLAY);
		header.m_aLF_ConfigOverlays.Insert(PRICES_OVERLAY);
		LF_MissionConfigPackSource source = new LF_MissionConfigPackSource();
		source.SetHeader(header);
		if (!RedProof())
			LF_ConfigService.SetPackSource(source);

		LF_LaunderConfig.ResetStatics();
		LF_BuyerPrices.ResetStatics();

		LF_LaunderConfig layered = LF_LaunderConfig.Get();
		ctx.NotNull(layered, "the launder config loads under a pack");
		if (layered)
		{
			ctx.True(layered.m_bCrimePaysMarked, "launder: the pack turns the dirty-money switch on");
			ctx.Equal(41, layered.m_iCutPct, "launder: the pack's cut replaces the core cut");
			ctx.Equal(777, layered.m_iInformantFee, "launder: the pack's informant fee replaces the core fee");
			ctx.Equal(2000, layered.m_iBatchMax, "launder: a field the pack leaves out stays core");
			ctx.Equal(10000, layered.m_iDailyCap, "launder: the cap the pack leaves out stays core");
		}

		ctx.Equal(91, LF_BuyerPrices.PriceOf(MOONSHINE), "buyer prices: the pack's moonshine price replaces the core price");
		ctx.Equal(12, LF_BuyerPrices.PriceOf(PLUM), "buyer prices: a new prefab in the overlay adds a row");
		LF_BuyerPrices prices = LF_BuyerPrices.Get();
		ctx.NotNull(prices, "the buyer prices load under a pack");
		if (prices)
		{
			ctx.Equal(2, prices.m_aPrices.Count(), "buyer prices: one row replaced, one added");
			ctx.Equal(7, Math.Round(prices.m_fReach), "buyer prices: the pack's reach replaces the core reach");
			ctx.Equal(1000, prices.m_iMinActionMs, "buyer prices: the repeat gap the pack leaves out stays core");
		}

		LF_ConfigService.Reset();
		LF_LaunderConfig.ResetStatics();
		LF_BuyerPrices.ResetStatics();
	}
}
