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
class LF_PropertyPlotDef
{
	[Attribute("", UIWidgets.EditBox, "Plot id, unique across the config")]
	string m_sPlotId;

	[Attribute("", UIWidgets.EditBox, "Tier id of the house that can be built here")]
	string m_sTierId;

	[Attribute("0", UIWidgets.Auto, "Purchase price of the plot")]
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
		return s_Config;
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

		return config;
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

		return Math.Max(0, plot.m_iPrice);
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

		return Math.Max(0, tier.m_iRentIntervalHours);
	}

	//------------------------------------------------------------------------------------------------
	//! Grace hours after a missed payment; 0 means none (also for an unknown tier).
	static int GraceHours(string tierId)
	{
		LF_PropertyTierDef tier = TierOf(tierId);
		if (!tier)
			return 0;

		return Math.Max(0, tier.m_iGraceHours);
	}
}

//------------------------------------------------------------------------------------------------
//! World-end reset of LF_PropertyConfig's statics (registered at script load; Core/LF_StaticReset.c).
class LF_PropertyConfig_StaticResetHook : LF_StaticResetHook
{
	protected static bool s_bRegistered = LF_StaticReset.Register(new LF_PropertyConfig_StaticResetHook());

	override string Owner()
	{
		return "LF_PropertyConfig";
	}

	override void Apply()
	{
		LF_PropertyConfig.Reload();
	}
}
