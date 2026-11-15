// red-proof: Observed 2026-11-15 with `tools/cli test --no-build --red LF_Test_ShopStockRules,LF_Test_FenceStockTrade`: RED-OK shop/fence-stock-trade, leaving the M9 unflagged failed `a flagged stolen M9 is bought` and `the flagged M9 is taken`. Hook off: green.
// tier: WORLD
//! The DebugWorld fence trades from config: buying a lockpick charges the stock price the server
//! reads (a changed config price changes the charge; the caller never names a price), listed
//! stolen goods sell at the stock's share of the street value, an unlisted item sells only once it
//! is flagged stolen, and the fence never buys back his own tools.
class LF_Test_FenceStockTrade : LF_Test
{
	protected static const ResourceName LOCKPICK = "{6B1D3F5A92C47E08}Prefabs/Items/Fence/LF_Lockpick.et";
	protected static const ResourceName PARTS = "{7A94C1E05D3B2F68}Prefabs/Items/Resources/LF_CarParts.et";
	protected static const ResourceName M9 = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";

	override string GetName()
	{
		return "shop/fence-stock-trade";
	}

	protected LF_ShopStockConfig BuildConfig(int lockpickPrice)
	{
		LF_ShopStockItem pick = new LF_ShopStockItem();
		pick.m_Prefab = LOCKPICK;
		pick.m_iPrice = lockpickPrice;

		LF_ShopStock stock = new LF_ShopStock();
		stock.m_sId = "fence";
		stock.m_aItems = {pick};
		stock.m_Buyback = new LF_ShopBuyback();
		stock.m_Buyback.m_aPrefabs = {PARTS};
		stock.m_Buyback.m_fFraction = 0.5;
		stock.m_Buyback.m_bStolen = true;

		LF_ShopStockConfig config = new LF_ShopStockConfig();
		config.m_aStocks = {stock};
		return config;
	}

	override void Run(LF_TestContext ctx)
	{
		IEntity fence = GetGame().GetWorld().FindEntityByName("FenceNpc");
		ctx.NotNull(fence, "FenceNpc resolves in the DebugWorld");
		LF_ShopComponent shop = LF_Component<LF_ShopComponent>.Find(fence);
		ctx.NotNull(shop, "the fence keeps a shop");
		if (!shop)
			return;

		ctx.True(shop.GetStockId() == "fence", "the fence names the fence stock");
		LF_ShopStockConfig.Set(BuildConfig(250));
		IEntity trader = LF_TestCharacterFixture.Spawn(ctx);
		if (!trader)
		{
			LF_ShopStockConfig.Set(null);
			return;
		}

		// Buy: the stock price, read on the server.
		ctx.Equal(250, shop.GetBuyPrice(LOCKPICK), "the lockpick is priced from the stock");
		LF_MoneyUtils.AddCash(trader, 300);
		ctx.True(shop.BuyItem(LOCKPICK, 1, trader), "the fence sells a lockpick");
		ctx.Equal(50, LF_MoneyUtils.GetCash(trader), "the buyer paid the stock price");
		ctx.True(LF_TestCharacterFixture.HasItem(trader, LOCKPICK), "the lockpick is delivered");

		LF_ShopStockConfig.Set(BuildConfig(40));
		ctx.True(shop.BuyItem(LOCKPICK, 1, trader), "a re-priced lockpick sells");
		ctx.Equal(10, LF_MoneyUtils.GetCash(trader), "the charge follows the config, not the caller");

		ctx.False(shop.SellItem(LOCKPICK, 1, trader), "the fence does not buy back his own tools");

		// Sell listed goods: half the street value.
		int partsPay = LF_ShopStockRules.BuybackPrice(LF_ShopComponent.StreetValue(PARTS), 0.5);
		ctx.True(partsPay > 0, "car parts have a buyback price");
		ctx.Equal(partsPay, shop.GetSellPrice(PARTS), "the shop quotes the same buyback price");
		LF_InventoryUtils.AddAmount(trader, PARTS, 1);
		int before = LF_MoneyUtils.GetCash(trader);
		ctx.True(shop.SellItem(PARTS, 1, trader), "the fence buys listed stolen goods");
		ctx.Equal(before + partsPay, LF_MoneyUtils.GetCash(trader), "the seller got the buyback price");

		// The stolen check: an unlisted item sells only flagged.
		LF_InventoryUtils.AddAmount(trader, M9, 1);
		ctx.False(shop.SellItem(M9, 1, trader), "an unflagged M9 is refused");
		// A handgun lands in the weapon slot, which FindItems does not walk: look there too.
		InventoryStorageManagerComponent storage = LF_InventoryUtils.GetResponsibleStorageManager(trader);
		array<IEntity> guns = LF_InventoryUtils.FindItemsByPrefab(storage, M9);
		foreach (IEntity held : LF_InventoryUtils.CollectCarried(trader))
		{
			if (held && guns.Find(held) < 0 && LF_Utils.SamePrefab(LF_Utils.GetPrefabName(held), M9))
				guns.Insert(held);
		}
		ctx.Equal(1, guns.Count(), "the M9 is still carried");
		if (!guns.IsEmpty() && !RedProof())
			LF_StolenGoods.Mark(guns[0]);

		before = LF_MoneyUtils.GetCash(trader);
		ctx.True(shop.SellItem(M9, 1, trader), "a flagged stolen M9 is bought");
		int gunPay = LF_ShopStockRules.BuybackPrice(LF_ShopComponent.StreetValue(M9), 0.5);
		ctx.Equal(before + gunPay, LF_MoneyUtils.GetCash(trader), "the seller got half the M9's street value");
		ctx.False(LF_TestCharacterFixture.HasItem(trader, M9), "the flagged M9 is taken");

		LF_TestCharacterFixture.Delete(trader);
		LF_ShopStockConfig.Set(null);
	}
}
