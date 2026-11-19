//------------------------------------------------------------------------------------------------
//! One fishing rod (#2927, docs/architecture/fishing.md section 2). The key is `m_Prefab`.
[BaseContainerProps()]
class LF_FishingRodDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Rod item prefab. The record key", "et")]
	ResourceName m_Prefab;

	[Attribute("20", UIWidgets.Auto, "Farthest cast in metres", params: "0 200")]
	float m_fCastRange;

	[Attribute("1", UIWidgets.Auto, "Bite speed multiplier: 2 halves the wait for a bite", params: "0 10")]
	float m_fBiteSpeed;

	[Attribute("0", UIWidgets.Auto, "FISHER level needed to use the rod", params: "0 100")]
	int m_iMinFisherLevel;

	//------------------------------------------------------------------------------------------------
	static LF_FishingRodDef Make(ResourceName prefab, float castRange, float biteSpeed, int minLevel)
	{
		LF_FishingRodDef def = new LF_FishingRodDef();
		def.m_Prefab = prefab;
		def.m_fCastRange = castRange;
		def.m_fBiteSpeed = biteSpeed;
		def.m_iMinFisherLevel = minLevel;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the row is usable, else why not.
	string Problem()
	{
		if (m_Prefab.IsEmpty())
			return "no prefab";
		if (m_fCastRange <= 0)
			return string.Format("cast range %1 is not above 0", m_fCastRange);
		if (m_fBiteSpeed <= 0)
			return string.Format("bite speed %1 is not above 0", m_fBiteSpeed);
		if (m_iMinFisherLevel < 0)
			return string.Format("minimum level %1 is negative", m_iMinFisherLevel);

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! One bait. The key is `m_Prefab`.
[BaseContainerProps()]
class LF_FishingBaitDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Bait item prefab. The record key", "et")]
	ResourceName m_Prefab;

	[Attribute("1", UIWidgets.Auto, "Bite speed multiplier, on top of the rod's", params: "0 10")]
	float m_fBiteSpeed;

	[Attribute("", UIWidgets.Auto, "Species ids this bait makes likelier (their table weight is multiplied by the config's bait boost)")]
	ref array<string> m_aBoostSpecies;

	//------------------------------------------------------------------------------------------------
	static LF_FishingBaitDef Make(ResourceName prefab, float biteSpeed, array<string> boostSpecies)
	{
		LF_FishingBaitDef def = new LF_FishingBaitDef();
		def.m_Prefab = prefab;
		def.m_fBiteSpeed = biteSpeed;
		def.m_aBoostSpecies = {};
		if (boostSpecies)
			def.m_aBoostSpecies.Copy(boostSpecies);

		return def;
	}

	//------------------------------------------------------------------------------------------------
	string Problem()
	{
		if (m_Prefab.IsEmpty())
			return "no prefab";
		if (m_fBiteSpeed <= 0)
			return string.Format("bite speed %1 is not above 0", m_fBiteSpeed);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when this bait boosts `speciesId`.
	bool Boosts(string speciesId)
	{
		if (!m_aBoostSpecies)
			return false;

		return m_aBoostSpecies.Contains(speciesId);
	}
}

//------------------------------------------------------------------------------------------------
//! One fish species. Size is a prefab, not a weight: index 0 small, 1 medium, 2 large. The key is `m_sId`.
[BaseContainerProps()]
class LF_FishSpeciesDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Species id. The record key")]
	string m_sId;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Fish prefabs: small, medium, large (exactly three). A table entry for a species with an empty size prefab is dropped at load", "et")]
	ref array<ResourceName> m_aSizePrefabs;

	[Attribute("", UIWidgets.Auto, "Water kinds it lives in: OCEAN, POND")]
	ref array<string> m_aWater;

	[Attribute("", UIWidgets.Auto, "Hours of the day (0 to 23) when it bites; empty means any hour")]
	ref array<int> m_aHours;

	[Attribute("0", UIWidgets.Auto, "FISHER level needed to land it", params: "0 100")]
	int m_iMinFisherLevel;

	//------------------------------------------------------------------------------------------------
	static LF_FishSpeciesDef Make(string id, array<ResourceName> sizePrefabs, array<string> water, array<int> hours, int minLevel)
	{
		LF_FishSpeciesDef def = new LF_FishSpeciesDef();
		def.m_sId = id;
		def.m_aSizePrefabs = {};
		if (sizePrefabs)
			def.m_aSizePrefabs.Copy(sizePrefabs);

		def.m_aWater = {};
		if (water)
			def.m_aWater.Copy(water);

		def.m_aHours = {};
		if (hours)
			def.m_aHours.Copy(hours);

		def.m_iMinFisherLevel = minLevel;
		return def;
	}

	//------------------------------------------------------------------------------------------------
	string Problem()
	{
		if (m_sId.IsEmpty())
			return "no id";
		if (!m_aSizePrefabs || m_aSizePrefabs.Count() != LF_FishingConfig.SIZE_COUNT)
			return "it needs exactly three size prefabs (small, medium, large)";
		if (!m_aWater || m_aWater.IsEmpty())
			return "it lives in no water";

		foreach (string kind : m_aWater)
		{
			if (!LF_FishingConfig.IsWaterKind(kind))
				return string.Format("unknown water kind '%1'", kind);
		}

		if (m_aHours)
		{
			foreach (int hour : m_aHours)
			{
				if (hour < 0 || hour > 23)
					return string.Format("hour %1 is outside 0 to 23", hour);
			}
		}

		if (m_iMinFisherLevel < 0)
			return string.Format("minimum level %1 is negative", m_iMinFisherLevel);

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! \return The first size (0 small, 1 medium, 2 large) whose prefab is empty, or -1 when all are set.
	int EmptyPrefabSize()
	{
		if (!m_aSizePrefabs)
			return 0;

		for (int i = 0; i < m_aSizePrefabs.Count(); i++)
		{
			if (m_aSizePrefabs[i].IsEmpty())
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when it lives in `water` (OCEAN or POND).
	bool LivesIn(string water)
	{
		if (!m_aWater)
			return false;

		return m_aWater.Contains(water);
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when it bites at `hour` (0 to 23); a species with no hours bites at any hour.
	bool BitesAt(int hour)
	{
		if (!m_aHours || m_aHours.IsEmpty())
			return true;

		return m_aHours.Contains(hour);
	}
}

//------------------------------------------------------------------------------------------------
//! One weighted species entry of a table.
[BaseContainerProps()]
class LF_FishWeightDef
{
	[Attribute("", UIWidgets.EditBox, "Species id")]
	string m_sSpecies;

	[Attribute("1", UIWidgets.Auto, "Weight in the table, above 0", params: "0 100000")]
	int m_iWeight;

	//------------------------------------------------------------------------------------------------
	static LF_FishWeightDef Make(string species, int weight)
	{
		LF_FishWeightDef def = new LF_FishWeightDef();
		def.m_sSpecies = species;
		def.m_iWeight = weight;
		return def;
	}
}

//------------------------------------------------------------------------------------------------
//! A table of species and size weights, for one spot or for open water. The key is `m_sId`.
[BaseContainerProps()]
class LF_FishTableDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Table id. The record key")]
	string m_sId;

	[Attribute("", UIWidgets.Object, "Species this table can give, with weights")]
	ref array<ref LF_FishWeightDef> m_aSpecies;

	[Attribute("", UIWidgets.Auto, "Size weights: small, medium, large (exactly three, none negative, not all 0)")]
	ref array<int> m_aSizeWeights;

	//------------------------------------------------------------------------------------------------
	static LF_FishTableDef Make(string id, array<ref LF_FishWeightDef> species, array<int> sizeWeights)
	{
		LF_FishTableDef def = new LF_FishTableDef();
		def.m_sId = id;
		def.m_aSpecies = {};
		if (species)
		{
			foreach (LF_FishWeightDef entry : species)
				def.m_aSpecies.Insert(entry);
		}

		def.m_aSizeWeights = {};
		if (sizeWeights)
			def.m_aSizeWeights.Copy(sizeWeights);

		return def;
	}

	//------------------------------------------------------------------------------------------------
	//! \return Empty when the row (not its species entries) is usable.
	string Problem()
	{
		if (m_sId.IsEmpty())
			return "no id";
		if (!m_aSizeWeights || m_aSizeWeights.Count() != LF_FishingConfig.SIZE_COUNT)
			return "it needs exactly three size weights (small, medium, large)";

		int total = 0;
		foreach (int weight : m_aSizeWeights)
		{
			if (weight < 0)
				return string.Format("size weight %1 is negative", weight);

			total += weight;
		}

		if (total <= 0)
			return "every size weight is 0";

		return "";
	}
}

//------------------------------------------------------------------------------------------------
//! The fishing tables. Loaded from Configs/Fishing/LF_Fishing.conf (#2927). The core ships one neutral
//! species set; the pack overlays the 1989 Everon species, tables and prices (#800). A bad row is
//! removed at load with `LF_Debug.Error("Fishing", ...)` and never disables another row.
[BaseContainerProps(configRoot: true)]
class LF_FishingConfig
{
	static const string FEATURE = "Fishing";
	static const ResourceName CONFIG_PATH = "{6C2F90A41B7D3E58}Configs/Fishing/LF_Fishing.conf";

	static const string WATER_OCEAN = "OCEAN";
	static const string WATER_POND = "POND";
	static const int SIZE_COUNT = 3;	// lf-fixed: small, medium, large

	[Attribute("15", UIWidgets.Auto, "Shortest wait for a bite, seconds", params: "1 3600")]
	float m_fBiteMinSeconds;

	[Attribute("60", UIWidgets.Auto, "Longest wait for a bite, seconds", params: "1 3600")]
	float m_fBiteMaxSeconds;

	[Attribute("2", UIWidgets.Auto, "How long a strike lands after the bite, seconds", params: "0.1 60")]
	float m_fStrikeWindowSeconds;

	[Attribute("0.5", UIWidgets.Auto, "Latency slack added to the window, seconds", params: "0 10")]
	float m_fStrikeSlackSeconds;

	[Attribute("1.5", UIWidgets.Auto, "Bite wait multiplier at an emptied spot (still held inside min and max)", params: "1 10")]
	float m_fEmptySpotSlowdown;

	[Attribute("2", UIWidgets.Auto, "Table weight multiplier for a species the bait boosts", params: "1 100")]
	float m_fBaitBoost;

	[Attribute("open_water", UIWidgets.EditBox, "Table for water with no spot or an emptied spot; small fish only")]
	string m_sOpenWaterTable;

	[Attribute("6", UIWidgets.Auto, "Most casts one player makes in a minute", params: "1 60")]
	int m_iCastsPerMinute;

	[Attribute("", UIWidgets.Object, "Rods")]
	ref array<ref LF_FishingRodDef> m_aRods;

	[Attribute("", UIWidgets.Object, "Baits")]
	ref array<ref LF_FishingBaitDef> m_aBaits;

	[Attribute("", UIWidgets.Object, "Species")]
	ref array<ref LF_FishSpeciesDef> m_aSpecies;

	[Attribute("", UIWidgets.Object, "Tables")]
	ref array<ref LF_FishTableDef> m_aTables;

	protected static ref LF_FishingConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	static bool IsWaterKind(string kind)
	{
		return kind == WATER_OCEAN || kind == WATER_POND;
	}

	//------------------------------------------------------------------------------------------------
	//! The loaded config with unusable rows removed, or null (logged once) when the file does not resolve.
	static LF_FishingConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error(FEATURE, string.Format("fishing config does not resolve: %1", CONFIG_PATH));
			return null;
		}

		s_Config = LF_FishingConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error(FEATURE, string.Format("fishing config has an unexpected root class: %1", CONFIG_PATH));
			return null;
		}

		LF_FishingLayers.Apply(s_Config);
		s_Config.Sanitize();
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
	static void UseForTest(LF_FishingConfig config)
	{
		s_Config = config;
		s_bLoadAttempted = true;
	}

	//------------------------------------------------------------------------------------------------
	//! A config with the documented global defaults and empty tables (tests build on it).
	static LF_FishingConfig MakeEmpty()
	{
		LF_FishingConfig config = new LF_FishingConfig();
		config.m_fBiteMinSeconds = 15;
		config.m_fBiteMaxSeconds = 60;
		config.m_fStrikeWindowSeconds = 2;
		config.m_fStrikeSlackSeconds = 0.5;
		config.m_fEmptySpotSlowdown = 1.5;
		config.m_fBaitBoost = 2;
		config.m_sOpenWaterTable = "open_water";
		config.m_iCastsPerMinute = 6;
		config.m_aRods = {};
		config.m_aBaits = {};
		config.m_aSpecies = {};
		config.m_aTables = {};
		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! Refuses the unusable rows at load: each is removed with `LF_Debug.Error("Fishing", ...)` naming
	//! it, so one bad row never degrades the others. A repeated key keeps its first row. A bad global
	//! falls back to its default. Species go first, so a table entry for a removed species is dropped
	//! (and a table left with no species is removed).
	//! \return How many rows and entries were removed.
	int Sanitize()
	{
		if (!m_aRods)
			m_aRods = {};
		if (!m_aBaits)
			m_aBaits = {};
		if (!m_aSpecies)
			m_aSpecies = {};
		if (!m_aTables)
			m_aTables = {};

		SanitizeGlobals();

		int removed = 0;
		removed += SanitizeRods();
		removed += SanitizeBaits();
		removed += SanitizeSpecies();
		removed += SanitizeTables();

		if (!FindTable(m_sOpenWaterTable))
			LF_Debug.Error(FEATURE, string.Format("open-water table '%1' is missing: a cast with no usable spot gives nothing", m_sOpenWaterTable));

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected void SanitizeGlobals()
	{
		if (m_fBiteMinSeconds <= 0 || m_fBiteMaxSeconds < m_fBiteMinSeconds)
		{
			LF_Debug.Error(FEATURE, string.Format("bite times %1 to %2 are not a valid range: 15 to 60 used", m_fBiteMinSeconds, m_fBiteMaxSeconds));
			m_fBiteMinSeconds = 15;
			m_fBiteMaxSeconds = 60;
		}

		if (m_fStrikeWindowSeconds <= 0)
		{
			LF_Debug.Error(FEATURE, string.Format("strike window %1 is not above 0: 2 used", m_fStrikeWindowSeconds));
			m_fStrikeWindowSeconds = 2;
		}

		if (m_fStrikeSlackSeconds < 0)
		{
			LF_Debug.Error(FEATURE, string.Format("strike slack %1 is negative: 0.5 used", m_fStrikeSlackSeconds));
			m_fStrikeSlackSeconds = 0.5;
		}

		if (m_fEmptySpotSlowdown < 1)
		{
			LF_Debug.Error(FEATURE, string.Format("empty-spot slowdown %1 is below 1: 1.5 used", m_fEmptySpotSlowdown));
			m_fEmptySpotSlowdown = 1.5;
		}

		if (m_fBaitBoost < 1)
		{
			LF_Debug.Error(FEATURE, string.Format("bait boost %1 is below 1: 2 used", m_fBaitBoost));
			m_fBaitBoost = 2;
		}

		if (m_iCastsPerMinute < 1)
		{
			LF_Debug.Error(FEATURE, string.Format("casts per minute %1 is below 1: 6 used", m_iCastsPerMinute));
			m_iCastsPerMinute = 6;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int SanitizeRods()
	{
		int removed = 0;
		for (int i = m_aRods.Count() - 1; i >= 0; i--)
		{
			LF_FishingRodDef def = m_aRods[i];
			string problem = "empty row";
			string name = "(empty)";
			if (def)
			{
				name = def.m_Prefab;
				problem = def.Problem();
				if (problem.IsEmpty() && FirstRodIndex(def.m_Prefab) != i)
					problem = "the rod is listed twice";
			}

			if (problem.IsEmpty())
				continue;

			LF_Debug.Error(FEATURE, string.Format("rod '%1' refused at load: %2", name, problem));
			m_aRods.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected int SanitizeBaits()
	{
		int removed = 0;
		for (int i = m_aBaits.Count() - 1; i >= 0; i--)
		{
			LF_FishingBaitDef def = m_aBaits[i];
			string problem = "empty row";
			string name = "(empty)";
			if (def)
			{
				name = def.m_Prefab;
				problem = def.Problem();
				if (problem.IsEmpty() && FirstBaitIndex(def.m_Prefab) != i)
					problem = "the bait is listed twice";
			}

			if (problem.IsEmpty())
				continue;

			LF_Debug.Error(FEATURE, string.Format("bait '%1' refused at load: %2", name, problem));
			m_aBaits.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected int SanitizeSpecies()
	{
		int removed = 0;
		for (int i = m_aSpecies.Count() - 1; i >= 0; i--)
		{
			LF_FishSpeciesDef def = m_aSpecies[i];
			string problem = "empty row";
			string name = "(empty)";
			if (def)
			{
				name = def.m_sId;
				problem = def.Problem();
				if (problem.IsEmpty() && FirstSpeciesIndex(def.m_sId) != i)
					problem = "the species is listed twice";
			}

			if (problem.IsEmpty())
				continue;

			LF_Debug.Error(FEATURE, string.Format("species '%1' refused at load: %2", name, problem));
			m_aSpecies.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected int SanitizeTables()
	{
		int removed = 0;
		for (int i = m_aTables.Count() - 1; i >= 0; i--)
		{
			LF_FishTableDef def = m_aTables[i];
			string problem = "empty row";
			string name = "(empty)";
			if (def)
			{
				name = def.m_sId;
				problem = def.Problem();
				if (problem.IsEmpty() && FirstTableIndex(def.m_sId) != i)
					problem = "the table is listed twice";

				if (problem.IsEmpty())
				{
					removed += SanitizeEntries(def);
					if (def.m_aSpecies.IsEmpty())
						problem = "no usable species left";
				}
			}

			if (problem.IsEmpty())
				continue;

			LF_Debug.Error(FEATURE, string.Format("table '%1' refused at load: %2", name, problem));
			m_aTables.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the entries of one table that name no species, name a species with an empty size prefab, or weigh nothing.
	protected int SanitizeEntries(LF_FishTableDef table)
	{
		if (!table.m_aSpecies)
			table.m_aSpecies = {};

		int removed = 0;
		for (int i = table.m_aSpecies.Count() - 1; i >= 0; i--)
		{
			LF_FishWeightDef entry = table.m_aSpecies[i];
			string problem = "empty entry";
			string name = "(empty)";
			if (entry)
			{
				name = entry.m_sSpecies;
				problem = "";
				LF_FishSpeciesDef species = FindSpecies(entry.m_sSpecies);
				if (!species)
					problem = "no such species";
				else if (species.EmptyPrefabSize() >= 0)
					problem = string.Format("the species has no prefab for size %1 (0 small, 1 medium, 2 large)", species.EmptyPrefabSize());
				else if (entry.m_iWeight <= 0)
					problem = string.Format("weight %1 is not above 0", entry.m_iWeight);
			}

			if (problem.IsEmpty())
				continue;

			LF_Debug.Error(FEATURE, string.Format("table '%1' entry '%2' dropped at load: %3", table.m_sId, name, problem));
			table.m_aSpecies.Remove(i);
			removed++;
		}

		return removed;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstRodIndex(ResourceName prefab)
	{
		for (int i = 0; i < m_aRods.Count(); i++)
		{
			if (m_aRods[i] && LF_Utils.SamePrefab(m_aRods[i].m_Prefab, prefab))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstBaitIndex(ResourceName prefab)
	{
		for (int i = 0; i < m_aBaits.Count(); i++)
		{
			if (m_aBaits[i] && LF_Utils.SamePrefab(m_aBaits[i].m_Prefab, prefab))
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstSpeciesIndex(string id)
	{
		for (int i = 0; i < m_aSpecies.Count(); i++)
		{
			if (m_aSpecies[i] && m_aSpecies[i].m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected int FirstTableIndex(string id)
	{
		for (int i = 0; i < m_aTables.Count(); i++)
		{
			if (m_aTables[i] && m_aTables[i].m_sId == id)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The rod row for `prefab`, or null.
	LF_FishingRodDef FindRod(ResourceName prefab)
	{
		if (!m_aRods || prefab.IsEmpty())
			return null;

		foreach (LF_FishingRodDef def : m_aRods)
		{
			if (def && LF_Utils.SamePrefab(def.m_Prefab, prefab))
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The bait row for `prefab`, or null.
	LF_FishingBaitDef FindBait(ResourceName prefab)
	{
		if (!m_aBaits || prefab.IsEmpty())
			return null;

		foreach (LF_FishingBaitDef def : m_aBaits)
		{
			if (def && LF_Utils.SamePrefab(def.m_Prefab, prefab))
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The species row for `id`, or null.
	LF_FishSpeciesDef FindSpecies(string id)
	{
		if (!m_aSpecies || id.IsEmpty())
			return null;

		foreach (LF_FishSpeciesDef def : m_aSpecies)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The table row for `id`, or null.
	LF_FishTableDef FindTable(string id)
	{
		if (!m_aTables || id.IsEmpty())
			return null;

		foreach (LF_FishTableDef def : m_aTables)
		{
			if (def && def.m_sId == id)
				return def;
		}

		return null;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_FishingConfig's statics.
class LF_FishingConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_FishingConfig";
	}

	override void Apply()
	{
		LF_FishingConfig.Reload();
	}
}
