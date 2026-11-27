// red-proof: Observed 2026-11-27 in the batched `tools\cli test --no-build --red` run of the six round-5 action tests: `RED PROOF OK (all 6 listed test(s) fail with the hook on)`; here `the buyer holds exactly one apple (expected 1, got 0)`, `the holder was paid the price less the cut (expected 95, got 0)`. Green with `--only`.
// tier: WORLD
//! market/stall-buy-action (#2926, coverage round 5): the "Buy from the stall" prompt. The perform buys the
//! cheapest listing on the server, under the acting character's own record key: a stall with nothing listed
//! sells nothing; a buyer one short moves nothing; the holder cannot buy from their own stall; a paying buyer
//! gets exactly one item for exactly the cheapest price, the seller is paid the price less the market's cut
//! and the dearer listing stays; after the closing hour nothing is sold. The prompt shows only on a rented stall.
[LF_TestInfo(LF_TestTier.WORLD)]
class LF_Test_StallBuyAction : LF_Test
{
	protected static const ResourceName STALL = "{5E2A7D9C4B1F8306}Prefabs/Props/Stalls/LF_MarketStall.et";
	protected static const ResourceName APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";
	protected static const string UID_HOLDER = "lf-test-stall-buy-holder";
	protected static const string UID_BUYER = "lf-test-stall-buy-buyer";
	//! Day 12 is day 5 of the week (12 % 7), the market day in the test config.
	protected static const int MARKET_DAY = 12;
	protected static const int FEE = 50;
	protected static const int PRICE = 100;
	//! The market's cut is 5 percent: a sale of 100 pays the seller 95.
	protected static const int SELLER_PAY = 95;

