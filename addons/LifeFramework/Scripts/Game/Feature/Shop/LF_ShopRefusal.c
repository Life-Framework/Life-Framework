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
	TOO_FAST,
	//! An outlet's gate (#988): the reply's detail names what the buyer lacks.
	NEEDS_LICENCE,
	NEEDS_JOB,
	NEEDS_RANK,
	NEEDS_ORG_RIGHT
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

		if (reason == LF_EShopRefusal.NEEDS_LICENCE)
			return "#LF-Shop_RefuseNeedsLicence";

		if (reason == LF_EShopRefusal.NEEDS_JOB)
			return "#LF-Shop_RefuseNeedsJob";

		if (reason == LF_EShopRefusal.NEEDS_RANK)
			return "#LF-Shop_RefuseNeedsRank";

		if (reason == LF_EShopRefusal.NEEDS_ORG_RIGHT)
			return "#LF-Shop_RefuseNeedsOrgRight";

		return string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	//! Slip line for a refusal with what the buyer lacks filled in (a gate names it, #988). Client.
	//! \param[in] detail A localization key (a licence's name, a rank's title) or plain text; empty
	//! for a refusal that names nothing.
	static string Text(LF_EShopRefusal reason, string detail)
	{
		string key = Key(reason);
		if (key.IsEmpty() || detail.IsEmpty())
			return key;

		return WidgetManager.Translate(key, WidgetManager.Translate(detail));
	}

	//------------------------------------------------------------------------------------------------
	//! A reason that arrives over the wire as an int; anything unknown reads as no shop.
	//! The upper bound is the last enum value: it was OUT_OF_STOCK, which read CLOSED as no shop.
	static LF_EShopRefusal FromWire(int reason)
	{
		if (reason < LF_EShopRefusal.NONE || reason > LF_EShopRefusal.NEEDS_ORG_RIGHT)
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

		if (next > LF_ShopLimitsConfig.OrderMaxQuantity())
			return LF_ShopLimitsConfig.OrderMaxQuantity();

		return next;
	}
}
