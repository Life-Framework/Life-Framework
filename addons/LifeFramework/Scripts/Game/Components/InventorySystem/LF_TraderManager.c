[BaseContainerProps()]
class LF_TraderItem
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Item prefab", "et")]
	ResourceName m_ItemPrefab;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Value per item in dollars")]
	int m_ValuePerItem;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Is this item illegal?")]
	bool m_bIllegal;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Posted on the chalk price board beside this trader. When any row of a trader is posted, the board lists those rows at this trader's own prices; with none posted it shows the market's goods")]
	bool m_bPosted;
}

class LF_TraderManagerComponentClass: GameComponentClass
{
};


class LF_TraderManagerComponent : GameComponent
{
	[Attribute("", UIWidgets.Object, "List of tradable items with values")]
	ref array<ref LF_TraderItem> m_aTradableItems;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Is this a black market trader?")]
	bool m_bBlackMarket;

	[Attribute("", UIWidgets.EditBox, "Premium buyer id (#3599). When a LF_BuyerDemand row names this buyer and the good, the sale pays the row's price up to its demand instead of the list value above. Empty is an ordinary trader")]
	string m_sBuyerId;

	//------------------------------------------------------------------------------------------------
	//! Sell seam: the unit value a trader pays for one prefab. An unlisted prefab, a missing list,
	//! or a null row is worth 0, which the sell path refuses.
	static int GetUnitValue(array<ref LF_TraderItem> tradableItems, ResourceName itemPrefab)
	{
		if (!tradableItems)
			return 0;

		foreach (LF_TraderItem tradableItem : tradableItems)
		{
			if (tradableItem && LF_Utils.SamePrefab(tradableItem.m_ItemPrefab, itemPrefab))
				return tradableItem.m_ValuePerItem;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Sell seam: black market access. Fails closed: an account that cannot be verified as
	//! civilian is not admitted.
	static bool CanAccessBlackMarket(LF_PlayerAccount account)
	{
		if (!account)
			return false;

		return account.GetFaction() == LF_Faction.CIVILIAN;
	}
}
