//------------------------------------------------------------------------------------------------
//! One kind of casualty call (#4827): where it happens (an LF_Places kind), what the board says and
//! how often it is picked against the other rows.
[BaseContainerProps()]
class LF_CasualtyCallRow
{
	[Attribute("", UIWidgets.EditBox, "LF_Places kind the casualty is found at, e.g. lumber_site")]
	string m_sPlaceKind;

	[Attribute("", UIWidgets.EditBox, "Summary on the medic board and pager: a localization key from the pack string table")]
	string m_sSummary;

	[Attribute("1", UIWidgets.Auto, "Weight against the other rows; 0 or less is never picked")]
	int m_iWeight;

	[Attribute("", UIWidgets.ResourceNamePicker, "Work outfit the casualty wears (#4863): an LF_NpcOutfitPreset conf under Configs/NpcOutfits/ that fits the place. Empty leaves the resident in the prefab's default look", "conf class=LF_NpcOutfitPreset")]
	ResourceName m_sOutfit;

	[Attribute("0", UIWidgets.CheckBox, "The place is a built deck or floor (a quay, a gallery): the body may lie on a built surface and under a roof; it still needs level, uncrowded ground")]
	bool m_bBuilt;

	static LF_CasualtyCallRow Make(string placeKind, string summary, int weight, ResourceName outfit = "", bool built = false)
	{
		LF_CasualtyCallRow row = new LF_CasualtyCallRow();
		row.m_sPlaceKind = placeKind;
		row.m_sSummary = summary;
		row.m_iWeight = weight;
		row.m_sOutfit = outfit;
		row.m_bBuilt = built;
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

	static const int DEFAULT_EXPIRY_SECONDS = 600;
	static const int MIN_EXPIRY_SECONDS = 30; // lf-fixed: floor against a typo in the config, not a game rule

	[Attribute("600", UIWidgets.Auto, "Seconds a casualty lasts untreated (#4852): the call leaves the board and the body is cleared, and the body's bleed-out is set to the same time. Long enough to read the board, draw an ambulance and drive; never above the shared board expiry (m_iBoardExpirySeconds)", params: "30 3600")]
	int m_iExpirySeconds;

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
			else
			{
				s_Config.ApplyLayers();
			}
		}

		if (!s_Config.m_aRows)
			s_Config.m_aRows = {};
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Each pack overlay (the mission header's m_aLF_ConfigOverlays, in load order) sets the fields it
	//! writes directly over this instance, which holds the core `.conf`: a pack that writes `m_aRows`
	//! replaces the (empty) row list whole, and one that writes `m_bEnabled 1` turns the generator on.
	void ApplyLayers()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_CasualtyCallConfig, "casualty-calls", resources, names);
		foreach (int i, Resource resource : resources)
		{
			BaseContainer root = resource.GetResource().ToBaseContainer();
			LF_CasualtyCallConfig layer = LF_CasualtyCallConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(root));
			if (!layer)
			{
				LF_Debug.Error(LF_CasualtyCalls.FEATURE, "casualty overlay did not load: " + names[i]);
				continue;
			}

			if (root.IsVariableSetDirectly("m_bEnabled"))
				m_bEnabled = layer.m_bEnabled;
			if (root.IsVariableSetDirectly("m_iIntervalSeconds"))
				m_iIntervalSeconds = layer.m_iIntervalSeconds;
			if (root.IsVariableSetDirectly("m_iPerMedic"))
				m_iPerMedic = layer.m_iPerMedic;
			if (root.IsVariableSetDirectly("m_iMaxOpen"))
				m_iMaxOpen = layer.m_iMaxOpen;
			if (root.IsVariableSetDirectly("m_fSpotRadius"))
				m_fSpotRadius = layer.m_fSpotRadius;
			if (root.IsVariableSetDirectly("m_iExpirySeconds"))
				m_iExpirySeconds = layer.m_iExpirySeconds;
			if (root.IsVariableSetDirectly("m_sPrefab"))
				m_sPrefab = layer.m_sPrefab;
			if (root.IsVariableSetDirectly("m_aRows") && layer.m_aRows)
			{
				m_aRows = new array<ref LF_CasualtyCallRow>();
				foreach (LF_CasualtyCallRow layerRow : layer.m_aRows)
					m_aRows.Insert(layerRow);
			}

			int rowCount;
			if (m_aRows)
				rowCount = m_aRows.Count();
			LF_Debug.Info(LF_CasualtyCalls.FEATURE, string.Format("casualty calls: pack overlay %1 applied, %2 rows, generator %3", names[i], rowCount, m_bEnabled));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds a casualty lasts untreated: the config value, at least MIN_EXPIRY_SECONDS.
	int ExpirySeconds()
	{
		if (m_iExpirySeconds <= 0)
			return DEFAULT_EXPIRY_SECONDS;

		return LF_Utils.MaxInt(m_iExpirySeconds, MIN_EXPIRY_SECONDS);
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
