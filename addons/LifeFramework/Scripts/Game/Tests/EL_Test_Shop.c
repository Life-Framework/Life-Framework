// red-proof: perturb a boundary, then run the fast tier. For shop/quantity-gate
// allow quantity 0 through EL_ShopComponent.IsQuantityAllowed; for shop/total-price
// remove the negative-price guard in ComputeTotalPrice; for atm/amount-guard drop
// the cap check in EL_ATMManager.IsValidAmount. Each perturbed rule turns its
// assertions red.

// tier: LOGIC
class EL_Test_ShopQuantityGate : EL_Test
{
	override string GetName()
	{
		return "shop/quantity-gate";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.True(EL_ShopComponent.IsQuantityAllowed(10, 1), "quantity 1 within max is allowed");
		ctx.True(EL_ShopComponent.IsQuantityAllowed(10, 10), "quantity at max is allowed");
		ctx.False(EL_ShopComponent.IsQuantityAllowed(10, 11), "quantity above max is rejected");
		ctx.False(EL_ShopComponent.IsQuantityAllowed(10, 0), "quantity 0 is rejected");
		ctx.False(EL_ShopComponent.IsQuantityAllowed(10, -1), "negative quantity is rejected");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_ShopTotalPrice : EL_Test
{
	override string GetName()
	{
		return "shop/total-price";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.Equal(30, EL_ShopComponent.ComputeTotalPrice(10, 3), "price is unit price times quantity");
		ctx.Equal(0, EL_ShopComponent.ComputeTotalPrice(-5, 3), "negative unit price costs nothing");
		ctx.Equal(0, EL_ShopComponent.ComputeTotalPrice(10, 0), "zero quantity costs nothing");
		ctx.Equal(0, EL_ShopComponent.ComputeTotalPrice(10, -2), "negative quantity costs nothing");
	}
};

//------------------------------------------------------------------------------------------------
// tier: LOGIC
class EL_Test_ATMAmountGuard : EL_Test
{
	override string GetName()
	{
		return "atm/amount-guard";
	}

	override void Run(EL_TestContext ctx)
	{
		ctx.False(EL_ATMManager.IsValidAmount(0), "zero amount is rejected");
		ctx.False(EL_ATMManager.IsValidAmount(-1), "negative amount is rejected");
		ctx.True(EL_ATMManager.IsValidAmount(EL_ATMManager.MAX_TRANSACTION_AMOUNT), "max amount is accepted");
		ctx.False(EL_ATMManager.IsValidAmount(EL_ATMManager.MAX_TRANSACTION_AMOUNT + 1), "oversized amount is rejected");
	}
};