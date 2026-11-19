//------------------------------------------------------------------------------------------------
//! The property config (#2647, slice P2 of #747): Configs/Property/LF_Property.conf. An owner describes
//! the plot catalogue, the house tiers and what each build stage needs in one file; the core ships the
//! structure and a neutral example, a pack overrides the file with its own plots and prices (#808).
//! Stages are numbered from 1 in the API and listed in order in the config.
//------------------------------------------------------------------------------------------------

//! One thing a stage consumes: a prefab and how many of it.
[BaseContainerProps()]
class LF_PropertyStageReqDef
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Item the builder must hand over", "et")]
	ResourceName m_Prefab;

	[Attribute("1", UIWidgets.Auto, "How many of the item this stage consumes")]
	int m_iCount;
}

//------------------------------------------------------------------------------------------------
//! One build stage: the hands-on work and the items it needs.
[BaseContainerProps()]
class LF_PropertyStageDef
{
	[Attribute("", UIWidgets.EditBox, "Stage id, unique inside the tier (excavation, foundation, walls-roof, doors-windows)")]
	string m_sStageId;

	[Attribute("10", UIWidgets.Auto, "Hands-on work units the stage needs. 0 means no work, items only")]
	int m_iWorkUnits;

	[Attribute("", UIWidgets.Object, "Items the stage consumes. Empty means work only")]
	ref array<ref LF_PropertyStageReqDef> m_aRequirements;
}

//------------------------------------------------------------------------------------------------
//! A house tier: its build stages in order and the prefab shown after each stage.
[BaseContainerProps()]
class LF_PropertyTierDef
{
	[Attribute("", UIWidgets.EditBox, "Tier id, unique across the config; a plot names it")]
	string m_sTierId;

	[Attribute("", UIWidgets.Object, "Build stages in order, stage 1 first")]
	ref array<ref LF_PropertyStageDef> m_aStages;

	[Attribute("", UIWidgets.ResourcePickerThumbnail, "Prefab shown once each stage is done, one per stage in order (a prefab swap, no new assets)", "et")]
	ref array<ResourceName> m_aShellPrefabs;

	[Attribute("0", UIWidgets.Auto, "Hours between rent payments. 0 means the property is owned outright")]
	int m_iRentIntervalHours;

	[Attribute("0", UIWidgets.Auto, "Hours of grace after a missed payment before the owner loses the property. 0 with no rent")]
	int m_iGraceHours;
}

//------------------------------------------------------------------------------------------------
//! A plot of land for sale: where it is is decided elsewhere (a place component); here only what it is.
[BaseContainerProps()]
class LF_PropertyPlotDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "Plot id, unique across the config")]
	string m_sPlotId;

	[Attribute("", UIWidgets.EditBox, "Tier id of the house that can be built here")]
	string m_sTierId;

	[Attribute("0", UIWidgets.Auto, "Purchase price of the plot")]
	int m_iPrice;
}

//------------------------------------------------------------------------------------------------
//! A house for sale at its "For sale" sign (#3207): what it is and what it costs, keyed by the house
//! identifier the sign names (LF_HouseManagerComponent's, the one a house key is bound to). Which house is
//! where is decided by the placed sign and the world; here only the price and the label. A pack prices its
//! houses by overlay, the same way plots work (#2976).
[BaseContainerProps()]
class LF_PropertyHouseDef : LF_ConfigRecord
{
	[Attribute("", UIWidgets.EditBox, "House identifier, unique across the config and never a plot id: the m_sDebugIdentifier of the house the sign sells")]
	string m_sHouseId;

	[Attribute("", UIWidgets.EditBox, "Tier label shown with the price (a string key or plain words): cottage, townhouse, farmhouse")]
	string m_sTierLabel;

