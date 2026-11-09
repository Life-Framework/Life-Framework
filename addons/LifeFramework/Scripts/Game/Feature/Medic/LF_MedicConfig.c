//------------------------------------------------------------------------------------------------
//! Medic tuning. Loaded from Configs/Medic/LF_Medic.conf (#255, the downed state; #256 and #259
//! add their own fields here). The field initializers match the flagship .conf so a missing file
//! degrades to the same values.
[BaseContainerProps(configRoot: true)]
class LF_MedicConfig
{
	static const ResourceName CONFIG_PATH = "{979CD272ABAF7596}Configs/Medic/LF_Medic.conf";
	static const bool DEFAULT_DOWNED_ENABLED = true;
	static const bool DEFAULT_DOWNED_CAN_BE_SEARCHED = true;
	static const float DEFAULT_DOWNED_HEALTH_SCALED = 0.1;

	[Attribute("1", UIWidgets.CheckBox, "Lethal damage to a player puts them in the downed state (vanilla unconsciousness) instead of killing them. Off: lethal damage kills as in vanilla")]
	bool m_bDownedEnabled = true;

	[Attribute("1", UIWidgets.CheckBox, "Other players can open a downed player's inventory (search, rob). Off: a downed body keeps its inventory but nobody else can touch it. A dead body is always lootable")]
	bool m_bDownedCanBeSearched = true;

	[Attribute("0.1", UIWidgets.Auto, "Health (fraction of max) a player is left with when lethal damage downs them", params: "0.01 1")]
	float m_fDownedHealthScaled = 0.1;

	[Attribute("180", UIWidgets.Auto, "Seconds a downed player bleeds out before they die for real (#256)", params: "10 3600")]
	float m_fBleedOutSeconds = 180;

	[Attribute("30", UIWidgets.Auto, "Seconds a player must be downed before they can give up", params: "0 3600")]
	float m_fGiveUpDelaySeconds = 30;

	[Attribute("1", UIWidgets.CheckBox, "Disconnecting while downed counts as death. Off: the body is left to the base game disconnect handling")]
	bool m_bDisconnectWhileDownedKills = true;

	[Attribute("1", UIWidgets.CheckBox, "Anyone can add bleed-out time to a downed player with a bandage, tourniquet or saline bag (#257). Off: only a player whose job is medic")]
	bool m_bFirstAidAnyone = true;

	[Attribute("30", UIWidgets.Auto, "Bleed-out seconds one bandage adds to a downed player", params: "0 600")]
	float m_fBandageSeconds = 30;

	[Attribute("45", UIWidgets.Auto, "Bleed-out seconds one tourniquet adds to a downed player", params: "0 600")]
	float m_fTourniquetSeconds = 45;

	[Attribute("60", UIWidgets.Auto, "Bleed-out seconds one saline bag adds to a downed player. Morphine adds nothing", params: "0 600")]
	float m_fSalineSeconds = 60;

	[Attribute("300", UIWidgets.Auto, "First aid never takes a downed player above this many seconds left", params: "10 3600")]
	float m_fFirstAidCapSeconds = 300;

	[Attribute("2.5", UIWidgets.Auto, "Metres within which an on-duty medic can revive a downed player", params: "0.5 10")]
	float m_fReviveRange = 2.5;

	protected static ref LF_MedicConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config. When the file does not resolve, logs once and returns built-in defaults.
	static LF_MedicConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (container && container.GetResource())
			s_Config = LF_MedicConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));

		if (!s_Config)
		{
			LF_Debug.Error(LF_Downed.FEATURE, string.Format("medic config does not resolve, using defaults: %1", CONFIG_PATH));
			s_Config = new LF_MedicConfig();
		}

		return s_Config;
	}
}
