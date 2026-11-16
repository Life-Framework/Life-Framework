//------------------------------------------------------------------------------------------------
//! Stock and restock of an outlet offer (#1064). An offer with `m_iStock` > 0 holds that many units;
//! each sale takes some, a sale beyond what is left is refused (OUT_OF_STOCK), and the first sale after
//! a full shelf starts the restock clock: `m_iRestockSeconds` later the shelf is full again.
//! `m_iStock` 0 is no limit, `m_iRestockSeconds` 0 is no restock until the server restarts.
//! Server memory only: nothing is saved, so a restart refills every outlet.
class LF_CatalogueStock
{
	protected static ref map<string, int> s_mLeft;
	protected static ref map<string, int> s_mRefillAtMs;

	//------------------------------------------------------------------------------------------------
	protected static void Ensure()
	{
		if (!s_mLeft)
			s_mLeft = new map<string, int>();
		if (!s_mRefillAtMs)
			s_mRefillAtMs = new map<string, int>();
	}

	//------------------------------------------------------------------------------------------------
	//! Forgets every count: every outlet is full again (a restart, a reload, tests).
	static void Reset()
	{
		s_mLeft = null;
		s_mRefillAtMs = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Units this offer can still sell now; -1 when the offer has no stock limit. A restock that has come
	//! due is applied here, so the answer is always current.
	static int Left(LF_OutletOffer offer, int nowMs)
	{
		if (!offer || offer.m_iStock <= 0)
			return -1;

		Ensure();
		string key = offer.Key();
		int refillAt;
		if (s_mRefillAtMs.Find(key, refillAt) && nowMs - refillAt >= 0)
		{
			s_mRefillAtMs.Remove(key);
			s_mLeft.Remove(key);
			LF_Debug.Info(LF_ItemCatalogue.FEATURE, string.Format("outlet %1 restocked %2 of %3", offer.m_sOutlet, offer.m_sItem, offer.m_iStock));
		}

		int left;
		if (!s_mLeft.Find(key, left))
			return offer.m_iStock;

		// A layer that lowered the stock lowers what is on the shelf too.
		if (left > offer.m_iStock)
			return offer.m_iStock;
		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the offer has `units` to sell now (always true without a stock limit).
	static bool CanSupply(LF_OutletOffer offer, int units, int nowMs)
	{
		int left = Left(offer, nowMs);
		if (left < 0)
			return true;
		return left >= units;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes `units` off the shelf after a sale and starts the restock clock when it is not running.
	static void Take(LF_OutletOffer offer, int units, int nowMs)
	{
		int left = Left(offer, nowMs);
		if (left < 0 || units <= 0)
			return;

		left -= units;
		if (left < 0)
			left = 0;

		string key = offer.Key();
		s_mLeft.Set(key, left);
		if (offer.m_iRestockSeconds > 0 && !s_mRefillAtMs.Contains(key))
			s_mRefillAtMs.Set(key, nowMs + offer.m_iRestockSeconds * 1000);

		LF_Debug.Info(LF_ItemCatalogue.FEATURE, string.Format("outlet %1 sold %2 x%3, %4 left", offer.m_sOutlet, offer.m_sItem, units, left));
	}
}
