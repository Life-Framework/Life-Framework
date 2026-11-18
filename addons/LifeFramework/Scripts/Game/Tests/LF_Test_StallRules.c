// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_StallRules`: RED-OK (146 assertions), RedProof() re-arms a settled escrow so `an escrow settles once (expected 13, got 14)` fails; hook off: green, passed=1 failed=0.
//! Stall market rules (#2924): a pitch rents only on market day inside the hours and only with the fee; a
//! listing refuses above the cap, below the floor and past the item limit; a buyer's cash sits in escrow,
//! which settles once (the seller gets the price minus the cut) or refunds once, never both; every money
//! sum is int maths; the core ships the market off with a usable table.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_StallRules : LF_Test
{
	static const ResourceName ITEM = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName ITEM_B = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	//! Day 12 is day 5 of the week (12 % 7), the market day in `MakeConfig`.
	static const int MARKET_DAY = 12;
	static const int OPEN_HOUR = 10;

	override string GetName()
	{
		return "market/stall-rules";
	}

	//------------------------------------------------------------------------------------------------
	//! Market day 5 of the week, 8 to 18 o'clock, pitch 50, at most 2 listings, price 10..500, cut 10 percent.
	protected LF_StallConfig MakeConfig()
	{
		return LF_StallConfig.Make(true, 5, 8, 18, 50, 2, 10, 500, 10);
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunRent(ctx);
		RunListing(ctx);
		RunSettle(ctx);
		RunRefund(ctx);
		RunMoney(ctx);
		RunShipped(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected LF_Stall RentedStall(LF_StallConfig config)
	{
		LF_Stall stall = LF_Stall.Make("seller");
		LF_StallRules.Rent(config, stall, MARKET_DAY, OPEN_HOUR, 1000);
		return stall;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRent(LF_TestContext ctx)
	{
		LF_StallConfig config = MakeConfig();
		ctx.EqualStr("", config.Problem(), "the test table is valid");

		ctx.True(LF_StallRules.IsMarketOpen(config, MARKET_DAY, OPEN_HOUR), "day 12 at 10 o'clock is market day");
		ctx.True(LF_StallRules.IsMarketOpen(config, MARKET_DAY + 7, 8), "the next week's market day opens at 8");
		ctx.False(LF_StallRules.IsMarketOpen(config, MARKET_DAY, 7), "an hour before opening is closed");
		ctx.False(LF_StallRules.IsMarketOpen(config, MARKET_DAY, 18), "the closing hour is closed");
		ctx.False(LF_StallRules.IsMarketOpen(config, MARKET_DAY + 1, OPEN_HOUR), "the day after market day is closed");

		LF_Stall stall = LF_Stall.Make("seller");
		int rentDay = MARKET_DAY + 1;
		ctx.Equal(LF_EStallResult.CLOSED, LF_StallRules.Rent(config, stall, rentDay, OPEN_HOUR, 1000), "renting outside market day refuses");
		ctx.False(LF_StallRules.IsRented(stall, rentDay), "a refused rent holds no pitch");
		ctx.Equal(LF_EStallResult.CLOSED, LF_StallRules.Rent(config, stall, MARKET_DAY, 20, 1000), "renting after closing refuses");
		ctx.Equal(LF_EStallResult.CANT_AFFORD, LF_StallRules.Rent(config, stall, MARKET_DAY, OPEN_HOUR, 49), "one short of the fee refuses");
		ctx.Equal(LF_EStallResult.BAD_INPUT, LF_StallRules.Rent(config, null, MARKET_DAY, OPEN_HOUR, 1000), "no stall refuses");

		LF_StallConfig off = MakeConfig();
		off.m_bEnabled = false;
		ctx.Equal(LF_EStallResult.OFF, LF_StallRules.Rent(off, stall, MARKET_DAY, OPEN_HOUR, 1000), "a disabled market refuses");

		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Rent(config, stall, MARKET_DAY, OPEN_HOUR, 50), "the exact fee rents the pitch");
		ctx.True(LF_StallRules.IsRented(stall, MARKET_DAY), "the stall holds this market day's pitch");
		ctx.False(LF_StallRules.IsRented(stall, MARKET_DAY + 7), "the pitch is for one market day only");
		ctx.Equal(LF_EStallResult.ALREADY_RENTED, LF_StallRules.Rent(config, stall, MARKET_DAY, OPEN_HOUR, 1000), "the same market day cannot be rented twice");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunListing(LF_TestContext ctx)
	{
		LF_StallConfig config = MakeConfig();
		LF_Stall idle = LF_Stall.Make("seller");
		int id;
		ctx.Equal(LF_EStallResult.NOT_RENTED, LF_StallRules.List(config, idle, MARKET_DAY, ITEM, 100, id), "listing without a pitch refuses");

		LF_Stall stall = RentedStall(config);
		ctx.Equal(LF_EStallResult.PRICE_HIGH, LF_StallRules.List(config, stall, MARKET_DAY, ITEM, 501, id), "a listing above the cap refuses");
		ctx.Equal(0, id, "a refused listing has no id");
		ctx.Equal(LF_EStallResult.PRICE_LOW, LF_StallRules.List(config, stall, MARKET_DAY, ITEM, 9, id), "a listing below the floor refuses");
		ctx.Equal(LF_EStallResult.BAD_INPUT, LF_StallRules.List(config, stall, MARKET_DAY, "", 100, id), "a listing with no item refuses");
		ctx.Equal(0, stall.m_aListings.Count(), "refused listings leave the stall empty");

		ctx.Equal(LF_EStallResult.OK, LF_StallRules.List(config, stall, MARKET_DAY, ITEM, 500, id), "the cap itself is allowed");
		ctx.Equal(1, id, "the first listing is id 1");
		ctx.Equal(LF_EStallResult.OK, LF_StallRules.List(config, stall, MARKET_DAY, ITEM_B, 10, id), "the floor itself is allowed");
		ctx.Equal(2, id, "the second listing is id 2");
		ctx.Equal(LF_EStallResult.FULL, LF_StallRules.List(config, stall, MARKET_DAY, ITEM, 100, id), "a listing past the item limit refuses");
		ctx.Equal(2, stall.m_aListings.Count(), "the stall holds exactly the limit");

		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Delist(stall, 1), "a listing with no escrow can be taken down");
		ctx.Equal(LF_EStallResult.NO_LISTING, LF_StallRules.Delist(stall, 1), "a gone listing cannot be taken down twice");
		ctx.Equal(LF_EStallResult.OK, LF_StallRules.List(config, stall, MARKET_DAY, ITEM, 100, id), "a freed slot lists again");
		ctx.Equal(3, id, "listing ids never repeat");
		ctx.Equal(LF_EStallResult.NOT_RENTED, LF_StallRules.List(config, stall, MARKET_DAY + 7, ITEM, 100, id), "a pitch rented for one week does not list the next");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSettle(LF_TestContext ctx)
	{
		LF_StallConfig config = MakeConfig();
		LF_Stall stall = RentedStall(config);
		int id;
		LF_StallRules.List(config, stall, MARKET_DAY, ITEM, 205, id);
		LF_StallEscrow escrow;
		ctx.Equal(LF_EStallResult.CANT_AFFORD, LF_StallRules.Reserve(config, stall, id, "buyer", 204, MARKET_DAY, OPEN_HOUR, escrow), "a buyer one short refuses");
		ctx.True(escrow == null, "a refused purchase holds no escrow");
		ctx.Equal(LF_EStallResult.SELF_BUY, LF_StallRules.Reserve(config, stall, id, "seller", 1000, MARKET_DAY, OPEN_HOUR, escrow), "the seller cannot buy their own item");
		ctx.Equal(LF_EStallResult.CLOSED, LF_StallRules.Reserve(config, stall, id, "buyer", 1000, MARKET_DAY, 20, escrow), "buying after closing refuses");
		ctx.Equal(LF_EStallResult.NO_LISTING, LF_StallRules.Reserve(config, stall, 99, "buyer", 1000, MARKET_DAY, OPEN_HOUR, escrow), "an unknown listing refuses");

		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Reserve(config, stall, id, "buyer", 205, MARKET_DAY, OPEN_HOUR, escrow), "the exact price reserves the listing");
		ctx.Equal(205, escrow.m_iAmount, "the escrow holds the whole price");
		LF_StallEscrow held = escrow;
		LF_StallEscrow second;
		ctx.Equal(LF_EStallResult.RESERVED, LF_StallRules.Reserve(config, stall, id, "other", 1000, MARKET_DAY, OPEN_HOUR, second), "a held listing cannot be reserved twice");
		ctx.Equal(LF_EStallResult.RESERVED, LF_StallRules.Delist(stall, id), "a held listing cannot be taken down");

		ctx.Equal(LF_EStallResult.ALREADY_RENTED, LF_StallRules.Rent(config, stall, MARKET_DAY, OPEN_HOUR, 1000), "no second rent while the escrow is open");

		int pay;
		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Settle(config, stall, held, pay), "handing the item over settles the escrow");
		ctx.Equal(185, pay, "the seller gets 205 minus the 10 percent cut of 20 (rounded down)");
		ctx.Equal(185, stall.m_iEarned, "the stall counts the seller's pay");
		ctx.Equal(20, stall.m_iCutKept, "the stall counts the cut");
		ctx.True(stall.Find(id) == null, "a sold listing is gone");

		// RedProof: the escrow looks held again, so the second settle pays out twice.
		if (RedProof())
			held.m_iState = LF_StallEscrow.HELD;

		int again = 77;
		ctx.Equal(LF_EStallResult.NOT_HELD, LF_StallRules.Settle(config, stall, held, again), "an escrow settles once");
		ctx.Equal(0, again, "a second settle pays nothing");
		int refund = 77;
		ctx.Equal(LF_EStallResult.NOT_HELD, LF_StallRules.Refund(stall, held, refund), "a settled escrow never refunds");
		ctx.Equal(0, refund, "a refused refund returns nothing");
		ctx.Equal(185, stall.m_iEarned, "the seller was paid exactly once");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRefund(LF_TestContext ctx)
	{
		LF_StallConfig config = MakeConfig();
		LF_Stall stall = RentedStall(config);
		int id;
		LF_StallRules.List(config, stall, MARKET_DAY, ITEM, 100, id);
		LF_StallEscrow escrow;
		LF_StallRules.Reserve(config, stall, id, "buyer", 300, MARKET_DAY, OPEN_HOUR, escrow);

		int refund;
		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Refund(stall, escrow, refund), "a fallen-through sale refunds");
		ctx.Equal(100, refund, "the buyer gets the whole price back, no cut");
		LF_StallListing back = stall.Find(id);
		ctx.True(back != null, "the refunded listing stays on the stall");
		if (back)
			ctx.False(back.m_bReserved, "the refunded listing is open to buyers again");
		ctx.Equal(0, stall.m_iEarned, "a refund pays the seller nothing");

		int again = 77;
		ctx.Equal(LF_EStallResult.NOT_HELD, LF_StallRules.Refund(stall, escrow, again), "an escrow refunds once");
		ctx.Equal(0, again, "a second refund returns nothing");
		int pay = 77;
		ctx.Equal(LF_EStallResult.NOT_HELD, LF_StallRules.Settle(config, stall, escrow, pay), "a refunded escrow never settles");
		ctx.Equal(0, pay, "a refused settle pays nothing");
		ctx.Equal(0, stall.m_iEarned, "the seller is still unpaid after the refused settle");

		LF_StallEscrow next;
		ctx.Equal(LF_EStallResult.OK, LF_StallRules.Reserve(config, stall, id, "other", 100, MARKET_DAY, OPEN_HOUR, next), "the freed listing reserves again");
		LF_Stall elsewhere = RentedStall(config);
		elsewhere.m_sHolder = "someone-else";
		int stray = 77;
		ctx.Equal(LF_EStallResult.WRONG_STALL, LF_StallRules.Settle(config, elsewhere, next, stray), "another stall cannot settle this escrow");
		ctx.Equal(LF_EStallResult.WRONG_STALL, LF_StallRules.Refund(elsewhere, next, stray), "another stall cannot refund this escrow");
		ctx.Equal(LF_StallEscrow.HELD, next.m_iState, "a wrong-stall attempt leaves the escrow held");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunMoney(LF_TestContext ctx)
	{
		// The cut is int maths: rounded down, split so a price near the largest int-safe cash cannot overflow.
		ctx.Equal(0, LF_StallRules.Cut(9, 10), "10 percent of 9 rounds down to 0");
		ctx.Equal(1, LF_StallRules.Cut(10, 10), "10 percent of 10 is 1");
		ctx.Equal(5, LF_StallRules.Cut(55, 10), "10 percent of 55 rounds down to 5");
		ctx.Equal(1000000, LF_StallRules.Cut(LF_StallConfig.MAX_CASH, 10), "10 percent of the largest cash is exact");
		ctx.Equal(LF_StallConfig.MAX_CASH, LF_StallRules.Cut(LF_StallConfig.MAX_CASH, 100), "a 100 percent cut takes it all");
		ctx.Equal(0, LF_StallRules.SellerShare(LF_StallConfig.MAX_CASH, 100), "a 100 percent cut leaves the seller nothing");
		ctx.Equal(9000000, LF_StallRules.SellerShare(LF_StallConfig.MAX_CASH, 10), "the seller keeps 90 percent of the largest cash");
		ctx.Equal(0, LF_StallRules.Cut(-5, 10), "a negative amount has no cut");
		ctx.Equal(50, LF_StallRules.SellerShare(50, 0), "no cut pays the seller the whole price");
		ctx.Equal(0, LF_StallRules.SellerShare(50, 250), "a cut above 100 percent is held to 100");

		// Pay plus cut always equals the price: nothing is created or lost.
		int price;
		for (price = 0; price < 400; price += 7)
		{
			int cut = LF_StallRules.Cut(price, 13);
			ctx.Equal(price, LF_StallRules.SellerShare(price, 13) + cut, "seller pay plus cut equals the price");
		}

		// Running totals stop at the guard, never wrap.
		LF_StallConfig config = LF_StallConfig.Make(true, 5, 8, 18, 0, 5, 1, LF_StallConfig.MAX_CASH, 0);
		LF_Stall stall = LF_Stall.Make("seller");
		LF_StallRules.Rent(config, stall, MARKET_DAY, OPEN_HOUR, 0);
		stall.m_iEarned = LF_StallRules.MAX_TOTAL - 5;
		int id;
		LF_StallRules.List(config, stall, MARKET_DAY, ITEM, LF_StallConfig.MAX_CASH, id);
		LF_StallEscrow escrow;
		LF_StallRules.Reserve(config, stall, id, "buyer", LF_StallConfig.MAX_CASH, MARKET_DAY, OPEN_HOUR, escrow);
		int pay;
		LF_StallRules.Settle(config, stall, escrow, pay);
		ctx.Equal(LF_StallConfig.MAX_CASH, pay, "a no-cut sale pays the whole price");
		ctx.Equal(LF_StallRules.MAX_TOTAL, stall.m_iEarned, "the earned total stops at the guard instead of wrapping");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunShipped(LF_TestContext ctx)
	{
		LF_StallConfig bad = MakeConfig();
		bad.m_iMinPrice = 600;
		ctx.False(bad.Sanitize(), "a floor above the cap is refused at load");
		ctx.False(bad.m_bEnabled, "a refused table switches the market off");
		LF_StallConfig hours = MakeConfig();
		hours.m_iCloseHour = hours.m_iOpenHour;
		ctx.False(hours.Problem().IsEmpty(), "hours with no open window are a problem");
		LF_StallConfig cut = MakeConfig();
		cut.m_iCutPct = 101;
		ctx.False(cut.Problem().IsEmpty(), "a cut above 100 percent is a problem");

		LF_StallConfig.Reload();
		LF_StallConfig shipped = LF_StallConfig.Get();
		ctx.True(shipped != null, "the shipped stall config resolves");
		if (shipped)
		{
			ctx.False(shipped.m_bEnabled, "the core ships stalls off");
			ctx.EqualStr("", shipped.Problem(), "the shipped table is valid");
		}

		LF_StallConfig.Reload();
	}
}
