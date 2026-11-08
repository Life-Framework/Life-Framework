// red-proof: 2026-10-11 fast boot logs_2026-10-11_15-02-48 printed
// [ELTEST]   - a stack of 5 sells for 5 times one unit (expected 7, got 35)
// while the assertion still expected one unit instead of five. Restored to 35.

// tier: LOGIC
class EL_Test_StackTrade : EL_Test
{
	override string GetName()
	{
		return "stack/trade";
	}

	override void Run(EL_TestContext ctx)
	{
		int unitPrice = 7;
		ctx.Equal(35, EL_ShopRules.StackTradeTotal(unitPrice, 5), "a stack of 5 sells for 5 times one unit");
		ctx.Equal(5, EL_QuantityComponent.CombineQuantities(2, 3), "combine of 2 and 3 yields 5");
		ctx.Equal(3, EL_QuantityComponent.QuantityRemainingAfterSplit(5, 2), "split of 5 into 2 leaves 3");
	}
};
