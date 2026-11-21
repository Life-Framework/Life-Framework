//------------------------------------------------------------------------------------------------
//! The premium buyers' live purses (#3599): server-only, in memory. One LF_BuyerPurse per buyer plus good
//! that has been sold to since the world started; a buyer with no purse yet stands full. The pure rules
//! (refill by elapsed seconds, accept up to the purse, price the units) are LF_BuyerDemand; this book only
//! holds the state between sales. Saved as the "buyerdemand" section (LF_BuyerDemandSave.c), so a restart
//! neither refills nor empties a purse: the saved stamp refills it for the time the server was down.
class LF_BuyerDemandBook
{
	static const string FEATURE = "Market";

	protected static ref map<string, ref LF_BuyerPurse> s_mPurses = new map<string, ref LF_BuyerPurse>();

	//------------------------------------------------------------------------------------------------
	//! \return The demand row for a buyer and a good, or null when the config has none (or did not load).
	static LF_BuyerDemandRow FindRow(string buyer, ResourceName good)
	{
		LF_BuyerDemandConfig config = LF_BuyerDemandConfig.Get();
		if (!config)
			return null;

		return config.Find(buyer, good);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: sells `units` of `good` to `buyer` at `nowUnix`. The purse is created full on the first
	//! sale, refilled for the time since its stamp, then spent by what the buyer accepts.
	//! \return What the sale came to, or null when the buyer has no demand row for the good (nothing booked).
	static LF_BuyerDemandSale Sell(string buyer, ResourceName good, int units, int nowUnix)
	{
		LF_BuyerDemandRow row = FindRow(buyer, good);
		if (!row)
			return null;

		LF_BuyerPurse purse = PurseFor(row, nowUnix);
		LF_BuyerDemandSale sale = LF_BuyerDemand.Sell(row, purse, units, nowUnix, LF_BuyerDemandConfig.CurveFor(row));
		LF_Debug.Info(FEATURE, string.Format("demand %1/%2 %3/%4 (sold %5, refused %6, paid %7)", buyer, good, purse.m_iRemaining, row.m_iCap, sale.m_iAccepted, sale.m_iRefused, sale.m_iPayout));
		return sale;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The lot size of the buyer's row for `good` (#3622), 1 with no row.
	static int LotOf(string buyer, ResourceName good)
	{
		LF_BuyerDemandRow row = FindRow(buyer, good);
		if (!row)
			return 1;

		return row.Lot();
	}

	//------------------------------------------------------------------------------------------------
	//! \return Units the buyer would take for `good` at `nowUnix` (refill applied); 0 with no row.
	static int RemainingAt(string buyer, ResourceName good, int nowUnix)
	{
		LF_BuyerDemandRow row = FindRow(buyer, good);
		if (!row)
			return 0;

		LF_BuyerPurse purse = PurseFor(row, nowUnix);
		LF_BuyerDemand.Refill(row, purse, nowUnix);
		return purse.m_iRemaining;
	}

	//------------------------------------------------------------------------------------------------
	//! \return A copy of the stored purse, or null when the buyer was never sold to for that good. A sale
	//! snapshots it first so a failed payout or item removal can put it back exactly.
	static LF_BuyerPurse Snapshot(string buyer, ResourceName good)
	{
		LF_BuyerPurse purse = s_mPurses.Get(KeyOf(buyer, good));
		if (!purse)
			return null;

		LF_BuyerPurse copy = new LF_BuyerPurse();
		copy.m_iRemaining = purse.m_iRemaining;
		copy.m_iStamp = purse.m_iStamp;
		return copy;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: puts back a state taken by Snapshot (null removes the purse: full again).
	static void Restore(string buyer, ResourceName good, LF_BuyerPurse snapshot)
	{
		string key = KeyOf(buyer, good);
		if (!snapshot)
		{
			s_mPurses.Remove(key);
			return;
		}

		LF_BuyerPurse copy = new LF_BuyerPurse();
		copy.m_iRemaining = snapshot.m_iRemaining;
		copy.m_iStamp = snapshot.m_iStamp;
		s_mPurses.Set(key, copy);
	}

	//------------------------------------------------------------------------------------------------
	//! The live purse for a row, a full one (clock at `nowUnix`) when none is stored yet.
	protected static LF_BuyerPurse PurseFor(notnull LF_BuyerDemandRow row, int nowUnix)
	{
		string key = KeyOf(row.m_sBuyer, row.m_sGood);
		LF_BuyerPurse purse = s_mPurses.Get(key);
		if (!purse)
		{
			purse = LF_BuyerPurse.Full(row, nowUnix);
			s_mPurses.Set(key, purse);
		}

		return purse;
	}

	//------------------------------------------------------------------------------------------------
	static string KeyOf(string buyer, ResourceName good)
	{
		return LF_BuyerDemandList.RowId(buyer, good);
	}

	//------------------------------------------------------------------------------------------------
	//! The live purses keyed by buyer plus good GUID (LF_BuyerDemandSavePayload reads and replaces them).
	static map<string, ref LF_BuyerPurse> Entries()
	{
		return s_mPurses;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_mPurses = new map<string, ref LF_BuyerPurse>();
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BuyerDemandBook's statics (LF_StaticReset finds it by reflection).
class LF_BuyerDemandBook_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BuyerDemandBook";
	}

	override void Apply()
	{
		LF_BuyerDemandBook.ResetStatics();
	}
}
