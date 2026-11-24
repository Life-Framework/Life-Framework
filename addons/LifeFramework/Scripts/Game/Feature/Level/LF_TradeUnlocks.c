//------------------------------------------------------------------------------------------------
//! What backs one unlock row (#4869). A row either names a real gate at its level and says which
//! one in m_sRef, or is a MILESTONE line that promises nothing. A flagship pack test
//! checks every non-milestone row against the config or the code that
//! holds the gate, by these two fields and never by the text.
enum LF_ETradeUnlockKind
{
	MILESTONE,		//!< An honest marker of the level: no gate, no perk. The card and F2 show its text as it is, with no "Opens:"
	LICENSE,		//!< A licence the jobs config gates at this level in this trade; m_sRef is the LF_ELicenseType name
	MACHINE,		//!< A machine whose m_sLevelJob and m_iMinLevel are this trade and level; m_sRef is the machine id
	MACHINE_STEP,	//!< A step of a machine (LF_MachineStep) that starts at this level; m_sRef is the machine id
	PERK_PICK,		//!< A perk pick (LF_PerkPick, levels 10 and 20) that the trade offers; m_sRef is empty
	FISH,			//!< A fish species whose m_iMinFisherLevel is this level; m_sRef is the species id
	ACTIVITY,		//!< An activity whose m_sTrade and m_iMinTradeLevel are this trade and level; m_sRef is the activity id
	DELIVERY_JOB,	//!< A delivery job sign-up (LF_CivilianJob.RequiredLevel); m_sRef is the LF_EJobType name
	EXPORT_ORDERS	//!< The export order board (LF_ExportOrderConfig.m_iMinLevel); m_sRef is empty
}

//------------------------------------------------------------------------------------------------
//! One thing a trade level opens (#4185): a site, a tool, a buyer or a perk. The text is a
//! localization key; the level-up card prefixes it with "Opens:" and F2 shows the next one. A row of
//! kind MILESTONE opens nothing and shows its text with no prefix (#4869).
[BaseContainerProps()]
class LF_TradeUnlockRow
{
	[Attribute("", UIWidgets.EditBox, "Trade track id, e.g. FARMER, MINER, SMELTING, DELIVERY")]
	string m_sJob;

	[Attribute("2", UIWidgets.Auto, "The level that opens this, 2 to 30")]
	int m_iLevel;

	[Attribute("", UIWidgets.EditBox, "Localization key naming what the level opens, the unlock text row")]
	string m_sTextKey;

	[Attribute("0", UIWidgets.ComboBox, "What backs the row: a real gate, or MILESTONE for an honest marker with no promise (#4869)", "", ParamEnumArray.FromEnum(LF_ETradeUnlockKind))]
	LF_ETradeUnlockKind m_eKind;

	[Attribute("", UIWidgets.EditBox, "The gate the row names, by its kind: a licence, machine, species, activity or job id. Empty for a milestone, a perk pick and the export board")]
	string m_sRef;

	//------------------------------------------------------------------------------------------------
	static LF_TradeUnlockRow Make(string job, int level, string key, LF_ETradeUnlockKind kind = LF_ETradeUnlockKind.MILESTONE, string gateRef = "")
	{
		LF_TradeUnlockRow row = new LF_TradeUnlockRow();
		row.m_sJob = job;
		row.m_iLevel = level;
		row.m_sTextKey = key;
		row.m_eKind = kind;
		row.m_sRef = gateRef;
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
	//! \return The row of `job` at `level`, or null when none names one.
	LF_TradeUnlockRow RowFor(string job, int level)
	{
		if (!m_aRows)
			return null;

		foreach (LF_TradeUnlockRow row : m_aRows)
		{
			if (row && row.m_sJob == job && row.m_iLevel == level)
				return row;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when the row carrying this text key is a MILESTONE (#4869).
	bool IsMilestoneKey(string key)
	{
		if (!m_aRows || key.IsEmpty())
			return false;

		foreach (LF_TradeUnlockRow row : m_aRows)
		{
			if (row && row.m_sTextKey == key)
				return row.m_eKind == LF_ETradeUnlockKind.MILESTONE;
		}

		return false;
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
	//! \return True when the row behind this text key is a milestone: the card and F2 show its text
	//! as it is, with no "Opens:" in front, because it opens nothing (#4869).
	static bool IsMilestoneKey(string key)
	{
		return LF_TradeUnlocksConfig.Get().IsMilestoneKey(key);
	}

	//------------------------------------------------------------------------------------------------
	//! \return The key of the next level's unlock after `level`, or empty at the top.
	static string NextKey(string job, int level)
	{
		return KeyFor(job, level + 1);
	}
}
