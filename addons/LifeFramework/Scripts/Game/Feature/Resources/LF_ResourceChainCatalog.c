// Copyright (c) LNGCGenetic
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// Adapted from LNGC_GatherZonesCatalog. A node id names the tool, the yield, and
// the cooldown. A machine id names inputs and outputs. LF_GatherAction stays on
// the node. This catalog does not grant items inside a radius, and yields are
// prefabs, not virtual item ids.
//------------------------------------------------------------------------------------------------
//! One gather node. The placed entity keeps LF_GatherAction and names this id.
[BaseContainerProps()]
class LF_ResourceNodeDef
{
	[Attribute("", UIWidgets.EditBox, "Node id, matched by LF_GatherAction.m_sNodeId")]
	string m_sNodeId;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Item granted per gather", "et")]
	ResourceName m_ItemPrefab;

	[Attribute("1", UIWidgets.Auto, "Items granted per gather")]
	int m_iYield;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Tool required in hand. Empty means no tool", "et")]
	ResourceName m_ToolPrefab;

	[Attribute("", UIWidgets.Object, "Better tools this node also accepts, each with a speed factor (#2706)")]
	ref array<ref LF_GatherToolRow> m_aTools;

	[Attribute("", UIWidgets.EditBox, "Job family the gather is filed under: MINING, DIGGING, FORAGING (#2706)")]
	string m_sJobFamily;

	[Attribute("0", UIWidgets.Auto, "Gathers a spot holds. 0 means unlimited. With m_fRegrowMs set this is the stock cap, otherwise the gathers before the cooldown")]
	int m_iMaxGathers;

	[Attribute("0", UIWidgets.Auto, "Cooldown in milliseconds once the node is exhausted, then it restocks in full. Ignored when m_fRegrowMs is set")]
	float m_fCooldownMs;

	[Attribute("0", UIWidgets.Auto, "Stock model (#2409): milliseconds for one gather to grow back, starting full at m_iMaxGathers. 0 keeps the cooldown model")]
	float m_fRegrowMs;
}

//------------------------------------------------------------------------------------------------
//! A perk of a machine that a worker earns with level (#4861): from m_iLevel in the row's
//! m_sLevelJob the machine runs a bigger batch cap and/or a shorter run. 0 leaves that part as it was.
[BaseContainerProps()]
class LF_MachineStep
{
	[Attribute("0", UIWidgets.Auto, "Level in the machine's m_sLevelJob from which this step applies")]
	int m_iLevel;

	[Attribute("0", UIWidgets.Auto, "Whole batches one interaction runs at most from this level. 0: unchanged; a step never lowers the cap")]
	int m_iMaxBatches;

	[Attribute("0", UIWidgets.Auto, "Seconds a run takes from this level. 0: unchanged; a step never lengthens a run")]
	int m_iProcessSeconds;
}

//------------------------------------------------------------------------------------------------
//! One machine recipe. The placed processor keeps LF_ProcessAction and names this id.
[BaseContainerProps()]
class LF_ResourceMachineDef
{
	[Attribute("", UIWidgets.EditBox, "Machine id, matched by LF_ProcessAction.m_sMachineId")]
	string m_sMachineId;

	[Attribute("", UIWidgets.Object, "Inputs consumed per process")]
	ref array<ref LF_ProcessingInput> m_aInputs;

	[Attribute("", UIWidgets.Object, "Outputs produced per process")]
	ref array<ref LF_ProcessingOutput> m_aOutputs;

	[Attribute("", UIWidgets.EditBox, "Job whose level the machine needs (MINER, ...). Empty: no level gate (#3226)")]
	string m_sLevelJob;

	[Attribute("0", UIWidgets.Auto, "Level in m_sLevelJob the server needs before it runs the machine. 0 or 1: no gate")]
	int m_iMinLevel;

	[Attribute("1", UIWidgets.Auto, "Whole batches one interaction runs at most, as far as the inventory covers them (#3226)")]
	int m_iMaxBatches;

	[Attribute("0", UIWidgets.Auto, "Seconds a run takes: the inputs go in, the outputs drop at the machine after the delay, one run at a time per machine. 0: instant (#3229)")]
	int m_iProcessSeconds;

	[Attribute("", UIWidgets.Object, "Perks earned with level in m_sLevelJob: a bigger batch cap and/or a shorter run from a level on (#4861)")]
	ref array<ref LF_MachineStep> m_aSteps;

	[Attribute("", UIWidgets.EditBox, "jobs.xpAwards action a finished batch pays the worker (smelt_batch for a furnace, #4337). Empty: no XP")]
	string m_sXpAction;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class LF_ResourceChainConfig
{
	[Attribute("", UIWidgets.Object, "Gather nodes")]
	ref array<ref LF_ResourceNodeDef> m_aNodes;

	[Attribute("", UIWidgets.Object, "Process machines")]
	ref array<ref LF_ResourceMachineDef> m_aMachines;

	[Attribute("", UIWidgets.Object, "Tiered illicit chains (LF_ChainCatalog)")]
	ref array<ref LF_ChainDef> m_aChains;
}

//------------------------------------------------------------------------------------------------
//! Loads Configs/Resources/ResourceChains.conf once and looks rows up by id.
class LF_ResourceChainCatalog
{
	static const ResourceName CONFIG_PATH = "{7AAA1371675C55DC}Configs/Resources/ResourceChains.conf";

