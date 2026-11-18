//------------------------------------------------------------------------------------------------
//! One tradable good's price curve (#2844). The key is `m_sGood`, the item prefab ResourceName the trader
//! list uses. All prices are whole cash units.
[BaseContainerProps()]
class LF_MarketGoodDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Item prefab, the same key the trader list uses. The record key", "et")]
	ResourceName m_sGood;

	[Attribute("0", UIWidgets.Auto, "Price a unit fetches when nothing has been sold lately", params: "0 10000000")]
	int m_iBase;

	[Attribute("0", UIWidgets.Auto, "Lowest a unit ever fetches", params: "0 10000000")]
	int m_iFloor;

	[Attribute("0", UIWidgets.Auto, "Highest a unit ever fetches", params: "0 10000000")]
	int m_iCeiling;

	[Attribute("0", UIWidgets.Auto, "Percent of the base each unit sold takes off the price; 0 keeps the price fixed", params: "0 100")]
	int m_iDropPerUnitPct;

	[Attribute("0", UIWidgets.Auto, "Percent of the gap to the base the price closes each real hour; 0 never recovers", params: "0 100")]
	int m_iRecoverPctPerHour;

	//------------------------------------------------------------------------------------------------
	static LF_MarketGoodDef Make(ResourceName good, int base, int floorPrice, int ceilingPrice, int dropPct, int recoverPct)
	{
		LF_MarketGoodDef def = new LF_MarketGoodDef();
		def.m_sGood = good;
		def.m_iBase = base;
		def.m_iFloor = floorPrice;
		def.m_iCeiling = ceilingPrice;
		def.m_iDropPerUnitPct = dropPct;
		def.m_iRecoverPctPerHour = recoverPct;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the row is usable, else why not: no good, a negative price, a floor above the
	//! ceiling, a base outside floor..ceiling, or a percent outside 0..100.
	string Problem()
	{
		if (m_sGood.IsEmpty())
			return "no good";
		if (m_iFloor < 0 || m_iBase < 0 || m_iCeiling < 0)
			return "a price is negative";
		if (m_iFloor > m_iCeiling)
			return string.Format("floor %1 is above ceiling %2", m_iFloor, m_iCeiling);
		if (m_iBase < m_iFloor || m_iBase > m_iCeiling)
			return string.Format("base %1 is outside floor %2 and ceiling %3", m_iBase, m_iFloor, m_iCeiling);
		if (m_iDropPerUnitPct < 0 || m_iDropPerUnitPct > 100)
			return string.Format("drop per unit %1 percent is outside 0 to 100", m_iDropPerUnitPct);
		if (m_iRecoverPctPerHour < 0 || m_iRecoverPctPerHour > 100)
			return string.Format("recovery %1 percent per hour is outside 0 to 100", m_iRecoverPctPerHour);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return Cash one sold unit takes off this good's price.
	int DropPerUnit()
	{
		return LF_PriceCurve.DropPerUnit(m_iBase, m_iDropPerUnitPct);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The next unit's price from `current`.
	int NextPrice(int current)
	{
		return LF_PriceCurve.NextPrice(current, m_iFloor, m_iCeiling);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The total `units` pay from `current`.
	int BulkTotal(int current, int units)
	{
		return LF_PriceCurve.BulkTotal(current, units, DropPerUnit(), m_iFloor, m_iCeiling);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price after `units` were sold from `current`.
	int PriceAfterSale(int current, int units)
	{
		return LF_PriceCurve.PriceAfterSale(current, units, DropPerUnit(), m_iFloor, m_iCeiling);
	}
}

//------------------------------------------------------------------------------------------------
//! The market's tables. Loaded from Configs/Market/LF_Market.conf. The core ships it off
//! (`m_bEnabled` 0) with three neutral goods, so no server changes behaviour until it opts in.
//! A pack overlay (`m_aLF_ConfigOverlays`, #3018) merges the goods by `m_sGood` and may switch the market
//! on; the owner config.json section comes with a later card.
[BaseContainerProps(configRoot: true)]
class LF_MarketConfig
{
	static const string FEATURE = "Market";
	static const ResourceName CONFIG_PATH = "{C09173518BDDA8A4}Configs/Market/LF_Market.conf";

	[Attribute("0", UIWidgets.CheckBox, "Whether sale prices follow the curve. Off keeps every price fixed")]
	bool m_bEnabled;

	[Attribute("168", UIWidgets.Auto, "Most real hours of recovery one gap counts: a long offline gap is clamped to this", params: "0 8760")]
	int m_iMaxRecoveryHours;

	[Attribute("", UIWidgets.Object, "Goods with a price curve; a good with no row sells at its fixed price")]
	ref array<ref LF_MarketGoodDef> m_aGoods;

	protected static ref LF_MarketConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config with unusable goods removed, or null (logged once) when the file does not resolve.
	static LF_MarketConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("market config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_MarketConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("market config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		s_Config.Sanitize();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay (the mission header's m_aLF_ConfigOverlays, in load order) over this
	//! instance, which holds the core `.conf`: the goods by `m_sGood` (add, replace, patch, remove;
	//! docs/config/override-model.md), then `m_bEnabled` and `m_iMaxRecoveryHours` when the overlay sets
	//! them on its root. Sanitize runs after, so a bad merged row is refused like a core one.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_MarketConfig, "market", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("market: pack overlay %1 changed %2 entries", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay: its `m_aGoods` by good, and the root fields it sets directly.
	//! \return Entries changed (goods plus root fields).
	//! \param[in] root The overlay's root container.
	//! \param[in] source The overlay file, for log lines.
	int ApplyPackContainer(notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!m_aGoods)
			m_aGoods = {};

		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_MarketGoodList(this), root, "m_aGoods", source, claims);
		bool enabled;
		if (root.IsVariableSetDirectly("m_bEnabled") && root.Get("m_bEnabled", enabled))
		{
			m_bEnabled = enabled;
			changed++;
		}

		int hours;
		if (root.IsVariableSetDirectly("m_iMaxRecoveryHours") && root.Get("m_iMaxRecoveryHours", hours))
		{
			m_iMaxRecoveryHours = hours;
			changed++;
		}

		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` again (world end, tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_MarketConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Refuses the unusable goods at load: each is removed with `LF_Debug.Error("Market", ...)` naming
	//! it, so one bad row never degrades the others. A repeated good keeps its first row.
	//! \return How many rows were removed.
	int Sanitize()
	{
		if (!m_aGoods)
			m_aGoods = {};

		if (m_iMaxRecoveryHours < 0)
			m_iMaxRecoveryHours = 0;

		int removed = 0;
		for (int i = m_aGoods.Count() - 1; i >= 0; i--)
		{
			LF_MarketGoodDef def = m_aGoods[i];
			string problem;
			if (!def)
				problem = "empty row";
			else
				problem = def.Problem();

			if (problem.IsEmpty() && FirstIndexOf(def.m_sGood) != i)
				problem = "the good is listed twice";

			if (problem.IsEmpty())
				continue;

			string name = "(empty)";
			if (def)
				name = def.m_sGood;

			LF_Debug.Error(FEATURE, string.Format("market good '%1' refused at load: %2", name, problem));
			m_aGoods.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstIndexOf(ResourceName good)
	{
		for (int i = 0; i < m_aGoods.Count(); i++)
		{
			if (m_aGoods[i] && m_aGoods[i].m_sGood == good)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The curve of a good, or null when it has none (it sells at its fixed price).
	LF_MarketGoodDef FindGood(ResourceName good)
	{
		if (!m_aGoods || good.IsEmpty())
			return null;

		foreach (LF_MarketGoodDef def : m_aGoods)
		{
			if (def && def.m_sGood == good)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The price of `good` after `seconds` of recovery from `current`; `current` when the good has no curve.
	int Recover(ResourceName good, int current, int seconds)
	{
		LF_MarketGoodDef def = FindGood(good);
		if (!def)
			return current;

		return LF_PriceCurve.Recover(current, def.m_iBase, def.m_iRecoverPctPerHour, seconds, m_iMaxRecoveryHours, def.m_iFloor, def.m_iCeiling);
	}
}

//------------------------------------------------------------------------------------------------
//! The goods list for the shared merge (LF_ConfigOverlay), keyed by the good's GUID. There is no owner
//! section, so only pack overlays reach it. A new or replaced row must pass LF_MarketGoodDef.Problem; a
//! patch that would leave a row unusable is refused whole and the row keeps its values.
class LF_MarketGoodList : LF_ConfigList
{
	protected LF_MarketConfig m_Config;

	void LF_MarketGoodList(LF_MarketConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aGoods)
			m_Config.m_aGoods = {};
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}path" to "GUID" in upper case; a bare path comes back upper-cased whole.
	static string GoodId(ResourceName good)
	{
		string text = good;
		if (text.StartsWith("{"))
		{
			int close = text.IndexOf("}");
			if (close > 1)
				text = text.Substring(1, close - 1);
		}

		text.ToUpper();
		return text;
	}

	override string GetName()
	{
		return "market.goods";
	}

	override int Count()
	{
		return m_Config.m_aGoods.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aGoods[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return GoodId(LF_MarketGoodDef.Cast(record).m_sGood);
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		ResourceName good;
		container.Get("m_sGood", good);
		return GoodId(good);
	}

	override string IdHint()
	{
		return "a good's prefab GUID";
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_MarketGoodDef def = LF_MarketGoodDef.Cast(record);
		if (index < m_Config.m_aGoods.Count())
			m_Config.m_aGoods.Set(index, def);
		else
			m_Config.m_aGoods.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aGoods.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_MarketGoodDef.Cast(record).Problem();
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_MarketGoodDef def = LF_MarketGoodDef.Cast(record);
		LF_MarketGoodDef before = LF_MarketGoodDef.Make(def.m_sGood, def.m_iBase, def.m_iFloor, def.m_iCeiling, def.m_iDropPerUnitPct, def.m_iRecoverPctPerHour);
		int changed;
		if (container.IsVariableSetDirectly("m_iBase") && container.Get("m_iBase", def.m_iBase))
			changed++;
		if (container.IsVariableSetDirectly("m_iFloor") && container.Get("m_iFloor", def.m_iFloor))
			changed++;
		if (container.IsVariableSetDirectly("m_iCeiling") && container.Get("m_iCeiling", def.m_iCeiling))
			changed++;
		if (container.IsVariableSetDirectly("m_iDropPerUnitPct") && container.Get("m_iDropPerUnitPct", def.m_iDropPerUnitPct))
			changed++;
		if (container.IsVariableSetDirectly("m_iRecoverPctPerHour") && container.Get("m_iRecoverPctPerHour", def.m_iRecoverPctPerHour))
			changed++;

		string problem = def.Problem();
		if (!problem.IsEmpty())
		{
			LF_Debug.Error(LF_MarketConfig.FEATURE, string.Format("market.goods %1: pack patch refused, keeping the shipped row: %2", def.m_sGood, problem));
			def.m_iBase = before.m_iBase;
			def.m_iFloor = before.m_iFloor;
			def.m_iCeiling = before.m_iCeiling;
			def.m_iDropPerUnitPct = before.m_iDropPerUnitPct;
			def.m_iRecoverPctPerHour = before.m_iRecoverPctPerHour;
			return 0;
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_MarketConfig's statics.
class LF_MarketConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_MarketConfig";
	}

	override void Apply()
	{
		LF_MarketConfig.Reload();
	}
}