	//------------------------------------------------------------------------------------------------
	override string GetName()
	{
		return "market/stall-buy-action";
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCash(IEntity character, int amount)
	{
		int carried = LF_MoneyUtils.GetCash(character);
		if (carried > 0)
			LF_MoneyUtils.RemoveCash(character, carried);

		if (amount > 0)
			LF_MoneyUtils.AddCash(character, amount);
	}

	//------------------------------------------------------------------------------------------------
	protected int Apples(IEntity character)
	{
		return LF_InventoryUtils.GetAmount(character, APPLE);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearApples(IEntity character)
	{
		int have = Apples(character);
		if (have > 0)
			LF_InventoryUtils.RemoveAmount(character, APPLE, have);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_StallConfig.UseForTest(LF_StallConfig.Make(true, 5, 8, 18, FEE, 6, 1, 1000, 5));
		LF_StallClock.SetForTest(MARKET_DAY, 10);

		IEntity holder = LF_TestCharacterFixture.Spawn(ctx);
		IEntity buyer = LF_TestCharacterFixture.Spawn(ctx);
		IEntity prop;
		if (holder)
			prop = LF_Utils.SpawnEntityPrefab(STALL, holder.GetOrigin() + "1 0 0");

		LF_StallComponent stall;
		LF_StallBuyAction action;
		if (prop)
		{
			stall = LF_Component<LF_StallComponent>.Find(prop);
			action = LF_StallBuyAction.Cast(LF_TestActionFinder.Find(prop, LF_StallBuyAction));
		}

		ctx.NotNull(stall, "the stall prefab spawns with LF_StallComponent");
		ctx.NotNull(action, "the stall carries the buy action");
		if (!holder || !buyer || !stall || !action)
		{
			Cleanup(holder, buyer, prop);
			return;
		}

		buyer.SetOrigin(holder.GetOrigin());
		LF_StallTrade.SetHolderForTest(holder, UID_HOLDER);
		ClearApples(holder);
		ClearApples(buyer);
		SetCash(holder, FEE);
		SetCash(buyer, 0);
		ctx.False(action.CanBeShownScript(buyer), "a free stall does not offer the buy prompt");
		ctx.Equal(LF_EStallResult.OK, stall.Rent(holder, UID_HOLDER), "the holder rents the stall");
		SetCash(holder, 0);
		ctx.True(action.CanBeShownScript(buyer), "a rented stall offers the buy prompt");
		ctx.True(action.CanBePerformedScript(buyer), "and it can be performed in market hours");

		// Nothing listed: nothing is sold.
		LF_Utils.SetUidForTests(buyer, UID_BUYER);
		SetCash(buyer, PRICE);
		action.PerformAction(prop, buyer);
		ctx.Equal(PRICE, LF_MoneyUtils.GetCash(buyer), "a stall with nothing listed takes no cash");
		ctx.Equal(0, Apples(buyer), "a stall with nothing listed hands over nothing");

		// Two apples listed, the dearer one first: the cheapest is the one sold.
		ctx.Equal(2, LF_InventoryUtils.AddAmount(holder, APPLE, 2, false), "two apples go into the holder's inventory");
		int dearId;
		int cheapId;
		LF_EStallResult rule;
		ctx.Equal(LF_EStallTrade.OK, LF_StallTrade.List(stall, holder, UID_HOLDER, APPLE, PRICE + 20, dearId, rule), "the holder lists the dearer apple");
		ctx.Equal(LF_EStallTrade.OK, LF_StallTrade.List(stall, holder, UID_HOLDER, APPLE, PRICE, cheapId, rule), "the holder lists the cheaper apple");
		ctx.Equal(cheapId, LF_StallTrade.CheapestOpen(stall.GetStall()), "the cheaper listing is the cheapest open one");

		// One short of the price: nothing moves.
		SetCash(buyer, PRICE - 1);
		action.PerformAction(prop, buyer);
		ctx.Equal(PRICE - 1, LF_MoneyUtils.GetCash(buyer), "a buyer one short loses nothing");
		ctx.Equal(0, Apples(buyer), "a buyer one short gets nothing");
		ctx.Equal(2, stall.GetStall().m_aListings.Count(), "both listings stay");

		// The holder cannot buy from their own stall.
		LF_Utils.SetUidForTests(holder, UID_HOLDER);
		SetCash(holder, PRICE);
		action.PerformAction(prop, holder);
		ctx.Equal(PRICE, LF_MoneyUtils.GetCash(holder), "the holder's own buy takes no cash");
		ctx.Equal(2, Apples(holder), "the holder's own buy moves no apple");
		ctx.Equal(2, stall.GetStall().m_aListings.Count(), "the holder's own buy sells no listing");
		SetCash(holder, 0);

		// A paying buyer gets one apple for the cheapest price.
		LF_Utils.SetUidForTests(buyer, UID_BUYER);
		SetCash(buyer, PRICE + 50);
		if (!RedProof())
			action.PerformAction(prop, buyer);

		ctx.Equal(50, LF_MoneyUtils.GetCash(buyer), "the buyer paid the cheapest price and no more");
		ctx.Equal(1, Apples(buyer), "the buyer holds exactly one apple");
		ctx.Equal(1, Apples(holder), "the holder lost exactly one apple");
		ctx.Equal(SELLER_PAY, LF_MoneyUtils.GetCash(holder), "the holder was paid the price less the cut");
		ctx.Equal(1, stall.GetStall().m_aListings.Count(), "one listing is left");
		ctx.True(stall.GetStall().Find(cheapId) == null, "the cheaper listing is the one sold");
		ctx.True(stall.GetStall().Find(dearId) != null, "the dearer listing stays");

		// After the closing hour nothing is sold.
		LF_StallClock.SetForTest(MARKET_DAY, 18);
		ctx.False(action.CanBePerformedScript(buyer), "after closing the prompt cannot be performed");
		SetCash(buyer, PRICE + 50);
		action.PerformAction(prop, buyer);
		ctx.Equal(PRICE + 50, LF_MoneyUtils.GetCash(buyer), "a buy after closing takes no cash");
		ctx.Equal(1, Apples(buyer), "a buy after closing hands over nothing");

		ClearApples(holder);
		ClearApples(buyer);
		SetCash(holder, 0);
		SetCash(buyer, 0);
		Cleanup(holder, buyer, prop);
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(IEntity holder, IEntity buyer, IEntity prop)
	{
		LF_Utils.SetUidForTests(null, "");
		LF_StallTrade.ResetStatics();
		LF_StallClock.ResetStatics();
		LF_StallConfig.Reload();
		LF_TestCharacterFixture.Delete(holder);
		LF_TestCharacterFixture.Delete(buyer);
		LF_TestCharacterFixture.Delete(prop);
	}
}
