//------------------------------------------------------------------------------------------------
//! Pure price-rule resolution for the shop system. Kept apart from the manager on purpose: the
//! same rules decide what a shop menu shows, what price the server charges, and what a player is
//! paid - one code path so the client's display and the server's authority cannot drift apart.
//! See EL_ShopPricesConfig for the rule shape.
//------------------------------------------------------------------------------------------------
class EL_ShopPriceResolver
{
	//------------------------------------------------------------------------------------------------
	//! Whether one rule applies to one item.
	//! A name filter wins over type/mode: a rule with m_sFind set matches by prefab-path substring
	//! regardless of arsenal data. A rule without one matches when the item's type flags intersect
	//! the rule's type (or the rule matches any type) and the mode matches (or the rule matches any
	//! mode, i.e. the DEFAULT wildcard).
	//! \param[in] rule The rule to test.
	//! \param[in] prefabName Prefab path of the item.
	//! \param[in] itemType Arsenal type flags of the item.
	//! \param[in] itemMode Arsenal mode of the item.
	//! \return True when the rule prices the item.
	static bool RuleMatches(EL_ShopPriceRule rule, ResourceName prefabName, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode)
	{
		if (!rule)
			return false;

		if (!rule.m_sFind.IsEmpty())
			return prefabName.IndexOf(rule.m_sFind) > -1;

		bool typeMatches = (int)rule.m_eItemType == 0 || ((int)itemType & (int)rule.m_eItemType) != 0;
		if (!typeMatches)
			return false;

		if (rule.m_eItemMode == SCR_EArsenalItemMode.DEFAULT)
			return true;

		return itemMode == rule.m_eItemMode;
	}

	//------------------------------------------------------------------------------------------------
	//! Runs the price rules over one item, in order, later matches overriding earlier ones.
	//! A hidden match ends the walk: hidden items have no price and are never sold.
	//! \param[in] rules The ordered rule list. May be null.
	//! \param[in] prefabName Prefab path of the item.
	//! \param[in] itemType Arsenal type flags of the item.
	//! \param[in] itemMode Arsenal mode of the item.
	//! \param[in] defaultBuyPrice Buy price when nothing matches.
	//! \param[in] defaultSellPrice Sell price when nothing matches.
	//! \param[out] buyPrice The buy price to show and charge.
	//! \param[out] sellPrice The sell price to show and pay.
	//! \param[out] hidden True when a hidden rule matched.
	static void ResolvePrices(array<ref EL_ShopPriceRule> rules, ResourceName prefabName, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode, int defaultBuyPrice, int defaultSellPrice, out int buyPrice, out int sellPrice, out bool hidden)
	{
		buyPrice = defaultBuyPrice;
		sellPrice = defaultSellPrice;
		hidden = false;

		if (!rules)
			return;

		foreach (EL_ShopPriceRule rule : rules)
		{
			if (!RuleMatches(rule, prefabName, itemType, itemMode))
				continue;

			if (rule.m_bHidden)
			{
				hidden = true;
				return;
			}

			buyPrice = rule.m_iBuyPrice;
			sellPrice = rule.m_iSellPrice;
		}
	}
};