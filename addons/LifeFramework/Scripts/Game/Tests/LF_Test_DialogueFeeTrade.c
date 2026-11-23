// red-proof: batched run `tools\cli test --no-build --red LF_Test_DialogueFeeTrade`: RedProof() skips the paid sale, so "the sale goes through", "the fee is taken" and "the item is handed over" fail (and the later "already held" check, which needs the item, fails too). Observed 2026-11-23: RED PROOF OK, "the fee is taken (expected 100, got 500)", "the item is handed over", "holding one already: refused".
//! #4381: the dialogue fee hook (LF_DialogueFeeTrade, action `FeeForItem`) on a real character. Short of
//! the fee: refused in words, nothing taken, nothing given. With the cash: the fee is taken and the item
//! handed over. Holding one already: refused, nothing taken. An item that cannot be handed over: the fee
//! comes back whole. The hook itself answers with the paid key and parses `<fee>|<prefab>`.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_DialogueFeeTrade : LF_Test
{
	//! A plain one-slot core item stands in for a pack's key.
	static const ResourceName ITEM = "{3735389521255DBE}Prefabs/Items/HouseKeyItem.et";
	static const int FEE = 400;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "dialogue/fee-trade";
	}

	//------------------------------------------------------------------------------------------------
	protected static void ParseCheck(LF_TestContext ctx)
	{
		int fee;
		ResourceName prefab;
		ctx.True(LF_DialogueFeeTrade.ParseParam("400|" + ITEM, fee, prefab), "a fee and a prefab parse");
		ctx.Equal(FEE, fee, "the fee reads back");
		ctx.EqualStr(ITEM, prefab, "the prefab reads back");
		ctx.False(LF_DialogueFeeTrade.ParseParam(ITEM, fee, prefab), "a param with no fee is refused");
		ctx.False(LF_DialogueFeeTrade.ParseParam("0|" + ITEM, fee, prefab), "a zero fee is refused");
		ctx.False(LF_DialogueFeeTrade.ParseParam("-5|" + ITEM, fee, prefab), "a negative fee is refused");
		ctx.False(LF_DialogueFeeTrade.ParseParam("40x|" + ITEM, fee, prefab), "a fee that is not a number is refused");
		ctx.False(LF_DialogueFeeTrade.ParseParam("400|", fee, prefab), "a param with no prefab is refused");
	}

	//------------------------------------------------------------------------------------------------
	protected static void RemoveItems(IEntity body)
	{
		SCR_InventoryStorageManagerComponent manager = LF_Component<SCR_InventoryStorageManagerComponent>.Find(body);
		if (!manager)
			return;

		foreach (IEntity item : LF_InventoryUtils.FindItemsByPrefab(manager, ITEM))
			SCR_EntityHelper.DeleteEntityAndChildren(item);
	}

	//------------------------------------------------------------------------------------------------
	protected void TradeCheck(LF_TestContext ctx, IEntity body)
	{
		LF_DialogueFeeTrade trade = new LF_DialogueFeeTrade();
		LF_EntityCashWallet wallet = new LF_EntityCashWallet(body, LF_DialogueFeeTrade.LEDGER_REASON, string.Empty);

		LF_MoneyUtils.AddCash(body, 100);
		ctx.Equal(LF_EFeeTrade.NOT_ENOUGH_CASH, trade.Sell(body, wallet, FEE, ITEM), "short of the fee: refused");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "short of the fee: nothing is taken");
		ctx.False(LF_TestCharacterFixture.HasItem(body, ITEM), "short of the fee: nothing is handed over");
		ctx.EqualStr(LF_DialogueFeeTrade.ANSWER_SHORT, LF_DialogueFeeTrade.AnswerKey(LF_EFeeTrade.NOT_ENOUGH_CASH), "short of the fee: the NPC says so");

		LF_MoneyUtils.AddCash(body, FEE);
		LF_EFeeTrade sold = LF_EFeeTrade.BAD_PARAM;
		if (!RedProof())
			sold = trade.Sell(body, wallet, FEE, ITEM);

		ctx.Equal(LF_EFeeTrade.SOLD, sold, "the sale goes through");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "the fee is taken");
		ctx.True(LF_TestCharacterFixture.HasItem(body, ITEM), "the item is handed over");

		ctx.Equal(LF_EFeeTrade.ALREADY_HELD, trade.Sell(body, wallet, FEE, ITEM), "holding one already: refused");
		ctx.Equal(100, LF_MoneyUtils.GetCash(body), "holding one already: nothing is taken");

		RemoveItems(body);
		LF_MoneyUtils.AddCash(body, FEE);
		LF_TestFeeTradeNoRoom full = new LF_TestFeeTradeNoRoom();
		ctx.Equal(LF_EFeeTrade.NO_ROOM, full.Sell(body, wallet, FEE, ITEM), "no room for the item: the sale is undone");
		ctx.Equal(100 + FEE, LF_MoneyUtils.GetCash(body), "no room for the item: the fee comes back whole");
		ctx.False(LF_TestCharacterFixture.HasItem(body, ITEM), "no room for the item: nothing is handed over");

		ctx.Equal(LF_EFeeTrade.BAD_PARAM, trade.Sell(body, wallet, 0, ITEM), "a zero fee sells nothing");
		ctx.Equal(LF_EFeeTrade.NO_PLAYER, trade.Sell(null, wallet, FEE, ITEM), "no player buys nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void HookCheck(LF_TestContext ctx, IEntity body)
	{
		LF_DialogueHook hook = LF_DialogueHook.Find("FeeForItem");
		ctx.NotNull(hook, "the FeeForItem action resolves");
		if (!hook)
			return;

		int before = LF_MoneyUtils.GetCash(body);
		array<string> spoken = {};
		hook.Run(body, null, FEE.ToString() + "|" + ITEM, spoken);
		ctx.Equal(1, spoken.Count(), "the hook answers once");
		if (!spoken.IsEmpty())
			ctx.EqualStr(LF_DialogueFeeTrade.ANSWER_PAID, spoken[0], "the hook answers with the paid key");
		ctx.Equal(before - FEE, LF_MoneyUtils.GetCash(body), "the hook takes the fee");
		ctx.True(LF_TestCharacterFixture.HasItem(body, ITEM), "the hook hands the item over");

		spoken.Clear();
		hook.Run(body, null, "not a param", spoken);
		ctx.Equal(1, spoken.Count(), "a broken param still gets an answer");
		if (!spoken.IsEmpty())
			ctx.EqualStr(LF_DialogueFeeTrade.ANSWER_FAILED, spoken[0], "a broken param is answered as a failure");
		ctx.Equal(before - FEE, LF_MoneyUtils.GetCash(body), "a broken param takes nothing");
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		ParseCheck(ctx);

		IEntity body = LF_TestCharacterFixture.Spawn(ctx);
		ctx.NotNull(body, "a character spawns");
		if (!body)
			return;

		LF_MoneyUtils.RemoveCash(body, LF_MoneyUtils.GetCash(body));
		TradeCheck(ctx, body);
		RemoveItems(body);
		LF_MoneyUtils.AddCash(body, FEE);
		HookCheck(ctx, body);

		RemoveItems(body);
		LF_TestCharacterFixture.Delete(body);
	}
}

//------------------------------------------------------------------------------------------------
//! The trade with a full inventory: the hand-over always fails.
class LF_TestFeeTradeNoRoom : LF_DialogueFeeTrade
{
	//------------------------------------------------------------------------------------------------
	override protected bool Give(IEntity player, ResourceName prefab)
	{
		return false;
	}
}
