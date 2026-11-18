//------------------------------------------------------------------------------------------------
//! Why a stall rule refused, or OK. A refused call changes nothing.
enum LF_EStallResult
{
	OK,
	OFF,				// stalls are not enabled
	CLOSED,				// not market day, or outside the market hours
	BAD_INPUT,			// no stall, no holder, no item or a negative day
	ALREADY_RENTED,		// the stall already holds this market day's pitch
	CANT_AFFORD,		// the cash offered does not cover the pitch fee or the price
	NOT_RENTED,			// the stall holds no pitch for this market day
	FULL,				// the stall already lists the most items
	PRICE_LOW,			// the price is below the config floor
	PRICE_HIGH,			// the price is above the config cap
	NO_LISTING,			// no such listing on the stall
	RESERVED,			// the listing's cash is already held in escrow
	SELF_BUY,			// the seller is buying their own item
	NOT_HELD,			// the escrow already settled or refunded
	WRONG_STALL			// the escrow belongs to another stall or listing
}

//------------------------------------------------------------------------------------------------
//! One item on a stall at the holder's price. `m_bReserved` while a buyer's cash sits in escrow for it.
class LF_StallListing : Managed
{
	int m_iId;
	ResourceName m_sItem;
	int m_iPrice;
	bool m_bReserved;
}

//------------------------------------------------------------------------------------------------
//! A buyer's cash held until the item is handed over. It ends once: settled (the seller is paid minus the
//! cut) or refunded (the buyer gets it back). State is `HELD`, `SETTLED` or `REFUNDED`.
class LF_StallEscrow : Managed
{
	static const int HELD = 0;
	static const int SETTLED = 1;
	static const int REFUNDED = 2;

	string m_sBuyer;
	int m_iListingId;
	int m_iAmount;
	int m_iState;
	//! Where this escrow lives; Settle and Refund refuse another stall's.
	string m_sStallKey;
}

//------------------------------------------------------------------------------------------------
//! One holder's stall for one market day: who holds it, which market day it was rented for, its listings,
//! and what it has earned. Plain data; `LF_StallRules` is the only thing that changes it.
class LF_Stall : Managed
{
	static const int NOT_RENTED = -1;

	string m_sHolder;
	//! The in-game day number the pitch was rented for, or NOT_RENTED.
	int m_iRentedDay = NOT_RENTED;
	int m_iNextId = 1;
	ref array<ref LF_StallListing> m_aListings = {};
	//! Cash paid out to the holder from settled sales, and kept as the market's cut.
	int m_iEarned;
	int m_iCutKept;

	//------------------------------------------------------------------------------------------------
	static LF_Stall Make(string holder)
	{
		LF_Stall stall = new LF_Stall();
		stall.m_sHolder = holder;
		return stall;
	}

