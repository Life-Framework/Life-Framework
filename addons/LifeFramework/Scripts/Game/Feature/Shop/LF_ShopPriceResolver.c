//------------------------------------------------------------------------------------------------
//! Pure price-rule resolution for the shop system. Kept apart from the manager on purpose: the
//! same rules decide what a shop menu shows, what price the server charges, and what a player is
//! paid - one code path so the client's display and the server's authority cannot drift apart.
//! See LF_ShopPricesConfig for the rule shape.
//------------------------------------------------------------------------------------------------
class LF_ShopPriceResolver
{
	//------------------------------------------------------------------------------------------------
	//! The stable key a config layer uses to address a rule: the find substring when set, else the
	//! type and mode it matches. Layers merge rules by this key, never by list position; it is also
	//! the record key in the owner's "priceRules" object.
	//! \param[in] find The rule's prefab-path substring.
	//! \param[in] itemType The rule's arsenal type flags.
	//! \param[in] itemMode The rule's arsenal mode.
	//! \return The key, e.g. "Prefabs/Items/Food/" or "type:64,mode:2".
	static string RuleKey(string find, int itemType, int itemMode)
	{
		if (!find.IsEmpty())
			return find;

		return string.Format("%1%2,mode:%3", TYPE_KEY_PREFIX, itemType, itemMode);
	}

	static const string TYPE_KEY_PREFIX = "type:";

	//------------------------------------------------------------------------------------------------
	//! Reverses RuleKey. "type:<flags>" or "type:<flags>,mode:<mode>" is a type rule (mode defaults
	//! to DEFAULT); any other non-empty key is a find substring.
	//! \return False for an empty key or a type key whose numbers do not parse.
	static bool ParseRuleKey(string key, out string find, out int itemType, out int itemMode)
	{
		find = string.Empty;
		itemType = 0;
		itemMode = SCR_EArsenalItemMode.DEFAULT;
		if (key.IsEmpty())
			return false;

		if (!key.StartsWith(TYPE_KEY_PREFIX))
		{
			find = key;
			return true;
		}

		array<string> parts = {};
		key.Split(",", parts, false);
		foreach (int i, string part : parts)
		{
			array<string> pair = {};
			part.Split(":", pair, false);
			if (pair.Count() != 2 || !IsDigits(pair[1]))
				return false;

			if (i == 0 && pair[0] == "type")
				itemType = pair[1].ToInt();
			else if (i == 1 && pair[0] == "mode")
				itemMode = pair[1].ToInt();
			else
				return false;
		}

		return parts.Count() <= 2;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDigits(string text)
	{
		if (text.IsEmpty())
			return false;

		for (int i = 0; i < text.Length(); i++)
		{
			string c = text.Get(i);
			if (c < "0" || c > "9")
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Finds the rule a layer addresses.
	//! \param[in] rules The rule list. May be null.
	//! \param[in] key A key from RuleKey.
	//! \return The last rule with that key (the one that wins the walk), or null.
	static LF_ShopPriceRule FindRule(array<ref LF_ShopPriceRule> rules, string key)
	{
		if (!rules)
			return null;

		for (int i = rules.Count() - 1; i >= 0; i--)
		{
			LF_ShopPriceRule rule = rules[i];
			if (rule && RuleKey(rule.m_sFind, rule.m_eItemType, rule.m_eItemMode) == key)
				return rule;
		}

		return null;
	}

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
	static bool RuleMatches(LF_ShopPriceRule rule, ResourceName prefabName, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode)
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
	static void ResolvePrices(array<ref LF_ShopPriceRule> rules, ResourceName prefabName, SCR_EArsenalItemType itemType, SCR_EArsenalItemMode itemMode, int defaultBuyPrice, int defaultSellPrice, out int buyPrice, out int sellPrice, out bool hidden)
	{
		buyPrice = defaultBuyPrice;
		sellPrice = defaultSellPrice;
		hidden = false;

		if (!rules)
			return;

		foreach (LF_ShopPriceRule rule : rules)
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