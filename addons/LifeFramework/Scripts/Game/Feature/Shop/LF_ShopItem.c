[BaseContainerProps()]
class LF_ShopItem
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab of the item to sell", "et")]
	protected ResourceName m_ItemPrefab;

	[Attribute("100", UIWidgets.Auto, desc: "Price of the item in dollars")]
	protected int m_iPrice;

	[Attribute("Item Name", UIWidgets.Auto, desc: "Display name of the item")]
	protected string m_sDisplayName;

	[Attribute("1", UIWidgets.Auto, desc: "Maximum quantity that can be bought at once")]
	protected int m_iMaxQuantity;

	//! Fence shops only (LF_ShopComponent.m_bFence): completed crimes on the buyer's record before
	//! this item is sold to them. 0 is open to everyone.
	[Attribute("0", UIWidgets.Auto, desc: "Fence shops: completed crimes on the buyer's record before this item is sold. 0 is no threshold")]
	protected int m_iMinCrimes;

	//------------------------------------------------------------------------------------------------
	int GetMinCrimes()
	{
		return m_iMinCrimes;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetItemPrefab()
	{
		return m_ItemPrefab;
	}

	//------------------------------------------------------------------------------------------------
	int GetPrice()
	{
		return m_iPrice;
	}

	//------------------------------------------------------------------------------------------------
	string GetDisplayName()
	{
		return m_sDisplayName;
	}

	//------------------------------------------------------------------------------------------------
	int GetMaxQuantity()
	{
		return m_iMaxQuantity;
	}
};