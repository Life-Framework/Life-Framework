//------------------------------------------------------------------------------------------------
//! The Laundering feature's one config (M7, parent #1025). Loaded from Configs/Crime/LF_Launder.conf;
//! a pack overrides the file. Core ships every switch off, so a server without the pack pays crime
//! in plain cash exactly as before.
[BaseContainerProps(configRoot: true)]
class LF_LaunderConfig
{
	static const ResourceName CONFIG_PATH = "{20A4113B243EC80B}Configs/Crime/LF_Launder.conf";

	[Attribute("0", UIWidgets.CheckBox, "Till robberies and bank vault bags pay marked notes (LF_MarkedCash) instead of cash. Chop shop and fence stay cash.")]
	bool m_bCrimePaysMarked;

	protected static ref LF_LaunderConfig s_Config;
	protected static bool s_bLoadAttempted;
	//! Tests only: -1 reads the config, 0 forces the switch off, 1 forces it on.
	protected static int s_iCrimePaysMarkedForTests = -1;

	//------------------------------------------------------------------------------------------------
	static LF_LaunderConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Laundering", string.Format("launder config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_LaunderConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Laundering", string.Format("launder config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether robberies and the bank pay marked notes. Off when the config is missing (logged in Get).
	static bool CrimePaysMarked()
	{
		if (s_iCrimePaysMarkedForTests >= 0)
			return s_iCrimePaysMarkedForTests == 1;

		LF_LaunderConfig config = Get();
		if (!config)
			return false;

		return config.m_bCrimePaysMarked;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: force the switch on or off; ClearCrimePaysMarkedForTests goes back to the config.
	static void SetCrimePaysMarkedForTests(bool on)
	{
		if (on)
			s_iCrimePaysMarkedForTests = 1;
		else
			s_iCrimePaysMarkedForTests = 0;
	}

	//------------------------------------------------------------------------------------------------
	static void ClearCrimePaysMarkedForTests()
	{
		s_iCrimePaysMarkedForTests = -1;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
		s_iCrimePaysMarkedForTests = -1;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_LaunderConfig's statics (LF_StaticReset finds it by reflection).
class LF_LaunderConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_LaunderConfig";
	}

	override void Apply()
	{
		LF_LaunderConfig.ResetStatics();
	}
}
