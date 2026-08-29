[BaseContainerProps()]
class EL_ShopItem
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Prefab of the item to sell", "et")]
	protected ResourceName m_ItemPrefab;

	[Attribute("100", UIWidgets.Auto, desc: "Price of the item in dollars")]
	protected int m_iPrice;

	[Attribute("Item Name", UIWidgets.Auto, desc: "Display name of the item")]
	protected string m_sDisplayName;

	[Attribute("1", UIWidgets.Auto, desc: "Maximum quantity that can be bought at once")]
	protected int m_iMaxQuantity;

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