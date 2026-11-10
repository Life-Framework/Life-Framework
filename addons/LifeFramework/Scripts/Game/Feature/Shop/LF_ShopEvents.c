//------------------------------------------------------------------------------------------------
//! Shop events on the server bus (docs/architecture/events.md). Fields are plain values: uids and
//! ints, never IEntity or EntityID. Published on the server only, after the state change happened.
//! A new event of this feature is appended to this file, not to a central list.

//! A fence sold a tool (#271). Published by LF_ShopComponent.Buy after the cash was taken and the
//! tool delivered, so the serial book (#272) can stamp the tool and name its buyer.
class LF_OnFenceSale : LF_Event
{
	string m_sBuyerUid;
	string m_sPrefab;
	int m_iQuantity;
	int m_iPrice;

	static LF_OnFenceSale Create(string buyerUid, string prefab, int quantity, int price)
	{
		LF_OnFenceSale e = new LF_OnFenceSale();
		e.m_sBuyerUid = buyerUid;
		e.m_sPrefab = prefab;
		e.m_iQuantity = quantity;
		e.m_iPrice = price;
		return e;
	}
}
