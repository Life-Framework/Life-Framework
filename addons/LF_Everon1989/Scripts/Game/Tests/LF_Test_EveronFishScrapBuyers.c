// red-proof: with the pack layer removed (an empty header as the pack source) the mechanic stock has no buyback, and the `RedProof()` hook points the fish checks at the core trader prefab, which lists no fish, so every `buys` and `wholesale lists` line fails.
//! The island's first buyers for what the fishing and scrapyard jobs produce (docs/player-manual/03-jobs.md).
//! Fish: the dock wholesale buyer (`LF_WholesaleBuyer_Everon1989`) lists every fish the fishing config can land
//! (each species, small, medium and large) and every smoked fish the smokehouse makes, in its storage filter and
//! its price list, at 65 percent of the fish counter's catalogue rate (a bulk buyer pays less than a counter), with no curve (a spot's stock is the limit, 40 fish an hour).
//! Scrap: the garage mechanic's stock buys scrap metal, copper wire and rubber scrap back at half their catalogue
//! price (4, 12 and 5), never stolen goods, never above what the goods cost new.
[LF_TestInfo(LF_TestTier.LOGIC, pack: true)]
class LF_Test_EveronFishScrapBuyers : LF_Test
{
	static const ResourceName WHOLESALE = "{7C1E5A2D9B3F4608}Prefabs/Trader/LF_WholesaleBuyer_Everon1989.et";
	//! The wholesale pays this share of the counter rate, rounded half up; the cove (Fisherman's Bay, $16.7 a fish at the counter) then pays about $430 an hour at a spot's 40 fish an hour, inside the duty band.
	static const int WHOLESALE_PCT = 65;
	static const ResourceName CORE_TRADER = "{5DDBFA4225659FFE}Prefabs/Trader/UniversalTrader.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/fish-scrap-buyers";
	}

	//------------------------------------------------------------------------------------------------
	//! One fish prefab against the wholesale trader: in its storage filter and its price list at the catalogue rate.
	protected void CheckFish(LF_TestContext ctx, ResourceName trader, BaseContainerList rows, LF_ItemCatalogueConfig catalogue, LF_MarketConfig market, string label, ResourceName prefab)
	{
		LF_CatalogueItem item = LF_ItemCatalogue.ItemByPrefab(catalogue, prefab);
		ctx.NotNull(item, label + " is a catalogue item");
		if (!item)
			return;

		ctx.True(item.m_iSellPrice > 0, label + " has a rate to pay");
		ctx.True(LF_Test_EveronMarket.TraderBuys(trader, prefab), label + " is in the wholesale storage filter");
		ctx.Equal((item.m_iSellPrice * WHOLESALE_PCT + 50) / 100, LF_Test_EveronMarket.TraderPrice(rows, prefab), label + " wholesale lists 65 percent of the counter rate");
		ctx.True(market.FindGood(prefab) == null, label + " has no curve (the spot stock limits the supply)");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckFishBuyer(LF_TestContext ctx)
	{
		ResourceName trader = WHOLESALE;
		if (RedProof())
			trader = CORE_TRADER;

		LF_ItemCatalogueConfig.Reload();
		LF_FishingConfig.Reload();
		LF_MarketConfig.Reload();
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		LF_FishingConfig fishing = LF_FishingConfig.Get();
		LF_MarketConfig market = LF_MarketConfig.Get();
		ctx.NotNull(catalogue, "the item catalogue loads under the pack header");
		ctx.NotNull(fishing, "the fishing config loads under the pack header");
		ctx.NotNull(market, "the market config loads under the pack header");
		BaseContainerList rows = LF_Test_EveronMarket.TraderRows(trader);
		ctx.True(rows != null, "the wholesale buyer lists its prices");
		if (!catalogue || !fishing || !market || !rows)
			return;

		// Every fish the rod can land: each size prefab of each species.
		int raw;
		foreach (LF_FishSpeciesDef species : fishing.m_aSpecies)
		{
			ctx.True(species.m_aSizePrefabs.Count() == 3, species.m_sId + " has small, medium and large");
			foreach (ResourceName size : species.m_aSizePrefabs)
			{
				CheckFish(ctx, trader, rows, catalogue, market, "wholesale lists " + species.m_sId + " " + size, size);
				raw++;
			}
		}

		ctx.True(raw >= 18, "the pack's fishing config yields at least six species of three sizes");

		// Every smoked fish: whatever the fish counter outlet offers (fresh and smoked) is a fish the wholesale buys.
		int offers;
		foreach (LF_OutletOffer offer : catalogue.m_aOffers)
		{
			if (offer.m_sOutlet != "fish_counter")
				continue;

			LF_CatalogueItem item = LF_ItemCatalogue.Item(catalogue, offer.m_sItem);
			if (!item || item.m_sId.IndexOf("fish_") != 0)
				continue;

			offers++;
			CheckFish(ctx, trader, rows, catalogue, market, "wholesale lists " + item.m_sId, item.m_Prefab);
		}

		ctx.True(offers >= 24, "the fish counter outlet offers the 18 fresh and 6 smoked fish");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckScrapBuyer(LF_TestContext ctx)
	{
		if (RedProof())
		{
			// No pack layer: an empty header as the pack source.
			LF_MissionConfigPackSource none = new LF_MissionConfigPackSource();
			none.SetHeader(new LF_MissionHeader());
			LF_ConfigService.SetPackSource(none);
		}

		LF_ShopStockConfig.Set(null);
		LF_ItemCatalogueConfig.Reload();
		LF_ShopStockConfig stocks = LF_ShopStockConfig.Get();
		LF_ItemCatalogueConfig catalogue = LF_ItemCatalogueConfig.Get();
		if (RedProof())
			LF_ConfigService.Reset();

		ctx.NotNull(stocks, "the shop stock config loads under the pack header");
		ctx.NotNull(catalogue, "the item catalogue loads under the pack header");
		if (!stocks || !catalogue)
			return;

		LF_ShopStock mechanic = stocks.Find("mechanic");
		ctx.NotNull(mechanic, "the garage has a mechanic stock");
		if (!mechanic)
			return;

		ctx.True(mechanic.m_Buyback != null, "the mechanic stock has a buyback");
		if (!mechanic.m_Buyback)
			return;

		ctx.True(!mechanic.m_Buyback.m_bStolen, "the garage buys no stolen goods");
		ctx.True(mechanic.m_Buyback.m_fFraction > 0 && mechanic.m_Buyback.m_fFraction <= 0.5, "the garage pays at most half the catalogue price");
		ctx.True(mechanic.FindItem(LF_Test_EveronScrapItems.SCRAP_METAL) == null, "the garage does not sell scrap back (no loop)");

		array<ResourceName> goods = {LF_Test_EveronScrapItems.SCRAP_METAL, LF_Test_EveronScrapItems.COPPER_WIRE, LF_Test_EveronScrapItems.RUBBER_SCRAP};
		array<int> expected = {4, 12, 5};
		foreach (int i, ResourceName good : goods)
		{
			ctx.True(mechanic.ListsBuyback(good), "the garage buys " + good);
			LF_CatalogueItem item = LF_ItemCatalogue.ItemByPrefab(catalogue, good);
			ctx.NotNull(item, good + " is a catalogue item");
			if (!item)
				continue;

			int paid = LF_ShopStockRules.BuybackPrice(item.m_iPrice, mechanic.m_Buyback.m_fFraction);
			ctx.Equal(expected[i], paid, good + " garage price");
			ctx.True(paid < item.m_iPrice, good + " pays less than it costs new");
		}
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckFishBuyer(ctx);
		CheckScrapBuyer(ctx);
	}
}
