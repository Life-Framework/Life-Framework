// red-proof: new behaviour with no failing state before it (the board never read a trader). Batched hook
// run: `tools\cli test --no-build --red LF_Test_PriceBoardTrader` skips the trader answer in RedProof()
// and the assertions fail. Observed 2026-11-22: `RED-OK market/price-board-trader (fails with the hook on, 9 assertions)`.
//! The stall's chalk board (first-two-hours review A20): a trader that posts rows gets a board of those
//! goods at its own one-unit prices, the same price its "Sell 1" row shows; a trader that posts nothing
//! answers null, so the board falls back to the market's goods.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_PriceBoardTrader : LF_Test
{
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "market/price-board-trader";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		// Pure: only posted rows with a value count, in list order.
		array<ref LF_TraderItem> list = {};
		LF_TraderItem posted = new LF_TraderItem();
		posted.m_ItemPrefab = APPLE;
		posted.m_ValuePerItem = 5;
		posted.m_bPosted = true;
		LF_TraderItem quiet = new LF_TraderItem();
		quiet.m_ItemPrefab = "{C3E35D690317B5BE}Prefabs/Items/Food/Plum.et";
		quiet.m_ValuePerItem = 4;
		LF_TraderItem free = new LF_TraderItem();
		free.m_ItemPrefab = "{9FEBBF75B1D525EE}Prefabs/Items/Food/Cabbage.et";
		free.m_bPosted = true;
		list.Insert(quiet);
		list.Insert(null);
		list.Insert(posted);
		list.Insert(free);
		array<ResourceName> goods = LF_PriceBoard.PostedGoods(list);
		ctx.Equal(1, goods.Count(), "one posted good with a value: the apple");
		ctx.Equal(0, LF_PriceBoard.PostedGoods(null).Count(), "no list posts nothing");

		IEntity owner = GetGame().GetWorld().FindEntityByName(LF_TestAnnex.APPLE_TRADE_CONTAINER);
		ctx.NotNull(owner, "Annex_AppleTradeContainer resolves in the DebugWorld");
		if (!owner)
			return;

		LF_TraderManagerComponent trader = LF_Component<LF_TraderManagerComponent>.Find(owner);
		ctx.NotNull(trader, "the container is a trader");
		if (!trader)
			return;

		int nowUnix = System.GetUnixTime();
		ctx.True(LF_PriceBoard.BuildTraderAnswerAt(trader, nowUnix) == null, "a trader that posts nothing gives no stall board");
		ctx.True(LF_PriceBoard.BuildTraderAnswerAt(null, nowUnix) == null, "no trader gives no stall board");

		// Post the apple row for this run only.
		LF_TraderItem appleRow;
		foreach (LF_TraderItem row : trader.m_aTradableItems)
		{
			if (row && LF_Utils.SamePrefab(row.m_ItemPrefab, APPLE))
				appleRow = row;
		}

		ctx.NotNull(appleRow, "the trader lists apples");
		if (!appleRow)
			return;

		bool was = appleRow.m_bPosted;
		appleRow.m_bPosted = true;
		LF_BoardAnswer answer;
		if (!RedProof())
			answer = LF_PriceBoard.BuildTraderAnswerAt(trader, nowUnix);
		appleRow.m_bPosted = was;

		ctx.NotNull(answer, "a trader that posts the apple gives a stall board");
		if (!answer)
			return;

		ctx.Equal(1, answer.m_aTexts.Count(), "one line: the apple");
		ctx.Equal(LF_PriceBoard.NUMBERS_PER_GOOD, answer.m_aNumbers.Count(), "a price and a below-base flag");
		if (answer.m_aNumbers.Count() < LF_PriceBoard.NUMBERS_PER_GOOD)
			return;

		ctx.Equal(LF_TraderSale.QuoteUnits(trader, APPLE, 1, nowUnix), answer.m_aNumbers[0], "the board shows what Sell 1 pays");
		ctx.True(answer.m_aNumbers[0] > 0, "and the price is above 0");
		ctx.Equal(1, LF_PriceBoardFace.RowsFromAnswer(answer).Count(), "the chalk face prints one row");
	}
}
