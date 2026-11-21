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
	NEEDS_ORG_RIGHT,
	//! The Shop system is off on this server (#1074).
	SYSTEM_OFF,
	//! The resident clerk turned the buyer away (#2911).
	RESIDENT_REFUSES,
	//! An implement the buyer already owns (#3295): bought once.
	ALREADY_OWNED,
	//! A licensed line (a firearm at the gun counter) is not sold to a wanted buyer.
	WANTED,
	//! A licensed line is not sold to a dead, downed or restrained buyer (a client can ask from any state).
	UNFIT
}

class LF_ShopRefusal
{
	//! The system id the Shop switch answers to (#1074).
	static const string SYSTEM = "Shop";

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

		if (reason == LF_EShopRefusal.SYSTEM_OFF)
			return "#LF-Systems_Off";

		if (reason == LF_EShopRefusal.RESIDENT_REFUSES)
			return "#LF-Shop_RefuseResident";

		if (reason == LF_EShopRefusal.ALREADY_OWNED)
			return "#LF-Shop_RefuseAlreadyOwned";

		if (reason == LF_EShopRefusal.WANTED)
			return "#LF-Shop_RefuseWanted";

		if (reason == LF_EShopRefusal.UNFIT)
			return "#LF-Shop_RefuseUnfit";

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

		string line = LF_Terms.Text(key, LF_Terms.Shown(detail));

		//! #3150: a licence refusal also says where to get it and what it costs.
		if (reason == LF_EShopRefusal.NEEDS_LICENCE)
			line = line + " " + LF_LicenceWhere.LineForName(detail);

		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! A reason that arrives over the wire as an int; anything unknown reads as no shop.
	//! The upper bound is the last enum value: it was OUT_OF_STOCK, which read CLOSED as no shop.
	static LF_EShopRefusal FromWire(int reason)
	{
		if (reason < LF_EShopRefusal.NONE || reason > LF_EShopRefusal.UNFIT)
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
