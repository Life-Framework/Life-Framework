//------------------------------------------------------------------------------------------------
//! One kind of casualty call (#4827): where it happens (an LF_Places kind), what the board says and
//! how often it is picked against the other rows.
[BaseContainerProps()]
class LF_CasualtyCallRow
{
	[Attribute("", UIWidgets.EditBox, "LF_Places kind the casualty is found at, e.g. lumber_site")]
	string m_sPlaceKind;

	[Attribute("", UIWidgets.EditBox, "Summary on the medic board and pager: a localization key, e.g. #LF-Casualty_Fall")]
	string m_sSummary;

	[Attribute("1", UIWidgets.Auto, "Weight against the other rows; 0 or less is never picked")]
	int m_iWeight;

	static LF_CasualtyCallRow Make(string placeKind, string summary, int weight)
	{
		LF_CasualtyCallRow row = new LF_CasualtyCallRow();
		row.m_sPlaceKind = placeKind;
		row.m_sSummary = summary;
		row.m_iWeight = weight;
		return row;
	}
}

//------------------------------------------------------------------------------------------------
//! Configs/Medic/LF_CasualtyCalls.conf. The core file has no rows and the generator off; the pack
//! fills it. A missing or broken file logs once and leaves the generator off.
[BaseContainerProps(configRoot: true)]
class LF_CasualtyCallConfig
{
	static const ResourceName CONFIG_PATH = "{7E3B9A51C2D4F860}Configs/Medic/LF_CasualtyCalls.conf";
	static const ResourceName DEFAULT_PREFAB = "{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et";

	[Attribute("0", UIWidgets.CheckBox, "Hurt residents call the medics while a medic is on duty")]
	bool m_bEnabled;

	[Attribute("300", UIWidgets.Auto, "Seconds between casualty rolls while a medic is on duty")]
	int m_iIntervalSeconds;

	[Attribute("1", UIWidgets.Auto, "Open casualties allowed per on-duty medic")]
	int m_iPerMedic;

	[Attribute("2", UIWidgets.Auto, "Open casualties allowed in all")]
	int m_iMaxOpen;

	[Attribute("6", UIWidgets.Auto, "Meters from the place's origin the casualty may lie")]
	float m_fSpotRadius;

	[Attribute("{9B5BB216CC7FF18E}Prefabs/Characters/Core/Character_Roleplay.et", UIWidgets.ResourcePickerThumbnail, "Resident character spawned as the casualty (needs an RplComponent)", "et")]
	ResourceName m_sPrefab;

	[Attribute("", UIWidgets.Object, "Casualty kinds")]
	ref array<ref LF_CasualtyCallRow> m_aRows;

	protected static ref LF_CasualtyCallConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or a disabled empty one when the file is missing.
	static LF_CasualtyCallConfig Get()
	{
		if (!s_bLoadAttempted)
		{
			s_bLoadAttempted = true;
			Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
			if (container && container.GetResource())
				s_Config = LF_CasualtyCallConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
			if (!s_Config)
			{
				LF_Debug.Error(LF_CasualtyCalls.FEATURE, "casualty config did not load: " + CONFIG_PATH + "; casualty calls are off");
				s_Config = new LF_CasualtyCallConfig();
			}
		}

		if (!s_Config.m_aRows)
			s_Config.m_aRows = {};
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: use this config instead of the file until ResetStatics.
	static void Override(LF_CasualtyCallConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	static void ResetStatics()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_CasualtyCallConfig's statics (LF_StaticReset finds it by reflection).
class LF_CasualtyCallConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_CasualtyCallConfig";
	}

	override void Apply()
	{
		LF_CasualtyCallConfig.ResetStatics();
	}
}
