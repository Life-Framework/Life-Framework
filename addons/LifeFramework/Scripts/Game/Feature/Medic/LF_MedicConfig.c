//------------------------------------------------------------------------------------------------
//! Medic tuning. Loaded from Configs/Medic/LF_Medic.conf (#255, the downed state; #256 and #259
//! add their own fields here). CreateDefault builds the flagship .conf values so a missing file
//! degrades to the same values.
[BaseContainerProps(configRoot: true)]
class LF_MedicConfig
{
	static const ResourceName CONFIG_PATH = "{979CD272ABAF7596}Configs/Medic/LF_Medic.conf";
	static const bool DEFAULT_DOWNED_ENABLED = true;
	static const bool DEFAULT_DOWNED_CAN_BE_SEARCHED = true;
	static const float DEFAULT_DOWNED_HEALTH_SCALED = 0.1;

	[Attribute("1", UIWidgets.CheckBox, "Lethal damage to a player puts them in the downed state (vanilla unconsciousness) instead of killing them. Off: lethal damage kills as in vanilla")]
	bool m_bDownedEnabled;

	[Attribute("1", UIWidgets.CheckBox, "Other players can open a downed player's inventory (search, rob). Off: a downed body keeps its inventory but nobody else can touch it. A dead body is always lootable")]
	bool m_bDownedCanBeSearched;

	[Attribute("0.1", UIWidgets.Auto, "Health (fraction of max) a player is left with when lethal damage downs them", params: "0.01 1")]
	float m_fDownedHealthScaled;

	[Attribute("180", UIWidgets.Auto, "Seconds a downed player bleeds out before they die for real (#256)", params: "10 3600")]
	float m_fBleedOutSeconds;

	[Attribute("30", UIWidgets.Auto, "Seconds a player must be downed before they can give up", params: "0 3600")]
	float m_fGiveUpDelaySeconds;

	[Attribute("1", UIWidgets.CheckBox, "Disconnecting while downed counts as death. Off: the body is left to the base game disconnect handling")]
	bool m_bDisconnectWhileDownedKills;

	[Attribute("1", UIWidgets.CheckBox, "Anyone can add bleed-out time to a downed player with a bandage, tourniquet or saline bag (#257). Off: only a player whose job is medic")]
	bool m_bFirstAidAnyone;

	[Attribute("30", UIWidgets.Auto, "Bleed-out seconds one bandage adds to a downed player", params: "0 600")]
	float m_fBandageSeconds;

	[Attribute("45", UIWidgets.Auto, "Bleed-out seconds one tourniquet adds to a downed player", params: "0 600")]
	float m_fTourniquetSeconds;

	[Attribute("60", UIWidgets.Auto, "Bleed-out seconds one saline bag adds to a downed player. Morphine adds nothing", params: "0 600")]
	float m_fSalineSeconds;

	[Attribute("300", UIWidgets.Auto, "First aid never takes a downed player above this many seconds left", params: "10 3600")]
	float m_fFirstAidCapSeconds;

	[Attribute("2.5", UIWidgets.Auto, "Metres within which an on-duty medic can revive a downed player", params: "0.5 10")]
	float m_fReviveRange;

	[Attribute("0.7", UIWidgets.Auto, "Resilience and blood (fraction of max) a revived player is lifted to so they wake at once; their health stays at the downed sliver", params: "0.1 1")]
	float m_fReviveRecoveryScaled;

	[Attribute("1", UIWidgets.CheckBox, "Anyone may drag or carry a downed player. Off: only an on-duty medic")]
	bool m_bCarryAnyone;

	[Attribute("2.5", UIWidgets.Auto, "Metres within which a player can start dragging or carrying a downed player", params: "0.5 10")]
	float m_fCarryRange;

	[Attribute("1.4", UIWidgets.Auto, "Metres behind the dragger the body follows", params: "0.5 4")]
	float m_fDragDistance;

	[Attribute("0.4", UIWidgets.Auto, "Metres behind the carrier the body follows when carried", params: "0.1 2")]
	float m_fCarryDistance;

	[Attribute("5", UIWidgets.Auto, "Metres within which a carried player can be loaded into a free vehicle seat", params: "1 20")]
	float m_fLoadRange;

	[Attribute("60", UIWidgets.Auto, "A body that is this many metres from its carrier (a teleport, a lag spike) is dropped", params: "3 200")]
	float m_fCarryReleaseDistance;

	[Attribute("0.25", UIWidgets.Auto, "Health (fraction of max) a revived player is lifted to. The downed sliver is lost again to bleeding within seconds, which downs them a second time (#427)", params: "0.05 1")]
	float m_fReviveHealthScaled;

	[Attribute("45", UIWidgets.Auto, "Playtest town only: seconds a treated practice patient is left alone before it is put back at its spot and downed again", params: "5 3600")]
	float m_fPatientResetSeconds;

	[Attribute("{A81F501D3EF6F38E}Prefabs/Items/Medicine/FieldDressing_01/FieldDressing_US_01.et", UIWidgets.ResourceNamePicker, "Item the medic uses up per revive. Empty: free", "et")]
	ResourceName m_sReviveItemPrefab;

