// red-proof: observed 2026-11-18 `tools/cli test --no-build --red LF_Test_BuyerDemand` printed `RED PROOF OK`; with the hook the rows never refill, so `an hour later one unit is back (expected 1, got 0)` and the refill, cap and 1.8e9 cases fail. Green with `--only`: passed.
//! Premium buyer with demand (#3267): a purse of `cap` units refills `refill` units an hour. A sale is
//! accepted up to what the purse holds and the rest is refused, a refill never passes the cap, a long gap
//! or a clock that moved back is clamped, a unix time near 1.8e9 stays exact (#2787), a row that names a
//! curve prices each unit marginally inside the cap, and a bad row is refused at load. The core ships no
//! rows.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_BuyerDemand : LF_Test
{
	static const ResourceName PLANK = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName BOARD = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	static const ResourceName OTHER = "{ACC9A10F0F20F5C4}Prefabs/Resources/Mining/GoldBar.et";
	static const string BUYER = "sp:b59";
	static const int HOUR = 3600;
	static const int T0 = 1800000480;	// lf-fixed: a unix time past 2^24, where float maths loses seconds

	override string GetName()
	{
		return "market/buyer-demand";
	}

	//------------------------------------------------------------------------------------------------
	//! The refill the cases use, or 0 under the red-proof hook (`--red`).
	protected int Refill(int perHour)
	{
		if (RedProof())
			return 0;

		return perHour;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunFull(ctx);
		RunRefill(ctx);
		RunCap(ctx);
		RunUnixTime(ctx);
		RunPartUnit(ctx);
		RunNever(ctx);
		RunCurve(ctx);
		RunRefused(ctx);
		RunShipped(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! A full purse of 40 takes 40 of 50 and refuses 10, pays the flat price, and says when it is back.
	protected void RunFull(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(BUYER, PLANK, 40, Refill(20), 8);
		ctx.EqualStr("", row.Problem(), "the test row is valid");
		LF_BuyerPurse purse = LF_BuyerPurse.Full(row, T0);
		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, 50, T0);
		ctx.Equal(40, sale.m_iAccepted, "a full purse of 40 takes 40 of 50");
		ctx.Equal(10, sale.m_iRefused, "and refuses 10");
		ctx.Equal(320, sale.m_iPayout, "40 units at a flat 8 pay 320");
		ctx.Equal(0, purse.m_iRemaining, "the purse is empty");
		ctx.Equal(180, sale.m_iSecondsToNext, "at 20 an hour the next unit is 180 s away");

		LF_BuyerDemandSale again = LF_BuyerDemand.Sell(row, purse, 5, T0 + 60);
		ctx.Equal(0, again.m_iAccepted, "an empty purse buys nothing");
		ctx.Equal(5, again.m_iRefused, "and refuses all 5");
		ctx.Equal(0, again.m_iPayout, "and pays nothing");
		ctx.Equal(120, again.m_iSecondsToNext, "60 s later the next unit is 120 s away");

		LF_BuyerDemandSale none = LF_BuyerDemand.Sell(row, purse, -3, T0 + 60);
		ctx.Equal(0, none.m_iAccepted, "a negative sale takes nothing");
		ctx.Equal(0, none.m_iRefused, "and refuses nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! One hour refills 20, and a sale in between uses the refilled purse.
	protected void RunRefill(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(BUYER, PLANK, 40, Refill(20), 8);
		LF_BuyerPurse purse = LF_BuyerPurse.Make(row, 0, T0);
		LF_BuyerDemand.Refill(row, purse, T0 + HOUR);
		ctx.Equal(20, purse.m_iRemaining, "one hour refills 20");
		LF_BuyerDemand.Refill(row, purse, T0 + HOUR);
		ctx.Equal(20, purse.m_iRemaining, "refilling twice at the same second adds nothing");

		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, 30, T0 + HOUR);
		ctx.Equal(20, sale.m_iAccepted, "a sale after the hour takes the 20 refilled");
		ctx.Equal(10, sale.m_iRefused, "and refuses the other 10");
		ctx.Equal(180, sale.m_iSecondsToNext, "and the next unit is 180 s away");

		LF_BuyerPurse half = LF_BuyerPurse.Make(row, 10, T0);
		LF_BuyerDemand.Refill(row, half, T0 + HOUR / 2);
		ctx.Equal(20, half.m_iRemaining, "half an hour refills 10 more");
	}

	//------------------------------------------------------------------------------------------------
	//! A refill never passes the cap, however long the gap; a clock that moved back moves the stamp only.
	protected void RunCap(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(BUYER, PLANK, 40, Refill(20), 8);
		LF_BuyerPurse purse = LF_BuyerPurse.Make(row, 35, T0);
		LF_BuyerDemand.Refill(row, purse, T0 + HOUR);
		ctx.Equal(40, purse.m_iRemaining, "an hour into a nearly full purse stops at the cap");
		ctx.Equal(T0 + HOUR, purse.m_iStamp, "a full purse follows the clock");

		LF_BuyerPurse empty = LF_BuyerPurse.Make(row, 0, T0);
		LF_BuyerDemand.Refill(row, empty, T0 + 9000 * HOUR);
		ctx.Equal(40, empty.m_iRemaining, "a gap of 9000 hours is clamped to the cap");
		ctx.Equal(T0 + 9000 * HOUR, empty.m_iStamp, "and the clock moves to now");

		LF_BuyerPurse back = LF_BuyerPurse.Make(row, 5, T0);
		LF_BuyerDemand.Refill(row, back, T0 - HOUR);
		ctx.Equal(5, back.m_iRemaining, "a clock that moved back refills nothing");
		ctx.Equal(T0 - HOUR, back.m_iStamp, "and restarts the count from there");

		LF_BuyerPurse over = LF_BuyerPurse.Make(row, 500, T0);
		ctx.Equal(40, over.m_iRemaining, "a purse made above the cap is held to it");
	}

	//------------------------------------------------------------------------------------------------
	//! #2787: a unix time near 1.8e9 stays exact: no second is lost to float rounding.
	protected void RunUnixTime(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(BUYER, PLANK, 40, Refill(20), 8);
		LF_BuyerPurse purse = LF_BuyerPurse.Make(row, 10, T0);
		LF_BuyerDemand.Refill(row, purse, T0 + HOUR);
		ctx.Equal(30, purse.m_iRemaining, "1.8e9 unix: an hour refills exactly 20");
		ctx.Equal(T0 + HOUR, purse.m_iStamp, "the stamp moves by exactly an hour");

		LF_BuyerPurse odd = LF_BuyerPurse.Make(row, 0, T0 + 1);
		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, odd, 1, T0 + 1 + 179);
		ctx.Equal(0, sale.m_iAccepted, "179 s at 20 an hour is not yet a unit");
		ctx.Equal(1, sale.m_iSecondsToNext, "and one second is left");
		sale = LF_BuyerDemand.Sell(row, odd, 1, T0 + 1 + 180);
		ctx.Equal(1, sale.m_iAccepted, "180 s is exactly one unit");
		ctx.Equal(T0 + 1 + 180, odd.m_iStamp, "and the stamp moves by exactly its 180 s");
	}

	//------------------------------------------------------------------------------------------------
	//! A part of a unit of progress is kept: 7 an hour is a unit every 514.3 s.
	protected void RunPartUnit(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(BUYER, PLANK, 10, Refill(7), 8);
		LF_BuyerPurse purse = LF_BuyerPurse.Make(row, 0, T0);
		LF_BuyerDemand.Refill(row, purse, T0 + 515);
		ctx.Equal(1, purse.m_iRemaining, "515 s at 7 an hour is one unit");
		LF_BuyerDemand.Refill(row, purse, T0 + HOUR);
		ctx.Equal(7, purse.m_iRemaining, "a full hour from the start is exactly 7 units, none lost to the first refill");
	}

	//------------------------------------------------------------------------------------------------
	//! A row that never refills (0 an hour) stays empty and says so.
	protected void RunNever(LF_TestContext ctx)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(BUYER, PLANK, 5, 0, 8);
		ctx.EqualStr("", row.Problem(), "a refill of 0 is a valid one-off purse");
		LF_BuyerPurse purse = LF_BuyerPurse.Full(row, T0);
		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, 5, T0);
		ctx.Equal(5, sale.m_iAccepted, "the one-off purse is spent once");
		ctx.Equal(-1, sale.m_iSecondsToNext, "and it never refills");
		LF_BuyerDemand.Refill(row, purse, T0 + 100 * HOUR);
		ctx.Equal(0, purse.m_iRemaining, "a hundred hours later it is still empty");
	}

	//------------------------------------------------------------------------------------------------
	//! A row with a curve: price 10, the curve drops 10 percent of it (1 a unit) down to a floor of 5. The
	//! price follows how much of the cap is used, so it falls inside one sale and between sales alike.
	protected void RunCurve(LF_TestContext ctx)
	{
		LF_MarketGoodDef curve = LF_MarketGoodDef.Make(BOARD, 100, 5, 150, 10, 5);
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Make(BUYER, BOARD, 10, Refill(5), 10, BOARD);
		LF_BuyerPurse purse = LF_BuyerPurse.Full(row, T0);

		LF_BuyerDemandSale first = LF_BuyerDemand.Sell(row, purse, 3, T0, curve);
		ctx.Equal(27, first.m_iPayout, "three units fall 10, 9, 8: marginal, 27");
		LF_BuyerDemandSale second = LF_BuyerDemand.Sell(row, purse, 2, T0, curve);
		ctx.Equal(13, second.m_iPayout, "the next two start where the first left off: 7 and 6");
		LF_BuyerDemandSale rest = LF_BuyerDemand.Sell(row, purse, 9, T0, curve);
		ctx.Equal(5, rest.m_iAccepted, "five units are left in the purse");
		ctx.Equal(25, rest.m_iPayout, "5, 5, 5, 5, 5: the curve's floor holds");
		ctx.Equal(4, rest.m_iRefused, "and four are refused");

		LF_BuyerPurse fresh = LF_BuyerPurse.Full(row, T0);
		ctx.Equal(65, LF_BuyerDemand.Sell(row, fresh, 10, T0, curve).m_iPayout, "all ten in one sale: 10+9+8+7+6 then five at the 5 floor = 65");
		ctx.Equal(100, LF_BuyerDemand.Sell(row, LF_BuyerPurse.Full(row, T0), 10, T0).m_iPayout, "with no curve the same ten units pay a flat 100");

		// Time back in the purse means the price comes back with it.
		LF_BuyerPurse spent = LF_BuyerPurse.Make(row, 0, T0);
		LF_BuyerDemandSale later = LF_BuyerDemand.Sell(row, spent, 1, T0 + HOUR, curve);
		ctx.Equal(1, later.m_iAccepted, "an hour later one unit is back");
		ctx.Equal(5, later.m_iPayout, "and it sells at the price of a half-empty purse, down at the floor");
	}

	//------------------------------------------------------------------------------------------------
	//! Bad rows are refused at load with LF_Debug.Error("Market", ...) naming each; the good one stays.
	protected void RunRefused(LF_TestContext ctx)
	{
		LF_BuyerDemandConfig config = new LF_BuyerDemandConfig();
		config.m_aRows = {};
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, PLANK, 0, 5, 8));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, BOARD, 10, -5, 8));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make("", OTHER, 10, 5, 8));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, "", 10, 5, 8));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, OTHER, LF_BuyerDemand.MAX_CAP + 1, 5, 8));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make(BUYER, OTHER, 10, 5, -1));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make("sp:b60", PLANK, 10, 5, 8));
		config.m_aRows.Insert(LF_BuyerDemandRow.Make("sp:b60", PLANK, 20, 5, 9));

		ctx.True(config.m_aRows[0].Problem().Contains("cap 0"), "a cap of 0 is named: " + config.m_aRows[0].Problem());
		ctx.True(config.m_aRows[1].Problem().Contains("negative refill"), "a negative refill is named: " + config.m_aRows[1].Problem());
		ctx.True(config.m_aRows[4].Problem().Contains("largest purse"), "a cap past the largest purse is named: " + config.m_aRows[4].Problem());
		ctx.True(config.m_aRows[5].Problem().Contains("price"), "a negative price is named: " + config.m_aRows[5].Problem());

		int removed = config.Sanitize();
		ctx.Equal(7, removed, "seven rows are refused: cap 0, negative refill, no buyer, no good, a huge cap, a negative price, a repeat");
		ctx.Equal(1, config.m_aRows.Count(), "one row survives");
		ctx.True(config.Find("sp:b60", PLANK) != null, "the valid row is kept");
		LF_BuyerDemandRow kept = config.Find("sp:b60", PLANK);
		if (kept)
			ctx.Equal(10, kept.m_iCap, "and a repeat keeps its first row");
		ctx.True(config.Find(BUYER, PLANK) == null, "a cap of 0 is not loaded");
		ctx.True(config.Find("", PLANK) == null, "an empty buyer finds nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! The core ships no rows: the file resolves and holds none.
	protected void RunShipped(LF_TestContext ctx)
	{
		LF_BuyerDemandConfig.Reload();
		LF_BuyerDemandConfig shipped = LF_BuyerDemandConfig.Get();
		ctx.True(shipped != null, "the core buyer demand config resolves");
		if (shipped)
			ctx.Equal(0, shipped.m_aRows.Count(), "the core ships no buyer rows");

		LF_BuyerDemandConfig.Reload();
	}
}
