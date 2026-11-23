// red-proof: with the pack layer removed (an empty header as the pack source, the `RedProof()` hook) the buyer demand table has no `power_plant` row for coal, so the row, the cap, the refill, the price and the refusal lines all fail.
//! #4413 (0.3.0 work A1, rows half): the power plant buys coal by the truck load, read through the overlays under the
//! Everon 1989 header (the pack leg). The row is `power_plant` + coal lump, a premium over the harbour wholesale curve
//! (7 to 5 a lump) so the East Rise coal feeder lifts from about $385 to about $450 an hour: the Entry rung of the
//! 0.3.0 pay ladder (the Mining rung, between the Cadet's $350 shift and the $480 postal run). The cap is a truck
//! load and refills over time; a sale past the cap is refused and the buyer says when it will take more
//! (`#LF-Market_BuyerFull`, "try again in about N min"), never a silent no-op. The place (the buyer NPC and the coal
//! yard) is the Day 5 card "A1 place"; this card is the rows only.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronPowerPlantCoal : LF_Test
{
	static const ResourceName COAL = "{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et";
	static const string BUYER = "power_plant";
	static const int NOW = 1800000480;	// lf-fixed: a unix time past 2^24, where float maths loses seconds
	//! Lumps an hour the coal feeder loop hauls (`tools\cli economy`, East Rise by car: 3.6 trips of 20).
	static const int LOOP_UNITS_PER_HOUR = 71;	// lf-fixed: the economy report's coal-feeder figure
	//! The 0.3.0 pay ladder (docs/roadmap/next-release-0.3.0.md section 2): the Cadet shift is the lowest rung, the
	//! Entry rung ends at the postal run's $480 an hour.
	static const int CADET_SHIFT = 350;	// lf-fixed: ladder floor
	static const int ENTRY_CEILING = 480;	// lf-fixed: ladder Entry ceiling

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/power-plant-coal";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_BuyerDemandConfig.Reload();
		LF_MarketConfig.Reload();
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

		Resource resource = Resource.Load(COAL);
		ctx.True(resource && resource.IsValid(), "the coal lump resolves to a real prefab");

		LF_BuyerDemandRow row = demand.Find(BUYER, COAL);
		ctx.NotNull(row, "the power plant has a buyer row for coal lumps");
		if (!row)
			return;

		ctx.EqualStr("", row.Problem(), "the coal row is usable");
		CheckCapAndRefill(ctx, row);
		CheckPriceRung(ctx, row, market);
		CheckRefusal(ctx, row);
		ctx.True(demand.Find("hardware", COAL) == null, "coal still has no premium row at the hardware store");
	}

	//------------------------------------------------------------------------------------------------
	//! A truck load a visit, a purse that fills again, an hourly refill (not a period row), one lump a unit.
	protected void CheckCapAndRefill(LF_TestContext ctx, LF_BuyerDemandRow row)
	{
		ctx.True(row.m_iCap >= 40, string.Format("the cap %1 is at least a truck load of coal (40)", row.m_iCap));
		ctx.True(row.m_iRefillPerHour > 0, "the demand refills over time");
		ctx.Equal(0, row.m_iPeriodHours, "the coal row refills by the hour, not by a weekly period");
		ctx.True(row.m_iRefillPerHour < LOOP_UNITS_PER_HOUR, "the refill is below what the loop hauls, so a full loop spills to wholesale");
		ctx.Equal(1, row.Lot(), "coal is bought by the lump");
	}

	//------------------------------------------------------------------------------------------------
	//! The price beats the wholesale base, and the loop's hour lands on the Entry rung: the premium share alone stays
	//! under the rung's ceiling, and even the rest sold at the wholesale floor stays over the Cadet shift.
	protected void CheckPriceRung(LF_TestContext ctx, LF_BuyerDemandRow row, LF_MarketConfig market)
	{
		LF_MarketGoodDef curve = market.FindGood(COAL);
		ctx.NotNull(curve, "coal has a wholesale curve to measure the premium against");
		if (!curve)
			return;

		ctx.True(row.m_iPrice > curve.m_iBase / curve.Lot(), string.Format("the power plant pays %1 a lump, above the wholesale base %2", row.m_iPrice, curve.m_iBase / curve.Lot()));

		int premiumPerHour = row.m_iRefillPerHour * row.m_iPrice;
		int rest = LOOP_UNITS_PER_HOUR - row.m_iRefillPerHour;
		int floorPerHour = premiumPerHour + rest * curve.m_iFloor / curve.Lot();
		ctx.True(premiumPerHour <= ENTRY_CEILING, string.Format("the premium share $%1 an hour stays inside the Entry rung (at most $%2)", premiumPerHour, ENTRY_CEILING));
		ctx.True(floorPerHour >= CADET_SHIFT, string.Format("the loop's worst steady hour $%1 stays at or above the Cadet shift ($%2)", floorPerHour, CADET_SHIFT));
	}

	//------------------------------------------------------------------------------------------------
	//! A full purse takes exactly its cap; the next lumps are refused as "enough for now", with a time to try again.
	protected void CheckRefusal(LF_TestContext ctx, LF_BuyerDemandRow row)
	{
		LF_BuyerPurse purse = LF_BuyerPurse.Full(row, NOW);
		LF_BuyerDemandSale first = LF_BuyerDemand.Sell(row, purse, row.m_iCap + 5, NOW);
		ctx.Equal(row.m_iCap, first.m_iAccepted, "a full purse takes the whole cap");
		ctx.Equal(5, first.m_iRefused, "the 5 lumps past the cap are refused");
		ctx.Equal(row.m_iCap * row.m_iPrice, first.m_iPayout, "the cap pays the flat row price a lump");

		LF_BuyerDemandSale spent = LF_BuyerDemand.Sell(row, purse, 10, NOW);
		ctx.Equal(0, spent.m_iAccepted, "a spent purse takes nothing");
		ctx.Equal(0, spent.m_iPayout, "a spent purse pays nothing");
		ctx.True(spent.m_iRefused > spent.m_iUnderLot, "the refusal is the buyer-full kind, which the seller is told in words");
		ctx.True(spent.m_iSecondsToNext > 0, "the buyer says when it will take more (it refills), not 'never'");

		int later = NOW + LF_PriceCurve.SECONDS_PER_HOUR;
		LF_BuyerDemandSale again = LF_BuyerDemand.Sell(row, purse, row.m_iCap, later);
		ctx.Equal(row.m_iRefillPerHour, again.m_iAccepted, "an hour later the purse has refilled by the hourly amount");
	}
}
