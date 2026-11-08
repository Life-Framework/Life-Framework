//------------------------------------------------------------------------------------------------
//! Survival tuning. Loaded from Configs/Survival/LF_Survival.conf; the stats hold no rates.
//! Survival needs are light (docs/decisions.md, 2026-11-08): hunger and thirst drain slowly, at
//! worst they hold health at a cap and make sprinting drain stamina faster, and they never kill.
//! The field initializers match the flagship .conf so a missing file degrades to the same values.
[BaseContainerProps(configRoot: true)]
class LF_SurvivalConfig
{
	static const ResourceName CONFIG_PATH = "{19BD17974CD58F52}Configs/Survival/LF_Survival.conf";

	[Attribute("0.01", UIWidgets.Auto, "Hunger lost per second (0-100 scale). 0.01 empties a full stomach in about 2.8 hours")]
	float m_fHungerDrainPerSecond = 0.01;

	[Attribute("0.0125", UIWidgets.Auto, "Thirst lost per second (0-100 scale). 0.0125 empties in about 2.2 hours")]
	float m_fThirstDrainPerSecond = 0.0125;

	[Attribute("20", UIWidgets.Auto, "Hunger or thirst below this counts as low needs")]
	float m_fLowNeedThreshold = 20;

	[Attribute("0.75", UIWidgets.Auto, "With low needs, the character's real health (fraction of max) falls to this cap and no further. Needs never kill", params: "0 1")]
	float m_fHealthCapScaled = 0.75;

	[Attribute("0.0005", UIWidgets.Auto, "Fraction of max health lost per second while needs are low and health is above the cap. 0.0005 reaches a 0.75 cap from full in about 8 minutes")]
	float m_fHealthDrainScaledPerSecond = 0.0005;

	[Attribute("0.001", UIWidgets.Auto, "Fraction of max health given back per second once needs are met, only up to what needs took. 0 disables the give-back")]
	float m_fHealthRestoreScaledPerSecond = 0.001;

	[Attribute("1.5", UIWidgets.Auto, "Stamina drain multiplier while needs are low (1.5 = sprinting tires 50% faster). 1 disables the penalty")]
	float m_fStaminaDrainMultiplier = 1.5;

	[Attribute("1", UIWidgets.Auto, "Seconds between server survival ticks")]
	float m_fTickIntervalSeconds = 1;

	[Attribute("5", UIWidgets.Auto, "Longest delta one tick may apply, so a server hitch does not drop a big chunk")]
	float m_fMaxTickDeltaSeconds = 5;

	protected static ref LF_SurvivalConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config. When the file does not resolve, logs once and returns built-in defaults.
	static LF_SurvivalConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_SurvivalConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			EL_Debug.Error("Survival", string.Format("survival config does not resolve, using defaults: %1", CONFIG_PATH));
			s_Config = new LF_SurvivalConfig();
		}

		return s_Config;
	}
}
