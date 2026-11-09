//------------------------------------------------------------------------------------------------
//! Money ledger retention. Loaded from Configs/Money/LF_Ledger.conf. A missing or broken file
//! logs once and falls back to the defaults.
[BaseContainerProps(configRoot: true)]
class LF_LedgerConfig
{
	static const ResourceName CONFIG_PATH = "{5A3C91E07B2D4F68}Configs/Money/LF_Ledger.conf";

	static const int DEFAULT_MAX_ENTRIES = 20000;
	static const int DEFAULT_MAX_AGE_DAYS = 30;

	[Attribute("20000", UIWidgets.Auto, "Most transactions kept. The oldest are dropped first")]
	int m_iMaxEntries;

	[Attribute("30", UIWidgets.Auto, "Transactions older than this many days are dropped. 0 keeps them until the count limit")]
	int m_iMaxAgeDays;

	protected static ref LF_LedgerConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_LedgerConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Ledger", string.Format("ledger config does not resolve, using defaults: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_LedgerConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Ledger", string.Format("ledger config has an unexpected root class, using defaults: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxEntries()
	{
		LF_LedgerConfig config = Get();
		if (!config || config.m_iMaxEntries <= 0)
			return DEFAULT_MAX_ENTRIES;

		return config.m_iMaxEntries;
	}

	//------------------------------------------------------------------------------------------------
	static int MaxAgeDays()
	{
		LF_LedgerConfig config = Get();
		if (!config || config.m_iMaxAgeDays < 0)
			return DEFAULT_MAX_AGE_DAYS;

		return config.m_iMaxAgeDays;
	}
}
