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

	[Attribute("0.7", UIWidgets.Auto, "Resilience and blood (fraction of max) a revived player is lifted to so they wake at once; their health stays at the downed sliver", params: "0.1 1")]
	float m_fReviveRecoveryScaled = 0.7;

	[Attribute("1", UIWidgets.CheckBox, "Anyone may drag or carry a downed player. Off: only an on-duty medic")]
	bool m_bCarryAnyone = true;

	[Attribute("2.5", UIWidgets.Auto, "Metres within which a player can start dragging or carrying a downed player", params: "0.5 10")]
	float m_fCarryRange = 2.5;

	[Attribute("1.4", UIWidgets.Auto, "Metres behind the dragger the body follows", params: "0.5 4")]
	float m_fDragDistance = 1.4;

	[Attribute("0.4", UIWidgets.Auto, "Metres behind the carrier the body follows when carried", params: "0.1 2")]
	float m_fCarryDistance = 0.4;

	[Attribute("5", UIWidgets.Auto, "Metres within which a carried player can be loaded into a free vehicle seat", params: "1 20")]
	float m_fLoadRange = 5;

	[Attribute("60", UIWidgets.Auto, "A body that is this many metres from its carrier (a teleport, a lag spike) is dropped", params: "3 200")]
	float m_fCarryReleaseDistance = 60;

	[Attribute("100", UIWidgets.Auto, "Cash on the paper treatment bill a revive writes for the patient, paid at the hospital desk (#260)", params: "1 100000")]
	int m_iReviveBillAmount = 100;

	[Attribute("0.5", UIWidgets.Auto, "Fraction of a paid revive bill the reviving medic earns in cash at clock-out", params: "0 1")]
	float m_fBillMedicShare = 0.5;

	[Attribute("", UIWidgets.ResourceNamePicker, "Vehicle prefabs (or prefabs inheriting them) whose passenger seats are a stretcher: a downed patient seated there does not bleed out (#262)", "et")]
	ref array<ResourceName> m_aStretcherVehicles = {"{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et"};

	[Attribute("3", UIWidgets.Auto, "Metres within which a medic can put a patient in a hospital bed", params: "0.5 10")]
	float m_fBedRange = 3;

	[Attribute("14", UIWidgets.Auto, "Metres from a hospital bed within which a stretcher vehicle's patient can be put in the bed", params: "2 60")]
	float m_fBedVehicleRange = 14;

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
