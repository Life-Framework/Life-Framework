// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronCafeTill`: RedProof() skips the purchase, so the till sells nothing and the cash/delivery checks fail.
//! everon/cafe-till (#3063): the Saint-Philippe pub's till sells the pack's `cafe` stock. The
//! `LF_CafeTill` prefab names the stock, every item is priced from it (not the core table), and a
//! buyer pays the stock price and gets the goods. Pack leg: needs the pack's stock overlay.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronCafeTill : LF_Test
{
	protected static const ResourceName TILL = "{EEFD19AAC8216449}Prefabs/Props/Commercial/LF_CafeTill.et";
	protected static const ResourceName WATER = "{A231A5F8D479B5DC}Prefabs/Items/Drinks/WaterBottle.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const ResourceName BEER = "{4CBB1F9A04D15772}Prefabs/Items/Drinks/BeerBottle.et";
	protected static const ResourceName CRACKERS = "{8F2C06D1F757C40F}Prefabs/Items/Food/LF_Crackers.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/cafe-till";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find("cafe");
		ctx.NotNull(stock, "the pack overlay carries the cafe stock");
		if (!stock)
			return;

		ctx.True(stock.m_aItems.Count() >= 4, "the cafe sells at least four things");
		foreach (LF_ShopStockItem item : stock.m_aItems)
		{
			ctx.True(item && item.m_iPrice > 0, "every cafe item has a price");
		}

		IEntity till = LF_Utils.SpawnEntityPrefab(TILL, Vector(0, 500, 0));
		ctx.NotNull(till, "the cafe till prefab spawns");
		if (!till)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(till);
		ctx.NotNull(shop, "the till carries a shop");
		if (shop)
			CheckSale(ctx, shop);

		SCR_EntityHelper.DeleteEntityAndChildren(till);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSale(LF_TestContext ctx, LF_ShopComponent shop)
	{
		ctx.True(shop.GetStockId() == "cafe", "the till names the cafe stock");
		ctx.True(shop.GetBuyPrice(WATER) > 0, "water has a price at the bar");
		ctx.True(shop.GetBuyPrice(JUICE) > 0, "apple juice has a price at the bar");
		ctx.True(shop.GetBuyPrice(BEER) > 0, "beer has a price at the bar");
		ctx.True(shop.GetBuyPrice(CRACKERS) > 0, "crackers have a price at the bar");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		int price = shop.GetBuyPrice(WATER);
		LF_MoneyUtils.AddCash(buyer, price + 5);
		bool sold = false;
		if (!RedProof())
			sold = shop.BuyItem(WATER, 1, buyer);

		ctx.True(sold, "the bar sells a bottle of water");
		ctx.Equal(5, LF_MoneyUtils.GetCash(buyer), "the buyer paid the bar's price");
		ctx.True(LF_TestCharacterFixture.HasItem(buyer, WATER), "the water is delivered");
		LF_TestCharacterFixture.Delete(buyer);
	}
}
