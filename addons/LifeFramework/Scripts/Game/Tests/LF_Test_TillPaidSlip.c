// red-proof: 2026-11-23 `node tools/cli.mjs test --no-build --red LF_Test_TillPaidSlip` (batched with a pack test): RED-OK shop/till-paid-slip (RedProof() keeps the slip and the old cash: "a paid buy empties the order slip", "the Cash line follows the cash after paying").
//! shop/till-paid-slip (first-2h re-review N2): after "Pay $150 cash" the till sheet showed the old cash and the full order
//! slip, so the purchase looked like it had not happened. A paid buy empties the slip; a refusal or a sale back keeps it.
//! The Cash line is redrawn whenever the carried cash is known and differs from the figure shown, and never overwritten
//! by an unknown (-1) read.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_TillPaidSlip : LF_Test
{
	override string GetName()
	{
		return "shop/till-paid-slip";
	}

	override void Run(LF_TestContext ctx)
	{
		bool paid = LF_ShopPriceList.ClearsSlipAfter(true, LF_EShopRefusal.NONE);
		int shown = 205;
		int carried = 55;
		if (RedProof())
		{
			paid = false;
			carried = shown;
		}

		ctx.True(paid, "a paid buy empties the order slip");
		ctx.False(LF_ShopPriceList.ClearsSlipAfter(true, LF_EShopRefusal.NOT_ENOUGH_CASH), "a refused buy keeps the slip to try again");
		ctx.False(LF_ShopPriceList.ClearsSlipAfter(false, LF_EShopRefusal.NONE), "a sale back keeps the slip");

		ctx.True(LF_ShopPriceList.CashLineStale(shown, carried), "the Cash line follows the cash after paying ($205 to $55)");
		ctx.False(LF_ShopPriceList.CashLineStale(55, 55), "an unchanged amount is not redrawn");
		ctx.False(LF_ShopPriceList.CashLineStale(55, -1), "an unknown read never overwrites the figure shown");
		ctx.True(LF_ShopPriceList.CashLineStale(-1, 0), "an empty pocket replaces an unknown figure");
	}
}
