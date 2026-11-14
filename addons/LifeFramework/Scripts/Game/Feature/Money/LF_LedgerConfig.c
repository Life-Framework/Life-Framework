//------------------------------------------------------------------------------------------------
//! Money ledger retention. Loaded from Configs/Money/LF_Ledger.conf. A missing or broken file
//! logs once and falls back to the defaults.
[BaseContainerProps(configRoot: true)]
class LF_LedgerConfig
{
	static const ResourceName CONFIG_PATH = "{5A3C91E07B2D4F68}Configs/Money/LF_Ledger.conf";

	static const int DEFAULT_MAX_ENTRIES = 20000;
	static const int DEFAULT_MAX_AGE_DAYS = 30;
	static const int DEFAULT_ATM_MAX_TRANSACTION = 1000000;
	static const float DEFAULT_ATM_RANGE = 5;

	[Attribute("20000", UIWidgets.Auto, "Most transactions kept. The oldest are dropped first")]
	int m_iMaxEntries;

	[Attribute("30", UIWidgets.Auto, "Transactions older than this many days are dropped. 0 keeps them until the count limit")]
	int m_iMaxAgeDays;

	[Attribute("1000000", UIWidgets.Auto, "Largest single ATM deposit or withdrawal the server accepts. Also guards balances against oversized payloads", params: "1 100000000")]
	int m_iAtmMaxTransaction;

	[Attribute("5", UIWidgets.Auto, "Metres a player may stand from an ATM when the server takes a deposit or withdrawal", params: "1 30")]
	float m_fAtmRange;

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

	//------------------------------------------------------------------------------------------------
	static int AtmMaxTransaction()
	{
		LF_LedgerConfig config = Get();
		if (!config || config.m_iAtmMaxTransaction <= 0)
			return DEFAULT_ATM_MAX_TRANSACTION;

		return config.m_iAtmMaxTransaction;
	}

	//------------------------------------------------------------------------------------------------
	static float AtmRange()
	{
		LF_LedgerConfig config = Get();
		if (!config || config.m_fAtmRange <= 0)
			return DEFAULT_ATM_RANGE;

		return config.m_fAtmRange;
	}
}
