[BaseContainerProps()]
class EL_TraderItem
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Item prefab", "et")]
	ResourceName m_ItemPrefab;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.Auto, desc: "Value per item in dollars")]
	int m_ValuePerItem;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Is this item illegal?")]
	bool m_bIllegal;
}

class EL_TraderManagerComponentClass: GameComponentClass
{
};


class EL_TraderManagerComponent : GameComponent
{
	[Attribute("", UIWidgets.Object, "List of tradable items with values")]
	ref array<ref EL_TraderItem> m_aTradableItems;

	[Attribute(defvalue: "false", uiwidget: UIWidgets.CheckBox, desc: "Is this a black market trader?")]
	bool m_bBlackMarket;

	//------------------------------------------------------------------------------------------------
	//! Sell seam: the unit value a trader pays for one prefab. An unlisted prefab, a missing list,
	//! or a null row is worth 0, which the sell path refuses.
	static int GetUnitValue(array<ref EL_TraderItem> tradableItems, ResourceName itemPrefab)
	{
		if (!tradableItems)
			return 0;

		foreach (EL_TraderItem tradableItem : tradableItems)
		{
			if (tradableItem && tradableItem.m_ItemPrefab == itemPrefab)
				return tradableItem.m_ValuePerItem;
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Sell seam: black market access. Fails closed: an account that cannot be verified as
	//! civilian is not admitted.
	static bool CanAccessBlackMarket(EL_PlayerAccount account)
	{
		if (!account)
			return false;

		return account.GetFaction() == EL_Faction.CIVILIAN;
	}
}
