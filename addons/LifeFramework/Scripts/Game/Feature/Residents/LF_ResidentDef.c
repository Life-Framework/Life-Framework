//------------------------------------------------------------------------------------------------
//! One line of a resident's day (#2665): from `m_sTime` on, the resident is at `m_sPlace` (their
//! home or their work), on `m_sAnchor` there, doing `m_sActivity`. The line holds until the next
//! one starts; the last line of the day holds across midnight to the first line of the next.
[BaseContainerProps()]
class LF_ResidentEntryDef
{
	[Attribute("", UIWidgets.EditBox, "Game time this line starts, 24 h 'HH:MM' (06:00, 18:30)")]
	string m_sTime;

	[Attribute("home", UIWidgets.EditBox, "Where: 'home' (the resident's home place kind) or 'work' (their work place kind)")]
	string m_sPlace;

	[Attribute("", UIWidgets.EditBox, "Anchor at that place the resident stands on (counter, bench, bus stop). Empty for anywhere at the place")]
	string m_sAnchor;

	[Attribute("", UIWidgets.EditBox, "What the resident does there: an emote or loiter id (#982). Empty for idle")]
	string m_sActivity;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentEntryDef Make(string time, string place, string anchor = "", string activity = "")
	{
		LF_ResidentEntryDef entry = new LF_ResidentEntryDef();
		entry.m_sTime = time;
		entry.m_sPlace = place;
		entry.m_sAnchor = anchor;
		entry.m_sActivity = activity;
		return entry;
	}
}

