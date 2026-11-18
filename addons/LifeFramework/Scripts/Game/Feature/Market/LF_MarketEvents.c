//------------------------------------------------------------------------------------------------
//! Market events on the server bus (docs/architecture/events.md). Fields are plain values, never
//! IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A player sale moved a good's price on the curve (#2845). Published by LF_TraderSale.Sell after the
//! sale completed (paid, price moved, item taken); a sale that rolled back publishes nothing.
//! Recovery over time is applied lazily on the next quote and publishes nothing.
class LF_OnMarketPriceMoved : LF_Event
{
	//! The item prefab, the market config's key.
	ResourceName m_sGood;
	int m_iOldPrice;
	int m_iNewPrice;

	static LF_OnMarketPriceMoved Create(ResourceName good, int oldPrice, int newPrice)
	{
		LF_OnMarketPriceMoved e = new LF_OnMarketPriceMoved();
		e.m_sGood = good;
		e.m_iOldPrice = oldPrice;
		e.m_iNewPrice = newPrice;
		return e;
	}
}
