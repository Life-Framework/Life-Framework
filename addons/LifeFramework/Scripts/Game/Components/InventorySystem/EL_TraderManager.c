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
}