// red-proof: Observed 2026-11-15 with `tools/cli test --no-build --red LF_Test_ShopStockRules,LF_Test_FenceStockTrade`: RED-OK shop/stock-rules, skipping the BuybackPrice call failed `200 at 0.4 pays 80, not 79 (expected 80, got 0)`. Hook off: green.
// tier: LOGIC
//! A stocked shop's pure decisions: the buyback pays a share of the street value, never more than
//! the goods cost (fraction held to 1); it buys listed goods flagged or not, and anything else only
//! when flagged and the stock takes stolen goods; a pack stock replaces the core stock of its id.
class LF_Test_ShopStockRules : LF_Test
{
	protected static const ResourceName PARTS = "{7A94C1E05D3B2F68}Prefabs/Items/Resources/LF_CarParts.et";
	protected static const ResourceName M9 = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	override string GetName()
	{
		return "shop/stock-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		int paid = 0;
		if (!RedProof())
			paid = LF_ShopStockRules.BuybackPrice(200, 0.4);
		ctx.Equal(80, paid, "200 at 0.4 pays 80, not 79");
		ctx.Equal(500, LF_ShopStockRules.BuybackPrice(500, 3.0), "a fraction above 1 pays the street value, no more");
		ctx.Equal(0, LF_ShopStockRules.BuybackPrice(500, 0), "a zero fraction pays nothing");
		ctx.Equal(0, LF_ShopStockRules.BuybackPrice(0, 0.5), "an unpriced item pays nothing");

		LF_ShopStock stock = new LF_ShopStock();
		stock.m_sId = "fence";
		stock.m_Buyback = new LF_ShopBuyback();
		stock.m_Buyback.m_aPrefabs = {PARTS};
		stock.m_Buyback.m_fFraction = 0.5;
		stock.m_Buyback.m_bStolen = true;

		ctx.True(LF_ShopStockRules.Buys(stock, PARTS, false), "listed goods are bought unflagged");
		ctx.False(LF_ShopStockRules.Buys(stock, M9, false), "an unlisted, unflagged item is refused");
		ctx.True(LF_ShopStockRules.Buys(stock, M9, true), "a flagged stolen item is bought");
		stock.m_Buyback.m_bStolen = false;
		ctx.False(LF_ShopStockRules.Buys(stock, M9, true), "a stock that takes no stolen goods refuses the flagged item");

		LF_ShopStockConfig core = new LF_ShopStockConfig();
		core.m_aStocks = {};
		LF_ShopStock empty = new LF_ShopStock();
		empty.m_sId = "fence";
		core.m_aStocks.Insert(empty);

		LF_ShopStockConfig pack = new LF_ShopStockConfig();
		pack.m_aStocks = {stock};
		ctx.Equal(1, core.Merge(pack), "the pack sets one stock");
		ctx.Equal(1, core.m_aStocks.Count(), "the same id replaces, it does not add");
		ctx.True(core.Find("fence") == stock, "the pack's fence stock is the one read");
		ctx.True(core.Find("grocer") == null, "an unknown stock id is none");
	}
}
