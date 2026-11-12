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
class LF_ShopPricesConfig
{
	//! Buy price applied to any item no rule matched. The player pays this to the shop.
	[Attribute("50", UIWidgets.Auto, desc: "Default buy price when no rule matches", params: "0 1000000")]
	int m_iDefaultBuyPrice;

	//! Sell price applied to any item no rule matched. The shop pays this to the player.
	[Attribute("25", UIWidgets.Auto, desc: "Default sell price when no rule matches", params: "0 1000000")]
	int m_iDefaultSellPrice;

	//! Ordered price rules. Later matches override earlier ones.
	[Attribute("", UIWidgets.Object, desc: "Ordered price rules; later matches override earlier ones")]
	ref array<ref LF_ShopPriceRule> m_aPriceRules;

	//! Prefabs to price beyond the entity catalog. The catalog only knows items registered in an
	//! entity catalog; the mod's own items (and any base item a server wants on the shelves) are
	//! listed here so the shop shows them with rule prices.
	[Attribute("", UIWidgets.ResourcePickerThumbnail, desc: "Extra item prefabs priced outside the entity catalog", "et")]
	ref array<ResourceName> m_aExtraItemPrefabs;

	//! The owner config.json section this class reads (LF_ConfigService).
	static const string SECTION = "shops";

	//! The limits of every price, the same numbers as the `params: "0 1000000"` on the price attributes.
	//! The owner reader refuses a price outside them (#279).
	static const int PRICE_MIN = 0;	// lf-fixed: the schema limit, mirrors the attribute params
	static const int PRICE_MAX = 1000000;	// lf-fixed: the schema limit, mirrors the attribute params

	//------------------------------------------------------------------------------------------------
	//! Merges the config layers over this instance, which holds the core `.conf`: first each pack
	//! overlay, then the owner's "shops" section. Each layer changes only the fields it sets; rules
	//! merge by LF_ShopPriceResolver.RuleKey, and a rule with a new key is appended (so it wins the walk).
	//! \param[in] coreConf The core resource this instance was created from.
	void ApplyLayers(ResourceName coreConf)
	{
		array<ResourceName> overlays = {};
		LF_ConfigService.GetPackOverlays(coreConf, overlays);
		foreach (ResourceName overlay : overlays)
		{
			Resource resource = BaseContainerTools.LoadContainer(overlay);
			if (!resource || !resource.IsValid() || !resource.GetResource())
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("shops: pack overlay does not resolve, skipped: %1", overlay));
				continue;
			}

