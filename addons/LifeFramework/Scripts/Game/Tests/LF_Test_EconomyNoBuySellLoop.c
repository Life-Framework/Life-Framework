// red-proof: 2026-11-18, on main the core trader paid 15 for a CementBag the shop sold for 12; the run failed with
// `economy/no-buy-sell-loop` naming CementBag, then passed once the trader price was 10. A new row in
// UniversalTrader.et priced above its ShopPrices.conf buy price fails it the same way.

//! No good can be bought at a shop and sold to the trader for more (#3121, #686). For every prefab
//! the core ShopPrices.conf sells and the core UniversalTrader prefab buys, the shop's buy price is
//! at least the trader's price. The test holds the rule, not the values.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_EconomyNoBuySellLoop : LF_Test
{
	protected static const ResourceName SHOP_PRICES = "{A547CE324ADDD178}Configs/Shop/ShopPrices.conf";
	protected static const ResourceName TRADER = "{5DDBFA4225659FFE}Prefabs/Trader/UniversalTrader.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "economy/no-buy-sell-loop";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		Resource container = BaseContainerTools.LoadContainer(SHOP_PRICES);
		ctx.True(container != null && container.GetResource() != null, "the core shop price config loads");
		if (!container || !container.GetResource())
			return;

		LF_ShopPricesConfig prices = LF_ShopPricesConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		ctx.NotNull(prices, "the shop price config has the expected root class");
		if (!prices)
			return;

		float groundY = GetGame().GetWorld().GetSurfaceY(340, 100);
		IEntity traderEntity = LF_Utils.SpawnEntityPrefab(TRADER, Vector(340, groundY, 100));
		ctx.NotNull(traderEntity, "the universal trader spawned");
		if (!traderEntity)
			return;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(traderEntity);
		ctx.NotNull(trader, "the universal trader carries the trader manager");
		if (trader && trader.m_aTradableItems)
			CheckRows(ctx, prices, trader.m_aTradableItems);

		SCR_EntityHelper.DeleteEntityAndChildren(traderEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! True when the shop catalogue lists the prefab (the extra-items list names every trader good).
	protected static bool IsListed(notnull LF_ShopPricesConfig prices, ResourceName prefab)
	{
		if (!prices.m_aExtraItemPrefabs)
			return false;

		foreach (ResourceName listed : prices.m_aExtraItemPrefabs)
		{
			if (LF_Utils.SamePrefab(listed, prefab))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRows(LF_TestContext ctx, notnull LF_ShopPricesConfig prices, notnull array<ref LF_TraderItem> rows)
	{
		int compared;
		foreach (LF_TraderItem row : rows)
		{
			if (!row)
				continue;

			if (!IsListed(prices, row.m_ItemPrefab))
				continue;

			bool hidden;
			int buyPrice = prices.GetBuyPrice(row.m_ItemPrefab, SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.DEFAULT, hidden);
			if (hidden)
				continue;

			compared++;
			ctx.True(buyPrice >= row.m_ValuePerItem, string.Format("buy-low sell-high loop: %1 buys at %2 and the trader pays %3", row.m_ItemPrefab, buyPrice, row.m_ValuePerItem));
		}

		ctx.True(compared > 0, "at least one good is both sold by the shop and bought by the trader");
	}
};
