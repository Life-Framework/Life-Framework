//------------------------------------------------------------------------------------------------
//! One premium buyer's appetite for one good (#3267, lumber design "Buyers"). The key is the buyer id
//! (a free string the pack picks, for example a building id) plus the good's prefab. The buyer pays a
//! fixed price up to its demand: a purse of `m_iCap` units that refills `m_iRefillPerHour` units each real
//! hour. A row may also name a curve good (#2844): the unit price then falls inside the cap, by that
//! curve's drop percent and floor.
[BaseContainerProps()]
class LF_BuyerDemandRow : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Buyer id: a free string the pack picks (a building id). Half of the record key")]
	string m_sBuyer;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item prefab the buyer wants, the same key the trader and market lists use. Half of the record key", "et")]
	ResourceName m_sGood;

	[Attribute("0", UIWidgets.Auto, "Units the buyer takes before it has enough: the purse when full", params: "0 60000")]
	int m_iCap;

	[Attribute("0", UIWidgets.Auto, "Units the purse refills each real hour; 0 never refills", params: "0 60000")]
	int m_iRefillPerHour;

	[Attribute("0", UIWidgets.Auto, "Whole cash one lot fetches from a full purse (one unit while the lot size is 1)", params: "0 10000000")]
	int m_iPrice;

	[Attribute("1", UIWidgets.Auto, "Units one price covers (#3622). The buyer takes whole lots only; a stack smaller than a lot is refused and the part lot stays with the seller. A curve good named below must use the same lot size", params: "1 60000")]
	int m_iLotSize;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Optional market curve good (LF_MarketConfig): its drop percent and floor make the price fall as the purse empties. Empty keeps a flat price", "et")]
	ResourceName m_sCurveGood;

	//------------------------------------------------------------------------------------------------
	static LF_BuyerDemandRow Make(string buyer, ResourceName good, int cap, int refillPerHour, int price, ResourceName curveGood = "", int lotSize = 1)
	{
		LF_BuyerDemandRow row = new LF_BuyerDemandRow();
		row.m_sBuyer = buyer;
		row.m_sGood = good;
		row.m_iCap = cap;
		row.m_iRefillPerHour = refillPerHour;
		row.m_iPrice = price;
		row.m_sCurveGood = curveGood;
		row.m_iLotSize = lotSize;
		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Units one price covers: `m_iLotSize`, 1 when the row leaves it unset.
	int Lot()
	{
		return LF_Utils.MaxInt(m_iLotSize, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! \return A short name for log lines: buyer and good.
	string Label()
	{
		return string.Format("%1 / %2", m_sBuyer, m_sGood);
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the row is usable, else why not: no buyer or good, a cap of 0 or past the
	//! largest purse, a negative refill, or a price outside 0..MAX_CASH.
	string Problem()
	{
		if (m_sBuyer.IsEmpty())
			return "no buyer";
		if (m_sGood.IsEmpty())
			return "no good";
		if (m_iCap <= 0)
			return string.Format("cap %1 is not above 0", m_iCap);
		if (m_iCap > LF_BuyerDemand.MAX_CAP)
			return string.Format("cap %1 is above the largest purse %2", m_iCap, LF_BuyerDemand.MAX_CAP);
		if (m_iRefillPerHour < 0)
			return string.Format("negative refill %1 per hour", m_iRefillPerHour);
		if (m_iRefillPerHour > LF_BuyerDemand.MAX_CAP)
			return string.Format("refill %1 per hour is above the largest purse %2", m_iRefillPerHour, LF_BuyerDemand.MAX_CAP);
		if (m_iPrice < 0 || m_iPrice > LF_PriceCurve.MAX_CASH)
			return string.Format("price %1 is outside 0 to %2", m_iPrice, LF_PriceCurve.MAX_CASH);
		if (m_iLotSize < 0 || m_iLotSize > LF_BuyerDemand.MAX_CAP)
			return string.Format("lot size %1 is outside 1 to %2", m_iLotSize, LF_BuyerDemand.MAX_CAP);
		if (m_iCap < Lot())
			return string.Format("cap %1 is below one lot of %2", m_iCap, Lot());

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! One buyer's live purse for one row: units left and the unix second the refill clock counts from.
//! The caller keeps it (a save section is a later card); the rules below move it.
class LF_BuyerPurse
{
	int m_iRemaining;
	int m_iStamp;

	//------------------------------------------------------------------------------------------------
	//! \return A full purse for `row`, its clock at `now`.
	static LF_BuyerPurse Full(notnull LF_BuyerDemandRow row, int now)
	{
		LF_BuyerPurse purse = new LF_BuyerPurse();
		purse.m_iRemaining = row.m_iCap;
		purse.m_iStamp = now;
		return purse;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A purse holding `remaining` units (held to 0..cap), its clock at `stamp`.
	static LF_BuyerPurse Make(notnull LF_BuyerDemandRow row, int remaining, int stamp)
	{
		LF_BuyerPurse purse = new LF_BuyerPurse();
		purse.m_iRemaining = LF_Utils.ClampInt(remaining, 0, row.m_iCap);
		purse.m_iStamp = stamp;
		return purse;
	}
}

//------------------------------------------------------------------------------------------------
//! What one sale to a buyer came to.
class LF_BuyerDemandSale
{
	int m_iAccepted;
	int m_iRefused;
	//! Of the refused units, those that only fall short of a whole lot (#3622): the seller keeps them and the
	//! buyer is not full. Refused minus this is what the purse could not hold.
	int m_iUnderLot;
	int m_iPayout;
	//! Seconds until the purse has one unit again: 0 when it has one now, -1 when it never refills.
	int m_iSecondsToNext;
}

//------------------------------------------------------------------------------------------------
//! Buyer-with-demand rules (#3267): pure and static, no world, no statics, no save. A premium buyer pays
//! a fixed price (or a falling one when its row names a curve) for as many units as its purse holds; the
//! purse refills by elapsed real seconds. All counts are ints through LF_Utils.MaxInt/MinInt/ClampInt
//! (never Math.Max/Min/Clamp, #2787), so a unix time near 1.8e9 stays exact. Wholesale buyers need no
//! class: they are `LF_PriceCurve`.
class LF_BuyerDemand
{
	//! Largest purse or hourly refill a row may use: the same bound as one bulk price (LF_PriceCurve), so a
	//! whole purse prices in one call and cap * 3600 stays inside an int.
	static const int MAX_CAP = 60000;	// lf-fixed: equals LF_PriceCurve.MAX_BULK_UNITS

	//------------------------------------------------------------------------------------------------
	//! Refills `purse` for the time since its stamp: refill units per hour, never past the cap. A gap long
	//! enough to fill it is clamped (full, stamp to `now`); a full purse keeps no head start, so its stamp
	//! follows `now`; a clock that moved back resets the stamp to `now`. The stamp moves by the time the
	//! units gained cost, so a part-unit of progress is kept for the next call.
	static void Refill(notnull LF_BuyerDemandRow row, notnull LF_BuyerPurse purse, int now)
	{
		int cap = row.m_iCap;
		purse.m_iRemaining = LF_Utils.ClampInt(purse.m_iRemaining, 0, cap);
		if (purse.m_iRemaining >= cap)
		{
			purse.m_iStamp = now;
			return;
		}

		int elapsed = now - purse.m_iStamp;
		if (elapsed < 0)
		{
			purse.m_iStamp = now;
			return;
		}

		int refill = LF_Utils.MinInt(row.m_iRefillPerHour, MAX_CAP);
		if (refill <= 0 || elapsed == 0)
		{
			if (refill <= 0)
				purse.m_iStamp = now;

			return;
		}

		int missing = cap - purse.m_iRemaining;
		// Seconds that fill the purse from here, rounded up. Both factors are bounded: missing * 3600 <= 2.2e8.
		int fullSeconds = (missing * LF_PriceCurve.SECONDS_PER_HOUR + refill - 1) / refill;
		if (elapsed >= fullSeconds)
		{
			purse.m_iRemaining = cap;
			purse.m_iStamp = now;
			return;
		}

		// elapsed < fullSeconds, so elapsed * refill < missing * 3600 + refill: inside an int.
		int gained = (elapsed * refill) / LF_PriceCurve.SECONDS_PER_HOUR;
		if (gained <= 0)
			return;

		purse.m_iRemaining = LF_Utils.MinInt(purse.m_iRemaining + gained, cap);
		purse.m_iStamp += (gained * LF_PriceCurve.SECONDS_PER_HOUR) / refill;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Seconds until the purse (after a refill to `now`) holds one lot (one unit while the lot size
	//! is 1): 0 when it holds one now, -1 when it holds less and never refills.
	static int SecondsToNextUnit(notnull LF_BuyerDemandRow row, notnull LF_BuyerPurse purse, int now)
	{
		Refill(row, purse, now);
		int lot = row.Lot();
		if (purse.m_iRemaining >= lot)
			return 0;

		int refill = LF_Utils.MinInt(row.m_iRefillPerHour, MAX_CAP);
		if (refill <= 0)
			return -1;

		// Units still missing from one lot: at most MAX_CAP, so missing * 3600 stays inside an int.
		int missing = lot - purse.m_iRemaining;
		int lotSeconds = (missing * LF_PriceCurve.SECONDS_PER_HOUR + refill - 1) / refill;
		return LF_Utils.MaxInt(lotSeconds - (now - purse.m_iStamp), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Sells `units` to the buyer at `now`: the purse is refilled first, the buyer accepts
	//! `min(units, remaining)` and refuses the rest, and the purse drops by what it accepted. The payout is
	//! whole cash: a flat price per unit, or, when `curve` is given, a price that starts at the row's price
	//! less the curve's drop for every unit already used from the cap and falls the same way unit by unit
	//! (the marginal sum, LF_PriceCurve.BulkTotal), never below the curve's floor.
	//! \param[in] curve The curve good named by the row, or null for a flat price.
	static LF_BuyerDemandSale Sell(notnull LF_BuyerDemandRow row, notnull LF_BuyerPurse purse, int units, int now, LF_MarketGoodDef curve = null)
	{
		LF_BuyerDemandSale sale = new LF_BuyerDemandSale();
		Refill(row, purse, now);
		units = LF_Utils.MaxInt(units, 0);
		// Whole lots only (#3622): the units the stack fills and the units the purse still holds, each
		// rounded down to a lot. A part lot is refused and stays with the seller.
		int lot = row.Lot();
		int stackLots = units / lot;
		int purseLots = purse.m_iRemaining / lot;
		sale.m_iAccepted = LF_Utils.MinInt(stackLots, purseLots) * lot;
		sale.m_iRefused = units - sale.m_iAccepted;
		sale.m_iUnderLot = units % lot;
		if (sale.m_iAccepted > 0)
		{
			int used = row.m_iCap - purse.m_iRemaining;
			sale.m_iPayout = Payout(row, used, sale.m_iAccepted, curve);
			purse.m_iRemaining -= sale.m_iAccepted;
		}

		sale.m_iSecondsToNext = SecondsToNextUnit(row, purse, now);
		return sale;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The whole cash `units` fetch when `used` units of the cap are already gone. The price is per
	//! lot (#3622): the sold units and the used units count in whole lots, rounded down, and the curve falls
	//! per lot.
	static int Payout(notnull LF_BuyerDemandRow row, int used, int units, LF_MarketGoodDef curve = null)
	{
		int lot = row.Lot();
		used = used / lot;
		units = units / lot;
		int ceilingFine = LF_PriceCurve.ToFine(row.m_iPrice);
		int drop = 0;
		int floorFine = ceilingFine;
		if (curve)
		{
			drop = LF_PriceCurve.DropPerUnit(row.m_iPrice, curve.m_iDropPerUnitPct);
			floorFine = LF_Utils.MinInt(curve.FloorFine(), ceilingFine);
		}

		int startFine = LF_PriceCurve.PriceAfterSale(ceilingFine, used, drop, floorFine, ceilingFine);
		return LF_PriceCurve.BulkTotal(startFine, units, drop, floorFine, ceilingFine);
	}
}
