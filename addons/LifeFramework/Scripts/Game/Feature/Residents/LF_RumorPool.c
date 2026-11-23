// LF_RumorPool.c - rumors as rows tied to a place (#4231, slice of #4170, plan W5.1).
// A row is offered to a character only while its place is unknown to them and its live conditions
// hold. A crime row (N32) goes only to a player the giver counts as friendly or better, and only
// from a giver listed as a crime giver (bar staff, the informant). The rows are data
// (Configs/Residents/LF_Rumors.conf); this is the pure rule. Who asks, and when, is the caller's.

//------------------------------------------------------------------------------------------------
//! One rumor: the place it points at, the words, and when it may be told.
[BaseContainerProps()]
class LF_RumorDef
{
	[Attribute("", UIWidgets.EditBox, "Unique rumor id")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "Map place id the rumor is about (LF_MapKnown key)")]
	string m_sPlaceId;

	[Attribute("", UIWidgets.EditBox, "Localization key of the line the giver says (#Key)")]
	string m_sLineKey;

	[Attribute("0", UIWidgets.CheckBox, "A crime rumor: only crime givers tell it, only to a friendly or better player")]
	bool m_bCrime;

	[Attribute("", UIWidgets.Object, "Givers that may tell it (shop, bar, informant, ...); empty means any giver")]
	ref array<string> m_aGivers;

	[Attribute("", UIWidgets.Object, "Live conditions that must hold (a leading ! needs the condition false)")]
	ref array<string> m_aConditions;

	//------------------------------------------------------------------------------------------------
	static LF_RumorDef Make(string id, string placeId, string lineKey, bool crime)
	{
		LF_RumorDef def = new LF_RumorDef();
		def.m_sId = id;
		def.m_sPlaceId = placeId;
		def.m_sLineKey = lineKey;
		def.m_bCrime = crime;
		def.m_aGivers = {};
		def.m_aConditions = {};
		return def;
	}
}

//------------------------------------------------------------------------------------------------
//! The live conditions that hold right now (`night`, `shop_open`, ...). The caller fills it from
//! the world; the pool only reads it.
class LF_RumorState
{
	ref array<string> m_aTrue = {};

	//------------------------------------------------------------------------------------------------
	//! True when `condition` holds; a leading ! asks for it not holding.
	bool Holds(string condition)
	{
		if (condition.IsEmpty())
			return true;

		if (condition.Get(0) == "!")
			return m_aTrue.Find(condition.Substring(1, condition.Length() - 1)) == -1;

		return m_aTrue.Find(condition) != -1;
	}
}

//------------------------------------------------------------------------------------------------
//! Loaded from Configs/Residents/LF_Rumors.conf. The core ships no rows (rumors are about places,
//! and places are a pack's); a pack adds its own.
[BaseContainerProps(configRoot: true)]
class LF_RumorsConfig
{
	static const ResourceName CONFIG_PATH = "{F03AAFDFA72ECA61}Configs/Residents/LF_Rumors.conf";

	[Attribute("", UIWidgets.Object, "Givers that may tell a crime rumor (bar, informant)")]
	ref array<string> m_aCrimeGivers;

	[Attribute("", UIWidgets.Object, "The rumors")]
	ref array<ref LF_RumorDef> m_aRumors;

	protected static ref LF_RumorsConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_RumorsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Residents", string.Format("rumor config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_RumorsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			LF_Debug.Error("Residents", string.Format("rumor config has an unexpected root class: %1", CONFIG_PATH));

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until ResetStatics.
	static void UseForTest(LF_RumorsConfig config)
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
class LF_RumorPool
{
	static const string FEATURE = "Residents";

	//------------------------------------------------------------------------------------------------
	//! A crime rumor needs a friendly or better player (N32).
	static bool CrimeTierOk(string tier)
	{
		return tier == LF_ResidentMemory.TIER_FRIENDLY || tier == LF_ResidentMemory.TIER_REGULAR;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether `def` may be told by `giver` to `characterId`, whom the giver counts as `tier`.
	static bool Eligible(notnull LF_RumorDef def, notnull LF_RumorsConfig config, string characterId, string giver, string tier, notnull LF_RumorState state)
	{
		if (def.m_sPlaceId.IsEmpty())
			return false;

		if (LF_MapKnown.KnownOf(characterId, def.m_sPlaceId) != LF_EMapPoiKnown.NONE)
			return false;

		if (def.m_aGivers && def.m_aGivers.Count() > 0 && def.m_aGivers.Find(giver) == -1)
			return false;

		if (def.m_bCrime)
		{
			if (!CrimeTierOk(tier))
				return false;

			if (!config.m_aCrimeGivers || config.m_aCrimeGivers.Find(giver) == -1)
				return false;
		}

		if (def.m_aConditions)
		{
			foreach (string condition : def.m_aConditions)
			{
				if (!state.Holds(condition))
					return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every rumor `giver` may tell `characterId` now, in config order. Empty when the config is missing.
	static array<ref LF_RumorDef> Offers(string characterId, string giver, string tier, notnull LF_RumorState state)
	{
		array<ref LF_RumorDef> offers = {};
		LF_RumorsConfig config = LF_RumorsConfig.Get();
		if (!config || !config.m_aRumors)
			return offers;

		foreach (LF_RumorDef def : config.m_aRumors)
		{
			if (def && Eligible(def, config, characterId, giver, tier, state))
				offers.Insert(def);
		}

		return offers;
	}

	//------------------------------------------------------------------------------------------------
	//! One offer chosen by `seed` (the caller's pick: a day number, a counter), or null for none.
	static LF_RumorDef Pick(notnull array<ref LF_RumorDef> offers, int seed)
	{
		if (offers.IsEmpty())
			return null;

		int index = Math.AbsInt(seed) % offers.Count();
		return offers.Get(index);
	}

	//------------------------------------------------------------------------------------------------
	//! The rumor with this id, or null.
	static LF_RumorDef Find(string id)
	{
		LF_RumorsConfig config = LF_RumorsConfig.Get();
		if (!config || !config.m_aRumors)
			return null;

		foreach (LF_RumorDef def : config.m_aRumors)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the character hears of the rumor's place (their own map, never the public one).
	//! \return True when the place is newly heard of.
	static bool Tell(string characterId, LF_RumorDef def)
	{
		if (!def)
		{
			LF_Debug.Warn(FEATURE, "rumor: nothing to tell");
			return false;
		}

		return LF_MapKnown.HearCharacter(characterId, def.m_sPlaceId) == LF_EMapKnownOutcome.HEARD;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_RumorsConfig's statics (LF_StaticReset finds it by reflection).
class LF_RumorsConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_RumorsConfig";
	}

	override void Apply()
	{
		LF_RumorsConfig.ResetStatics();
	}
}