	protected static ref LF_ResourceChainConfig s_Config;
	protected static bool s_bLoadAttempted;

	//------------------------------------------------------------------------------------------------
	protected static void EnsureLoaded()
	{
		if (s_bLoadAttempted)
			return;

		s_bLoadAttempted = true;
		Resource container = BaseContainerTools.LoadContainer(CONFIG_PATH);
		if (!container || !container.GetResource())
		{
			LF_Debug.Error("Resources", string.Format("chain config does not resolve: %1", CONFIG_PATH));
			return;
		}

		s_Config = LF_ResourceChainConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
		{
			LF_Debug.Error("Resources", string.Format("chain config has an unexpected root class: %1", CONFIG_PATH));
			return;
		}

		ApplyPackOverlays();
	}

	//------------------------------------------------------------------------------------------------
	//! Adds the machine rows of each pack overlay (the mission header's m_aLF_ConfigOverlays, then
	//! the folder beside a listed file) to the core config (#3450). A pack adds machines: an id the
	//! config already has is refused and logged, so a pack never rewrites a core recipe. Nodes and
	//! chains are not overlaid.
	protected static void ApplyPackOverlays()
	{
		array<ref Resource> resources = {};
		array<ResourceName> names = {};
		LF_ConfigOverlay.LoadPackOverlays(CONFIG_PATH, LF_ResourceChainConfig, "chains", resources, names);
		foreach (int i, Resource resource : resources)
		{
			int added = ApplyPackMachines(resource.GetResource().ToBaseContainer(), names[i]);
			LF_Debug.Info("Resources", string.Format("chains: pack overlay %1 added %2 machines", names[i], added));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! \return Machines added from one overlay's root container.
	protected static int ApplyPackMachines(BaseContainer root, string source)
	{
		if (!root || !s_Config)
			return 0;

		if (!s_Config.m_aMachines)
			s_Config.m_aMachines = {};

		BaseContainerList entries = root.GetObjectArray("m_aMachines");
		if (!entries)
			return 0;

		int added;
		for (int i = 0; i < entries.Count(); i++)
		{
			BaseContainer entry = entries.Get(i);
			if (!entry)
				continue;

			LF_ResourceMachineDef machine = LF_ResourceMachineDef.Cast(BaseContainerTools.CreateInstanceFromContainer(entry));
			if (!machine || machine.m_sMachineId.IsEmpty())
			{
				LF_Debug.Error("Resources", string.Format("chains: %1 has a machine row with no id, skipped", source));
				continue;
			}

			bool known;
			foreach (LF_ResourceMachineDef have : s_Config.m_aMachines)
			{
				if (have && have.m_sMachineId == machine.m_sMachineId)
					known = true;
			}

			if (known)
			{
				LF_Debug.Warn("Resources", string.Format("chains: %1 names machine %2 the config already has, skipped", source, machine.m_sMachineId));
				continue;
			}

			s_Config.m_aMachines.Insert(machine);
			added++;
		}

		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the cached config; the next lookup loads the `.conf` and its pack overlays again (tests).
	static void Reload()
	{
		s_Config = null;
		s_bLoadAttempted = false;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ResourceChainConfig GetConfig()
	{
		EnsureLoaded();
		return s_Config;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ResourceNodeDef FindNode(string nodeId)
	{
		EnsureLoaded();
		if (nodeId.IsEmpty() || !s_Config || !s_Config.m_aNodes)
			return null;

		foreach (LF_ResourceNodeDef node : s_Config.m_aNodes)
		{
			if (node && node.m_sNodeId == nodeId)
				return node;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static LF_ResourceMachineDef FindMachine(string machineId)
	{
		EnsureLoaded();
		if (machineId.IsEmpty() || !s_Config || !s_Config.m_aMachines)
			return null;

		foreach (LF_ResourceMachineDef machine : s_Config.m_aMachines)
		{
			if (machine && machine.m_sMachineId == machineId)
				return machine;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A node with no yield prefab, a non-positive yield, or a prefab that does
	//! not resolve is unusable. The action logs and grants nothing.
	static bool IsNodeUsable(LF_ResourceNodeDef node)
	{
		if (!node)
			return false;

		if (node.m_ItemPrefab.IsEmpty() || node.m_iYield < 1)
			return false;

		if (node.m_iMaxGathers < 0 || node.m_fCooldownMs < 0 || node.m_fRegrowMs < 0)
			return false;

		Resource yield = Resource.Load(node.m_ItemPrefab);
		if (!yield || !yield.IsValid())
			return false;

		if (node.m_ToolPrefab.IsEmpty())
			return true;

		Resource tool = Resource.Load(node.m_ToolPrefab);
		return tool && tool.IsValid();
	}

	//------------------------------------------------------------------------------------------------
	//! An empty recipe, or one whose prefabs do not resolve, is unusable.
	static bool IsMachineUsable(LF_ResourceMachineDef machine)
	{
		if (!machine)
			return false;

		if (!LF_ProcessAction.IsRecipeConfigured(machine.m_aInputs, machine.m_aOutputs))
			return false;

		return LF_ProcessAction.AreRecipeResourcesAvailable(machine.m_aInputs, machine.m_aOutputs);
	}
}
