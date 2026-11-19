// red-proof: observed 2026-11-19 with `tools\cli test --no-build --red LF_Test_FuelStockRules`: RED PROOF OK (all 1 listed test(s) fail with the hook on), first `no room left (expected 0, got 20)`. RedProof() skips ApplyDelivery and ApplySale, so the stock assertions after them fail; green with `--only`: passed=1 failed=0.
//! Fuel station stock rules (#3327): an empty station refuses; a fill is clamped by stock, then by money, and
//! charges only for the litres that moved; a delivery adds up to the room left and pays for what arrived; a
//! sub-unit price is carried, never rounded away; price 0 is the kill switch; the shipped table is usable.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_FuelStockRules : LF_Test
{
	override string GetName()
	{
		return "fuel/fuel-stock-rules";
	}

	//------------------------------------------------------------------------------------------------
	override void Run(LF_TestContext ctx)
	{
		RunEmpty(ctx);
		RunSale(ctx);
		RunDelivery(ctx);
		RunCarry(ctx);
		RunKillSwitch(ctx);
		RunConfig(ctx);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunEmpty(LF_TestContext ctx)
	{
		LF_FuelStock station = LF_FuelStock.Make(100, 0);
		LF_FuelPlan plan = LF_FuelStockRules.PlanSale(station, 10, 1000, 200, 0, 1);
		ctx.Equal(LF_EFuelResult.EMPTY, plan.m_eResult, "an empty station refuses a sale");
		ctx.Equal(0, plan.m_iLitres, "an empty station moves no litres");
		ctx.Equal(0, plan.m_iCash, "an empty station charges nothing");

		LF_FuelStock low = LF_FuelStock.Make(100, 4);
		ctx.Equal(LF_EFuelResult.EMPTY, LF_FuelStockRules.PlanSale(low, 2, 1000, 200, 0, 5).m_eResult, "stock under the minimum to sell refuses");

		ctx.Equal(LF_EFuelResult.BAD_INPUT, LF_FuelStockRules.PlanSale(null, 10, 1000, 200, 0, 1).m_eResult, "no station refuses");
		ctx.Equal(LF_EFuelResult.BAD_INPUT, LF_FuelStockRules.PlanSale(station, 0, 1000, 200, 0, 1).m_eResult, "zero litres wanted refuses");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunSale(LF_TestContext ctx)
	{
		// 2.50 a litre (250 hundredths).
		LF_FuelStock station = LF_FuelStock.Make(100, 30);

		LF_FuelPlan plain = LF_FuelStockRules.PlanSale(station, 10, 1000, 250, 0, 1);
		ctx.Equal(LF_EFuelResult.OK, plain.m_eResult, "a sale inside stock and money goes ahead");
		ctx.Equal(10, plain.m_iLitres, "ten wanted, ten planned");
		ctx.Equal(25, plain.m_iCash, "ten litres at 2.50 cost 25");
		ctx.Equal(30, station.m_iLitres, "planning does not move stock");

		LF_FuelPlan byStock = LF_FuelStockRules.PlanSale(station, 50, 100000, 250, 0, 1);
		ctx.Equal(30, byStock.m_iLitres, "a fill is clamped by stock first");
		ctx.Equal(75, byStock.m_iCash, "and charges only for the 30 litres");

		LF_FuelPlan byMoney = LF_FuelStockRules.PlanSale(station, 50, 10, 250, 0, 1);
		ctx.Equal(LF_EFuelResult.OK, byMoney.m_eResult, "a fill the buyer cannot fully pay still sells what they can");
		ctx.Equal(4, byMoney.m_iLitres, "then by money: 10 cash buys 4 litres at 2.50");
		ctx.Equal(10, byMoney.m_iCash, "the charge is exactly what moved");

		LF_FuelPlan broke = LF_FuelStockRules.PlanSale(station, 5, 1, 250, 0, 1);
		ctx.Equal(LF_EFuelResult.NO_MONEY, broke.m_eResult, "cash that cannot buy a litre refuses");
		ctx.Equal(0, broke.m_iCash, "a refused sale charges nothing");

		// Red-proof hook: with --red the apply is skipped, so the stock assertions after it must fail.
		if (!RedProof())
			LF_FuelStockRules.ApplySale(station, byMoney);
		ctx.Equal(26, station.m_iLitres, "applying a sale removes exactly the litres that moved");
		LF_FuelStockRules.ApplySale(station, byStock);
		ctx.Equal(0, station.m_iLitres, "stock never goes below zero");
		LF_FuelStockRules.ApplySale(station, broke);
		ctx.Equal(0, station.m_iLitres, "a refused plan moves nothing");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunDelivery(LF_TestContext ctx)
	{
		// Pays 1.20 a litre (120 hundredths); room for 20 more.
		LF_FuelStock station = LF_FuelStock.Make(100, 80);

		LF_FuelPlan over = LF_FuelStockRules.PlanDelivery(station, 50, 120, 0);
		ctx.Equal(LF_EFuelResult.OK, over.m_eResult, "a delivery past the cap still lands what fits");
		ctx.Equal(20, over.m_iLitres, "a delivery past the cap adds only the room left");
		ctx.Equal(24, over.m_iCash, "and pays only for the room: 20 litres at 1.20");

		if (!RedProof())
			LF_FuelStockRules.ApplyDelivery(station, over);
		ctx.Equal(100, station.m_iLitres, "the station is now at capacity");
		ctx.Equal(0, station.Room(), "no room left");

		LF_FuelPlan full = LF_FuelStockRules.PlanDelivery(station, 10, 120, 0);
		ctx.Equal(LF_EFuelResult.FULL, full.m_eResult, "a full station refuses a delivery");
		ctx.Equal(0, full.m_iCash, "a refused delivery pays nothing");

		LF_FuelStock part = LF_FuelStock.Make(100, 10);
		LF_FuelPlan fits = LF_FuelStockRules.PlanDelivery(part, 30, 120, 0);
		ctx.Equal(30, fits.m_iLitres, "a delivery that fits lands whole");
		ctx.Equal(36, fits.m_iCash, "30 litres at 1.20 pay 36");
		ctx.Equal(LF_EFuelResult.BAD_INPUT, LF_FuelStockRules.PlanDelivery(part, 0, 120, 0).m_eResult, "an empty delivery refuses");
		ctx.Equal(LF_EFuelResult.BAD_INPUT, LF_FuelStockRules.PlanDelivery(null, 5, 120, 0).m_eResult, "no station refuses");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunCarry(LF_TestContext ctx)
	{
		// 0.30 a litre: three litres cost 0.90, so one cash unit is due only after the fourth.
		LF_FuelStock station = LF_FuelStock.Make(1000, 1000);
		LF_FuelChargeLedger ledger = new LF_FuelChargeLedger();
		int paid;
		int litres;
		for (int i = 0; i < 10; i++)
		{
			LF_FuelPlan step = LF_FuelStockRules.PlanSale(station, 1, 1000, 30, ledger.Carry("buyer"), 1);
			ledger.Commit("buyer", step);
			paid += step.m_iCash;
			litres += step.m_iLitres;
		}

		ctx.Equal(10, litres, "ten single-litre sales move ten litres");
		ctx.Equal(3, paid, "ten litres at 0.30 cost exactly 3, not 0 and not 10");
		ctx.Equal(0, ledger.Carry("buyer"), "the carry has settled to nothing");

		LF_FuelPlan once = LF_FuelStockRules.PlanSale(station, 1, 1000, 30, 0, 1);
		ctx.Equal(0, once.m_iCash, "one litre at 0.30 charges no whole cash");
		ctx.Equal(30, once.m_iCarryAfter, "but carries 0.30");
		ledger.Commit("other", once);
		ctx.Equal(30, ledger.Carry("other"), "the ledger holds the carry per key");
		ctx.Equal(0, ledger.Carry("buyer"), "another key is untouched");
		ledger.Clear();
		ctx.Equal(0, ledger.Carry("other"), "a cleared ledger holds nothing");

		LF_FuelStock empty = LF_FuelStock.Make(100, 0);
		LF_FuelPlan refused = LF_FuelStockRules.PlanSale(empty, 1, 1000, 30, 70, 1);
		ledger.Commit("keep", refused);
		ctx.Equal(0, ledger.Carry("keep"), "a refused plan stores no carry");

		// The delivery payout carries the same way: three litres at 0.30 pay nothing yet.
		LF_FuelPlan drop = LF_FuelStockRules.PlanDelivery(LF_FuelStock.Make(100, 0), 3, 30, 0);
		ctx.Equal(0, drop.m_iCash, "a small delivery at a sub-unit rate pays no whole cash");
		ctx.Equal(90, drop.m_iCarryAfter, "and carries the 0.90 owed");
		LF_FuelPlan next = LF_FuelStockRules.PlanDelivery(LF_FuelStock.Make(100, 0), 1, 30, drop.m_iCarryAfter);
		ctx.Equal(1, next.m_iCash, "the carried 0.90 and the next 0.30 pay one cash unit");
		ctx.Equal(20, next.m_iCarryAfter, "leaving 0.20");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunKillSwitch(LF_TestContext ctx)
	{
		LF_FuelStock station = LF_FuelStock.Make(100, 100);
		LF_FuelPlan plan = LF_FuelStockRules.PlanSale(station, 10, 1000, 0, 0, 1);
		ctx.Equal(LF_EFuelResult.OFF, plan.m_eResult, "price 0 is the kill switch: no sale");
		ctx.Equal(0, plan.m_iLitres, "the kill switch moves no litres");
		ctx.Equal(100, station.m_iLitres, "and leaves stock alone");

		LF_FuelConfig config = LF_FuelConfig.Make(false, 200, 100, 100, 100, 20, 1);
		ctx.Equal(LF_EFuelResult.OFF, LF_FuelStockRules.PlanSaleFor(config, station, 10, 1000, 0).m_eResult, "a disabled config is off");
		config.m_bEnabled = true;
		ctx.Equal(LF_EFuelResult.OK, LF_FuelStockRules.PlanSaleFor(config, station, 10, 1000, 0).m_eResult, "an enabled config sells");
		config.m_iPricePerLitre = 0;
		ctx.Equal(LF_EFuelResult.OFF, LF_FuelStockRules.PlanSaleFor(config, station, 10, 1000, 0).m_eResult, "price 0 in the config is off");
		ctx.Equal(LF_EFuelResult.BAD_INPUT, LF_FuelStockRules.PlanSaleFor(null, station, 10, 1000, 0).m_eResult, "no config refuses");

		// Money and litre arguments large enough to break a naive multiply stay inside an int.
		LF_FuelStock big = LF_FuelStock.Make(100000, 100000);
		LF_FuelPlan huge = LF_FuelStockRules.PlanSale(big, 2000000000, 2000000000, 10000, 0, 1);
		ctx.Equal(LF_EFuelResult.OK, huge.m_eResult, "a huge request still plans");
		ctx.True(huge.m_iLitres <= 100000 && huge.m_iLitres > 0, "huge wanted litres stay within stock");
		ctx.True(huge.m_iCash > 0 && huge.m_iCash <= LF_FuelStockRules.MAX_CASH, "huge money stays inside the cash cap");
	}

	//------------------------------------------------------------------------------------------------
	protected void RunConfig(LF_TestContext ctx)
	{
		LF_FuelConfig good = LF_FuelConfig.Make(true, 200, 100, 1000, 500, 50, 1);
		ctx.EqualStr("", good.Problem(), "the test table is valid");
		LF_FuelStock station = good.NewStation();
		ctx.Equal(1000, station.m_iCapacity, "a station takes the configured capacity");
		ctx.Equal(500, station.m_iLitres, "and the configured starting litres");

		ctx.True(LF_FuelConfig.Make(true, -1, 100, 1000, 500, 50, 1).Problem() != "", "a negative price is a problem");
		ctx.True(LF_FuelConfig.Make(true, 200, 100, 0, 0, 50, 1).Problem() != "", "no capacity is a problem");
		ctx.True(LF_FuelConfig.Make(true, 200, 100, 1000, 1001, 50, 1).Problem() != "", "start litres above capacity is a problem");
		ctx.True(LF_FuelConfig.Make(true, 200, 100, 1000, 500, 0, 1).Problem() != "", "a delivery of 0 litres is a problem");

		LF_FuelConfig bad = LF_FuelConfig.Make(true, 200, 100, 1000, 1001, 50, 1);
		ctx.False(bad.Sanitize(), "an unusable table is refused at load");
		ctx.False(bad.m_bEnabled, "and switched off");

		LF_FuelConfig.Reload();
		LF_FuelConfig shipped = LF_FuelConfig.Get();
		ctx.NotNull(shipped, "the shipped Configs/Fuel/LF_Fuel.conf resolves");
		if (shipped)
		{
			ctx.EqualStr("", shipped.Problem(), "the shipped table is usable");
			ctx.False(shipped.m_bEnabled, "the core ships fuel stock off");
		}

		LF_FuelConfig.Reload();
	}
}
