//------------------------------------------------------------------------------------------------
//! One product line of a physical store (#202): which item, how many the store holds, how fast it
//! restocks, and where the display copies stand. Config on LF_StoreComponent.m_aLines.
[BaseContainerProps()]
class LF_StoreLine
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab of the item on the shelf. Its price comes from the shop's price list", "et")]
	ResourceName m_ItemPrefab;

	[Attribute("6", UIWidgets.Auto, "Most units the store holds of this item, shelf and back room together")]
	int m_iMaxStock;

	[Attribute("-1", UIWidgets.Auto, "Units on a fresh world. -1 starts full")]
	int m_iStartStock;

	[Attribute("2", UIWidgets.Auto, "Units added each restock interval, up to the maximum")]
	int m_iRestockAmount;

	[Attribute("", UIWidgets.EditBox, "Shelf points, local to the store entity, written \"x y z | x y z\". One display copy stands at each; more stock than points waits in the back room")]
	string m_sSlots;
}

//------------------------------------------------------------------------------------------------
//! One saved stock count (the "storestock" payload, save version 21). Keyed by the store's stable
//! key and the item prefab, never by an entity id.
class LF_StoreStockRow : Managed
{
	string m_sStoreKey;
	string m_sPrefab;
	int m_iStock;

	//------------------------------------------------------------------------------------------------
	LF_StoreStockRow Copy()
	{
		LF_StoreStockRow copy = new LF_StoreStockRow();
		copy.m_sStoreKey = m_sStoreKey;
		copy.m_sPrefab = m_sPrefab;
		copy.m_iStock = m_iStock;
		return copy;
	}
}

//------------------------------------------------------------------------------------------------
//! What the till said (LF_Stores.Pay).
enum LF_EStorePay
{
	PAID,
	NOTHING,
	NOT_ENOUGH_CASH,
	NO_STORE,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! The pure rules of a physical store. No world, no entity; tests drive these directly.
class LF_StoreRules
{
	//! A shelf item that moves this far from its slot (metres) was taken, even if it is not in a pocket.
	static const float TAKEN_DISTANCE = 1.0;

	//------------------------------------------------------------------------------------------------
	//! \return How many display copies stand on the shelf: the stock, never more than the slots.
	static int ShelfCount(int stock, int slots)
	{
		if (stock <= 0 || slots <= 0)
			return 0;

		return Math.Min(stock, slots);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stock after one restock step: up by amount, never above the maximum. A stock
	//! already above the maximum is left alone.
	static int Restocked(int stock, int maxStock, int amount)
	{
		if (amount <= 0 || stock >= maxStock)
			return stock;

		return Math.Min(maxStock, stock + amount);
	}

	//------------------------------------------------------------------------------------------------
	//! \return A stock clamped to 0..maxStock (a saved count after the config shrank the shelf).
	static int ClampStock(int stock, int maxStock)
	{
		return Math.Clamp(stock, 0, Math.Max(maxStock, 0));
	}

	//------------------------------------------------------------------------------------------------
	//! \return The stock a fresh world starts with: startStock, or the maximum for a negative one.
	static int StartStock(int startStock, int maxStock)
	{
		if (startStock < 0)
			return Math.Max(maxStock, 0);

		return ClampStock(startStock, maxStock);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a point (local to the store, boundary centre already subtracted) is inside
	//! the flat boundary box.
	static bool InsideBox(vector local, vector halfExtents)
	{
		if (Math.AbsFloat(local[0]) > halfExtents[0])
			return false;

		return Math.AbsFloat(local[2]) <= halfExtents[2];
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a restock interval has run out. A zero interval never restocks.
	static bool RestockDue(float nowMs, float lastMs, int intervalSeconds)
	{
		if (intervalSeconds <= 0)
			return false;

		return nowMs - lastMs >= intervalSeconds * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Reads shelf points written "x y z | x y z". A piece that is empty is skipped.
	static void ParseSlots(string text, notnull array<vector> outSlots)
	{
		outSlots.Clear();
		array<string> pieces = {};
		text.Split("|", pieces, true);
		foreach (string piece : pieces)
		{
			piece.TrimInPlace();
			if (!piece.IsEmpty())
				outSlots.Insert(piece.ToVector());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return The sale total of a set of unit prices.
	static int Total(notnull array<int> prices)
	{
		int total;
		foreach (int price : prices)
		{
			if (price > 0)
				total += price;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The till toast key for a result.
	static string PayKey(LF_EStorePay result)
	{
		switch (result)
		{
			case LF_EStorePay.PAID: return "#LF-Store_Paid";
			case LF_EStorePay.NOTHING: return "#LF-Store_NothingToPay";
			case LF_EStorePay.NOT_ENOUGH_CASH: return "#LF-Store_NotEnough";
			case LF_EStorePay.NO_STORE: return "#LF-Store_NoStore";
			case LF_EStorePay.FAILED: return "#LF-Store_PayFailed";
		}

		return "";
	}
}
