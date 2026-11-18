//------------------------------------------------------------------------------------------------
//! The premium buyers' table (#3267). Loaded from Configs/Market/LF_BuyerDemand.conf. The core ships no
//! rows: which buyer wants which good, how much and at what price is the pack's decision. A pack overlay
//! (`m_aLF_ConfigOverlays`, #3018) merges the rows by buyer plus good, the way LF_MarketConfig merges
//! goods; there is no owner config.json section.
[BaseContainerProps(configRoot: true)]
class LF_BuyerDemandConfig
{
	static const string FEATURE = "Market";
	static const ResourceName CONFIG_PATH = "{ABF6B5EAA951F747}Configs/Market/LF_BuyerDemand.conf";

	[Attribute("", UIWidgets.Object, "One row per buyer and good")]
	ref array<ref LF_BuyerDemandRow> m_aRows;

	protected static ref LF_BuyerDemandConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config with unusable rows removed, or null (logged once) when the file does not resolve.
	static LF_BuyerDemandConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("buyer demand config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_BuyerDemandConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("buyer demand config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		s_Config.Sanitize();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay (the mission header's m_aLF_ConfigOverlays, in load order) over this
	//! instance, which holds the core `.conf`: rows by buyer plus good (add, replace, patch, remove;
	//! docs/config/override-model.md). Sanitize runs after, so a bad merged row is refused like a core one.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_BuyerDemandConfig, "buyer-demand", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info(FEATURE, string.Format("buyer demand: pack overlay %1 changed %2 entries", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay's `m_aRows`. \return Entries changed.
	int ApplyPackContainer(notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!m_aRows)
			m_aRows = {};

		return LF_ConfigOverlay.ApplyContainerList(new LF_BuyerDemandList(this), root, "m_aRows", source, claims);
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
	static void UseForTest(LF_BuyerDemandConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Refuses the unusable rows at load: each is removed with `LF_Debug.Error("Market", ...)` naming it,
	//! so one bad row never degrades the others. A repeated buyer plus good keeps its first row.
	//! \return How many rows were removed.
	int Sanitize()
	{
		if (!m_aRows)
			m_aRows = {};

		int removed = 0;
		for (int i = m_aRows.Count() - 1; i >= 0; i--)
		{
			LF_BuyerDemandRow row = m_aRows[i];
			string problem;
			if (!row)
				problem = "empty row";
			else
				problem = row.Problem();

			if (problem.IsEmpty() && FirstIndexOf(row.m_sBuyer, row.m_sGood) != i)
				problem = "the buyer and good are listed twice";

			if (problem.IsEmpty())
				continue;

			string name = "(empty)";
			if (row)
				name = row.Label();

			LF_Debug.Error(FEATURE, string.Format("buyer demand '%1' refused at load: %2", name, problem));
			m_aRows.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstIndexOf(string buyer, ResourceName good)
	{
		for (int i = 0; i < m_aRows.Count(); i++)
		{
			if (m_aRows[i] && m_aRows[i].m_sBuyer == buyer && m_aRows[i].m_sGood == good)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The row for a buyer and a good, or null when the buyer has no demand row for it.
	LF_BuyerDemandRow Find(string buyer, ResourceName good)
	{
		if (!m_aRows || buyer.IsEmpty() || good.IsEmpty())
			return null;

		foreach (LF_BuyerDemandRow row : m_aRows)
		{
			if (row && row.m_sBuyer == buyer && row.m_sGood == good)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The market curve a row names, or null for a flat price. A named good the market config does
	//! not list logs `LF_Debug.Warn("Market", ...)` and prices flat.
	static LF_MarketGoodDef CurveFor(notnull LF_BuyerDemandRow row)
	{
		if (row.m_sCurveGood.IsEmpty())
			return null;

		LF_MarketConfig market = LF_MarketConfig.Get();
		LF_MarketGoodDef curve;
		if (market)
			curve = market.FindGood(row.m_sCurveGood);

		if (!curve)
			LF_Debug.Warn(FEATURE, string.Format("buyer demand '%1' names curve good %2 that the market does not list; pricing flat", row.Label(), row.m_sCurveGood));

		return curve;
	}
}

//------------------------------------------------------------------------------------------------
//! The rows list for the shared merge (LF_ConfigOverlay), keyed by buyer plus the good's GUID. A new or
//! replaced row must pass LF_BuyerDemandRow.Problem; a patch that would leave a row unusable is refused
//! whole and the row keeps its values.
class LF_BuyerDemandList : LF_ConfigList
{
	protected LF_BuyerDemandConfig m_Config;

	void LF_BuyerDemandList(LF_BuyerDemandConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aRows)
			m_Config.m_aRows = {};
	}

	//------------------------------------------------------------------------------------------------
	static string RowId(string buyer, ResourceName good)
	{
		return buyer + "|" + LF_MarketGoodList.GoodId(good);
	}

	override string GetName()
	{
		return "buyerDemand.rows";
	}

	override int Count()
	{
		return m_Config.m_aRows.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aRows[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Cast(record);
		return RowId(row.m_sBuyer, row.m_sGood);
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string buyer;
		ResourceName good;
		container.Get("m_sBuyer", buyer);
		container.Get("m_sGood", good);
		return RowId(buyer, good);
	}

	override string IdHint()
	{
		return "a buyer id and a good's prefab GUID";
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Cast(record);
		if (index < m_Config.m_aRows.Count())
			m_Config.m_aRows.Set(index, row);
		else
			m_Config.m_aRows.Insert(row);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRows.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		return LF_BuyerDemandRow.Cast(record).Problem();
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_BuyerDemandRow row = LF_BuyerDemandRow.Cast(record);
		LF_BuyerDemandRow before = LF_BuyerDemandRow.Make(row.m_sBuyer, row.m_sGood, row.m_iCap, row.m_iRefillPerHour, row.m_iPrice, row.m_sCurveGood);
		int changed;
		if (container.IsVariableSetDirectly("m_iCap") && container.Get("m_iCap", row.m_iCap))
			changed++;
		if (container.IsVariableSetDirectly("m_iRefillPerHour") && container.Get("m_iRefillPerHour", row.m_iRefillPerHour))
			changed++;
		if (container.IsVariableSetDirectly("m_iPrice") && container.Get("m_iPrice", row.m_iPrice))
			changed++;
		if (container.IsVariableSetDirectly("m_sCurveGood") && container.Get("m_sCurveGood", row.m_sCurveGood))
			changed++;

		string problem = row.Problem();
		if (!problem.IsEmpty())
		{
			LF_Debug.Error(LF_BuyerDemandConfig.FEATURE, string.Format("buyerDemand.rows %1: pack patch refused, keeping the shipped row: %2", row.Label(), problem));
			row.m_iCap = before.m_iCap;
			row.m_iRefillPerHour = before.m_iRefillPerHour;
			row.m_iPrice = before.m_iPrice;
			row.m_sCurveGood = before.m_sCurveGood;
			return 0;
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_BuyerDemandConfig's statics.
class LF_BuyerDemandConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_BuyerDemandConfig";
	}

	override void Apply()
	{
		LF_BuyerDemandConfig.Reload();
	}
}
