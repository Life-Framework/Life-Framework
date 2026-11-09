// red-proof: perturb a boundary, then run the fast tier. For shop/quantity-gate
// allow quantity 0 through LF_ShopComponent.IsQuantityAllowed; for shop/total-price
// remove the negative-price guard in ComputeTotalPrice; for atm/amount-guard drop
// the cap check in LF_ATMManager.IsValidAmount. Each perturbed rule turns its
// assertions red.

// tier: LOGIC
class LF_Test_ShopQuantityGate : LF_Test
{
	override string GetName()
	{
		return "shop/quantity-gate";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.True(LF_ShopComponent.IsQuantityAllowed(10, 1), "quantity 1 within max is allowed");
		ctx.True(LF_ShopComponent.IsQuantityAllowed(10, 10), "quantity at max is allowed");
		ctx.False(LF_ShopComponent.IsQuantityAllowed(10, 11), "quantity above max is rejected");
		ctx.False(LF_ShopComponent.IsQuantityAllowed(10, 0), "quantity 0 is rejected");
		ctx.False(LF_ShopComponent.IsQuantityAllowed(10, -1), "negative quantity is rejected");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_ShopTotalPrice : LF_Test
{
	override string GetName()
	{
		return "shop/total-price";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.Equal(30, LF_ShopComponent.ComputeTotalPrice(10, 3), "price is unit price times quantity");
		ctx.Equal(0, LF_ShopComponent.ComputeTotalPrice(-5, 3), "negative unit price costs nothing");
		ctx.Equal(0, LF_ShopComponent.ComputeTotalPrice(10, 0), "zero quantity costs nothing");
		ctx.Equal(0, LF_ShopComponent.ComputeTotalPrice(10, -2), "negative quantity costs nothing");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class LF_Test_ATMAmountGuard : LF_Test
{
	override string GetName()
	{
		return "atm/amount-guard";
	}

	override void Run(LF_TestContext ctx)
	{
		ctx.False(LF_ATMManager.IsValidAmount(0), "zero amount is rejected");
		ctx.False(LF_ATMManager.IsValidAmount(-1), "negative amount is rejected");
		ctx.True(LF_ATMManager.IsValidAmount(LF_ATMManager.MAX_TRANSACTION_AMOUNT), "max amount is accepted");
		ctx.False(LF_ATMManager.IsValidAmount(LF_ATMManager.MAX_TRANSACTION_AMOUNT + 1), "oversized amount is rejected");
	}
};