// red-proof: observed 2026-11-19 in the batched `--red` run with LF_Test_MarketLotSale and LF_Test_MarketLotSize: RED PROOF OK. With the pack layer removed (an empty header as the pack source, the `RedProof()` hook) the market has no grain curve and the buyer demand table has no bakery flour row, so every grain and flour line fails.
//! #3622: the pack's grain buyers (harvester spike, section 6), read through the overlays under the Everon 1989
//! header (the pack leg). Grain is counted in kg, so its prices are per lot: the granary curve is $9 a 200 kg lot
//! ($0.045 a kg) falling 1 percent a lot to a $7 floor, and the bakery's flour row is $1 a 5 kg lot ($0.20 a kg)
//! from a 1,000 kg purse. A sale pays whole lots only, the milled kg of flour fetches more than the grain it came
//! from, and the table sells a field of grain (2,600 kg, 13 lots) at about $0.04 a kg.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronGrainBuyers : LF_Test
{
	static const ResourceName GRANARY_OVERLAY = "{8D41A7C2E5F30B90}Configs/Market/LF_Market_Granary_Everon1989.conf";
	static const ResourceName BAKERY_OVERLAY = "{8D41A7C2E5F30B92}Configs/Market/LF_BuyerDemand_Bakery_Everon1989.conf";
	static const ResourceName GRAIN = "{7B2E5A1C93D40F11}Prefabs/Items/Harvest/LF_GrainSack.et";
	static const ResourceName FLOUR = "{7B2E5A1C93D40F12}Prefabs/Items/Harvest/LF_FlourSack.et";
	static const int NOW = 1800000480;	// lf-fixed: a unix time past 2^24, where float maths loses seconds

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/grain-buyers";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
		{
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(GRANARY_OVERLAY) != -1, "the header lists the granary overlay");
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(BAKERY_OVERLAY) != -1, "the header lists the bakery overlay");
		}

		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_BuyerDemandConfig.Reload();
		LF_MarketConfig.Reload();
		LF_MarketBook.ResetStatics();
		LF_BuyerDemandConfig demand = LF_BuyerDemandConfig.Get();
		LF_MarketConfig market = LF_MarketConfig.Get();
		if (RedProof())
		{
			LF_ConfigService.Reset();
			LF_BuyerDemandConfig.Reload();
			LF_MarketConfig.Reload();
		}

		ctx.NotNull(demand, "the buyer demand config loads under the pack header");
		ctx.NotNull(market, "the market config loads under the pack header");
		if (!demand || !market)
			return;

		CheckGranary(ctx, market);
		CheckBakery(ctx, demand, market);
		LF_MarketBook.ResetStatics();
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckGranary(LF_TestContext ctx, LF_MarketConfig market)
	{
		Resource resource = Resource.Load(GRAIN);
		ctx.True(resource && resource.IsValid(), "the grain sack resolves to a real prefab");

		LF_MarketGoodDef curve = market.FindGood(GRAIN);
		ctx.NotNull(curve, "grain has a wholesale curve at the granary");
		if (!curve)
			return;

		ctx.EqualStr("", curve.Problem(), "the grain curve is valid");
		ctx.Equal(200, curve.m_iLotSize, "grain is priced per 200 kg");
		ctx.Equal(9, curve.m_iBase, "a lot starts at $9, which is $0.045 a kg");
		ctx.Equal(9, curve.m_iCeiling, "the ceiling is the base");
		ctx.Equal(7, curve.m_iFloor, "the floor is $7 a lot");
		ctx.Equal(1, curve.m_iDropPerUnitPct, "each lot sold takes 1 percent off");
		ctx.True(curve.m_iRecoverPctPerHour > 0, "the price recovers");

		// A field of grain (2,600 kg, 13 lots) sells at the granary near $0.04 a kg, never at a whole $1 a kg.
		ctx.True(market.m_bEnabled, "the pack switches the market on");
		int field = LF_MarketBook.QuoteAt(GRAIN, 2600, NOW);
		ctx.True(field > 90 && field <= 117, string.Format("a 2,600 kg field pays between 90 and 117, got %1", field));
		ctx.Equal(0, LF_MarketBook.QuoteAt(GRAIN, 199, NOW), "199 kg is below one lot and pays nothing");
		ctx.Equal(400, LF_MarketBook.SellableUnits(GRAIN, 450), "450 kg sells 400 and keeps 50");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckBakery(LF_TestContext ctx, LF_BuyerDemandConfig demand, LF_MarketConfig market)
	{
		Resource resource = Resource.Load(FLOUR);
		ctx.True(resource && resource.IsValid(), "the flour sack resolves to a real prefab");

		LF_BuyerDemandRow row = demand.Find("bakery", FLOUR);
		ctx.NotNull(row, "the bakery has a premium row for flour");
		if (!row)
			return;

		ctx.EqualStr("", row.Problem(), "the flour row is valid");
		ctx.Equal(5, row.m_iLotSize, "flour is priced per 5 kg");
		ctx.Equal(1, row.m_iPrice, "a lot fetches $1, which is $0.20 a kg");
		ctx.Equal(1000, row.m_iCap, "the bakery takes 1,000 kg");
		ctx.True(row.m_iRefillPerHour > 0, "the bakery's demand refills");

		// 2 kg of grain make 1 kg of flour: the flour fetches more than the grain it came from.
		LF_MarketGoodDef curve = market.FindGood(GRAIN);
		if (curve)
		{
			int flourPerKgCents = row.m_iPrice * 100 / row.m_iLotSize;
			int grainPerKgCents = curve.m_iBase * 100 / curve.m_iLotSize;
			ctx.True(flourPerKgCents > grainPerKgCents * 2, string.Format("a kg of flour (%1 cents) beats the 2 kg of grain it needs (%2 cents)", flourPerKgCents, grainPerKgCents * 2));
		}

		LF_BuyerPurse purse = LF_BuyerPurse.Full(row, NOW);
		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, 52, NOW);
		ctx.Equal(50, sale.m_iAccepted, "52 kg of flour sells 50 kg");
		ctx.Equal(2, sale.m_iRefused, "the 2 kg part lot stays with the seller");
		ctx.Equal(10, sale.m_iPayout, "ten 5 kg lots at $1 pay $10");
	}
}
