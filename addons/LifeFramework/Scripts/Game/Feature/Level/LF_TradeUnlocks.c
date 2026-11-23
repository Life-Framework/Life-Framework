//------------------------------------------------------------------------------------------------
//! One thing a trade level opens (#4185): a site, a tool, a buyer or a perk. The text is a
//! localization key; the level-up card prefixes it with "Opens:" and F2 shows the next one.
[BaseContainerProps()]
class LF_TradeUnlockRow
{
	[Attribute("", UIWidgets.EditBox, "Trade track id, e.g. FARMER, MINER, SMELTING, DELIVERY")]
	string m_sJob;

	[Attribute("2", UIWidgets.Auto, "The level that opens this", params: "2 30")]
	int m_iLevel;

	[Attribute("", UIWidgets.EditBox, "Localization key naming what the level opens, the unlock text row")]
	string m_sTextKey;

	//------------------------------------------------------------------------------------------------
	static LF_TradeUnlockRow Make(string job, int level, string key)
	{
		LF_TradeUnlockRow row = new LF_TradeUnlockRow();
		row.m_sJob = job;
		row.m_iLevel = level;
		row.m_sTextKey = key;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! The unlock table (#4185): one row per trade per level 2 to 30. The core ships no rows (what a
//! level opens is what this server is); a pack lists a `.conf` of this class in its header and
//! `Get` layers it, a later row for the same trade and level replacing an earlier one.
[BaseContainerProps(configRoot: true)]
class LF_TradeUnlocksConfig
{
	static const int FIRST_LEVEL = 2;
	static const int LAST_LEVEL = 30;
	static const string SECTION = "tradeUnlocks";

	[Attribute("", UIWidgets.Auto, "Unlock rows: trade, level, text key")]
	ref array<ref LF_TradeUnlockRow> m_aRows;

	protected static ref LF_TradeUnlocksConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The layered table, never null; empty when no pack lists an overlay.
	static LF_TradeUnlocksConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = new LF_TradeUnlocksConfig();
		s_Config.m_aRows = {};

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(LF_JobsConfig.CONFIG_PATH, LF_TradeUnlocksConfig, SECTION, resources, names);
		foreach (int i, Resource resource : resources)
		{
			LF_TradeUnlocksConfig layer = FromContainer(resource.GetResource().ToBaseContainer());
			if (!layer)
			{
				LF_Debug.Error("Level", string.Format("trade unlock overlay has an unexpected root class, skipped: %1", names[i]));
				continue;
			}

			foreach (LF_TradeUnlockRow row : layer.m_aRows)
				s_Config.Put(row);
		}

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads one `.conf` of this class by resource name, or null (logged) when it does not resolve.
	static LF_TradeUnlocksConfig Load(ResourceName path)
	{
		Resource resource = BaseContainerTools.LoadContainer(path);
		if (!resource || !resource.IsValid() || !resource.GetResource())
		{
			LF_Debug.Error("Level", string.Format("trade unlock config does not resolve: %1", path));
			return null;
		}

		return FromContainer(resource.GetResource().ToBaseContainer());
	}

	//------------------------------------------------------------------------------------------------
	protected static LF_TradeUnlocksConfig FromContainer(BaseContainer container)
	{
		if (!container)
			return null;

		LF_TradeUnlocksConfig config = LF_TradeUnlocksConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
		if (config && !config.m_aRows)
			config.m_aRows = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: drops the cached table so the next Get layers the pack again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void Put(LF_TradeUnlockRow row)
	{
		if (!row)
			return;

		foreach (int i, LF_TradeUnlockRow have : m_aRows)
		{
			if (have.m_sJob == row.m_sJob && have.m_iLevel == row.m_iLevel)
			{
				m_aRows[i] = row;
				return;
			}
		}

		m_aRows.Insert(row);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The text key naming what `job` opens at `level`, or empty when no row names one.
	string KeyFor(string job, int level)
	{
		if (!m_aRows)
			return "";

		foreach (LF_TradeUnlockRow row : m_aRows)
		{
			if (row && row.m_sJob == job && row.m_iLevel == level)
				return row.m_sTextKey;
		}

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! What a level opens, for the level-up card and the F2 detail card (#4185).
class LF_TradeUnlocks
{
	//------------------------------------------------------------------------------------------------
	//! \return The `#` key naming what `job` opens at `level`; empty for level 1, past 30 or a
	//! trade with no row (the card then shows no "Opens:" line).
	static string KeyFor(string job, int level)
	{
		if (level < LF_TradeUnlocksConfig.FIRST_LEVEL || level > LF_TradeUnlocksConfig.LAST_LEVEL)
			return "";

		return LF_TradeUnlocksConfig.Get().KeyFor(job, level);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The key of the next level's unlock after `level`, or empty at the top.
	static string NextKey(string job, int level)
	{
		return KeyFor(job, level + 1);
	}
}
