// red-proof: with the pack layer removed (an empty header as the pack source, the `RedProof()` hook) the buyer demand table is empty and the market has no lumber curves, so every `has a premium row` and `has a wholesale row` line fails.
//! #3269: the pack's lumber buyers (docs/roadmap/lumber.md "Buyers"), read through the overlays under the Everon
//! 1989 header (the pack leg). Every lumber good (plank, hardwood board, fence post, firewood bundle) has a
//! premium row with a demand cap that refills (hardware store, seed and feed shop, bakery) and a wholesale curve
//! row in the market (the harbour warehouse, #3273). The premium price beats the wholesale base for every good, and
//! no buyer pays more than the store charges for the same good (#3121).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronLumberBuyers : LF_Test
{
	static const ResourceName OVERLAY = "{6C1D4F82B9A3E500}Configs/Market/LF_BuyerDemand_Everon1989.conf";
	static const ResourceName PLANK = "{45D1B36F99DD2FC3}Prefabs/Resources/WoodCutting/WoodPlank.et";
	static const ResourceName BOARD = "{D2A8639FA0CA09EF}Prefabs/Resources/WoodCutting/HardwoodBoard.et";
	static const ResourceName POST = "{7EB79C84E76D667D}Prefabs/Resources/WoodCutting/FencePost.et";
	static const ResourceName FIREWOOD = "{F96D821A8E47FBBA}Prefabs/Resources/WoodCutting/FirewoodBundle.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/lumber-buyers";
	}

	//------------------------------------------------------------------------------------------------
	//! True when `good` is one of the four lumber goods this card prices.
	static bool IsLumberGood(ResourceName good)
	{
		return LF_Utils.SamePrefab(good, PLANK) || LF_Utils.SamePrefab(good, BOARD) || LF_Utils.SamePrefab(good, POST) || LF_Utils.SamePrefab(good, FIREWOOD);
	}

	//------------------------------------------------------------------------------------------------
	//! One lumber good against the design table: its premium buyer, price, cap and refill; its wholesale base,
	//! floor, drop and recovery.
	protected void CheckGood(LF_TestContext ctx, LF_BuyerDemandConfig demand, LF_MarketConfig market, string label, ResourceName good, string buyer, int price, int cap, int refill, int wholesaleBase, int wholesaleFloor, int drop, int recover)
	{
		Resource resource = Resource.Load(good);
		ctx.True(resource && resource.IsValid(), label + " resolves to a real prefab");

		LF_BuyerDemandRow row = demand.Find(buyer, good);
		ctx.NotNull(row, label + " has a premium row at " + buyer);
		if (row)
		{
			ctx.Equal(price, row.m_iPrice, label + " premium price");
			ctx.Equal(cap, row.m_iCap, label + " demand cap");
			ctx.Equal(refill, row.m_iRefillPerHour, label + " refill per hour");
			ctx.True(row.m_iRefillPerHour > 0, label + " demand refills");
		}

		LF_MarketGoodDef curve = market.FindGood(good);
		ctx.NotNull(curve, label + " has a wholesale curve row");
		if (curve)
		{
			ctx.Equal(wholesaleBase, curve.m_iBase, label + " wholesale base");
			ctx.Equal(wholesaleBase, curve.m_iCeiling, label + " wholesale ceiling is the base");
			ctx.Equal(wholesaleFloor, curve.m_iFloor, label + " wholesale floor");
			ctx.Equal(drop, curve.m_iDropPerUnitPct, label + " wholesale drop per unit");
			ctx.Equal(recover, curve.m_iRecoverPctPerHour, label + " wholesale recovery per hour");
			if (row)
				ctx.True(row.m_iPrice > curve.m_iBase, string.Format("%1 premium %2 beats the wholesale base %3", label, row.m_iPrice, curve.m_iBase));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! #3121: neither buyer pays more for a good than any store stock that sells it charges.
	protected void CheckNoLoop(LF_TestContext ctx, LF_BuyerDemandConfig demand, LF_MarketConfig market)
	{
		array<ResourceName> goods = {PLANK, BOARD, POST, FIREWOOD};
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
					ctx.True(curve.m_iBase <= item.m_iPrice, string.Format("wholesale pays %1 for %2, the %3 store sells it at %4 (#3121)", curve.m_iBase, good, stockId, item.m_iPrice));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MissionHeader header = LF_MissionHeader.GetCurrent();
		ctx.NotNull(header, "the pack leg runs under an LF mission header");
		if (header)
			ctx.True(header.m_aLF_ConfigOverlays && header.m_aLF_ConfigOverlays.Find(OVERLAY) != -1, "the header lists the lumber buyer overlay");

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

		CheckGood(ctx, demand, market, "plank", PLANK, "hardware", 8, 40, 20, 6, 4, 1, 5);
		CheckGood(ctx, demand, market, "hardwood board", BOARD, "hardware", 15, 32, 28, 7, 4, 1, 5);	// #4826: the truck rung; #4898: purse 32 +28/h for the measured 7.9 min cycle
		CheckGood(ctx, demand, market, "fence post", POST, "seed-feed", 11, 12, 6, 5, 3, 2, 5);
		CheckGood(ctx, demand, market, "firewood bundle", FIREWOOD, "bakery", 4, 30, 30, 2, 1, 2, 10);
		CheckNoLoop(ctx, demand, market);
	}
}
