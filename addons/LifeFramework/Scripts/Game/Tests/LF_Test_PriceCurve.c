// red-proof: #3028 observed 2026-11-18 `toolscli test --no-build --red LF_Test_PriceCurve,LF_Test_MarketTrader,LF_Test_MarketPersist`: RED PROOF OK (all 3 listed test(s) fail with the hook on), first `a huge sale still stops at the floor (expected 100, got 500)`. RedProof() makes every curve drop 0, so the cheap-apple cases (ten 5-cash apples must end at 4; the old whole-cash curve ended them at 3) fail. Green with `--only`: passed.
//! Market price curve (#2844, #3028): prices run in hundredths of a cash unit, so a 2 percent drop on a
//! 5-cash good takes 0.10 off per unit instead of a whole 1. A sale drops the price by dropPerUnitPct of
//! the base and never below the floor, a bulk sale is the exact sum of single sales rounded to whole cash
//! once, a drop of 0 keeps the price fixed, recovery closes recoverPctPerHour of the gap and never passes
//! the base, a gap longer than maxRecoveryHours is clamped, big sums stay exact or stop at INT_MAX, and a
//! bad good is refused at load. The core file ships off with three valid goods.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_PriceCurve : LF_Test
{
	static const ResourceName GOOD = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
	static const ResourceName SLOW = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
	static const int HOUR = 3600;

	override string GetName()
	{
		return "market/price-curve";
	}

	//------------------------------------------------------------------------------------------------
	//! A whole price in hundredths, to keep the cases below readable.
	protected int F(int cash)
	{
		return LF_PriceCurve.ToFine(cash);
	}

	//------------------------------------------------------------------------------------------------
	//! The drop percent the cases use, or 0 under the red-proof hook (`--red`).
	protected int Pct(int pct)
	{
		if (RedProof())
			return 0;

		return pct;
	}

	//------------------------------------------------------------------------------------------------
	//! GOOD: base 100, floor 50, ceiling 150, drop 10 percent (10 cash a unit), recovery 20 percent an hour.
	//! SLOW: base 100, floor 50, ceiling 150, no drop, recovery 1 percent an hour. Cap 24 hours.
	protected LF_MarketConfig MakeConfig()
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = true;
		config.m_iMaxRecoveryHours = 24;
		config.m_aGoods = {};
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(GOOD, 100, 50, 150, Pct(10), 20));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(SLOW, 100, 50, 150, 0, 1));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MarketConfig config = MakeConfig();
		ctx.EqualStr("", config.m_aGoods[0].Problem(), "the test curve is valid");
		ctx.Equal(0, config.Sanitize(), "a valid table loses no row");

		RunUnits(ctx);
		RunDrop(ctx, config);
		RunCheap(ctx);
		RunBulk(ctx, config);
		RunBig(ctx);
		RunFixed(ctx, config);
		RunRecovery(ctx, config);
		RunClamp(ctx, config);
		RunRefused(ctx);
		RunShipped(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunUnits(LF_TestContext ctx)
	{
		ctx.Equal(500, LF_PriceCurve.ToFine(5), "5 cash is 500 hundredths");
		ctx.Equal(0, LF_PriceCurve.ToFine(-3), "a negative price is 0");
		ctx.Equal(LF_PriceCurve.MAX_CASH * LF_PriceCurve.PRICE_SCALE, LF_PriceCurve.ToFine(2000000000), "a huge price stops at the largest one");
		ctx.Equal(5, LF_PriceCurve.ToCash(500), "500 hundredths is 5 cash");
		ctx.Equal(5, LF_PriceCurve.ToCash(450), "half a cash rounds up");
		ctx.Equal(4, LF_PriceCurve.ToCash(449), "just under half rounds down");
		ctx.Equal(0, LF_PriceCurve.ToCash(-40), "a negative fine price is 0 cash");
		ctx.Equal(LF_PriceCurve.MAX_CASH, LF_PriceCurve.ToCash(2000000000), "a huge fine price stops at the largest cash price");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDrop(LF_TestContext ctx, LF_MarketConfig config)
	{
		LF_MarketGoodDef good = config.FindGood(GOOD);
		ctx.True(good != null, "the good has a row");
		if (!good)
			return;

		ctx.Equal(F(10), good.DropPerUnit(), "a unit takes 10 percent of the base 100 off the price");
		ctx.Equal(F(100), good.NextPrice(F(100)), "an untouched good fetches its base");
		ctx.Equal(F(90), good.PriceAfterSale(F(100), 1), "one unit sold drops the price by 10");
		ctx.Equal(F(70), good.PriceAfterSale(F(100), 3), "three units drop it by 30");
		ctx.Equal(F(50), good.PriceAfterSale(F(100), 5), "five units reach the floor");
		ctx.Equal(F(50), good.PriceAfterSale(F(100), 6), "a sixth unit never goes below the floor");
		ctx.Equal(F(50), good.PriceAfterSale(F(100), 5000), "a huge sale stops at the floor");
		ctx.Equal(F(150), good.NextPrice(F(200)), "a price above the ceiling fetches the ceiling");
		ctx.Equal(F(100), good.PriceAfterSale(F(100), 0), "no units leave the price alone");

		// A drop that does not divide the span lands on the floor, not below it.
		ctx.Equal(F(50), LF_PriceCurve.PriceAfterSale(F(100), 4, F(15), F(50), F(150)), "a 15 drop from 100 lands on the floor");
	}

	//------------------------------------------------------------------------------------------------
	//! Cheap goods (#3028): 2 percent of a 5-cash apple is 0.10 a unit, not a whole 1.
	protected void RunCheap(LF_TestContext ctx)
	{
		LF_MarketGoodDef apple = LF_MarketGoodDef.Make(GOOD, 5, 3, 8, Pct(2), 10);
		ctx.EqualStr("", apple.Problem(), "the cheap apple row is valid");
		ctx.Equal(10, apple.DropPerUnit(), "2 percent of a 5-cash base is 10 hundredths a unit");
		ctx.Equal(15, LF_PriceCurve.DropPerUnit(5, 3), "3 percent of a 5-cash base is 15 hundredths, not a whole 1");
		ctx.Equal(490, apple.PriceAfterSale(F(5), 1), "one apple takes 0.10 off, not 1");
		ctx.Equal(F(4), apple.PriceAfterSale(F(5), 10), "ten apples at 5 leave the price at 4, not the old floor of 3");
		ctx.Equal(4, LF_PriceCurve.ToCash(apple.PriceAfterSale(F(5), 10)), "the board shows 4 after ten apples");
		ctx.Equal(F(3), apple.PriceAfterSale(F(5), 20), "twenty apples reach the floor 3");
		ctx.Equal(46, apple.BulkTotal(F(5), 10), "ten apples pay 5.00 + 4.90 + ... + 4.10 = 45.50, rounded to 46");
		ctx.Equal(5, apple.BulkTotal(F(5), 1), "one apple at the base pays 5");

		// Half the price takes about 25 units, as the card asks (floor lowered so it does not stop it).
		LF_MarketGoodDef deep = LF_MarketGoodDef.Make(GOOD, 5, 1, 8, Pct(2), 10);
		ctx.Equal(250, deep.PriceAfterSale(F(5), 25), "25 apples at 2 percent take the price to half (2.50)");
		ctx.Equal(F(1), deep.PriceAfterSale(F(5), 400), "a huge sale still stops at the floor");

		// Recovery of the same good moves in hundredths too: 10 percent of a 0.60 gap is 0.06.
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_iMaxRecoveryHours = 24;
		config.m_aGoods = {};
		config.m_aGoods.Insert(apple);
		ctx.Equal(446, config.Recover(GOOD, 440, HOUR), "an hour closes 10 percent of the 0.60 gap");
		ctx.Equal(500, config.Recover(GOOD, 499, HOUR), "a 0.01 gap still closes");
		ctx.Equal(500, config.Recover(GOOD, 500, 5 * HOUR), "a price at the base stays");
	}

	//------------------------------------------------------------------------------------------------
	//! The reference: sell one unit at a time, in hundredths.
	protected int SingleFine(LF_MarketGoodDef good, int start, int units, out int endPrice)
	{
		int price = good.NextPrice(start);
		int total = 0;
		for (int i = 0; i < units; i++)
		{
			total += price;
			price = good.PriceAfterSale(price, 1);
		}

		endPrice = price;
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! A bulk sale of 0..60 units from each start equals the sum of single sales in hundredths, rounded
	//! to whole cash once; the price after it equals the price after the single sales.
	protected bool CheckBulk(LF_TestContext ctx, LF_MarketGoodDef good, array<int> starts)
	{
		foreach (int start : starts)
		{
			for (int units = 0; units <= 60; units++)
			{
				int endSingles;
				int fine = SingleFine(good, start, units, endSingles);
				int expected = (fine + 50) / 100;
				if (good.BulkTotal(start, units) != expected)
				{
					ctx.Fail(string.Format("bulk of %1 from %2 paid %3, single sales sum to %4 hundredths (%5 cash)", units, start, good.BulkTotal(start, units), fine, expected));
					return false;
				}

				if (good.PriceAfterSale(start, units) != endSingles)
				{
					ctx.Fail(string.Format("price after %1 units from %2 was %3, single sales end at %4", units, start, good.PriceAfterSale(start, units), endSingles));
					return false;
				}
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void RunBulk(LF_TestContext ctx, LF_MarketConfig config)
	{
		LF_MarketGoodDef good = config.FindGood(GOOD);
		if (!good)
			return;

		array<int> starts = {F(150), F(100), F(73), F(55), F(50), F(200), 7333, 5049};
		bool whole = CheckBulk(ctx, good, starts);
		LF_MarketGoodDef cheap = LF_MarketGoodDef.Make(GOOD, 5, 1, 8, Pct(2), 10);
		array<int> cheapStarts = {F(8), F(5), 433, 301, 300, 101, 100, F(20)};
		bool fine = CheckBulk(ctx, cheap, cheapStarts);
		if (whole && fine)
			ctx.Pass("a bulk sale of 0..60 units equals the sum of single sales, rounded once, from whole and part-cash starting prices");

		ctx.Equal(100 + 90 + 80, good.BulkTotal(F(100), 3), "three units pay 100 + 90 + 80");
		ctx.Equal(100 + 90 + 80 + 70 + 60 + 50 + 50, good.BulkTotal(F(100), 7), "units past the floor pay the floor");
		ctx.Equal(0, good.BulkTotal(F(100), 0), "no units pay nothing");
		ctx.Equal(0, good.BulkTotal(F(100), -4), "a negative count pays nothing");
	}

	//------------------------------------------------------------------------------------------------
	//! Large ints stay exact (#2787): sums past 2^24 are not float-rounded, and ones past 2^31 stop at INT_MAX.
	protected void RunBig(LF_TestContext ctx)
	{
		int top = LF_PriceCurve.MAX_CASH * LF_PriceCurve.PRICE_SCALE;
		ctx.Equal(1500000000, LF_PriceCurve.BulkTotal(F(5000000), 300, 0, 0, top), "300 units of 5,000,000 cash pay exactly 1,500,000,000");
		ctx.Equal(2000000000, LF_PriceCurve.BulkTotal(F(5000000), 400, 0, 0, top), "400 units of 5,000,000 cash pay exactly 2,000,000,000");
		ctx.Equal(LF_PriceCurve.INT_MAX, LF_PriceCurve.BulkTotal(top, LF_PriceCurve.MAX_BULK_UNITS, 0, 0, top), "a sale worth more than an int stops at INT_MAX");
		ctx.Equal(LF_PriceCurve.INT_MAX, LF_PriceCurve.BulkTotal(top, LF_PriceCurve.MAX_BULK_UNITS, 1000, 0, top), "so does a falling one");

		// 1000 units from 1,000,000 cash, 100 cash off each: 1000 * 1,000,000 - 100 * (1000 * 999 / 2).
		ctx.Equal(950050000, LF_PriceCurve.BulkTotal(F(1000000), 1000, F(100), 0, F(1000000)), "a big falling run pays exactly 950,050,000");
		ctx.Equal(F(1000000) - 1000 * F(100), LF_PriceCurve.PriceAfterSale(F(1000000), 1000, F(100), 0, F(1000000)), "and the price after it is exact");

		// A part-cash price keeps its hundredths: 100 units of 12345.67 cash.
		ctx.Equal(1234567, LF_PriceCurve.BulkTotal(1234567, 100, 0, 0, top), "100 units of 12345.67 cash pay 1,234,567");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFixed(LF_TestContext ctx, LF_MarketConfig config)
	{
		LF_MarketGoodDef slow = config.FindGood(SLOW);
		if (!slow)
			return;

		ctx.Equal(0, slow.DropPerUnit(), "a drop of 0 takes nothing off");
		ctx.Equal(F(100), slow.PriceAfterSale(F(100), 40), "a drop of 0 keeps today's fixed price after 40 units");
		ctx.Equal(700, slow.BulkTotal(F(100), 7), "a drop of 0 pays units times the price");
		ctx.Equal(0, LF_PriceCurve.DropPerUnit(100, 0), "0 percent is no drop");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRecovery(LF_TestContext ctx, LF_MarketConfig config)
	{
		ctx.Equal(F(50), config.Recover(GOOD, F(50), 0), "no time passed, no recovery");
		ctx.Equal(F(50), config.Recover(GOOD, F(50), HOUR - 1), "under an hour is not a recovery hour");
		ctx.Equal(F(60), config.Recover(GOOD, F(50), HOUR), "one hour closes 20 percent of the gap 50");
		ctx.Equal(F(68), config.Recover(GOOD, F(50), 2 * HOUR), "two hours close 20 percent of what is left");
		ctx.Equal(F(68), config.Recover(GOOD, F(50), 2 * HOUR + HOUR - 1), "a part hour is not paid");
		ctx.Equal(F(50), config.Recover(GOOD, F(50), -5 * HOUR), "a clock that moved back recovers nothing");

		// Never past the base, from below or above.
		int price = F(50);
		for (int h = 1; h <= 24; h++)
		{
			int next = config.Recover(GOOD, F(50), h * HOUR);
			if (next > F(100) || next < price)
			{
				ctx.Fail(string.Format("recovery hour %1 went from %2 to %3 (base %4)", h, price, next, F(100)));
				return;
			}

			price = next;
		}

		ctx.Equal(100, LF_PriceCurve.ToCash(price), "24 hours bring the price back to the base, to the cent");
		ctx.True(price > F(100) - 50, "within half a cash of the base after 24 hours");
		ctx.Equal(F(100), config.Recover(GOOD, F(100), 10 * HOUR), "a price at the base stays");
		ctx.Equal(F(140), config.Recover(GOOD, F(150), HOUR), "a price above the base eases down by the percent of the gap");
		int settled = config.Recover(GOOD, F(150), 24 * HOUR);
		ctx.True(settled >= F(100) && settled < F(100) + 50, "from above it settles toward the base, never past it");
		ctx.Equal(10000, config.Recover(SLOW, 9999, HOUR), "a gap of one hundredth still closes");
		ctx.Equal(9406, config.Recover(SLOW, 9400, HOUR), "1 percent of a gap of 6.00 cash is 0.06");
		ctx.Equal(77, config.Recover("{00000000AAAAAAAA}Prefabs/NoSuch.et", 77, 9 * HOUR), "a good with no curve keeps its price");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunClamp(LF_TestContext ctx, LF_MarketConfig config)
	{
		// SLOW recovers 1 percent of the gap an hour, so the cap of 24 hours shows in the price.
		int capped = config.Recover(SLOW, F(50), 24 * HOUR);
		ctx.True(capped > F(50) + 1000 && capped < F(50) + 1200, "24 hours at 1 percent of the gap close about a fifth of it");
		ctx.Equal(capped, config.Recover(SLOW, F(50), 25 * HOUR), "a gap past the cap counts as the cap");
		ctx.Equal(capped, config.Recover(SLOW, F(50), 100000 * HOUR), "a very long offline gap is clamped, no overflow");
		ctx.Equal(capped, config.Recover(SLOW, F(50), 2000000000), "a gap near the int limit is clamped");
		ctx.Equal(24, LF_PriceCurve.RecoveryHours(900 * HOUR, 24), "the cap is 24 recovery hours");
		ctx.Equal(3, LF_PriceCurve.RecoveryHours(3 * HOUR + 59, 24), "whole hours inside the cap");
		ctx.False(LF_PriceCurve.IsClamped(24 * HOUR, 24), "exactly the cap is not clamped");
		ctx.True(LF_PriceCurve.IsClamped(25 * HOUR, 24), "one hour past the cap is clamped");
		ctx.Equal(0, LF_PriceCurve.RecoveryHours(10 * HOUR, 0), "a cap of 0 hours recovers nothing");
		ctx.Equal(1800000480 / HOUR, LF_PriceCurve.RecoveryHours(1800000480, 1000000), "a unix-size gap keeps its low bits");

		// A top-of-range gap times 100 percent would overflow a plain size * percent.
		int top = LF_PriceCurve.MAX_CASH * LF_PriceCurve.PRICE_SCALE;
		ctx.Equal(top, LF_PriceCurve.Recover(0, top, 100, HOUR, 24, 0, top), "a 100 percent recovery closes the biggest gap in one step, no overflow");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRefused(LF_TestContext ctx)
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_aGoods = {};
		ResourceName keep = "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et";
		ResourceName floorHigh = "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et";
		ResourceName baseLow = "{ACC9A10F0F20F5C4}Prefabs/Resources/Mining/GoldBar.et";
		ResourceName baseHigh = "{8F81BD0BF343CF49}Prefabs/Resources/Mining/GoldNugget.et";
		ResourceName tooDear = "{00000000BBBBBBBB}Prefabs/NoSuchDear.et";
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(floorHigh, 100, 120, 80, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(baseLow, 10, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(baseHigh, 200, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(keep, 100, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(keep, 100, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make("", 100, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", 100, 50, 150, 150, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(tooDear, 100, 50, LF_PriceCurve.MAX_CASH + 1, 5, 5));

		ctx.True(config.m_aGoods[0].Problem().Contains("floor 120 is above ceiling 80"), "a floor above the ceiling is named: " + config.m_aGoods[0].Problem());
		ctx.True(config.m_aGoods[1].Problem().Contains("outside"), "a base below the floor is named");
		ctx.True(config.m_aGoods[2].Problem().Contains("outside"), "a base above the ceiling is named");
		ctx.True(config.m_aGoods[7].Problem().Contains("above the largest price"), "a ceiling past the largest price is named: " + config.m_aGoods[7].Problem());

		int removed = config.Sanitize();
		ctx.Equal(7, removed, "seven rows are refused: floor above ceiling, base low, base high, a repeat, no good, a bad percent, a ceiling past the largest price");
		ctx.Equal(1, config.m_aGoods.Count(), "one good survives");
		ctx.True(config.FindGood(keep) != null, "the valid good is kept");
		ctx.True(config.FindGood(floorHigh) == null, "a floor above the ceiling is not loaded");
		ctx.True(config.FindGood(baseLow) == null, "a base outside the range is not loaded");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunShipped(LF_TestContext ctx)
	{
		LF_MarketConfig.Reload();
		LF_MarketConfig shipped = LF_MarketConfig.Get();
		ctx.True(shipped != null, "the core market config resolves");
		if (shipped)
		{
			ctx.False(shipped.m_bEnabled, "the market is off in the core");
			ctx.Equal(3, shipped.m_aGoods.Count(), "the core ships three goods and refuses none");
			ctx.Equal(0, shipped.Sanitize(), "the shipped rows are all valid");
			ctx.True(shipped.m_iMaxRecoveryHours > 0, "the shipped config has a recovery cap");
		}

		LF_MarketConfig.Reload();
	}
}
