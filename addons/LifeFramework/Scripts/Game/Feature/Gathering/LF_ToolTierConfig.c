//------------------------------------------------------------------------------------------------
//! The tool tier table (#4300): how much condition one use costs at each tier. Mining, felling and
//! fishing all wear the tool by this amount (LF_ToolWear). A missing or broken file degrades to the
//! built-in defaults, so wear never stops and never VMEs.
[BaseContainerProps(configRoot: true)]
class LF_ToolTierConfig
{
	static const ResourceName CONFIG_PATH = "{9B1FE28C5B730314}Configs/Gathering/LF_ToolTiers.conf";

	static const float DEFAULT_WORN = 2.0; // lf-fixed: fallback when the config does not load
	static const float DEFAULT_STANDARD = 1.0; // lf-fixed: fallback when the config does not load
	static const float DEFAULT_FINE = 0.5; // lf-fixed: fallback when the config does not load

	[Attribute("2", UIWidgets.EditBox, "Condition one use costs a worn tool (of 100)", params: "0.01 100")]
	float m_fWornWear;

	[Attribute("1", UIWidgets.EditBox, "Condition one use costs a standard tool (of 100)", params: "0.01 100")]
	float m_fStandardWear;

	[Attribute("0.5", UIWidgets.EditBox, "Condition one use costs a fine tool (of 100)", params: "0.01 100")]
	float m_fFineWear;

	protected static ref LF_ToolTierConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The core file, loaded once. Null when it is missing or broken (logged).
	static LF_ToolTierConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		s_Config = Load(CONFIG_PATH);
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads one file. Exposed for the test, which reads the shipped file directly.
	static LF_ToolTierConfig Load(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.IsValid() || !container.GetResource())
		{
			LF_Debug.Error("ToolWear", string.Format("tool tier config does not resolve: %1", path));
			return null;
		}

		LF_ToolTierConfig config = LF_ToolTierConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
		{
			LF_Debug.Error("ToolWear", string.Format("tool tier config has an unexpected root class: %1", path));
			return null;
		}

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Condition one use costs at a tier (0 worn, 1 standard, 2 fine). A tier whose row is zero or
	//! negative reads as the built-in default, so a bad edit never makes a tool immortal.
	float WearOf(int tier)
	{
		float wear = m_fStandardWear;
		float fallback = DEFAULT_STANDARD;
		if (tier <= 0)
		{
			wear = m_fWornWear;
			fallback = DEFAULT_WORN;
		}
		else if (tier >= 2)
		{
			wear = m_fFineWear;
			fallback = DEFAULT_FINE;
		}

		if (wear <= 0)
		{
			LF_Debug.Error("ToolWear", string.Format("tool tier %1 has no wear in the config, using %2", tier, fallback));
			return fallback;
		}
		return wear;
	}

	//------------------------------------------------------------------------------------------------
	//! Wear per use at a tier from the shipped config, or the built-in default when it did not load.
	static float WearPerUse(int tier)
	{
		LF_ToolTierConfig config = Get();
		if (config)
			return config.WearOf(tier);

		if (tier <= 0)
			return DEFAULT_WORN;
		if (tier >= 2)
			return DEFAULT_FINE;
		return DEFAULT_STANDARD;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
class LF_ToolTierConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ToolTierConfig";
	}

	override void Apply()
	{
		LF_ToolTierConfig.ResetStatics();
	}
}
