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
// the cooldown. A machine id names inputs and outputs. EL_GatherAction stays on
// the node. This catalog does not grant items inside a radius, and yields are
// prefabs, not virtual item ids.
//------------------------------------------------------------------------------------------------
//! One gather node. The placed entity keeps EL_GatherAction and names this id.
[BaseContainerProps()]
class EL_ResourceNodeDef
{
	[Attribute("", UIWidgets.EditBox, "Node id, matched by EL_GatherAction.m_sNodeId")]
	string m_sNodeId;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Item granted per gather", "et")]
	ResourceName m_ItemPrefab;

	[Attribute("1", UIWidgets.Auto, "Items granted per gather")]
	int m_iYield;

	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "Tool required in hand. Empty means no tool", "et")]
	ResourceName m_ToolPrefab;

	[Attribute("0", UIWidgets.Auto, "Gathers before the cooldown. 0 means unlimited")]
	int m_iMaxGathers;

	[Attribute("0", UIWidgets.Auto, "Cooldown in milliseconds once the node is exhausted")]
	float m_fCooldownMs;
}

//------------------------------------------------------------------------------------------------
//! One machine recipe. The placed processor keeps EL_ProcessAction and names this id.
[BaseContainerProps()]
class EL_ResourceMachineDef
{
	[Attribute("", UIWidgets.EditBox, "Machine id, matched by EL_ProcessAction.m_sMachineId")]
	string m_sMachineId;

	[Attribute("", UIWidgets.Object, "Inputs consumed per process")]
	ref array<ref EL_ProcessingInput> m_aInputs;

	[Attribute("", UIWidgets.Object, "Outputs produced per process")]
	ref array<ref EL_ProcessingOutput> m_aOutputs;
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class EL_ResourceChainConfig
{
	[Attribute("", UIWidgets.Object, "Gather nodes")]
	ref array<ref EL_ResourceNodeDef> m_aNodes;

	[Attribute("", UIWidgets.Object, "Process machines")]
	ref array<ref EL_ResourceMachineDef> m_aMachines;
}

//------------------------------------------------------------------------------------------------
//! Loads Configs/Resources/ResourceChains.conf once and looks rows up by id.
class EL_ResourceChainCatalog
{
	static const ResourceName CONFIG_PATH = "{7AAA1371675C55DC}Configs/Resources/ResourceChains.conf";

	protected static ref EL_ResourceChainConfig s_Config;
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
			EL_Debug.Error("Resources", string.Format("chain config does not resolve: %1", CONFIG_PATH));
			return;
		}

		s_Config = EL_ResourceChainConfig.Cast(BaseContainerTools.CreateInstanceFromContainer(container.GetResource().ToBaseContainer()));
		if (!s_Config)
			EL_Debug.Error("Resources", string.Format("chain config has an unexpected root class: %1", CONFIG_PATH));
	}

	//------------------------------------------------------------------------------------------------
	static EL_ResourceNodeDef FindNode(string nodeId)
	{
		EnsureLoaded();
		if (nodeId.IsEmpty() || !s_Config || !s_Config.m_aNodes)
			return null;

		foreach (EL_ResourceNodeDef node : s_Config.m_aNodes)
		{
			if (node && node.m_sNodeId == nodeId)
				return node;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	static EL_ResourceMachineDef FindMachine(string machineId)
	{
		EnsureLoaded();
		if (machineId.IsEmpty() || !s_Config || !s_Config.m_aMachines)
			return null;

		foreach (EL_ResourceMachineDef machine : s_Config.m_aMachines)
		{
			if (machine && machine.m_sMachineId == machineId)
				return machine;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! A node with no yield prefab, a non-positive yield, or a prefab that does
	//! not resolve is unusable. The action logs and grants nothing.
	static bool IsNodeUsable(EL_ResourceNodeDef node)
	{
		if (!node)
			return false;

		if (node.m_ItemPrefab.IsEmpty() || node.m_iYield < 1)
			return false;

		if (node.m_iMaxGathers < 0 || node.m_fCooldownMs < 0)
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
	static bool IsMachineUsable(EL_ResourceMachineDef machine)
	{
		if (!machine)
			return false;

		if (!EL_ProcessAction.IsRecipeConfigured(machine.m_aInputs, machine.m_aOutputs))
			return false;

		return EL_ProcessAction.AreRecipeResourcesAvailable(machine.m_aInputs, machine.m_aOutputs);
	}
}
