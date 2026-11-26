// red-proof: with `RedProof()` on the rules are not asked (every answer keeps its wrong default), so the "buys",
// "multiplier", "stack total", "can sell" and "reason" checks fail.

//! shop/sell-rules: what a shop pays a player (LF_ShopRules). Vehicle and procurement shops buy nothing, a gun
//! dealer buys only while its multiplier is above zero and pays at that multiplier, a stack pays one unit price
//! per quantity (never for a zero price or an empty stack), and an item sells only when it is not equipped, the
//! shop buys it and it has a value, with the block reason naming the first rule that fired.
[LF_TestInfo(LF_TestTier.LOGIC)]
class LF_Test_ShopSellRules : LF_Test
{
	override string GetName()
	{
		return "shop/sell-rules";
	}

	override void Run(LF_TestContext ctx)
	{
		bool generalBuys = false;
		bool drugBuys = false;
		bool procurementBuys = true;
		bool vehicleBuys = true;
		bool gunBuys = false;
		bool gunZeroBuys = true;
		float gunMultiplier = 0;
		float generalMultiplier = 0;
		int fiveUnits = -1;
		int noUnits = -1;
		int negUnits = -1;
		int total = -1;
		int freeTotal = -1;
		int negPriceTotal = -1;
		int emptyTotal = -1;
		bool sellable = false;
		bool equipped = true;
		bool notBought = true;
		bool noValue = true;
		string reasonOk = "x";
		string reasonEquipped = "";
		string reasonNotBought = "";
		string reasonNoValue = "";
		string reasonFirst = "";
		string reasonSecond = "";
		if (!RedProof())
		{
			generalBuys = LF_ShopRules.ShopBuysFromPlayers(LF_ShopType.SHOP_GENERAL, false, 0);
			drugBuys = LF_ShopRules.ShopBuysFromPlayers(LF_ShopType.SHOP_DRUG, false, 0);
			procurementBuys = LF_ShopRules.ShopBuysFromPlayers(LF_ShopType.SHOP_GENERAL, true, 1);
			vehicleBuys = LF_ShopRules.ShopBuysFromPlayers(LF_ShopType.SHOP_VEHICLE, false, 1);
			gunBuys = LF_ShopRules.ShopBuysFromPlayers(LF_ShopType.SHOP_GUNDEALER, false, 0.5);
			gunZeroBuys = LF_ShopRules.ShopBuysFromPlayers(LF_ShopType.SHOP_GUNDEALER, false, 0);
			gunMultiplier = LF_ShopRules.GetSellMultiplier(LF_ShopType.SHOP_GUNDEALER, 0.5);
			generalMultiplier = LF_ShopRules.GetSellMultiplier(LF_ShopType.SHOP_GENERAL, 0.5);
			fiveUnits = LF_ShopRules.StackTradeOutputCount(5);
			noUnits = LF_ShopRules.StackTradeOutputCount(0);
			negUnits = LF_ShopRules.StackTradeOutputCount(-3);
			total = LF_ShopRules.StackTradeTotal(20, 5);
			freeTotal = LF_ShopRules.StackTradeTotal(0, 5);
			negPriceTotal = LF_ShopRules.StackTradeTotal(-5, 5);
			emptyTotal = LF_ShopRules.StackTradeTotal(20, 0);
			sellable = LF_ShopRules.CanSellItem(false, true, 10);
			equipped = LF_ShopRules.CanSellItem(true, true, 10);
			notBought = LF_ShopRules.CanSellItem(false, false, 10);
			noValue = LF_ShopRules.CanSellItem(false, true, 0);
			reasonOk = LF_ShopRules.GetBlockReasonKey(false, true, 10);
			reasonEquipped = LF_ShopRules.GetBlockReasonKey(true, true, 10);
			reasonNotBought = LF_ShopRules.GetBlockReasonKey(false, false, 10);
			reasonNoValue = LF_ShopRules.GetBlockReasonKey(false, true, 0);
			reasonFirst = LF_ShopRules.GetBlockReasonKey(true, false, 0);
			reasonSecond = LF_ShopRules.GetBlockReasonKey(false, false, 0);
		}

		ctx.True(generalBuys, "a general shop buys from players");
		ctx.True(drugBuys, "a drug shop buys from players");
		ctx.False(procurementBuys, "a procurement shop (garage, helipad) buys nothing back");
		ctx.False(vehicleBuys, "a vehicle shop buys nothing back");
		ctx.True(gunBuys, "a gun dealer with a multiplier above zero buys");
		ctx.False(gunZeroBuys, "a gun dealer with a zero multiplier does not buy: one config value turns the sell path off");
		ctx.EqualFloat(0.5, gunMultiplier, 0.001, "a gun dealer pays at its multiplier");
		ctx.EqualFloat(1.0, generalMultiplier, 0.001, "every other shop pays the base rate");

		ctx.Equal(5, fiveUnits, "a stack of 5 trades 5 units");
		ctx.Equal(0, noUnits, "an empty stack is not a trade");
		ctx.Equal(0, negUnits, "a negative stack is not a trade");
		ctx.Equal(100, total, "a stack of 5 at 20 pays 100");
		ctx.Equal(0, freeTotal, "a zero unit price pays nothing");
		ctx.Equal(0, negPriceTotal, "a negative unit price pays nothing");
		ctx.Equal(0, emptyTotal, "an empty stack pays nothing");

		ctx.True(sellable, "an unequipped item a shop buys at a price sells");
		ctx.False(equipped, "an equipped item never sells");
		ctx.False(notBought, "an item the shop does not buy never sells");
		ctx.False(noValue, "an item with no price never sells");

		ctx.EqualStr("", reasonOk, "a sellable item has no block reason");
		ctx.EqualStr(LF_ShopRules.REASON_EQUIPPED, reasonEquipped, "equipped names the equipped rule");
		ctx.EqualStr(LF_ShopRules.REASON_SHOP_DOES_NOT_BUY, reasonNotBought, "a shop that does not buy names that rule");
		ctx.EqualStr(LF_ShopRules.REASON_NO_VALUE, reasonNoValue, "no price names the no-value rule");
		ctx.EqualStr(LF_ShopRules.REASON_EQUIPPED, reasonFirst, "equipped wins over the later rules, like CanSellItem");
		ctx.EqualStr(LF_ShopRules.REASON_SHOP_DOES_NOT_BUY, reasonSecond, "shop-does-not-buy wins over no-value");
	}
}