	[Attribute("1", UIWidgets.Auto, "How many of that item one revive uses", params: "0 10")]
	int m_iReviveItemCount;

	[Attribute("100", UIWidgets.Auto, "Cash on the paper treatment bill a revive writes for the patient, paid at the hospital desk (#260)", params: "1 100000")]
	int m_iReviveBillAmount;

	[Attribute("48", UIWidgets.Auto, "In-game hours after which an unpaid treatment bill becomes debt, taken from the bank as soon as it holds money (#464). 0: never", params: "0 720")]
	int m_iBillDeadlineHours;

	[Attribute("0.5", UIWidgets.Auto, "Fraction of a paid revive bill the reviving medic earns in cash at clock-out", params: "0 1")]
	float m_fBillMedicShare;

	[Attribute("", UIWidgets.ResourceNamePicker, "Vehicle prefabs (or prefabs inheriting them) whose passenger seats are a stretcher: a downed patient seated there does not bleed out (#262)", "et")]
	ref array<ResourceName> m_aStretcherVehicles;

	[Attribute("3", UIWidgets.Auto, "Metres within which a medic can put a patient in a hospital bed", params: "0.5 10")]
	float m_fBedRange;

	[Attribute("14", UIWidgets.Auto, "Metres from a hospital bed within which a stretcher vehicle's patient can be put in the bed", params: "2 60")]
	float m_fBedVehicleRange;

	[Attribute("1", UIWidgets.CheckBox, "New Life Rule (#264): a real death puts a wristband on the wake, and returning near the death scene inside the window gets a reminder and an incident row. A revive gives none. Off: neither")]
	bool m_bNewLifeEnabled;

	[Attribute("15", UIWidgets.Auto, "Minutes the New Life Rule lasts after waking; also printed on the hospital bill", params: "1 240")]
	int m_iNewLifeMinutes;

	[Attribute("75", UIWidgets.Auto, "Metres from the death scene that count as returning to it", params: "5 500")]
	float m_fNewLifeRadius;

	[Attribute("5", UIWidgets.Auto, "Seconds between two server checks of a wristband wearer's distance to the scene", params: "1 60")]
	float m_fNewLifeCheckSeconds;

	[Attribute("5", UIWidgets.Auto, "Unpaid treatment bills one patient may carry; a further bill is not written", params: "1 50")]
	int m_iBillMaxOpenPerPatient;

	[Attribute("3", UIWidgets.Auto, "Paid bills kept per patient so a receipt reprints after a restart", params: "0 50")]
	int m_iBillKeepPaidPerPatient;

	[Attribute("60", UIWidgets.Auto, "Seconds between two sweeps that turn overdue bills into debt and let the bank pay debt off", params: "5 3600")]
	int m_iDebtSweepSeconds;

	//------------------------------------------------------------------------------------------------
	//! A config with the attribute defaults, built in script (the fallback when the `.conf` does not resolve, tests).
	//! The members carry no initializers: an initializer beats a `.conf` value of 0, so a pack overlay that
	//! turned a field off would read as on (#1077).
	static LF_MedicConfig CreateDefault()
	{
		LF_MedicConfig config = new LF_MedicConfig();
		config.m_bDownedEnabled = true;
		config.m_bDownedCanBeSearched = true;
		config.m_fDownedHealthScaled = 0.1;
		config.m_fBleedOutSeconds = 180;
		config.m_fGiveUpDelaySeconds = 30;
		config.m_bDisconnectWhileDownedKills = true;
		config.m_bFirstAidAnyone = true;
		config.m_fBandageSeconds = 30;
		config.m_fTourniquetSeconds = 45;
		config.m_fSalineSeconds = 60;
		config.m_fFirstAidCapSeconds = 300;
		config.m_fReviveRange = 2.5;
		config.m_fReviveRecoveryScaled = 0.7;
		config.m_bCarryAnyone = true;
		config.m_fCarryRange = 2.5;
		config.m_fDragDistance = 1.4;
		config.m_fCarryDistance = 0.4;
		config.m_fLoadRange = 5;
		config.m_fCarryReleaseDistance = 60;
		config.m_fReviveHealthScaled = 0.25;
		config.m_fPatientResetSeconds = 45;
		config.m_iReviveItemCount = 1;
		config.m_iReviveBillAmount = 100;
		config.m_iBillDeadlineHours = 48;
		config.m_fBillMedicShare = 0.5;
		config.m_fBedRange = 3;
		config.m_fBedVehicleRange = 14;
		config.m_bNewLifeEnabled = true;
		config.m_iNewLifeMinutes = 15;
		config.m_fNewLifeRadius = 75;
		config.m_fNewLifeCheckSeconds = 5;
		config.m_iBillMaxOpenPerPatient = 5;
		config.m_iBillKeepPaidPerPatient = 3;
		config.m_iDebtSweepSeconds = 60;
		config.m_aStretcherVehicles = new array<ResourceName>();
		config.m_aStretcherVehicles.Insert("{D8C109C681E7AAF1}Prefabs/Vehicles/Wheeled/M997/LF_M997_Ambulance.et");
		return config;
	}

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
			s_Config = LF_MedicConfig.CreateDefault();
		}

		return s_Config;
	}
}
