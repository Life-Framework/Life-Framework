// red-proof: observed 2026-11-10 with `tools\cli test --no-build --only LF_Test_StoreRules,...`: LF_StoreRules.ShelfCount returning Math.Max(stock, slots) failed `- six in stock on four slots: four on the shelf (expected 4, got 6)`; restored, green.

// tier: LOGIC
//! The pure rules of the physical store (#202): how many display copies stand on a shelf, the
//! restock step, the saved-stock clamp, the boundary box, the restock clock, the till total and the
//! till toast keys. No world, no entity.
class LF_Test_StoreRules : LF_Test
{
	override string GetName()
	{
		return "store/rules";
	}

	override void Run(LF_TestContext ctx)
	{
		// The shelf shows the stock, never more than its slots; the rest waits in the back room.
		ctx.Equal(4, LF_StoreRules.ShelfCount(6, 4), "six in stock on four slots: four on the shelf");
		ctx.Equal(2, LF_StoreRules.ShelfCount(2, 4), "two in stock: two on the shelf");
		ctx.Equal(0, LF_StoreRules.ShelfCount(0, 4), "nothing in stock: an empty shelf");
		ctx.Equal(0, LF_StoreRules.ShelfCount(5, 0), "no slots: nothing on the shelf, the stock is back room only");
		ctx.Equal(0, LF_StoreRules.ShelfCount(-3, 4), "a negative stock shows nothing");

		// Restock goes up by the amount and stops at the maximum.
		ctx.Equal(4, LF_StoreRules.Restocked(2, 6, 2), "restock adds the amount");
		ctx.Equal(6, LF_StoreRules.Restocked(5, 6, 3), "restock stops at the maximum");
		ctx.Equal(2, LF_StoreRules.Restocked(2, 6, 0), "a zero amount restocks nothing");
		ctx.Equal(9, LF_StoreRules.Restocked(9, 6, 2), "a stock above the maximum is left alone");

		// A saved count is clamped to what the shelf can hold now; a fresh world starts full.
		ctx.Equal(6, LF_StoreRules.ClampStock(40, 6), "a saved count above the new maximum is clamped");
		ctx.Equal(0, LF_StoreRules.ClampStock(-1, 6), "a negative saved count is zero");
		ctx.Equal(6, LF_StoreRules.StartStock(-1, 6), "a negative start stock starts full");
		ctx.Equal(3, LF_StoreRules.StartStock(3, 6), "an explicit start stock is kept");
		ctx.Equal(6, LF_StoreRules.StartStock(9, 6), "a start stock above the maximum is clamped");

		// The boundary is a flat box.
		vector half = "6 0 4";
		ctx.True(LF_StoreRules.InsideBox("5.9 0 -3.9", half), "a point inside the box");
		ctx.False(LF_StoreRules.InsideBox("6.1 0 0", half), "just past the x edge is outside");
		ctx.False(LF_StoreRules.InsideBox("0 0 -4.1", half), "just past the z edge is outside");
		ctx.True(LF_StoreRules.InsideBox("0 30 0", half), "height does not matter, a roof counts");

		// The restock clock.
		ctx.True(LF_StoreRules.RestockDue(121000, 0, 120), "the interval has run out");
		ctx.False(LF_StoreRules.RestockDue(119000, 0, 120), "the interval has not run out");
		ctx.False(LF_StoreRules.RestockDue(999999, 0, 0), "a zero interval never restocks");

		// The till total ignores a free or broken price.
		int paidNone;
		array<int> prices = {10, 25, 0, -5, 5};
		ctx.Equal(40, LF_StoreRules.Total(prices), "the total counts only real prices");

		// The server pays only a customer standing at the register (#582).
		ctx.True(LF_StoreRules.TillInReach(1.2), "a customer at the register pays");
		ctx.True(LF_StoreRules.TillInReach(LF_StoreRules.TILL_REACH), "the edge of the reach still pays");
		ctx.False(LF_StoreRules.TillInReach(LF_StoreRules.TILL_REACH + 0.5), "a payer across the shop is refused");
		ctx.False(LF_StoreRules.TillInReach(-1), "a nonsense distance is refused");
		ctx.Equal(LF_EStorePay.NO_STORE, LF_Stores.Pay(null, null, paidNone), "a register with no store takes nothing");

		ctx.True(LF_StoreRules.PayKey(LF_EStorePay.PAID) != "", "a payment has a toast");
		ctx.True(LF_StoreRules.PayKey(LF_EStorePay.NOT_ENOUGH_CASH) != "", "a short purse has a toast");
		ctx.True(LF_ShopRefusal.Key(LF_EShopRefusal.OUT_OF_STOCK) != "", "an empty shelf has a slip line");
		ctx.Equal(LF_EShopRefusal.OUT_OF_STOCK, LF_ShopRefusal.FromWire(LF_EShopRefusal.OUT_OF_STOCK), "the refusal survives the wire");
	}
}
