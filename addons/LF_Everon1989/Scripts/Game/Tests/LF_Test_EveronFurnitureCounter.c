// red-proof: 2026-11-19 `tools\cli test --no-build --red LF_Test_EveronFurnitureCounter,LF_Test_EveronFurnitureStock`: RED PROOF OK (both fail with the hook on); RedProof() skips the purchase, so the counter sells nothing and the cash and delivery checks fail.
//! everon/furniture-counter (#3256): the Saint-Philippe furniture store's counter sells the pack's
//! `furniture` stock. The `LF_FurnitureCounter` prefab names the stock, every piece on it has a price
//! from that stock, and a buyer pays the stock price and gets the piece. Pack leg: needs the pack's
//! stock overlay.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFurnitureCounter : LF_Test
{
	protected static const ResourceName COUNTER = "{B0C1CC8AA15B21D2}Prefabs/Props/Commercial/LF_FurnitureCounter.et";
	protected static const ResourceName LAMP = "{03AB5928B9B6C230}Prefabs/Placeables/Furniture/Furniture_Lamp_Item.et";
	protected static const ResourceName ARMCHAIR = "{D4C9091800ACED97}Prefabs/Placeables/Furniture/Furniture_Armchair_Item.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/furniture-counter";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find("furniture");
		ctx.NotNull(stock, "the pack overlay carries the furniture stock");
		if (!stock)
			return;

		ctx.True(stock.m_aItems.Count() >= 18, "the furniture store sells at least the eighteen plain pieces");
		foreach (LF_ShopStockItem item : stock.m_aItems)
		{
			ctx.True(item && item.m_iPrice > 0, "every furniture piece has a price");
		}

		IEntity counter = LF_Utils.SpawnEntityPrefab(COUNTER, Vector(0, 500, 0));
		ctx.NotNull(counter, "the furniture counter prefab spawns");
		if (!counter)
			return;

		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(counter);
		ctx.NotNull(shop, "the counter carries a shop");
		if (shop)
			CheckSale(ctx, shop);

		SCR_EntityHelper.DeleteEntityAndChildren(counter);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckSale(LF_TestContext ctx, LF_ShopComponent shop)
	{
		ctx.True(shop.GetStockId() == "furniture", "the counter names the furniture stock");
		ctx.True(shop.GetBuyPrice(ARMCHAIR) > 0, "an armchair has a price at the counter");
		ctx.True(shop.GetBuyPrice(LAMP) > 0, "a lamp has a price at the counter");

		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		if (!buyer)
			return;

		int price = shop.GetBuyPrice(LAMP);
		LF_MoneyUtils.AddCash(buyer, price + 5);
		bool sold = false;
		if (!RedProof())
			sold = shop.BuyItem(LAMP, 1, buyer);

		ctx.True(sold, "the counter sells a lamp");
		ctx.Equal(5, LF_MoneyUtils.GetCash(buyer), "the buyer paid the counter's price");
		ctx.True(LF_TestCharacterFixture.HasItem(buyer, LAMP), "the lamp is delivered");
		LF_TestCharacterFixture.Delete(buyer);
	}
}
