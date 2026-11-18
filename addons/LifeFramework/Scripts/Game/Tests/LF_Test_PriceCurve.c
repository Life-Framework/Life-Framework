// red-proof: observed 2026-11-18 with `tools\cli test --no-build --red LF_Test_PriceCurve`: RED-OK, all 1 listed test(s) fail with the hook on, first `a huge sale stops at the floor (expected 50, got 100)`. Green with `--only`: passed=1 failed=0.
//! Market price curve (#2844): a sale drops the price by dropPerUnitPct of the base and never below the
//! floor, a bulk sale is the sum of single sales, a drop of 0 keeps the price fixed, recovery closes
//! recoverPctPerHour of the gap and never passes the base, a gap longer than maxRecoveryHours is clamped,
//! and a bad good is refused at load. The core file ships off with three valid goods.
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
	//! GOOD: base 100, floor 50, ceiling 150, drop 10 percent (10 a unit), recovery 20 percent an hour.
	//! SLOW: base 100, floor 50, ceiling 150, no drop, recovery 1 percent an hour. Cap 24 hours.
	protected LF_MarketConfig MakeConfig()
	{
		LF_MarketConfig config = new LF_MarketConfig();
		config.m_bEnabled = true;
		config.m_iMaxRecoveryHours = 24;
		config.m_aGoods = {};
		int drop = 10;
		// RedProof: a curve that never drops, so every assertion about a falling price fails (`--red`).
		if (RedProof())
			drop = 0;

		config.m_aGoods.Insert(LF_MarketGoodDef.Make(GOOD, 100, 50, 150, drop, 20));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(SLOW, 100, 50, 150, 0, 1));
		return config;
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		LF_MarketConfig config = MakeConfig();
		ctx.EqualStr("", config.m_aGoods[0].Problem(), "the test curve is valid");
		ctx.Equal(0, config.Sanitize(), "a valid table loses no row");

		RunDrop(ctx, config);
		RunBulk(ctx, config);
		RunFixed(ctx, config);
		RunRecovery(ctx, config);
		RunClamp(ctx, config);
		RunRefused(ctx);
		RunShipped(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDrop(LF_TestContext ctx, LF_MarketConfig config)
	{
		LF_MarketGoodDef good = config.FindGood(GOOD);
		ctx.True(good != null, "the good has a row");
		if (!good)
			return;

		ctx.Equal(10, good.DropPerUnit(), "a unit takes 10 percent of the base 100 off the price");
		ctx.Equal(100, good.NextPrice(100), "an untouched good fetches its base");
		ctx.Equal(90, good.PriceAfterSale(100, 1), "one unit sold drops the price by 10");
		ctx.Equal(70, good.PriceAfterSale(100, 3), "three units drop it by 30");
		ctx.Equal(50, good.PriceAfterSale(100, 5), "five units reach the floor");
		ctx.Equal(50, good.PriceAfterSale(100, 6), "a sixth unit never goes below the floor");
		ctx.Equal(50, good.PriceAfterSale(100, 5000), "a huge sale stops at the floor");
		ctx.Equal(150, good.NextPrice(200), "a price above the ceiling fetches the ceiling");
		ctx.Equal(100, good.PriceAfterSale(100, 0), "no units leave the price alone");

		// A drop that does not divide the span lands on the floor, not below it.
		ctx.Equal(50, LF_PriceCurve.PriceAfterSale(100, 4, 15, 50, 150), "a 15 drop from 100 lands on the floor");
		// A small base with a percent still moves by at least 1.
		ctx.Equal(1, LF_PriceCurve.DropPerUnit(5, 3), "a tiny base still drops by one unit");
	}

	//------------------------------------------------------------------------------------------------
	//! The reference: sell one unit at a time.
	protected int SingleTotal(LF_MarketGoodDef good, int start, int units, out int endPrice)
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
	protected void RunBulk(LF_TestContext ctx, LF_MarketConfig config)
	{
		LF_MarketGoodDef good = config.FindGood(GOOD);
		if (!good)
			return;

		array<int> starts = {150, 100, 73, 55, 50, 200};
		foreach (int start : starts)
		{
			for (int units = 0; units <= 30; units++)
			{
				int endSingles;
				int singles = SingleTotal(good, start, units, endSingles);
				if (good.BulkTotal(start, units) != singles)
				{
					ctx.Fail(string.Format("bulk of %1 from %2 paid %3, single sales paid %4", units, start, good.BulkTotal(start, units), singles));
					return;
				}

				if (good.PriceAfterSale(start, units) != endSingles)
				{
					ctx.Fail(string.Format("price after %1 units from %2 was %3, single sales end at %4", units, start, good.PriceAfterSale(start, units), endSingles));
					return;
				}
			}
		}

		ctx.Pass("a bulk sale of 0..30 units equals the sum of single sales from six starting prices");
		ctx.Equal(100 + 90 + 80, good.BulkTotal(100, 3), "three units pay 100 + 90 + 80");
		ctx.Equal(100 + 90 + 80 + 70 + 60 + 50 + 50, good.BulkTotal(100, 7), "units past the floor pay the floor");
		ctx.Equal(0, good.BulkTotal(100, 0), "no units pay nothing");
		ctx.Equal(0, good.BulkTotal(100, -4), "a negative count pays nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunFixed(LF_TestContext ctx, LF_MarketConfig config)
	{
		LF_MarketGoodDef slow = config.FindGood(SLOW);
		if (!slow)
			return;

		ctx.Equal(0, slow.DropPerUnit(), "a drop of 0 takes nothing off");
		ctx.Equal(100, slow.PriceAfterSale(100, 40), "a drop of 0 keeps today's fixed price after 40 units");
		ctx.Equal(700, slow.BulkTotal(100, 7), "a drop of 0 pays units times the price");
		ctx.Equal(0, LF_PriceCurve.DropPerUnit(100, 0), "0 percent is no drop");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunRecovery(LF_TestContext ctx, LF_MarketConfig config)
	{
		ctx.Equal(50, config.Recover(GOOD, 50, 0), "no time passed, no recovery");
		ctx.Equal(50, config.Recover(GOOD, 50, HOUR - 1), "under an hour is not a recovery hour");
		ctx.Equal(60, config.Recover(GOOD, 50, HOUR), "one hour closes 20 percent of the gap 50");
		ctx.Equal(68, config.Recover(GOOD, 50, 2 * HOUR), "two hours close 20 percent of what is left");
		ctx.Equal(68, config.Recover(GOOD, 50, 2 * HOUR + HOUR - 1), "a part hour is not paid");
		ctx.Equal(50, config.Recover(GOOD, 50, -5 * HOUR), "a clock that moved back recovers nothing");

		// Never past the base, from below or above.
		int price = 50;
		for (int h = 1; h <= 24; h++)
		{
			int next = config.Recover(GOOD, 50, h * HOUR);
			if (next > 100 || next < price)
			{
				ctx.Fail(string.Format("recovery hour %1 went from %2 to %3 (base 100)", h, price, next));
				return;
			}

			price = next;
		}

		ctx.Equal(100, price, "24 hours bring the price back to the base");
		ctx.Equal(100, config.Recover(GOOD, 100, 10 * HOUR), "a price at the base stays");
		ctx.Equal(140, config.Recover(GOOD, 150, HOUR), "a price above the base eases down by the percent of the gap");
		ctx.Equal(100, config.Recover(GOOD, 150, 24 * HOUR), "from above it settles on the base, not past it");
		ctx.Equal(95, config.Recover(SLOW, 94, HOUR), "a small gap still closes by at least one unit");
		ctx.Equal(77, config.Recover("{00000000AAAAAAAA}Prefabs/NoSuch.et", 77, 9 * HOUR), "a good with no curve keeps its price");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunClamp(LF_TestContext ctx, LF_MarketConfig config)
	{
		// SLOW recovers 1 unit an hour, so the cap of 24 hours shows in the price.
		ctx.Equal(74, config.Recover(SLOW, 50, 24 * HOUR), "24 hours at 1 unit an hour");
		ctx.Equal(74, config.Recover(SLOW, 50, 25 * HOUR), "a gap past the cap counts as the cap");
		ctx.Equal(74, config.Recover(SLOW, 50, 100000 * HOUR), "a very long offline gap is clamped, no overflow");
		ctx.Equal(74, config.Recover(SLOW, 50, 2000000000), "a gap near the int limit is clamped");
		ctx.Equal(24, LF_PriceCurve.RecoveryHours(900 * HOUR, 24), "the cap is 24 recovery hours");
		ctx.Equal(3, LF_PriceCurve.RecoveryHours(3 * HOUR + 59, 24), "whole hours inside the cap");
		ctx.False(LF_PriceCurve.IsClamped(24 * HOUR, 24), "exactly the cap is not clamped");
		ctx.True(LF_PriceCurve.IsClamped(25 * HOUR, 24), "one hour past the cap is clamped");
		ctx.Equal(0, LF_PriceCurve.RecoveryHours(10 * HOUR, 0), "a cap of 0 hours recovers nothing");
		ctx.Equal(1800000480 / HOUR, LF_PriceCurve.RecoveryHours(1800000480, 1000000), "a unix-size gap keeps its low bits");
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
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(floorHigh, 100, 120, 80, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(baseLow, 10, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(baseHigh, 200, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(keep, 100, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make(keep, 100, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make("", 100, 50, 150, 5, 5));
		config.m_aGoods.Insert(LF_MarketGoodDef.Make("{23341E2840F5ACB4}Prefabs/Tools/Pickaxe.et", 100, 50, 150, 150, 5));

		ctx.True(config.m_aGoods[0].Problem().Contains("floor 120 is above ceiling 80"), "a floor above the ceiling is named: " + config.m_aGoods[0].Problem());
		ctx.True(config.m_aGoods[1].Problem().Contains("outside"), "a base below the floor is named");
		ctx.True(config.m_aGoods[2].Problem().Contains("outside"), "a base above the ceiling is named");

		int removed = config.Sanitize();
		ctx.Equal(6, removed, "six rows are refused: floor above ceiling, base low, base high, a repeat, no good, a bad percent");
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
