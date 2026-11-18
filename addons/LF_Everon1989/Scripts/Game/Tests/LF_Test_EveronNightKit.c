// red-proof: 2026-11-18 `tools\cli test --no-build --red LF_Test_EveronNightKit` (RedProof() looks up a prefab the general store does not stock): RED-OK, `the flashlight is in the general store stock` failed. Green with the stock row: `--only LF_Test_EveronNightKit` passed 10 assertions.
//! #3153 in game: under the Everon 1989 header (the pack leg of `tools\cli test`), the 1989 general store
//! sells the vanilla Soviet flashlight (`Flashlight_Soviet_01`), so a player can buy a light for the night.
//! The till's buy list (the pack stock overlay, `LF_ShopStock_Everon1989.conf`, store id `general_store`)
//! carries the flashlight with a price; the merged price config prices it (buy above sell-back, so no profit
//! loop) and lists it as an extra item prefab; the prefab resource loads.
[LF_TestInfo(LF_TestTier.WORLD, pack: true)]
class LF_Test_EveronNightKit : LF_Test
{
	static const ResourceName FLASHLIGHT = "{575EA58E67448C2A}Prefabs/Items/Equipment/Flashlights/Flashlight_Soviet_01/Flashlight_Soviet_01.et";
	static const ResourceName NOT_STOCKED = "{70BC751317551D9B}Prefabs/Items/Equipment/Canteens/Canteen_Soviet_01.et";
	protected static const string STOCK = "general_store";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "everon/night-kit";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		CheckPrefabLoads(ctx);
		CheckStock(ctx);
		CheckPrices(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrefabLoads(LF_TestContext ctx)
	{
		Resource resource = Resource.Load(FLASHLIGHT);
		ctx.True(resource && resource.IsValid(), "the vanilla Flashlight_Soviet_01 prefab loads");
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckStock(LF_TestContext ctx)
	{
		LF_ShopStock stock = LF_ShopStockConfig.Get().Find(STOCK);
		ctx.NotNull(stock, "the general store has a stock under the pack header");
		if (!stock)
			return;

		ResourceName probe = FLASHLIGHT;
		if (RedProof())
			probe = NOT_STOCKED;

		LF_ShopStockItem row = stock.FindItem(probe);
		ctx.NotNull(row, "the flashlight is in the general store stock");
		if (row)
			ctx.True(row.m_iPrice > 0, string.Format("the flashlight has a stock price (got %1)", row.m_iPrice));
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckPrices(LF_TestContext ctx)
	{
		LF_ShopManagerComponent manager = LF_ShopManagerComponent.GetInstance();
		ctx.NotNull(manager, "the shop manager exists");
		if (!manager)
			return;

		LF_ShopPricesConfig prices = manager.GetConfig();
		ctx.NotNull(prices, "the merged shop price config loads");
		if (!prices)
			return;

		bool listed = false;
		if (prices.m_aExtraItemPrefabs)
			listed = prices.m_aExtraItemPrefabs.Contains(FLASHLIGHT);
		ctx.True(listed, "the flashlight is in the price config's extra item prefabs");

		int buyPrice = 0;
		int sellPrice = 0;
		bool hidden = true;
		manager.GetPrices(FLASHLIGHT, buyPrice, sellPrice, hidden);
		ctx.True(buyPrice > 0, string.Format("the flashlight has a buy price (got %1)", buyPrice));
		ctx.False(hidden, "the flashlight is not hidden from the shop");
		ctx.True(buyPrice > sellPrice, string.Format("the flashlight costs more (%1) than it sells back for (%2): no profit loop", buyPrice, sellPrice));
	}
}
