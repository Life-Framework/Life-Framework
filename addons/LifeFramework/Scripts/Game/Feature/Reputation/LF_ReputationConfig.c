//------------------------------------------------------------------------------------------------
//! One faction's standing ladder (#2696): the points each tier starts at, in rising order. Six
//! thresholds give tiers -2..3: the points a character holds are clamped to the first and the last
//! threshold, and the tier is how many thresholds they have reached, counted from -2.
[BaseContainerProps()]
class LF_ReputationFactionDef : LF_ConfigRecord
{
	//! The tier of the first threshold; tier 0 is "no standing yet" with the shipped six thresholds.
	static const int FIRST_TIER = -2;	// lf-fixed: the tier scale of docs/architecture/reputation.md

	[Attribute("", UIWidgets.EditBox, "Faction id: TOWN, POLICE or UNDERWORLD. The record key")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Core label, a # localization key. The pack overlays the words")]
	string m_sLabel;

	[Attribute("", UIWidgets.Auto, "Points each tier starts at, rising, may be negative: -300 -100 0 100 400 1000 are tiers -2..3")]
	ref array<int> m_aTierThresholds;

	[Attribute("", UIWidgets.Auto, "One # localization key per tier, lowest first")]
	ref array<string> m_aTierLabels;

	[Attribute("1", UIWidgets.CheckBox, "Whether a screen may show this faction's tier. False for the underworld")]
	bool m_bShown;

	[Attribute("0", UIWidgets.Auto, "Most points one character gains with this faction per in-game day; 0 for no cap. A loss is never capped", params: "0 100000")]
	int m_iDailyCap;

	//------------------------------------------------------------------------------------------------
	static LF_ReputationFactionDef Make(string id, notnull array<int> thresholds, int dailyCap = 0)
	{
		LF_ReputationFactionDef faction = new LF_ReputationFactionDef();
		faction.m_sId = id;
		faction.m_aTierThresholds = {};
		faction.m_aTierThresholds.Copy(thresholds);
		faction.m_iDailyCap = dailyCap;
		faction.m_bShown = true;
		return faction;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The lowest points a character can hold (the first threshold); 0 with no table.
	int MinPoints()
	{
		if (!m_aTierThresholds || m_aTierThresholds.IsEmpty())
			return 0;

		int lowest = m_aTierThresholds[0];
		foreach (int threshold : m_aTierThresholds)
		{
			if (threshold < lowest)
				lowest = threshold;
		}

		return lowest;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The highest points a character can hold (the last threshold); 0 with no table.
	int MaxPoints()
	{
		if (!m_aTierThresholds || m_aTierThresholds.IsEmpty())
			return 0;

		int highest = m_aTierThresholds[0];
		foreach (int threshold : m_aTierThresholds)
		{
			if (threshold > highest)
				highest = threshold;
		}

		return highest;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The points held inside the first and last threshold.
	int Clamp(int points)
	{
		return Math.ClampInt(points, MinPoints(), MaxPoints());
	}

	//------------------------------------------------------------------------------------------------
	//! \return The tier of `points`: FIRST_TIER plus the thresholds reached, less one. A table written
	//! out of order still gives a tier that never falls as points rise (the count ignores the order).
	//! 0 for a faction with no table.
	int TierFor(int points)
	{
		if (!m_aTierThresholds || m_aTierThresholds.IsEmpty())
			return 0;

		int reached = 0;
		foreach (int threshold : m_aTierThresholds)
		{
			if (points >= threshold)
				reached++;
		}

		if (reached < 1)
			reached = 1;

		return FIRST_TIER + reached - 1;
	}
}

//------------------------------------------------------------------------------------------------
//! Points one action awards (#2696): which faction, how many (signed), and how soon the same
//! character may earn it again. The game names the action (LF_Reputation.Award); the numbers live here.
[BaseContainerProps()]
class LF_ReputationAwardDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Action id the game awards, e.g. job_completed:PARCEL, fence_sale. The record key")]
	string m_sAction;

	[Attribute("", UIWidgets.EditBox, "Faction id the points go to")]
	string m_sFaction;

	[Attribute("0", UIWidgets.Auto, "Points for one unit of the action; negative takes standing away", params: "-100000 100000")]
	int m_iPoints;

	[Attribute("0", UIWidgets.Auto, "Seconds before the same character earns this action again; 0 for none", params: "0 86400")]
	float m_fCooldownSeconds;

	//------------------------------------------------------------------------------------------------
	static LF_ReputationAwardDef Make(string action, string faction, int points, float cooldownSeconds = 0)
	{
		LF_ReputationAwardDef award = new LF_ReputationAwardDef();
		award.m_sAction = action;
		award.m_sFaction = faction;
		award.m_iPoints = points;
		award.m_fCooldownSeconds = cooldownSeconds;
		return award;
	}
}

//------------------------------------------------------------------------------------------------
//! Standing tables. Loaded from Configs/Reputation/LF_Reputation.conf. Core rows are neutral
//! (# keys, conservative awards); the pack's words and awards come as an overlay in a later slice,
//! as does the owner's config.json section `reputation` (#2696 ships the core file only).
[BaseContainerProps(configRoot: true)]
class LF_ReputationConfig
{
	static const ResourceName CONFIG_PATH = "{FDB8BA82F909CB1B}Configs/Reputation/LF_Reputation.conf";

	//! No owner config.json section yet: the owner key will be "reputation" with the overlay card. A
	//! `SECTION` constant is left out until the owner reader exists (the schema check reads it).

	[Attribute("", UIWidgets.Object, "Factions: the tier ladder, labels, whether shown and the daily cap of each")]
	ref array<ref LF_ReputationFactionDef> m_aFactions;

	[Attribute("", UIWidgets.Object, "Points each in-world action awards, the faction it feeds and its cooldown")]
	ref array<ref LF_ReputationAwardDef> m_aAwards;

	protected static ref LF_ReputationConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_ReputationConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(LF_Reputation.FEATURE, string.Format("reputation config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ReputationConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error(LF_Reputation.FEATURE, string.Format("reputation config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` again (world end, tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_ReputationConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The faction with this id, or null.
	LF_ReputationFactionDef FindFaction(string id)
	{
		if (!m_aFactions || id.IsEmpty())
			return null;

		foreach (LF_ReputationFactionDef faction : m_aFactions)
		{
			if (faction && faction.m_sId == id)
				return faction;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The award of an action, or null when the config has none.
	LF_ReputationAwardDef FindAward(string action)
	{
		if (!m_aAwards || action.IsEmpty())
			return null;

		foreach (LF_ReputationAwardDef award : m_aAwards)
		{
			if (award && award.m_sAction == action)
				return award;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_ReputationConfig's statics.
class LF_ReputationConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_ReputationConfig";
	}

	override void Apply()
	{
		LF_ReputationConfig.Reload();
	}
}