	[Attribute("0", UIWidgets.Auto, "Purchase price of the house. 0 means not for sale")]
	int m_iPrice;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_PropertyConfig
{
	static const ResourceName CONFIG_PATH = "{D9770AC8486445C3}Configs/Property/LF_Property.conf";

	[Attribute("", UIWidgets.Object, "The house tiers")]
	ref array<ref LF_PropertyTierDef> m_aTiers;

	[Attribute("", UIWidgets.Object, "The plot catalogue")]
	ref array<ref LF_PropertyPlotDef> m_aPlots;

	[Attribute("", UIWidgets.Object, "The houses for sale at a sign (#3207), by house identifier")]
	ref array<ref LF_PropertyHouseDef> m_aHouses;

	protected static ref LF_PropertyConfig s_Config;
	protected static bool s_bLoadAttempted;
	protected static ResourceName s_PathOverride;

	//------------------------------------------------------------------------------------------------
	//! The loaded config, or null (logged once) when the file does not resolve.
	static LF_PropertyConfig Get()
	{
		if (s_bLoadAttempted)
			return s_Config;

		s_bLoadAttempted = true;
		ResourceName path = CONFIG_PATH;
		if (!s_PathOverride.IsEmpty())
			path = s_PathOverride;

		s_Config = LoadFrom(path);
		// A path override (tests) replaces the shipped file whole; only the shipped file takes pack layers.
		if (s_Config && s_PathOverride.IsEmpty())
			s_Config.ApplyLayers();

		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	//! Merges each pack overlay (the mission header's m_aLF_ConfigOverlays, in load order) over this
	//! instance, which holds the core `.conf`: the plot catalogue by plot id (add, replace, patch,
	//! remove; docs/config/override-model.md), and the house catalogue (#3207) the same way. Tiers are not
	//! layered. A plot a pack adds whose key is a house identifier is refused like a core one (#2783).
	void ApplyLayers()
	{
		if (!m_aPlots)
			m_aPlots = new array<ref LF_PropertyPlotDef>();

		if (!m_aHouses)
			m_aHouses = new array<ref LF_PropertyHouseDef>();

		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_PropertyConfig, "property", resources, names);
		LF_ConfigClaims claims = new LF_ConfigClaims();
		foreach (int i, Resource resource : resources)
		{
			int changed = ApplyPackContainer(resource.GetResource().ToBaseContainer(), names[i], claims);
			LF_Debug.Info("Property", string.Format("property: pack overlay %1 changed %2 plots", names[i], changed));
		}

		if (!resources.IsEmpty())
			RefuseHouseCollisions();
	}

	//------------------------------------------------------------------------------------------------
	//! Applies one pack overlay's `m_aPlots` by plot id and `m_aHouses` by house id. \return Entries changed.
	//! \param[in] root The overlay's root container.
	//! \param[in] source The overlay file, for log lines.
	int ApplyPackContainer(notnull BaseContainer root, string source, LF_ConfigClaims claims = null)
	{
		if (!m_aPlots)
			m_aPlots = new array<ref LF_PropertyPlotDef>();

		if (!m_aHouses)
			m_aHouses = new array<ref LF_PropertyHouseDef>();

		int changed = LF_ConfigOverlay.ApplyContainerList(new LF_PropertyPlotList(this), root, "m_aPlots", source, claims);
		changed += LF_ConfigOverlay.ApplyContainerList(new LF_PropertyHouseList(this), root, "m_aHouses", source, claims);
		return changed;
	}

	//------------------------------------------------------------------------------------------------
	//! Loads one config file. A broken file logs LF_Debug.Error and returns null; nothing else is affected.
	static LF_PropertyConfig LoadFrom(ResourceName path)
	{
		Resource container = BaseContainerTools.LoadContainer(path);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Property", string.Format("property config does not resolve, using defaults: %1", path));
			return null;
		}

		LF_PropertyConfig config = LF_PropertyConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!config)
			LF_Debug.Error("Property", string.Format("property config has an unexpected root class, using defaults: %1", path));
		else
			config.RefuseHouseCollisions();

