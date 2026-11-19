// red-proof: with the pack layer removed (an empty header as the pack source, the `RedProof()` hook) the market has no copper bar, coal or lot-priced iron and cement curves and the buyer demand table has no mining rows, so every `has a wholesale curve` and `has a premium row` line fails.
//! #3237: the pack's mining buyers (docs/roadmap, mining chains "Selling"), read through the overlays under the Everon
//! 1989 header (the pack leg). Every product has its own wholesale curve at the harbour warehouse: iron bar 20 / 14,
//! copper bar 40 / 20, cement bag 7 / 5, coal lump 7 / 5 (tuned into the duty band by #3246). The price drop and the recovery differ per product so a flooded
//! market pushes players to the next chain: copper falls fastest (3 percent a unit) and recovers slowest, the cement
//! bag falls slowest (0.2 percent a unit), iron recovers faster than copper. A drop below 1 percent is a lot price:
//! 1 percent a lot of 2 iron bars is 0.5 percent a bar, 1 percent a lot of 5 bags is 0.2 percent a bag. The premium
//! rows (copper bar, cement bag, iron bar at the hardware store) beat the wholesale base for the same good, copper has
//! the smallest purse and the slowest refill, cement the largest purse and the fastest refill. Coal has no premium row.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronMiningMarket : LF_Test
{
	static const ResourceName IRON_BAR = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	static const ResourceName COPPER_BAR = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName CEMENT = "{D58674633332DC86}Prefabs/Items/Resources/CementBag.et";
	static const ResourceName COAL = "{C2AE9B7E62F0369F}Prefabs/Resources/Mining/CoalLump.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/mining-market";
	}

	//------------------------------------------------------------------------------------------------
	//! True when `good` is one of the four mining products this card prices.
	static bool IsMiningGood(ResourceName good)
	{
		return LF_Utils.SamePrefab(good, IRON_BAR) || LF_Utils.SamePrefab(good, COPPER_BAR) || LF_Utils.SamePrefab(good, CEMENT) || LF_Utils.SamePrefab(good, COAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Tenths of a percent the price loses per unit: the drop of one lot spread over its units, so 1 percent a
	//! lot of 5 is 2. Lower is slower.
	static int DropTenthsPerUnit(LF_MarketGoodDef curve)
	{
		return curve.m_iDropPerUnitPct * 10 / curve.Lot();
	}

	//------------------------------------------------------------------------------------------------
	//! One product's wholesale curve against the design table. The prices are per lot, so a lot of 2 doubles them.
	protected LF_MarketGoodDef CheckCurve(LF_TestContext ctx, LF_MarketConfig market, string label, ResourceName good, int unitBase, int unitFloor, int dropPct, int recover, int lot)
	{
		Resource resource = Resource.Load(good);
		ctx.True(resource && resource.IsValid(), label + " resolves to a real prefab");

		LF_MarketGoodDef curve = market.FindGood(good);
		ctx.NotNull(curve, label + " has a wholesale curve");
		if (!curve)
			return null;

		ctx.Equal(unitBase * lot, curve.m_iBase, label + " wholesale base");
		ctx.Equal(unitBase * lot, curve.m_iCeiling, label + " wholesale ceiling is the base");
		ctx.Equal(unitFloor * lot, curve.m_iFloor, label + " wholesale floor");
		ctx.Equal(dropPct, curve.m_iDropPerUnitPct, label + " drop per lot");
		ctx.Equal(recover, curve.m_iRecoverPctPerHour, label + " recovery per hour");
		ctx.Equal(lot, curve.Lot(), label + " lot size");
		return curve;
	}

	//------------------------------------------------------------------------------------------------
	protected LF_BuyerDemandRow CheckPremium(LF_TestContext ctx, LF_BuyerDemandConfig demand, string label, ResourceName good, int price, int cap, int refill, LF_MarketGoodDef curve)
	{
		LF_BuyerDemandRow row = demand.Find("hardware", good);
		ctx.NotNull(row, label + " has a premium row at the hardware store");
		if (!row)
			return null;

		ctx.Equal(price, row.m_iPrice, label + " premium price");
		ctx.Equal(cap, row.m_iCap, label + " demand cap");
		ctx.Equal(refill, row.m_iRefillPerHour, label + " refill per hour");
		ctx.True(row.m_iRefillPerHour > 0, label + " demand refills");
		ctx.True(row.Problem().IsEmpty(), label + " premium row is usable: " + row.Problem());
		if (curve)
			ctx.True(row.m_iPrice * curve.Lot() > curve.m_iBase, string.Format("%1 premium %2 beats the wholesale base %3 a unit", label, row.m_iPrice, curve.m_iBase / curve.Lot()));
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! #3121: no buyer pays more for a good than any store stock that sells it charges.
	protected void CheckNoLoop(LF_TestContext ctx, LF_BuyerDemandConfig demand, LF_MarketConfig market)
	{
		array<ResourceName> goods = {IRON_BAR, COPPER_BAR, CEMENT, COAL};
		array<string> stocks = {"hardware", "general_store"};
		foreach (string stockId : stocks)
		{
			LF_ShopStock stock = LF_ShopStockConfig.Get().Find(stockId);
			if (!stock)
				continue;

			foreach (ResourceName good : goods)
			{
				LF_ShopStockItem item = stock.FindItem(good);
				if (!item)
					continue;

				foreach (LF_BuyerDemandRow row : demand.m_aRows)
				{
					if (row && LF_Utils.SamePrefab(row.m_sGood, good))
						ctx.True(row.m_iPrice <= item.m_iPrice, string.Format("%1 buys %2 at %3, the %4 store sells it at %5 (#3121)", row.m_sBuyer, good, row.m_iPrice, stockId, item.m_iPrice));
				}

				LF_MarketGoodDef curve = market.FindGood(good);
				if (curve)
					ctx.True(curve.m_iBase <= item.m_iPrice * curve.Lot(), string.Format("wholesale pays %1 a lot for %2, the %3 store sells it at %4 a unit (#3121)", curve.m_iBase, good, stockId, item.m_iPrice));
			}
		}
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

		LF_MarketGoodDef iron = CheckCurve(ctx, market, "iron bar", IRON_BAR, 20, 14, 1, 30, 2);
		LF_MarketGoodDef copper = CheckCurve(ctx, market, "copper bar", COPPER_BAR, 40, 20, 3, 10, 1);
		LF_MarketGoodDef cement = CheckCurve(ctx, market, "cement bag", CEMENT, 7, 5, 1, 40, 5);
		LF_MarketGoodDef coal = CheckCurve(ctx, market, "coal lump", COAL, 7, 5, 1, 25, 1);

		// The gate: each product falls at its own rate.
		if (iron && copper && cement && coal)
		{
			ctx.True(DropTenthsPerUnit(copper) > DropTenthsPerUnit(coal), "copper drops faster than coal");
			ctx.True(DropTenthsPerUnit(coal) > DropTenthsPerUnit(iron), "coal drops faster than iron");
			ctx.True(DropTenthsPerUnit(iron) > DropTenthsPerUnit(cement), "iron drops faster than cement");
			ctx.Equal(30, DropTenthsPerUnit(copper), "copper drops 3 percent a bar");
			ctx.Equal(5, DropTenthsPerUnit(iron), "iron drops 0.5 percent a bar");
			ctx.Equal(2, DropTenthsPerUnit(cement), "cement drops 0.2 percent a bag, the slowest");
			ctx.True(iron.m_iRecoverPctPerHour > copper.m_iRecoverPctPerHour, "iron recovers faster than copper");
			ctx.True(copper.m_iRecoverPctPerHour < coal.m_iRecoverPctPerHour, "copper recovers slowest of the metals and coal");
			ctx.True(cement.m_iRecoverPctPerHour > iron.m_iRecoverPctPerHour, "cement recovers fastest");

			// Flooding one product moves players to another: after the same 40 units sold the price of copper has
			// lost a larger share than iron or cement.
			int share = FloodShareLost(copper, 40);
			ctx.True(share > FloodShareLost(iron, 40), "40 copper bars cost the copper price more of its range than 40 iron bars");
			ctx.True(FloodShareLost(iron, 40) > FloodShareLost(cement, 40), "40 iron bars cost the iron price more of its range than 40 cement bags");
		}

		// Premium rows: copper the smallest purse and slowest refill, cement the largest purse and fastest refill.
		LF_BuyerDemandRow copperRow = CheckPremium(ctx, demand, "copper bar", COPPER_BAR, 52, 12, 4, copper);
		LF_BuyerDemandRow ironRow = CheckPremium(ctx, demand, "iron bar", IRON_BAR, 22, 30, 15, iron);
		LF_BuyerDemandRow cementRow = CheckPremium(ctx, demand, "cement bag", CEMENT, 10, 120, 60, cement);
		if (copperRow && ironRow && cementRow)
		{
			ctx.True(copperRow.m_iCap < ironRow.m_iCap && ironRow.m_iCap < cementRow.m_iCap, "the purse grows from copper to iron to cement");
			ctx.True(copperRow.m_iRefillPerHour < ironRow.m_iRefillPerHour && ironRow.m_iRefillPerHour < cementRow.m_iRefillPerHour, "the refill grows from copper to iron to cement");
		}

		ctx.True(demand.Find("hardware", COAL) == null, "coal has no premium row at the hardware store");
		CheckNoLoop(ctx, demand, market);
	}

	//------------------------------------------------------------------------------------------------
	//! Tenths of a percent of the base-to-floor range the price has lost after `units` sold, stopping at the floor.
	protected int FloodShareLost(LF_MarketGoodDef curve, int units)
	{
		int lots = units / curve.Lot();
		int lostFine = lots * curve.DropPerUnit();
		int rangeFine = curve.BaseFine() - curve.FloorFine();
		if (rangeFine <= 0)
			return 0;
		if (lostFine > rangeFine)
			lostFine = rangeFine;
		return lostFine * 1000 / rangeFine;
	}
}
