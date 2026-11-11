// LF_ShopRefusal.c — why the till said no. The server decides; the reply carries the reason and
// the price list prints it on its order slip (no toast).

enum LF_EShopRefusal
{
	NONE,
	NOT_SOLD_HERE,
	UNPRICED,
	NOT_ENOUGH_CASH,
	POCKETS_FULL,
	NOTHING_TO_SELL,
	BAD_QUANTITY,
	TOO_FAR,
	NO_SHOP,
	NOT_TRUSTED,
	OUT_OF_STOCK,
	CLOSED,
	TOO_FAST
}

class LF_ShopRefusal
{
	//------------------------------------------------------------------------------------------------
	//! Slip line for a refusal; empty for NONE.
	static string Key(LF_EShopRefusal reason)
	{
		if (reason == LF_EShopRefusal.NOT_SOLD_HERE)
			return "#LF-Shop_RefuseNotSold";

		if (reason == LF_EShopRefusal.UNPRICED)
			return "#LF-Shop_RefuseUnpriced";

		if (reason == LF_EShopRefusal.NOT_ENOUGH_CASH)
			return "#LF-Shop_RefuseCash";

		if (reason == LF_EShopRefusal.POCKETS_FULL)
			return "#LF-Shop_RefusePockets";

		if (reason == LF_EShopRefusal.NOTHING_TO_SELL)
			return "#LF-Shop_RefuseNothing";

		if (reason == LF_EShopRefusal.BAD_QUANTITY)
			return "#LF-Shop_RefuseQuantity";

		if (reason == LF_EShopRefusal.TOO_FAR)
			return "#LF-Shop_RefuseFar";

		if (reason == LF_EShopRefusal.NO_SHOP)
			return "#LF-Shop_RefuseNoShop";

		if (reason == LF_EShopRefusal.NOT_TRUSTED)
			return "#LF-Shop_RefuseNotTrusted";

		if (reason == LF_EShopRefusal.CLOSED)
			return "#LF-Shop_RefuseClosed";

		if (reason == LF_EShopRefusal.OUT_OF_STOCK)
			return "#LF-Shop_RefuseOutOfStock";

		if (reason == LF_EShopRefusal.TOO_FAST)
			return "#LF-Shop_RefuseTooFast";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! A reason that arrives over the wire as an int; anything unknown reads as no shop.
	//! The upper bound is the last enum value: it was OUT_OF_STOCK, which read CLOSED as no shop.
	static LF_EShopRefusal FromWire(int reason)
	{
		if (reason < LF_EShopRefusal.NONE || reason > LF_EShopRefusal.TOO_FAST)
			return LF_EShopRefusal.NO_SHOP;

		return reason;
	}

	//------------------------------------------------------------------------------------------------
	//! Order slip quantity after a -/+ press: never below 1, never above the server's cap.
	static int StepQuantity(int quantity, int step)
	{
		int next = quantity + step;
		if (next < 1)
			return 1;

		if (next > LF_CharacterShopComponent.MAX_TRANSACTION_QUANTITY)
			return LF_CharacterShopComponent.MAX_TRANSACTION_QUANTITY;

		return next;
	}
}