			BaseContainer container = resource.GetResource().ToBaseContainer();
			typename overlayType;
			if (container)
				overlayType = container.GetClassName().ToType();
			if (!overlayType || !overlayType.IsInherited(LF_ShopPricesConfig))
			{
				LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("shops: pack overlay is not an LF_ShopPricesConfig, skipped: %1", overlay));
				continue;
			}

			int changed = ApplyContainer(container);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("shops: pack overlay %1 set %2 fields", overlay, changed));
		}

		LF_ConfigReader reader = LF_ConfigService.OpenOwnerSection(SECTION);
		if (reader)
		{
			ApplyOwner(reader);
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("shops: owner config read %1 fields, rejected %2", reader.GetAppliedCount(), reader.GetRejectedCount()));
		}

		// Client: the server's effective values replace the result (#285). Null on the server.
		LF_ConfigReader streamed = LF_ConfigStream.OpenReceivedSection(SECTION);
		if (streamed && ApplyStream(streamed))
			LF_Debug.Info(LF_ConfigService.FEATURE, string.Format("shops: using the server's values (%1 rules, default buy %2, sell %3)", m_aPriceRules.Count(), m_iDefaultBuyPrice, m_iDefaultSellPrice));
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the effective values a client reads into the config stream (LF_ConfigStream, #285):
	//! the defaults, every rule in walk order, and the extra prefabs. Every field here is client
	//! visible (the price list shows it). Rules are keyed "0".."n-1", not by RuleKey: prefab rules
	//! share a key, and ApplyStream replaces the list instead of merging it.
	//! \param[in] ctx A save context with this section's object open.
	void WriteStream(notnull SCR_JsonSaveContext ctx)
	{
		ctx.WriteValue("defaultBuyPrice", m_iDefaultBuyPrice);
		ctx.WriteValue("defaultSellPrice", m_iDefaultSellPrice);

		ctx.StartObject("priceRules");
		if (m_aPriceRules)
		{
			int index;
			foreach (LF_ShopPriceRule rule : m_aPriceRules)
			{
				if (!rule)
					continue;

				ctx.StartObject(index.ToString());
				string rulePrefab = rule.m_Prefab;
				if (!rulePrefab.IsEmpty())
					ctx.WriteValue("prefab", rulePrefab);
				if (!rule.m_sFind.IsEmpty())
					ctx.WriteValue("find", rule.m_sFind);
				int itemType = rule.m_eItemType;
				if (itemType != 0)
					ctx.WriteValue("type", itemType);
				int itemMode = rule.m_eItemMode;
				if (itemMode != SCR_EArsenalItemMode.DEFAULT)
					ctx.WriteValue("mode", itemMode);
				ctx.WriteValue("buyPrice", rule.m_iBuyPrice);
				ctx.WriteValue("sellPrice", rule.m_iSellPrice);
				if (rule.m_bHidden)
					ctx.WriteValue("hidden", true);
				ctx.EndObject();
				index++;
			}
		}
		ctx.EndObject();

		array<string> extra = {};
		if (m_aExtraItemPrefabs)
		{
			foreach (ResourceName extraPrefab : m_aExtraItemPrefabs)
			{
				extra.Insert(extraPrefab);
			}
		}
		ctx.WriteValue("extraItemPrefabs", extra);
	}

	//------------------------------------------------------------------------------------------------
	//! Replaces this instance's values with the server's from the config stream. Reads into a copy
	//! and commits only when every field read: a stream with a bad field changes nothing (logged).
	//! A key the stream leaves out keeps this instance's value.
	//! \param[in] reader A reader on the stream's "shops" section.
	//! \return True when the server's values were applied.
	bool ApplyStream(notnull LF_ConfigReader reader)
	{
		int defaultBuy = m_iDefaultBuyPrice;
		int defaultSell = m_iDefaultSellPrice;
		reader.ReadInt("defaultBuyPrice", defaultBuy, PRICE_MIN, PRICE_MAX);
		reader.ReadInt("defaultSellPrice", defaultSell, PRICE_MIN, PRICE_MAX);

		array<ref LF_ShopPriceRule> rules;
		int count = reader.BeginRecords("priceRules");
		if (count >= 0)
		{
			rules = new array<ref LF_ShopPriceRule>();
			for (int i = 0; i < count; i++)
			{
				string key = reader.GetRecordKey(i);
				if (key != i.ToString())
				{
					reader.RejectRecord(string.Format("record %1 is keyed '%2', not '%1'", i, key));
					continue;
				}

				if (!reader.BeginRecord(key))
					continue;

				string prefab;
				string find;
				int itemType = 0;
				int itemMode = SCR_EArsenalItemMode.DEFAULT;
				reader.ReadString("prefab", prefab);
				reader.ReadString("find", find);
				reader.ReadInt("type", itemType);
				reader.ReadInt("mode", itemMode);

				LF_ShopPriceRule rule = LF_ShopPriceRule.Create(find, itemType, itemMode);
				rule.m_Prefab = prefab;
				reader.ReadInt("buyPrice", rule.m_iBuyPrice, PRICE_MIN, PRICE_MAX);
				reader.ReadInt("sellPrice", rule.m_iSellPrice, PRICE_MIN, PRICE_MAX);
				reader.ReadBool("hidden", rule.m_bHidden);
				rules.Insert(rule);
				reader.EndRecord();
			}
			reader.EndRecords();
		}

		array<string> extraRead = {};
		bool hasExtra = reader.ReadStrings("extraItemPrefabs", extraRead);

		if (reader.GetRejectedCount() > 0)
		{
			LF_Debug.Error(LF_ConfigService.FEATURE, string.Format("stream: %1 shop values from the server did not read; the client keeps its own shop prices", reader.GetRejectedCount()));
			return false;
		}

		m_iDefaultBuyPrice = defaultBuy;
		m_iDefaultSellPrice = defaultSell;
		if (rules)
			m_aPriceRules = rules;
		if (hasExtra)
		{
			m_aExtraItemPrefabs = new array<ResourceName>();
			foreach (string extra : extraRead)
			{
				m_aExtraItemPrefabs.Insert(extra);
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf` of this class: only the fields the file sets directly.
	//! \param[in] container The overlay's root container.
	//! \return The number of fields it set.
	int ApplyContainer(notnull BaseContainer container)
	{
		int changed;
		if (container.IsVariableSetDirectly("m_iDefaultBuyPrice") && container.Get("m_iDefaultBuyPrice", m_iDefaultBuyPrice))
			changed++;
		if (container.IsVariableSetDirectly("m_iDefaultSellPrice") && container.Get("m_iDefaultSellPrice", m_iDefaultSellPrice))
			changed++;

		if (!m_aPriceRules)
			m_aPriceRules = new array<ref LF_ShopPriceRule>();

		BaseContainerList rules = container.GetObjectArray("m_aPriceRules");
		if (rules)
		{
			for (int i = 0; i < rules.Count(); i++)
			{
				BaseContainer ruleContainer = rules.Get(i);
				if (!ruleContainer)
					continue;

				string find;
				int itemType;
				int itemMode;
				ruleContainer.Get("m_sFind", find);
				ruleContainer.Get("m_eItemType", itemType);
				ruleContainer.Get("m_eItemMode", itemMode);

				LF_ShopPriceRule rule = LF_ShopPriceResolver.FindRule(m_aPriceRules, LF_ShopPriceResolver.RuleKey(find, itemType, itemMode));
				if (!rule)
				{
					rule = LF_ShopPriceRule.Cast(BaseContainerTools.CreateInstanceFromContainer(ruleContainer));
					if (!rule)
						continue;
					m_aPriceRules.Insert(rule);
					changed++;
					continue;
				}

				if (ruleContainer.IsVariableSetDirectly("m_iBuyPrice") && ruleContainer.Get("m_iBuyPrice", rule.m_iBuyPrice))
					changed++;
				if (ruleContainer.IsVariableSetDirectly("m_iSellPrice") && ruleContainer.Get("m_iSellPrice", rule.m_iSellPrice))
					changed++;
				if (ruleContainer.IsVariableSetDirectly("m_bHidden") && ruleContainer.Get("m_bHidden", rule.m_bHidden))
					changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_aExtraItemPrefabs"))
		{
			array<ResourceName> extra = {};
			if (container.Get("m_aExtraItemPrefabs", extra))
				changed += AddExtraItems(extra);
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Applies the owner's "shops" section. Keys: defaultBuyPrice, defaultSellPrice, priceRules
	//! and extraItemPrefabs (added to the shipped list). priceRules is an object keyed by
	//! LF_ShopPriceResolver.RuleKey (the find substring, or "type:<flags>,mode:<mode>"); each record
	//! may set prefab (a {GUID}path, matched by GUID), buyPrice, sellPrice and hidden. A key no shipped rule has adds a rule.
	//! \param[in] reader A reader on the section.
	void ApplyOwner(notnull LF_ConfigReader reader)
	{
		array<string> ownerKeys = {"defaultBuyPrice", "defaultSellPrice", "priceRules", "extraItemPrefabs"};
		reader.WarnUnknownKeys(ownerKeys);
		reader.ReadInt("defaultBuyPrice", m_iDefaultBuyPrice, PRICE_MIN, PRICE_MAX);
		reader.ReadInt("defaultSellPrice", m_iDefaultSellPrice, PRICE_MIN, PRICE_MAX);

		if (!m_aPriceRules)
			m_aPriceRules = new array<ref LF_ShopPriceRule>();

		int count = reader.BeginRecords("priceRules");
		for (int i = 0; i < count; i++)
		{
			string key = reader.GetRecordKey(i);
			if (!reader.BeginRecord(key))
				continue;

			string find;
			int itemType;
			int itemMode;
			if (!LF_ShopPriceResolver.ParseRuleKey(key, find, itemType, itemMode))
			{
				reader.RejectRecord("the key is neither a prefab-path substring nor type:<flags>,mode:<mode>");
				reader.EndRecord();
				continue;
			}

			// "type:64" and "type:64,mode:2" address the same rule.
			LF_ShopPriceRule rule = LF_ShopPriceResolver.FindRule(m_aPriceRules, LF_ShopPriceResolver.RuleKey(find, itemType, itemMode));
			if (!rule)
			{
				rule = LF_ShopPriceRule.Create(find, itemType, itemMode);
				m_aPriceRules.Insert(rule);
			}

			array<string> ruleKeys = {"prefab", "buyPrice", "sellPrice", "hidden"};
			reader.WarnUnknownKeys(ruleKeys);
			string prefab = rule.m_Prefab;
			if (reader.ReadString("prefab", prefab))
				rule.m_Prefab = prefab;
			reader.ReadInt("buyPrice", rule.m_iBuyPrice, PRICE_MIN, PRICE_MAX);
			reader.ReadInt("sellPrice", rule.m_iSellPrice, PRICE_MIN, PRICE_MAX);
			reader.ReadBool("hidden", rule.m_bHidden);
			reader.EndRecord();
		}

		if (count >= 0)
			reader.EndRecords();

		array<string> extra = {};
		if (reader.ReadStrings("extraItemPrefabs", extra))
		{
			array<ResourceName> prefabs = {};
			foreach (string prefab : extra)
			{
				prefabs.Insert(prefab);
			}
			AddExtraItems(prefabs);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Adds prefabs to the extra list, skipping ones already there.
	//! \return How many were added.
	protected int AddExtraItems(array<ResourceName> prefabs)
	{
		if (!m_aExtraItemPrefabs)
			m_aExtraItemPrefabs = new array<ResourceName>();

		int added;
		foreach (ResourceName prefab : prefabs)
		{
			if (prefab.IsEmpty() || m_aExtraItemPrefabs.Contains(prefab))
				continue;
			m_aExtraItemPrefabs.Insert(prefab);
			added++;
		}
		return added;
	}

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
		LF_ShopPriceResolver.ResolvePrices(m_aPriceRules, prefabName, itemType, itemMode, m_iDefaultBuyPrice, m_iDefaultSellPrice, buyPrice, sellPrice, hidden);
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
		LF_ShopPriceResolver.ResolvePrices(m_aPriceRules, prefabName, itemType, itemMode, m_iDefaultBuyPrice, m_iDefaultSellPrice, buyPrice, sellPrice, hidden);
		return sellPrice;
	}
};

//------------------------------------------------------------------------------------------------
//! One priced class of items. Matches one prefab by GUID when m_Prefab is set; else by prefab-path
//! substring when m_sFind is set; else by arsenal type/mode. An empty m_Prefab and m_sFind plus the
//! default type 0 and DEFAULT mode prices everything. A rule for one prefab uses m_Prefab: the GUID
//! survives a move into the pack, the path text does not. m_sFind is for folders and name families,
//! and a resource move must update it.
//------------------------------------------------------------------------------------------------
[BaseContainerProps()]
class LF_ShopPriceRule
{
	//! Arsenal type flags this rule applies to. 0 matches any type.
	[Attribute("0", UIWidgets.Auto, desc: "Arsenal item type flags; 0 matches any type")]
	SCR_EArsenalItemType m_eItemType;

	//! Arsenal mode this rule applies to. DEFAULT matches any mode.
	[Attribute("2", UIWidgets.Auto, desc: "Arsenal item mode; DEFAULT matches any mode")]
	SCR_EArsenalItemMode m_eItemMode;

	//! One prefab, matched by GUID (LF_Utils.SamePrefab). Wins over m_sFind and type/mode.
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "One prefab, matched by GUID; wins over the path substring and type/mode", "et")]
	ResourceName m_Prefab;

	//! Substring to match against the prefab path. Blank matches the whole type/mode.
	[Attribute("", UIWidgets.Auto, desc: "Substring of the prefab path; blank matches the whole type/mode")]
	string m_sFind;

	//! Price the player pays to buy this item.
	[Attribute("100", UIWidgets.Auto, desc: "Buy price: what the player pays", params: "0 1000000")]
	int m_iBuyPrice;

	//! Price the shop pays the player for this item.
	[Attribute("50", UIWidgets.Auto, desc: "Sell price: what the shop pays the player", params: "0 1000000")]
	int m_iSellPrice;

	//! When a matching rule is hidden, the item is never sold anywhere. A hidden match ends the
	//! rule walk, so later rules do not re-enable it.
	[Attribute("false", UIWidgets.CheckBox, desc: "Never offer this item in any shop")]
	bool m_bHidden;

	//------------------------------------------------------------------------------------------------
	//! A rule built in script, with the same defaults a `.conf` rule gets from its attributes
	//! (buy 100, sell 50, shown). The owner layer adds rules this way.
	static LF_ShopPriceRule Create(string find, int itemType, int itemMode)
	{
		LF_ShopPriceRule rule = new LF_ShopPriceRule();
		rule.m_sFind = find;
		rule.m_eItemType = itemType;
		rule.m_eItemMode = itemMode;
		rule.m_iBuyPrice = 100;
		rule.m_iSellPrice = 50;
		rule.m_bHidden = false;
		return rule;
	}
};