//------------------------------------------------------------------------------------------------
//! A resident as data (#2665). The core knows "a resident"; a pack says who they are. Places are
//! kinds found through LF_Places, never names or coordinates (rule 9, map independence).
[BaseContainerProps()]
class LF_ResidentDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Stable resident id (baker, newsagent). The record key and the key later features save under")]
	string m_sId;

	[Attribute("", UIWidgets.EditBox, "LF_Places kind of this resident's home")]
	string m_sHomeKind;

	[Attribute("", UIWidgets.EditBox, "LF_Places id of the one house of the home kind this resident lives in (#3231). Empty: the nearest place of the kind. An id no place holds warns and falls back to the nearest")]
	string m_sHomePlaceId;

	[Attribute("", UIWidgets.EditBox, "LF_Places kind of this resident's workplace. Empty for a resident who never works")]
	string m_sWorkKind;

	[Attribute("", UIWidgets.EditBox, "LF_Places id of the one workplace of the work kind this resident works at (#4560), so four StoreCounter keepers each stand at their own shop. Empty: the nearest place of the kind. An id no place holds warns and falls back to the nearest")]
	string m_sWorkPlaceId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Character prefab this resident spawns as (#2858). Empty: the neutral civilian", "et")]
	ResourceName m_sPrefab;

	[Attribute("", UIWidgets.EditBox, "Quest id this resident asks a passing player for (#2862), from Configs/Quests/LF_ResidentNeedQuests.conf. Empty: no need")]
	string m_sNeedQuest;

	[Attribute("", UIWidgets.Object, "The day, per game day (no weekdays yet): lines by start time, none sharing a start")]
	ref array<ref LF_ResidentEntryDef> m_aTimetable;

	//------------------------------------------------------------------------------------------------
	static LF_ResidentDef Make(string id, string homeKind, string workKind)
	{
		LF_ResidentDef def = new LF_ResidentDef();
		def.m_sId = id;
		def.m_sHomeKind = homeKind;
		def.m_sWorkKind = workKind;
		def.m_aTimetable = {};
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! Appends a line. \return this, so a test builds a day in one expression.
	LF_ResidentDef Add(string time, string place, string anchor = "", string activity = "")
	{
		if (!m_aTimetable)
			m_aTimetable = {};

		m_aTimetable.Insert(LF_ResidentEntryDef.Make(time, place, anchor, activity));
		return this;
	}
}

//------------------------------------------------------------------------------------------------
//! The residents a server has. Loaded from Configs/Residents/LF_Residents.conf; the core file ships
//! two neutral examples and a pack overlay adds, replaces or removes residents by id (#962).
[BaseContainerProps(configRoot: true)]
class LF_ResidentsConfig
{
	static const ResourceName CONFIG_PATH = "{2E26442027E3F339}Configs/Residents/LF_Residents.conf";

	[Attribute("", UIWidgets.Object, "Every resident: id, home and work place kinds, and the day")]
	ref array<ref LF_ResidentDef> m_aResidents;

	// The spawner (#2858). A field left 0 means its default (SpawnRadius, DespawnRadius, MaxLive, SweepSeconds). Hysteresis: a resident appears inside the spawn radius and leaves outside the
	// despawn radius, so one standing on the edge does not flicker. Radii are horizontal metres.
	[Attribute("150", UIWidgets.EditBox, "A due resident appears when a player is within this many metres of their anchor")]
	int m_iSpawnRadiusM;

	[Attribute("200", UIWidgets.EditBox, "A live resident leaves when no player is within this many metres of them")]
	int m_iDespawnRadiusM;

	[Attribute("24", UIWidgets.EditBox, "Most residents live in the world at once; the rest wait, first by id")]
	int m_iMaxLive;

	[Attribute("5", UIWidgets.EditBox, "Seconds between spawner sweeps")]
	int m_iSweepSeconds;

	[Attribute("4", UIWidgets.EditBox, "Most residents walking between anchors at once (#2909); further moves place the resident directly")]
	int m_iMaxWalking;

	protected static ref LF_ResidentsConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	//! The loaded config with the pack overlays merged over it, or null (logged once) when the file
	//! does not resolve.
	static LF_ResidentsConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Residents", string.Format("residents config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_ResidentsConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error("Residents", string.Format("residents config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		s_Config.ApplyLayers();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance; the next Get loads the `.conf` again.
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tests: makes `config` the one Get returns until Reload.
	static void UseForTest(LF_ResidentsConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay of this class over this instance, in load order.
	void ApplyLayers()
	{
		if (!m_aResidents)
			m_aResidents = new array<ref LF_ResidentDef>();

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_ResidentsConfig, "residents", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			BaseContainer root = resource.GetResource().ToBaseContainer();
			int changed = LF_ConfigOverlay.ApplyContainerList(new LF_ResidentList(this), root, "m_aResidents", names[i], claims);
			LF_Debug.Info("Residents", string.Format("pack overlay %1 changed %2 residents", names[i], changed));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The spawner settings (#2858): a value the conf leaves at 0 reads as its default.
	int SpawnRadius()
	{
		if (m_iSpawnRadiusM <= 0)
			return 150;	// lf-fixed: default spawn radius when the conf leaves it unset

		return m_iSpawnRadiusM;
	}

	//------------------------------------------------------------------------------------------------
	int DespawnRadius()
	{
		if (m_iDespawnRadiusM <= 0)
			return 200;	// lf-fixed: default despawn radius when the conf leaves it unset

		return m_iDespawnRadiusM;
	}

	//------------------------------------------------------------------------------------------------
	int MaxLive()
	{
		if (m_iMaxLive <= 0)
			return 24;	// lf-fixed: default live cap when the conf leaves it unset

		return m_iMaxLive;
	}

	//------------------------------------------------------------------------------------------------
	int MaxWalking()
	{
		if (m_iMaxWalking <= 0)
			return 4;	// lf-fixed: default walking cap when the conf leaves it unset

		return m_iMaxWalking;
	}

	//------------------------------------------------------------------------------------------------
	int SweepSeconds()
	{
		if (m_iSweepSeconds <= 0)
			return 5;	// lf-fixed: default sweep period when the conf leaves it unset

		return m_iSweepSeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The resident count (0 when the list is missing).
	int Count()
	{
		if (!m_aResidents)
			return 0;

		return m_aResidents.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! The residents list as the overlay merge sees it. A pack adds, replaces or removes a resident by
//! id; a replace swaps the whole record, day included.
class LF_ResidentList : LF_ConfigList
{
	protected LF_ResidentsConfig m_Config;

	void LF_ResidentList(LF_ResidentsConfig config)
	{
		m_Config = config;
	}

	override string GetName()
	{
		return "residents.residents";
	}

	override int Count()
	{
		return m_Config.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aResidents[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_ResidentDef.Cast(record).m_sId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sId", id);
		return id;
	}

	override string IdHint()
	{
		return "a resident id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		return LF_ResidentDef.Make(id, "", "");
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_ResidentDef def = LF_ResidentDef.Cast(record);
		if (index < m_Config.m_aResidents.Count())
			m_Config.m_aResidents.Set(index, def);
		else
			m_Config.m_aResidents.Insert(def);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aResidents.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_ResidentDef def = LF_ResidentDef.Cast(record);
		if (def.m_sHomeKind.IsEmpty())
			return "a resident needs a home place kind";

		if (!def.m_aTimetable || def.m_aTimetable.IsEmpty())
			return "a resident needs a timetable";

		return string.Empty;
	}
}
