// red-proof: 2026-11-18 on the unpriced overlay the test FAILED (the four items were not in the shop catalogue); `tools\cli test --no-build --red LF_Test_EveronFurnitureStock`: RED PROOF OK (RedProof() skips the catalogue and furniture stock lookups). Green in the pack leg.
//! shop/furniture-stock (#2916): the 1989 general store sells the four house furniture items
//! (chair, table, shelf, lamp), so a player can furnish a house. Each item resolves a visible buy
//! price from the pack's general store overlay (`LF_ShopPrices_GeneralStore.conf`), and each buy
//! price beats the sell-back value, so buying and selling back is never a profit loop.
//! Lives in the pack because it needs the pack's price overlay (pack leg).
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronFurnitureStock : LF_Test
{
	static const ResourceName CHAIR_ITEM = "{D77E0D49C10C35BB}Prefabs/Placeables/Furniture/Furniture_Chair_Item.et";
	static const ResourceName TABLE_ITEM = "{FEF81109318DEF2C}Prefabs/Placeables/Furniture/Furniture_Table_Item.et";
	static const ResourceName SHELF_ITEM = "{6FAE5088FA3B3519}Prefabs/Placeables/Furniture/Furniture_Shelf_Item.et";
	static const ResourceName LAMP_ITEM = "{03AB5928B9B6C230}Prefabs/Placeables/Furniture/Furniture_Lamp_Item.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/furniture-stock";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		CheckItem(ctx, manager, "chair", CHAIR_ITEM);
		CheckItem(ctx, manager, "table", TABLE_ITEM);
		CheckItem(ctx, manager, "shelf", SHELF_ITEM);
		CheckItem(ctx, manager, "lamp", LAMP_ITEM);

		// The four stay on the general store until the furniture store's counter lands, and are on the furniture stock too.
		CheckStocks(ctx, CHAIR_ITEM);
		CheckStocks(ctx, TABLE_ITEM);
		CheckStocks(ctx, SHELF_ITEM);
		CheckStocks(ctx, LAMP_ITEM);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStocks(LF_TestContext ctx, ResourceName prefab)
	{
		int onFurniture = 0;
		if (!RedProof())
			onFurniture = StockPrice("furniture", prefab);

		ctx.True(onFurniture > 0, "the furniture stock lists " + prefab);
		ctx.Equal(onFurniture, StockPrice("general_store", prefab), "the general store still sells it at the same price: " + prefab);
	}

	//------------------------------------------------------------------------------------------------
	//! The price of a prefab on a stock, 0 when the stock or the row is missing.
	protected static int StockPrice(string stockId, ResourceName prefab)
	{
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(stockId);
		if (!stock)
			return 0;

		LF_ShopStockItem row = stock.FindItem(prefab);
		if (!row)
			return 0;

		return row.m_iPrice;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckItem(LF_TestContext ctx, LF_ShopManagerComponent manager, string label, ResourceName prefab)
	{
		int buyPrice = 0;
		int sellPrice = 0;
		bool hidden = true;
		LF_ShopCatalogItem entry = null;
		if (!RedProof())
			entry = manager.GetItem(prefab);

		// An unlisted prefab still answers GetPrices with the config's flat default, so the proof is
		// the catalogue entry (the overlay's extra prefab) priced by the overlay's own rule.
		ctx.NotNull(entry, string.Format("the %1 is in the shop catalogue (the overlay lists it)", label));
		if (!entry)
			return;

		buyPrice = entry.m_iBuyPrice;
		sellPrice = entry.m_iSellPrice;
		hidden = entry.m_bHidden;

		LF_ShopPricesConfig prices = manager.GetConfig();
		bool hasRule = false;
		if (prices && prices.m_aPriceRules)
		{
			foreach (LF_ShopPriceRule rule : prices.m_aPriceRules)
			{
				if (rule && rule.m_Prefab == prefab)
					hasRule = true;
			}
		}

		ctx.True(hasRule, string.Format("the %1 has its own general store price rule, not the flat default", label));
		ctx.True(buyPrice > 0, string.Format("the %1 resolves a general store price (got %2)", label, buyPrice));
		ctx.False(hidden, string.Format("the %1 is not hidden from the shop", label));
		ctx.True(sellPrice > 0, string.Format("the %1 has a sell-back value (got %2)", label, sellPrice));
		ctx.True(buyPrice > sellPrice, string.Format("the %1 costs more (%2) than it sells back for (%3): no profit loop", label, buyPrice, sellPrice));
	}
}