		return config;
	}

	//------------------------------------------------------------------------------------------------
	//! LF_Properties keys houses and plots in one table and a house wins, so a plot key equal to a house
	//! identifier would sell that house at the plot price (#2783). Drops every plot whose key is a known
	//! house (a saved row or a live house) and logs LF_Debug.Error naming both. The other plots stay.
	//! \return How many plots were refused.
	int RefuseHouseCollisions()
	{
		if (!m_aPlots)
			return 0;

		int refused = 0;
		for (int i = m_aPlots.Count() - 1; i >= 0; i--)
		{
			LF_PropertyPlotDef plot = m_aPlots[i];
			if (!plot || plot.m_sPlotId.IsEmpty())
				continue;

			// A house for sale in the catalogue (#3207) counts as a house here: its id never sells as a plot.
			if (!LF_Houses.Get(plot.m_sPlotId) && !LF_Houses.Find(plot.m_sPlotId) && !FindHouse(plot.m_sPlotId))
				continue;

			LF_Debug.Error("Property", string.Format("plot key %1 equals the house identifier %1: the plot is not for sale (rename the plot key in the property config, the house is untouched)", plot.m_sPlotId));
			m_aPlots.RemoveOrdered(i);
			refused++;
		}

		return refused;
	}

	//------------------------------------------------------------------------------------------------
	//! A house registered after the config loaded (the config usually loads first): drops the loaded
	//! plot with the same key. Does nothing when no config is loaded; LoadFrom checks the houses then.
	//! \return True when a plot was refused.
	static bool RefuseHouseId(string houseId)
	{
		if (!s_Config || !s_Config.m_aPlots || houseId.IsEmpty())
			return false;

		for (int i = 0; i < s_Config.m_aPlots.Count(); i++)
		{
			LF_PropertyPlotDef plot = s_Config.m_aPlots[i];
			if (!plot || plot.m_sPlotId != houseId)
				continue;

			LF_Debug.Error("Property", string.Format("plot key %1 equals the house identifier %1: the plot is not for sale (rename the plot key in the property config, the house is untouched)", houseId));
			s_Config.m_aPlots.RemoveOrdered(i);
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached instance so the next Get() loads again. A path reads that file instead of the
	//! shipped one (tests); no path restores the shipped config.
	static void Reload(ResourceName path = ResourceName.Empty)
	{
		s_Config = null;
		s_bLoadAttempted = false;
		s_PathOverride = path;
	}

	//------------------------------------------------------------------------------------------------
	LF_PropertyPlotDef FindPlot(string plotId)
	{
		if (!m_aPlots || plotId.IsEmpty())
			return null;

		foreach (LF_PropertyPlotDef plot : m_aPlots)
		{
			if (plot && plot.m_sPlotId == plotId)
				return plot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The house for sale with this identifier, null when the catalogue has none.
	LF_PropertyHouseDef FindHouse(string houseId)
	{
		if (!m_aHouses || houseId.IsEmpty())
			return null;

		foreach (LF_PropertyHouseDef house : m_aHouses)
		{
			if (house && house.m_sHouseId == houseId)
				return house;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Price of a house for sale; 0 for an unknown house, a house that is not for sale or a missing config.
	static int HousePrice(string houseId)
	{
		LF_PropertyConfig config = Get();
		if (!config)
			return 0;

		LF_PropertyHouseDef house = config.FindHouse(houseId);
		if (!house)
			return 0;

		return LF_Utils.MaxInt(0, house.m_iPrice);
	}

	//------------------------------------------------------------------------------------------------
	//! Tier label of a house for sale; empty for an unknown house or a missing config.
	static string HouseTierLabel(string houseId)
	{
		LF_PropertyConfig config = Get();
		if (!config)
			return "";

		LF_PropertyHouseDef house = config.FindHouse(houseId);
		if (!house)
			return "";

		return house.m_sTierLabel;
	}

	//------------------------------------------------------------------------------------------------
	LF_PropertyTierDef FindTier(string tierId)
	{
		if (!m_aTiers || tierId.IsEmpty())
			return null;

		foreach (LF_PropertyTierDef tier : m_aTiers)
		{
			if (tier && tier.m_sTierId == tierId)
				return tier;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! The tier with this id from the loaded config, or null (unknown tier or missing config).
	protected static LF_PropertyTierDef TierOf(string tierId)
	{
		LF_PropertyConfig config = Get();
		if (!config)
			return null;

		return config.FindTier(tierId);
	}

	//------------------------------------------------------------------------------------------------
	//! Stage `stage` (1-based) of a tier, or null when the tier or the stage does not exist.
	protected static LF_PropertyStageDef FindStage(string tierId, int stage)
	{
		LF_PropertyTierDef tier = TierOf(tierId);
		if (!tier || !tier.m_aStages || stage < 1 || stage > tier.m_aStages.Count())
			return null;

		return tier.m_aStages[stage - 1];
	}

	//------------------------------------------------------------------------------------------------
	static int PlotCount()
	{
		LF_PropertyConfig config = Get();
		if (!config || !config.m_aPlots)
			return 0;

		return config.m_aPlots.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Price of a plot; 0 for an unknown plot or a missing config.
	static int PlotPrice(string plotId)
	{
		LF_PropertyConfig config = Get();
		if (!config)
			return 0;

		LF_PropertyPlotDef plot = config.FindPlot(plotId);
		if (!plot)
			return 0;

		return LF_Utils.MaxInt(0, plot.m_iPrice);
	}

	//------------------------------------------------------------------------------------------------
	//! Tier id of a plot; empty for an unknown plot or a missing config.
	static string PlotTierId(string plotId)
	{
		LF_PropertyConfig config = Get();
		if (!config)
			return "";

		LF_PropertyPlotDef plot = config.FindPlot(plotId);
		if (!plot)
			return "";

		return plot.m_sTierId;
	}

	//------------------------------------------------------------------------------------------------
	//! Number of build stages of a tier; 0 for an unknown tier or a missing config.
	static int StageCount(string tierId)
	{
		LF_PropertyTierDef tier = TierOf(tierId);
		if (!tier || !tier.m_aStages)
			return 0;

		return tier.m_aStages.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Work units stage `stage` (1-based) needs; 0 for an unknown stage.
	static int StageWork(string tierId, int stage)
	{
		LF_PropertyStageDef def = FindStage(tierId, stage);
		if (!def)
			return 0;

		return Math.Max(0, def.m_iWorkUnits);
	}

	//------------------------------------------------------------------------------------------------
	//! How many kinds of item stage `stage` consumes; 0 for work-only or unknown stages.
	static int RequirementCount(string tierId, int stage)
	{
		LF_PropertyStageDef def = FindStage(tierId, stage);
		if (!def || !def.m_aRequirements)
			return 0;

		return def.m_aRequirements.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Requirement `index` (0-based) of stage `stage` (1-based), or null.
	static LF_PropertyStageReqDef Requirement(string tierId, int stage, int index)
	{
		LF_PropertyStageDef def = FindStage(tierId, stage);
		if (!def || !def.m_aRequirements || index < 0 || index >= def.m_aRequirements.Count())
			return null;

		return def.m_aRequirements[index];
	}

	//------------------------------------------------------------------------------------------------
	static int ShellPrefabCount(string tierId)
	{
		LF_PropertyTierDef tier = TierOf(tierId);
		if (!tier || !tier.m_aShellPrefabs)
			return 0;

		return tier.m_aShellPrefabs.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab shown once stage `stage` (1-based) is done; empty for an unknown stage.
	static ResourceName ShellPrefab(string tierId, int stage)
	{
		LF_PropertyTierDef tier = TierOf(tierId);
		if (!tier || !tier.m_aShellPrefabs || stage < 1 || stage > tier.m_aShellPrefabs.Count())
			return ResourceName.Empty;

		return tier.m_aShellPrefabs[stage - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! Hours between rent payments; 0 means owned outright (also for an unknown tier).
	static int RentIntervalHours(string tierId)
	{
		LF_PropertyTierDef tier = TierOf(tierId);
		if (!tier)
			return 0;

		return LF_Utils.MaxInt(0, tier.m_iRentIntervalHours);
	}

	//------------------------------------------------------------------------------------------------
	//! Grace hours after a missed payment; 0 means none (also for an unknown tier).
	static int GraceHours(string tierId)
	{
		LF_PropertyTierDef tier = TierOf(tierId);
		if (!tier)
			return 0;

		return LF_Utils.MaxInt(0, tier.m_iGraceHours);
	}
}

//------------------------------------------------------------------------------------------------
//! The plot catalogue for the shared merge (LF_ConfigOverlay), keyed by plot id. A new plot needs a
//! tier id; its price starts at 0 (not for sale) until the entry gives one.
class LF_PropertyPlotList : LF_ConfigList
{
	protected LF_PropertyConfig m_Config;

	void LF_PropertyPlotList(LF_PropertyConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aPlots)
			m_Config.m_aPlots = new array<ref LF_PropertyPlotDef>();
	}

	override string GetName()
	{
		return "property.plots";
	}

	override int Count()
	{
		return m_Config.m_aPlots.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aPlots[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_PropertyPlotDef.Cast(record).m_sPlotId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sPlotId", id);
		return id;
	}

	override string IdHint()
	{
		return "a plot id";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_PropertyPlotDef plot = new LF_PropertyPlotDef();
		plot.m_sPlotId = id;
		return plot;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_PropertyPlotDef plot = LF_PropertyPlotDef.Cast(record);
		if (index < m_Config.m_aPlots.Count())
			m_Config.m_aPlots.Set(index, plot);
		else
			m_Config.m_aPlots.Insert(plot);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aPlots.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_PropertyPlotDef plot = LF_PropertyPlotDef.Cast(record);
		if (plot.m_sPlotId.IsEmpty())
			return "the entry has no plot id";
		if (plot.m_sTierId.IsEmpty())
			return "a plot needs a tier id (the house that can be built on it)";
		if (plot.m_iPrice < 0)
			return string.Format("price %1 is below 0", plot.m_iPrice);

		return string.Empty;
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_PropertyPlotDef plot = LF_PropertyPlotDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sTierId"))
		{
			string tierId;
			if (container.Get("m_sTierId", tierId) && !tierId.IsEmpty())
			{
				plot.m_sTierId = tierId;
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_iPrice"))
		{
			int price;
			if (container.Get("m_iPrice", price) && price >= 0)
			{
				plot.m_iPrice = price;
				changed++;
			}
			else
			{
				LF_Debug.Error("Property", string.Format("property.plots %1: pack price is below 0; keeping %2", plot.m_sPlotId, plot.m_iPrice));
			}
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! The house catalogue for the shared merge (LF_ConfigOverlay), keyed by house id (#3207). A new house
//! starts at price 0 (not for sale) until the entry gives one. A house id equal to a plot key is refused:
//! LF_Properties keys both in one table.
class LF_PropertyHouseList : LF_ConfigList
{
	protected LF_PropertyConfig m_Config;

	void LF_PropertyHouseList(LF_PropertyConfig config)
	{
		m_Config = config;
		if (!m_Config.m_aHouses)
			m_Config.m_aHouses = new array<ref LF_PropertyHouseDef>();
	}

	override string GetName()
	{
		return "property.houses";
	}

	override int Count()
	{
		return m_Config.m_aHouses.Count();
	}

	override LF_ConfigRecord Get(int index)
	{
		return m_Config.m_aHouses[index];
	}

	override string IdOf(notnull LF_ConfigRecord record)
	{
		return LF_PropertyHouseDef.Cast(record).m_sHouseId;
	}

	override string IdOfContainer(notnull BaseContainer container)
	{
		string id;
		container.Get("m_sHouseId", id);
		return id;
	}

	override string IdHint()
	{
		return "a house identifier";
	}

	override LF_ConfigRecord NewRecord(string id)
	{
		LF_PropertyHouseDef house = new LF_PropertyHouseDef();
		house.m_sHouseId = id;
		return house;
	}

	override void Set(int index, notnull LF_ConfigRecord record)
	{
		LF_PropertyHouseDef house = LF_PropertyHouseDef.Cast(record);
		if (index < m_Config.m_aHouses.Count())
			m_Config.m_aHouses.Set(index, house);
		else
			m_Config.m_aHouses.Insert(house);
	}

	override void RemoveAt(int index)
	{
		m_Config.m_aHouses.RemoveOrdered(index);
	}

	override string Incomplete(notnull LF_ConfigRecord record)
	{
		LF_PropertyHouseDef house = LF_PropertyHouseDef.Cast(record);
		if (house.m_sHouseId.IsEmpty())
			return "the entry has no house id";
		if (house.m_iPrice < 0)
			return string.Format("price %1 is below 0", house.m_iPrice);
		if (m_Config.FindPlot(house.m_sHouseId))
			return string.Format("house id %1 equals a plot key (rename the house or the plot)", house.m_sHouseId);

		return string.Empty;
	}

	override int PatchFromContainer(notnull LF_ConfigRecord record, notnull BaseContainer container)
	{
		LF_PropertyHouseDef house = LF_PropertyHouseDef.Cast(record);
		int changed;
		if (container.IsVariableSetDirectly("m_sTierLabel"))
		{
			string label;
			if (container.Get("m_sTierLabel", label))
			{
				house.m_sTierLabel = label;
				changed++;
			}
		}

		if (container.IsVariableSetDirectly("m_iPrice"))
		{
			int price;
			if (container.Get("m_iPrice", price) && price >= 0)
			{
				house.m_iPrice = price;
				changed++;
			}
			else
			{
				LF_Debug.Error("Property", string.Format("property.houses %1: pack price is below 0; keeping %2", house.m_sHouseId, house.m_iPrice));
			}
		}

		return changed;
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PropertyConfig's statics (LF_StaticReset finds it by reflection).
class LF_PropertyConfig_StaticResetHook : LF_StaticResetHook
{
	override string Owner()
	{
		return "LF_PropertyConfig";
	}

	override void Apply()
	{
		LF_PropertyConfig.Reload();
	}
}
