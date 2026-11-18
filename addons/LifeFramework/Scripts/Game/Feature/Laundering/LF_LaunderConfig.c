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

	[Attribute("4", UIWidgets.Auto, "Real hours the illicit buyer stays at one spot before moving to the next (1..168). The rotation seed is not here: it is a server secret in the profile (LF_BuyerRotation).")]
	int m_iBuyerPeriodHours;

	[Attribute("30", UIWidgets.EditBox, "House cut in percent of the marked notes washed (0..90). The clean share rounds down; the remainder stays with the house.")]
	int m_iCutPct;

	[Attribute("2000", UIWidgets.EditBox, "Largest batch of marked notes one wash takes; the rest stays marked.")]
	int m_iBatchMax;

	[Attribute("60", UIWidgets.EditBox, "Seconds one batch takes to wash.")]
	int m_iBatchSeconds;

	[Attribute("10000", UIWidgets.EditBox, "Most marked notes one character may wash per 24 real hours (session only: a restart forgives the day).")]
	int m_iDailyCap;

	protected static ref LF_LaunderConfig s_Config;
	protected static bool s_bLoadAttempted;
	//! Whether the wash fields (cut, batch, cap) passed Validate; the dirty-money switch works either way.
	protected static bool s_bRulesValid;
	//! Tests only: -1 reads the config, 0 forces the switch off, 1 forces it on.
	protected static int s_iCrimePaysMarkedForTests = -1;
	//! Tests only: 0 reads the config, above 0 is the buyer period in hours.
	protected static int s_iBuyerPeriodHoursForTests;

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

		LF_LaunderConfig loaded = LF_LaunderConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!loaded)
		{
			LF_Debug.Error("Laundering", string.Format("launder config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		Install(loaded);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Take `config` as the loaded config (Get calls it; tests call it with a built config). Bad wash
	//! fields refuse only the wash rules (GetRules reads null); the dirty-money switch still applies,
	//! so a typo in the cut never turns robberies back into clean cash.
	static void Install(LF_LaunderConfig config)
	{
		s_bLoadAttempted = true;
		s_Config = config;
		s_bRulesValid = Validate(config);
	}

	//------------------------------------------------------------------------------------------------
	//! The config for the wash rules: null when it is missing or its cut, batch or cap was refused.
	static LF_LaunderConfig GetRules()
	{
		LF_LaunderConfig config = Get();
		if (!config || !s_bRulesValid)
			return null;

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! A config the rules can use: cut 0..90 %, batch max, batch seconds and daily cap at least 1.
	//! Logs a Laundering error naming the bad field and returns false; GetRules then reads as missing.
	static bool Validate(LF_LaunderConfig config)
	{
		if (!config)
			return false;

		if (config.m_iCutPct < 0 || config.m_iCutPct > 90)
		{
			LF_Debug.Error("Laundering", string.Format("launder config refused: m_iCutPct %1 is outside 0..90", config.m_iCutPct));
			return false;
		}

		if (config.m_iBatchMax < 1 || config.m_iBatchSeconds < 1 || config.m_iDailyCap < 1)
		{
			LF_Debug.Error("Laundering", string.Format("launder config refused: m_iBatchMax %1, m_iBatchSeconds %2, m_iDailyCap %3 must each be at least 1", config.m_iBatchMax, config.m_iBatchSeconds, config.m_iDailyCap));
			return false;
		}

		return true;
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
	//! Hours the buyer stays at one spot, 1..168; 4 when the config is missing (logged in Get).
	static int BuyerPeriodHours()
	{
		if (s_iBuyerPeriodHoursForTests > 0)
			return s_iBuyerPeriodHoursForTests;

		LF_LaunderConfig config = Get();
		if (!config)
			return 4;

		return LF_Utils.ClampInt(config.m_iBuyerPeriodHours, 1, 168);
	}

	//------------------------------------------------------------------------------------------------
	//! Tests only: force the period; 0 goes back to the config.
	static void SetBuyerPeriodHoursForTests(int hours)
	{
		s_iBuyerPeriodHoursForTests = hours;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
		s_bRulesValid = false;
		s_iCrimePaysMarkedForTests = -1;
		s_iBuyerPeriodHoursForTests = 0;
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
