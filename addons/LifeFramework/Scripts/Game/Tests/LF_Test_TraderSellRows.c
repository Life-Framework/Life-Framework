// red-proof: new rules with no failing state before them (the sell rows did not exist). Batched hook
// run: `tools\cli test --no-build --red LF_Test_TraderSellRows,LF_Test_TraderSellAll` skips the
// grouping in RedProof() and the assertions fail. Observed 2026-11-22:
// `RED-OK trader/sell-rows (fails with the hook on, 2 assertions)`. Green in the same session.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TraderSellRows : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const ResourceName JUICE = "{1DBD99E6347DE1AD}Prefabs/Items/Drinks/AppleJuice.et";
	protected static const ResourceName PLUM = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
	protected static const ResourceName NUGGET = "{6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "trader/sell-rows";
	}

	//------------------------------------------------------------------------------------------------
	protected LF_TraderItem Row(ResourceName prefab, int value)
	{
		LF_TraderItem row = new LF_TraderItem();
		row.m_ItemPrefab = prefab;
		row.m_ValuePerItem = value;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// The buyer's list: juice first, then apples; plums listed at 0 (not bought); a null row.
		array<ref LF_TraderItem> list = {};
		list.Insert(Row(JUICE, 18));
		list.Insert(Row(APPLE, 5));
		list.Insert(Row(PLUM, 0));
		list.Insert(null);

		// Carried: an apple stack of 4, a nugget the buyer does not list, two single juices, plums, an apple of 1.
		array<ResourceName> prefabs = {APPLE, NUGGET, JUICE, JUICE, PLUM, APPLE};
		array<int> units = {4, 1, 1, 1, 3, 1};

		array<ref LF_TraderSellGood> goods = {};
		if (!RedProof())
			goods = LF_TraderSellRows.Group(list, prefabs, units);

		ctx.Equal(2, goods.Count(), "two goods the buyer takes: juice and apples (not the nugget, not plums at 0)");
		if (goods.Count() != 2)
			return;

		ctx.True(LF_Utils.SamePrefab(JUICE, goods[0].m_sPrefab), "rows follow the buyer's list order: juice first");
		ctx.Equal(2, goods[0].m_iItems, "two juice bottles");
		ctx.Equal(2, goods[0].m_iUnits, "two juice units");
		ctx.Equal(1, goods[0].m_iFirstUnits, "a juice row sells one bottle");

		ctx.True(LF_Utils.SamePrefab(APPLE, goods[1].m_sPrefab), "apples second");
		ctx.Equal(2, goods[1].m_iItems, "two apple entities");
		ctx.Equal(5, goods[1].m_iUnits, "five apples over both");
		ctx.Equal(4, goods[1].m_iFirstUnits, "the apple row sells the first stack, 4");

		ctx.Equal(7, LF_TraderSellRows.TotalUnits(goods), "sell all counts 7 units");

		// A good listed twice keeps one row.
		list.Insert(Row(APPLE, 9));
		ctx.Equal(2, LF_TraderSellRows.Group(list, prefabs, units).Count(), "a good listed twice is still one row");

		// Nothing carried: no rows, and sell-all reads "Sell goods".
		array<ResourceName> noPrefabs = {};
		array<int> noUnits = {};
		ctx.Equal(0, LF_TraderSellRows.Group(list, noPrefabs, noUnits).Count(), "empty pockets give no rows");
		ctx.Equal(0, LF_TraderSellRows.Group(null, prefabs, units).Count(), "no list gives no rows");

		// Labels.
		ctx.EqualStr("#LF-Trader_SellOne", LF_TraderSellRows.GoodKey(18), "a priced row: Sell 1 X: $18");
		ctx.EqualStr("#LF-Trader_SellOneAsk", LF_TraderSellRows.GoodKey(LF_TraderSellRows.UNKNOWN), "price not here yet: no $ part");
		ctx.EqualStr("#LF-Trader_SellOneAsk", LF_TraderSellRows.GoodKey(0), "a good the buyer pays 0 for has no $ part");
		ctx.EqualStr("#LF-Trader_SellAll", LF_TraderSellRows.AllKey(7, 61), "sell all with a total");
		ctx.EqualStr("#LF-Trader_SellAllAsk", LF_TraderSellRows.AllKey(7, LF_TraderSellRows.UNKNOWN), "sell all before the total");
		ctx.EqualStr("#LF-Trader_SellNothing", LF_TraderSellRows.AllKey(0, 0), "sell all with nothing to sell");

		// The signature moves when the goods do, so the client asks for new prices after a sale.
		string before = LF_TraderSellRows.Signature(goods);
		array<ResourceName> leftPrefabs = {APPLE, JUICE};
		array<int> leftUnits = {4, 1};
		array<ref LF_TraderSellGood> after = LF_TraderSellRows.Group(list, leftPrefabs, leftUnits);
		ctx.True(before != LF_TraderSellRows.Signature(after), "selling a juice changes the signature");
		ctx.EqualStr(before, LF_TraderSellRows.Signature(LF_TraderSellRows.Group(list, prefabs, units)), "same goods, same signature");
	}
}
