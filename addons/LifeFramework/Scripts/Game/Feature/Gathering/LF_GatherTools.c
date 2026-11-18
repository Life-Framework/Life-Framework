//------------------------------------------------------------------------------------------------
//! A better tool for a gather node (#2706): the same job, done faster. The node's own m_ToolPrefab is
//! the base tool at speed 1; each row here is another tool the node accepts, with its speed factor.
[BaseContainerProps()]
class LF_GatherToolRow
{
	[Attribute(ResourceName.Empty, UIWidgets.ResourcePickerThumbnail, "A tool this node accepts", "et")]
	ResourceName m_ToolPrefab;

	[Attribute("1", UIWidgets.Auto, "Speed factor: 1.5 means the hold finishes in two thirds of the time")]
	float m_fSpeed;
}

//------------------------------------------------------------------------------------------------
//! The tool gate rules of a gather node: which held tool passes, how fast it works, the words when
//! none passes, and the job family the gather is filed under. Pure functions over the node row.
class LF_GatherTools
{
	static const float MIN_SPEED = 0.1; // lf-fixed: floor that keeps a bad config row from dividing by zero
	static const string FAMILY_MINING = "MINING";
	static const string FAMILY_DIGGING = "DIGGING";
	static const string FAMILY_FORAGING = "FORAGING";

	//------------------------------------------------------------------------------------------------
	//! True when the node needs a tool at all.
	static bool NeedsTool(LF_ResourceNodeDef row)
	{
		if (!row)
			return false;
		if (!row.m_ToolPrefab.IsEmpty())
			return true;
		return row.m_aTools && row.m_aTools.Count() > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! True when the held prefab may work the node. A node with no tool accepts anything.
	static bool Accepts(LF_ResourceNodeDef row, ResourceName held)
	{
		if (!NeedsTool(row))
			return true;
		return Speed(row, held) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Speed factor of the held prefab on this node: 1 for the node's base tool or no tool needed,
	//! the row's factor for a listed tool, 0 when the held prefab does not pass.
	static float Speed(LF_ResourceNodeDef row, ResourceName held)
	{
		if (!NeedsTool(row))
			return 1.0;
		if (!held.IsEmpty() && !row.m_ToolPrefab.IsEmpty() && LF_Utils.SamePrefab(row.m_ToolPrefab, held))
			return 1.0;
		if (held.IsEmpty() || !row.m_aTools)
			return 0.0;

		foreach (LF_GatherToolRow tool : row.m_aTools)
		{
			if (tool && LF_Utils.SamePrefab(tool.m_ToolPrefab, held))
				return Math.Max(tool.m_fSpeed, MIN_SPEED);
		}
		return 0.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds one hold takes with a tool of this speed.
	static float Duration(float baseSeconds, float speed)
	{
		return baseSeconds / Math.Max(speed, MIN_SPEED);
	}

	//------------------------------------------------------------------------------------------------
	//! Job family the gather is filed under. A row names it; a tool-gated row without a name is mining.
	static string JobFamily(LF_ResourceNodeDef row)
	{
		if (row && !row.m_sJobFamily.IsEmpty())
			return row.m_sJobFamily;
		if (NeedsTool(row))
			return FAMILY_MINING;
		return FAMILY_FORAGING;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key shown when no passing tool is held: words, no numbers.
	static string RefusalKey(LF_ResourceNodeDef row, bool toolMayBeInInventory)
	{
		string family = JobFamily(row);
		if (family == FAMILY_DIGGING)
			return "#LF-Gather_NeedShovel";
		if (family == FAMILY_MINING)
			return "#LF-Gather_NeedPickaxe";
		if (toolMayBeInInventory)
			return "#LF-Gather_RequiresItem";
		return "#LF-Gather_RequiresItemInHands";
	}

	//------------------------------------------------------------------------------------------------
	//! The first of the user's hands that holds a tool this node accepts: its prefab, or empty.
	static ResourceName HeldTool(IEntity user, LF_ResourceNodeDef row, bool checkInventory)
	{
		if (!user || !NeedsTool(row))
			return ResourceName.Empty;

		CharacterControllerComponent controller = LF_Component<CharacterControllerComponent>.Find(user);
		if (controller)
		{
			ResourceName right = LF_Utils.GetPrefabName(controller.GetRightHandItem());
			if (Accepts(row, right))
				return right;
			ResourceName left = LF_Utils.GetPrefabName(controller.GetAttachedGadgetAtLeftHandSlot());
			if (Accepts(row, left))
				return left;
		}

		if (checkInventory)
		{
			if (!row.m_ToolPrefab.IsEmpty() && LF_InventoryUtils.GetAmount(user, row.m_ToolPrefab) > 0)
				return row.m_ToolPrefab;
			if (row.m_aTools)
			{
				foreach (LF_GatherToolRow tool : row.m_aTools)
				{
					if (tool && LF_InventoryUtils.GetAmount(user, tool.m_ToolPrefab) > 0)
						return tool.m_ToolPrefab;
				}
			}
		}
		return ResourceName.Empty;
	}
}