	//------------------------------------------------------------------------------------------------
	LF_StallListing Find(int id)
	{
		foreach (LF_StallListing listing : m_aListings)
		{
			if (listing && listing.m_iId == id)
				return listing;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! Stall market rules (#2924): pure server logic, no world, no RPC, no save, no statics. The caller owns
//! the cash: a rule says what to take or pay (`config.m_iPitchFee`, `escrow.m_iAmount`, the `sellerPay` out
//! value) and the caller moves it only when the rule answered OK. The day is an in-game day number (the week
//! is `dayNumber % 7`), the hour an in-game hour 0..23. Every money sum goes through
//! LF_Utils.MaxInt/MinInt/ClampInt (never Math.Max/Min/Clamp, #2787).
class LF_StallRules
{
	static const int DAYS_PER_WEEK = 7;	// lf-fixed: the in-game week
	//! A running total stops here, so adding two totals never overflows an int.
	static const int MAX_TOTAL = 1000000000;	// lf-fixed: int overflow guard for money sums

	//------------------------------------------------------------------------------------------------
	//! \return true on market day (`m_iMarketDay` of the week) between the opening hour and the closing hour.
	static bool IsMarketOpen(LF_StallConfig config, int dayNumber, int hour)
	{
		if (!config || dayNumber < 0)
			return false;
		if ((dayNumber % DAYS_PER_WEEK) != config.m_iMarketDay)
			return false;

		return hour >= config.m_iOpenHour && hour < config.m_iCloseHour;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The market's cut of a sale: cutPct of the amount, rounded down, split so it cannot overflow.
	static int Cut(int amount, int cutPct)
	{
		int pct = LF_Utils.ClampInt(cutPct, 0, 100);
		int safeAmount = LF_Utils.MaxInt(amount, 0);
		int whole = (safeAmount / 100) * pct;
		int part = ((safeAmount % 100) * pct) / 100;
		return LF_Utils.MinInt(whole + part, safeAmount);
	}

	//------------------------------------------------------------------------------------------------
	//! \return What the seller gets from a sale of `amount`: the amount minus the cut, never negative.
	static int SellerShare(int amount, int cutPct)
	{
		int safeAmount = LF_Utils.MaxInt(amount, 0);
		return LF_Utils.MaxInt(safeAmount - Cut(safeAmount, cutPct), 0);
	}

	//------------------------------------------------------------------------------------------------
	protected static int AddTotal(int total, int add)
	{
		return LF_Utils.ClampInt(LF_Utils.MaxInt(total, 0) + LF_Utils.ClampInt(add, 0, MAX_TOTAL), 0, MAX_TOTAL);
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EStallResult Refuse(LF_EStallResult result, string why)
	{
		LF_Debug.Warn(LF_StallConfig.FEATURE, string.Format("stall refused: %1", why));
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! \return true while the stall holds the pitch for `dayNumber`.
	static bool IsRented(LF_Stall stall, int dayNumber)
	{
		return stall && dayNumber >= 0 && stall.m_iRentedDay == dayNumber;
	}

	//------------------------------------------------------------------------------------------------
	//! Rents the stall's pitch for the market day `dayNumber`, which must be open at `hour`. On OK the caller
	//! takes `config.m_iPitchFee` from `holderCash`'s owner. Listings left from an earlier market day that
	//! no buyer has reserved are cleared; a reserved one stays so its escrow can still settle or refund.
	static LF_EStallResult Rent(LF_StallConfig config, LF_Stall stall, int dayNumber, int hour, int holderCash)
	{
		if (!config || !stall || stall.m_sHolder.IsEmpty() || dayNumber < 0)
			return Refuse(LF_EStallResult.BAD_INPUT, "rent needs a config, a stall with a holder and a day");
		if (!config.m_bEnabled)
			return Refuse(LF_EStallResult.OFF, "stalls are off");
		if (!IsMarketOpen(config, dayNumber, hour))
			return Refuse(LF_EStallResult.CLOSED, string.Format("day %1 hour %2 is outside market hours", dayNumber, hour));
		if (stall.m_iRentedDay == dayNumber)
			return Refuse(LF_EStallResult.ALREADY_RENTED, string.Format("%1 already rents a pitch for day %2", stall.m_sHolder, dayNumber));
		if (holderCash < config.m_iPitchFee)
			return Refuse(LF_EStallResult.CANT_AFFORD, string.Format("%1 has %2, the pitch costs %3", stall.m_sHolder, holderCash, config.m_iPitchFee));

		for (int i = stall.m_aListings.Count() - 1; i >= 0; i--)
		{
			LF_StallListing old = stall.m_aListings[i];
			if (!old || !old.m_bReserved)
				stall.m_aListings.Remove(i);
		}

		stall.m_iRentedDay = dayNumber;
		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("%1 rented a pitch for day %2 at %3", stall.m_sHolder, dayNumber, config.m_iPitchFee));
		return LF_EStallResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Lists one `item` at `price` on a stall rented for `dayNumber`. The price must sit inside the config
	//! floor..cap and the stall may list at most `m_iMaxListings`. `listingId` is the new listing's id (0 on a refusal).
	static LF_EStallResult List(LF_StallConfig config, LF_Stall stall, int dayNumber, ResourceName item, int price, out int listingId)
	{
		listingId = 0;
		if (!config || !stall || item.IsEmpty())
			return Refuse(LF_EStallResult.BAD_INPUT, "a listing needs a config, a stall and an item");
		if (!config.m_bEnabled)
			return Refuse(LF_EStallResult.OFF, "stalls are off");
		if (!IsRented(stall, dayNumber))
			return Refuse(LF_EStallResult.NOT_RENTED, string.Format("%1 holds no pitch for day %2", stall.m_sHolder, dayNumber));
		if (stall.m_aListings.Count() >= config.m_iMaxListings)
			return Refuse(LF_EStallResult.FULL, string.Format("%1 already lists %2 items", stall.m_sHolder, stall.m_aListings.Count()));
		if (price < config.m_iMinPrice)
			return Refuse(LF_EStallResult.PRICE_LOW, string.Format("price %1 is below the floor %2", price, config.m_iMinPrice));
		if (price > config.m_iMaxPrice)
			return Refuse(LF_EStallResult.PRICE_HIGH, string.Format("price %1 is above the cap %2", price, config.m_iMaxPrice));

		LF_StallListing listing = new LF_StallListing();
		listing.m_iId = stall.m_iNextId;
		listing.m_sItem = item;
		listing.m_iPrice = price;
		stall.m_iNextId = stall.m_iNextId + 1;
		stall.m_aListings.Insert(listing);
		listingId = listing.m_iId;
		return LF_EStallResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a listing back off the stall. Refused while a buyer's cash is in escrow for it.
	static LF_EStallResult Delist(LF_Stall stall, int listingId)
	{
		if (!stall)
			return Refuse(LF_EStallResult.BAD_INPUT, "delist needs a stall");

		LF_StallListing listing = stall.Find(listingId);
		if (!listing)
			return Refuse(LF_EStallResult.NO_LISTING, string.Format("no listing %1", listingId));
		if (listing.m_bReserved)
			return Refuse(LF_EStallResult.RESERVED, string.Format("listing %1 has cash in escrow", listingId));

		stall.m_aListings.RemoveItem(listing);
		return LF_EStallResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! A buyer reserves a listing: on OK `escrow` holds the price and the caller takes `escrow.m_iAmount`
	//! from the buyer's cash. The listing cannot be reserved twice. The market must be open on `dayNumber`
	//! at `hour` and the stall rented for it. `escrow` is null on a refusal.
	static LF_EStallResult Reserve(LF_StallConfig config, LF_Stall stall, int listingId, string buyer, int buyerCash, int dayNumber, int hour, out LF_StallEscrow escrow)
	{
		escrow = null;
		if (!config || !stall || buyer.IsEmpty())
			return Refuse(LF_EStallResult.BAD_INPUT, "a purchase needs a config, a stall and a buyer");
		if (!config.m_bEnabled)
			return Refuse(LF_EStallResult.OFF, "stalls are off");
		if (!IsMarketOpen(config, dayNumber, hour))
			return Refuse(LF_EStallResult.CLOSED, string.Format("day %1 hour %2 is outside market hours", dayNumber, hour));
		if (!IsRented(stall, dayNumber))
			return Refuse(LF_EStallResult.NOT_RENTED, string.Format("%1 holds no pitch for day %2", stall.m_sHolder, dayNumber));

		LF_StallListing listing = stall.Find(listingId);
		if (!listing)
			return Refuse(LF_EStallResult.NO_LISTING, string.Format("no listing %1", listingId));
		if (listing.m_bReserved)
			return Refuse(LF_EStallResult.RESERVED, string.Format("listing %1 is already reserved", listingId));
		if (buyer == stall.m_sHolder)
			return Refuse(LF_EStallResult.SELF_BUY, string.Format("%1 cannot buy their own listing", buyer));
		if (buyerCash < listing.m_iPrice)
			return Refuse(LF_EStallResult.CANT_AFFORD, string.Format("%1 has %2, listing %3 costs %4", buyer, buyerCash, listingId, listing.m_iPrice));

		listing.m_bReserved = true;
		escrow = new LF_StallEscrow();
		escrow.m_sBuyer = buyer;
		escrow.m_iListingId = listingId;
		escrow.m_iAmount = listing.m_iPrice;
		escrow.m_iState = LF_StallEscrow.HELD;
		escrow.m_sStallKey = stall.m_sHolder;
		return LF_EStallResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_EStallResult CheckHeld(LF_Stall stall, LF_StallEscrow escrow)
	{
		if (!stall || !escrow)
			return Refuse(LF_EStallResult.BAD_INPUT, "an escrow step needs a stall and an escrow");
		if (escrow.m_sStallKey != stall.m_sHolder)
			return Refuse(LF_EStallResult.WRONG_STALL, string.Format("escrow of %1 does not belong to the stall of %2", escrow.m_sBuyer, stall.m_sHolder));
		if (escrow.m_iState != LF_StallEscrow.HELD)
			return Refuse(LF_EStallResult.NOT_HELD, string.Format("escrow of %1 already ended (state %2)", escrow.m_sBuyer, escrow.m_iState));
		if (!stall.Find(escrow.m_iListingId))
			return Refuse(LF_EStallResult.WRONG_STALL, string.Format("escrow listing %1 is not on the stall", escrow.m_iListingId));

		return LF_EStallResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The item was handed over: the escrow settles once. On OK the caller pays the seller `sellerPay`
	//! (the held amount minus the config cut); the market keeps the rest, and the listing is gone.
	static LF_EStallResult Settle(LF_StallConfig config, LF_Stall stall, LF_StallEscrow escrow, out int sellerPay)
	{
		sellerPay = 0;
		if (!config)
			return Refuse(LF_EStallResult.BAD_INPUT, "settle needs a config");

		LF_EStallResult held = CheckHeld(stall, escrow);
		if (held != LF_EStallResult.OK)
			return held;

		int pay = SellerShare(escrow.m_iAmount, config.m_iCutPct);
		int kept = LF_Utils.MaxInt(escrow.m_iAmount - pay, 0);
		escrow.m_iState = LF_StallEscrow.SETTLED;
		stall.m_aListings.RemoveItem(stall.Find(escrow.m_iListingId));
		stall.m_iEarned = AddTotal(stall.m_iEarned, pay);
		stall.m_iCutKept = AddTotal(stall.m_iCutKept, kept);
		sellerPay = pay;
		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("escrow of %1 settled: seller %2 paid %3, market kept %4", escrow.m_sBuyer, stall.m_sHolder, pay, kept));
		return LF_EStallResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! The sale fell through: the escrow refunds once. On OK the caller returns `refund` (the whole held amount)
	//! to the buyer and the listing is open to buyers again.
	static LF_EStallResult Refund(LF_Stall stall, LF_StallEscrow escrow, out int refund)
	{
		refund = 0;
		LF_EStallResult held = CheckHeld(stall, escrow);
		if (held != LF_EStallResult.OK)
			return held;

		escrow.m_iState = LF_StallEscrow.REFUNDED;
		LF_StallListing listing = stall.Find(escrow.m_iListingId);
		if (listing)
			listing.m_bReserved = false;
		refund = escrow.m_iAmount;
		LF_Debug.Info(LF_StallConfig.FEATURE, string.Format("escrow of %1 refunded %2", escrow.m_sBuyer, refund));
		return LF_EStallResult.OK;
	}
}
