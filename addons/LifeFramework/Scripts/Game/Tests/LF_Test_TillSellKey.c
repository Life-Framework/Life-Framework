// red-proof: 2026-11-22 `node tools/cli.mjs test --no-build --red LF_Test_OreStackCount,LF_Test_TillSellKey,LF_Test_ProcessRunClock`: RED PROOF OK (all 3 fail with the hook on). RedProof() gives the buy-only line a sell price.
//! shop/till-sell-key (first-2h review B5): a till shows its Sell key only when the counter pays for
//! at least one of its lines; a buy-only till (the miner's supply) shows Pay alone.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TillSellKey : LF_Test
{
	override string GetName()
	{
		return "shop/till-sell-key";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.False(LF_ShopPriceList.BuysBack(null), "no lines: no Sell key");

		LF_ShopCatalogItem pickaxe = new LF_ShopCatalogItem();
		pickaxe.m_iBuyPrice = 150;
		pickaxe.m_iSellPrice = 0;
		array<ref LF_ShopCatalogItem> buyOnly = {};
		buyOnly.Insert(pickaxe);
		if (RedProof())
			pickaxe.m_iSellPrice = 40;

		ctx.False(LF_ShopPriceList.BuysBack(buyOnly), "a buy-only till has no Sell key");

		LF_ShopCatalogItem scrap = new LF_ShopCatalogItem();
		scrap.m_iBuyPrice = 0;
		scrap.m_iSellPrice = 12;
		array<ref LF_ShopCatalogItem> buyback = {};
		buyback.Insert(pickaxe);
		buyback.Insert(scrap);
		ctx.True(LF_ShopPriceList.BuysBack(buyback), "a till that pays for a line keeps its Sell key");
	}
}
