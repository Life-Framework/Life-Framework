// LF_RumorPool.c - rumors as rows tied to a place (#4231, slice of #4170, plan W5.1).
// A row is offered to a character only while its place is unknown to them and its live conditions
// hold. A crime row (N32) goes only to a player the giver counts as friendly or better, and only
// from a giver listed as a crime giver (bar staff, the informant). The rows are data
// (Configs/Residents/LF_Rumors.conf); this is the pure rule. Who asks, and when, is the caller's.

//------------------------------------------------------------------------------------------------
//! One rumor: the place it points at, the words, and when it may be told.
[BaseContainerProps()]
class LF_RumorDef : LF_ConfigRecord
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
//! and places are a pack's); a pack adds its own as an overlay of this class listed in its mission
//! header (`m_aLF_ConfigOverlays`, section `rumors`): rows by id through the override model (add,
//! replace, patch, remove), plus the crime givers and voice conversations it lists, added to the core's.
[BaseContainerProps(configRoot: true)]
class LF_RumorsConfig
{
	static const ResourceName CONFIG_PATH = "{F03AAFDFA72ECA61}Configs/Residents/LF_Rumors.conf";
	static const string OVERLAY_NAME = "rumors";

	[Attribute("", UIWidgets.Object, "Givers that may tell a crime rumor (bar, informant)")]
	ref array<string> m_aCrimeGivers;

	[Attribute("", UIWidgets.Object, "The rumors")]
	ref array<ref LF_RumorDef> m_aRumors;

	[Attribute("", UIWidgets.Object, "Recorded conversations (dialogue .conf) whose lines the rows speak: a row's m_sLineKey is the text key of one of their lines")]
	ref array<ResourceName> m_aVoices;

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
		else
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Layers the pack overlays of the core file, in load order, over this instance.
	void ApplyLayers(ResourceName coreConf = CONFIG_PATH)
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(coreConf, LF_RumorsConfig, OVERLAY_NAME, resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info("Residents", string.Format("rumors: pack overlay %1 changed %2 entries", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack `.conf` of this class: its rows by the override verbs (`m_sOp`), and the crime
	//! givers and voice conversations it lists, added to this instance's (never removed).
	//! \return Rows, crime givers and voices added or changed.
	int ApplyContainer(notnull BaseContainer container, string source = "pack", LF_ConfigClaims claims = null)
	{
		if (!m_aRumors)
			m_aRumors = new array<ref LF_RumorDef>();

		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_RumorList(this), container, "m_aRumors", source, claims);
		if (!m_aCrimeGivers)
			m_aCrimeGivers = {};

		array<string> givers = {};
		if (container.IsVariableSetDirectly("m_aCrimeGivers") && container.Get("m_aCrimeGivers", givers))
		{
			foreach (string giver : givers)
			{
				if (m_aCrimeGivers.Find(giver) == -1)
				{
					m_aCrimeGivers.Insert(giver);
					changed++;
				}
			}
		}

		if (!m_aVoices)
			m_aVoices = {};

		array<ResourceName> voices = {};
		if (container.IsVariableSetDirectly("m_aVoices") && container.Get("m_aVoices", voices))
		{
			foreach (ResourceName voice : voices)
			{
				if (m_aVoices.Find(voice) == -1)
				{
					m_aVoices.Insert(voice);
					changed++;
				}
			}
		}

		return changed;
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
//! The rumor rows as the override model sees them. The id is the row's `m_sId`.
class LF_RumorList : LF_ConfigList
{
	protected LF_RumorsConfig m_Config;

	void LF_RumorList(LF_RumorsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "rumors.rumors";
	}

	override int Count()
	{
		return m_Config.m_aRumors.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aRumors[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_RumorDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a rumor id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_RumorDef.Make(id, string.Empty, string.Empty, false);
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_RumorDef def = LF_RumorDef.Cast(record);
		if (index < m_Config.m_aRumors.Count())
			m_Config.m_aRumors.Set(index, def);
		else
			m_Config.m_aRumors.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aRumors.RemoveOrdered(index);
	}

	//! A patch changes only the fields the pack sets; a list it sets replaces the row's own.
	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_RumorDef def = LF_RumorDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sPlaceId") && container.Get("m_sPlaceId", def.m_sPlaceId))
			changed++;

		if (container.IsVariableSetDirectly("m_sLineKey") && container.Get("m_sLineKey", def.m_sLineKey))
			changed++;

		if (container.IsVariableSetDirectly("m_bCrime") && container.Get("m_bCrime", def.m_bCrime))
			changed++;

		array<string> list = {};
		if (container.IsVariableSetDirectly("m_aGivers") && container.Get("m_aGivers", list))
		{
			def.m_aGivers = list;
			changed++;
		}

		array<string> conditions = {};
		if (container.IsVariableSetDirectly("m_aConditions") && container.Get("m_aConditions", conditions))
		{
			def.m_aConditions = conditions;
			changed++;
		}

		return changed;
	}

	//! A row with no place or no line cannot be told.
	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_RumorDef def = LF_RumorDef.Cast(record);
		if (def.m_sPlaceId.IsEmpty())
			return "a rumor needs a place id (m_sPlaceId)";

		if (def.m_sLineKey.IsEmpty())
			return "a rumor needs a line key (m_sLineKey)";

		return string.Empty;
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
