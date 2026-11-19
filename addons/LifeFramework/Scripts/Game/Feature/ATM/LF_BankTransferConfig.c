//------------------------------------------------------------------------------------------------
//! Bank transfer settings. Loaded from Configs/Money/LF_BankTransfer.conf. A missing or broken file
//! logs once and the getters fall back to the defaults. Read by LF_BankTransferRules (#3318, #638).
[BaseContainerProps(configRoot: true)]
class LF_BankTransferConfig
{
	static const ResourceName CONFIG_PATH = "{97EBA3F005CE13F5}Configs/Money/LF_BankTransfer.conf";

	static const int DEFAULT_FEE = 0;
	static const int DEFAULT_FEE_PERCENT = 0;
	static const int DEFAULT_DAILY_LIMIT = 50000;

	[Attribute("1", UIWidgets.CheckBox, "Whether bank transfers are on. Off refuses every transfer")]
	bool m_bEnabled = true;

	[Attribute("0", UIWidgets.Auto, "Flat fee charged to the sender on every transfer, on top of the amount", params: "0 1000000")]
	int m_iFee = 0;

	[Attribute("0", UIWidgets.Auto, "Percent of the amount charged to the sender on top of the flat fee (whole percent, rounded down)", params: "0 100")]
	int m_iFeePercent = 0;

	[Attribute("50000", UIWidgets.Auto, "Most one account may send in a day, fees not counted. 0 means no limit", params: "0 100000000")]
	int m_iDailyLimit = 50000;

	protected static ref LF_BankTransferConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! Reads a config file. Null (logged) when it does not resolve or has another root class.
	static LF_BankTransferConfig Load(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("BankTransfer", string.Format("transfer config does not resolve, using defaults: %1", path));
			return null;
		}

		LF_BankTransferConfig config = LF_BankTransferConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error("BankTransfer", string.Format("transfer config has an unexpected root class, using defaults: %1", path));

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped config, or null (logged once) when the file does not resolve.
	static LF_BankTransferConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = Load(CONFIG_PATH);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! The shipped config, or a fresh object holding the defaults when the file is unusable.
	static LF_BankTransferConfig GetOrDefaults()
	{
		LF_BankTransferConfig config = Get();
		if (config)
			return config;

		return new LF_BankTransferConfig();
	}
}
