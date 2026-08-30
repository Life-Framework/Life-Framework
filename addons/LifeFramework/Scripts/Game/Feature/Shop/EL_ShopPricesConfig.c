//------------------------------------------------------------------------------------------------
//! Price table for the shop system. A config root resource referenced from the game mode:
//! Configs/Shop/ShopPrices.conf. Rules are walked in order; a later match overrides an earlier
//! one, so authors put broad defaults first and name-specific overrides last.
//!
//! Model borrowed from Overthrow's OVT_PricesConfig (MIT): instead of listing every prefab, a
//! rule prices a whole class of items by arsenal type/mode or by a substring of the prefab path.
//! Every in-game item therefore gets a default buy and sell price without hand-curating a catalog.
//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class EL_ShopPricesConfig
{
	//! Buy price applied to any item no rule matched. The player pays this to the shop.
	[Attribute("50", UIWidgets.Auto, desc: "Default buy price when no rule matches")]
	int m_iDefaultBuyPrice;

	//! Sell price applied to any item no rule matched. The shop pays this to the player.
	[Attribute("25", UIWidgets.Auto, desc: "Default sell price when no rule matches")]
	int m_iDefaultSellPrice;

	//! Ordered price rules. Later matches override earlier ones.
	[Attribute("", UIWidgets.Object, desc: "Ordered price rules; later matches override earlier ones")]
	ref array<ref EL_ShopPriceRule> m_aPriceRules;

	//! Prefabs to price beyond the entity catalog. The catalog only knows items registered in an
	//! entity catalog; the mod's own items (and any base item a server wants on the shelves) are
	//! listed here so the shop shows them with rule prices.
	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Extra item prefabs priced outside the entity catalog", "et")]
	ref array<ResourceName> m_aExtraItemPrefabs;

	//------------------------------------------------------------------------------------------------
	//! Resolves the configured (default or ruled) buy price for one item.
	//! \param[in] prefabName Prefab path of the item.
	//! \param[in] itemType Arsenal type flags of the item.
	//! \param[in] itemMode Arsenal mode of the item.
	//! \param[out] hidden Set when a hidden rule matched; such an item is never sold.
	//! \return The buy price.
	int GetBuyPrice(ResourceName prefabName, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode, out bool hidden)
	{
		int buyPrice;
		int sellPrice;
		EL_ShopPriceResolver.ResolvePrices(m_aPriceRules, prefabName, itemType, itemMode, m_iDefaultBuyPrice, m_iDefaultSellPrice, buyPrice, sellPrice, hidden);
		return buyPrice;
	}

	//------------------------------------------------------------------------------------------------
	//! Resolves the configured (default or ruled) sell price for one item.
	//! \param[in] prefabName Prefab path of the item.
	//! \param[in] itemType Arsenal type flags of the item.
	//! \param[in] itemMode Arsenal mode of the item.
	//! \param[out] hidden Set when a hidden rule matched; such an item is never sold.
	//! \return The sell price.
	int GetSellPrice(ResourceName prefabName, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode, out bool hidden)
	{
		int buyPrice;
		int sellPrice;
		EL_ShopPriceResolver.ResolvePrices(m_aPriceRules, prefabName, itemType, itemMode, m_iDefaultBuyPrice, m_iDefaultSellPrice, buyPrice, sellPrice, hidden);
		return sellPrice;
	}
};

//------------------------------------------------------------------------------------------------
//! One priced class of items. Matches by prefab-path substring when m_sFind is set, otherwise by
//! arsenal type/mode. An empty m_sFind plus the default type 0 and DEFAULT mode prices everything.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class EL_ShopPriceRule
{
	//! Arsenal type flags this rule applies to. 0 matches any type.
	[Attribute("0", UIWidgets.Auto, desc: "Arsenal item type flags; 0 matches any type")]
	SCR_EArsenalItemType m_eItemType;

	//! Arsenal mode this rule applies to. DEFAULT matches any mode.
	[Attribute("2", UIWidgets.Auto, desc: "Arsenal item mode; DEFAULT matches any mode")]
	SCR_EArsenalItemMode m_eItemMode;

	//! Substring to match against the prefab path. Blank matches the whole type/mode.
	[Attribute("", UIWidgets.Auto, desc: "Substring of the prefab path; blank matches the whole type/mode")]
	string m_sFind;

	//! Price the player pays to buy this item.
	[Attribute("100", UIWidgets.Auto, desc: "Buy price: what the player pays")]
	int m_iBuyPrice;

	//! Price the shop pays the player for this item.
	[Attribute("50", UIWidgets.Auto, desc: "Sell price: what the shop pays the player")]
	int m_iSellPrice;

	//! When a matching rule is hidden, the item is never sold anywhere. A hidden match ends the
	//! rule walk, so later rules do not re-enable it.
	[Attribute("false", UIWidgets.CheckBox, desc: "Never offer this item in any shop")]
	bool m_bHidden;
};