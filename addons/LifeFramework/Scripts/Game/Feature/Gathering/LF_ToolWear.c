//------------------------------------------------------------------------------------------------
//! Gather wear (#4300): mining, felling and fishing each lower the tool that did the work by its own
//! tier's rate (LF_ToolTierConfig). A worn-out tool still works at full pace (the hold is timed on the
//! client), so the chosen consequence is the warning: a notice when the tool drops into a worse band,
//! and again after every use once it is worn out. The condition lives on the tool item
//! (LF_ToolConditionComponent, #4821), so two pickaxes keep their own and a fine one wears slowest.
class LF_ToolWear
{
	static const string FEATURE = "ToolWear";

	//------------------------------------------------------------------------------------------------
	//! The tool kind a gather family wears: the pickaxe for mining, the axe for logging, "" for any
	//! family that uses no tracked tool (digging, foraging).
	static string KindOfFamily(string family)
	{
		if (family == LF_GatherTools.FAMILY_MINING)
			return LF_ToolBench.KIND_PICKAXE;
		if (family == "LOGGING")
			return LF_ToolBench.KIND_AXE;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key naming a tool kind, for the warning sentence.
	static string KindKey(string kind)
	{
		if (kind == LF_ToolBench.KIND_AXE)
			return "#LF-ToolKind_Axe";
		if (kind == LF_ToolBench.KIND_ROD)
			return "#LF-ToolKind_Rod";
		return "#LF-ToolKind_Pickaxe";
	}

	//------------------------------------------------------------------------------------------------
	//! The warning a use deserves: the state sentence key when the tool dropped into a worse band, or
	//! is worn out (told after every use); "" when nothing changed. Pure.
	static string WarnKey(float before, float after)
	{
		int stateAfter = LF_ToolCondition.StateOf(after);
		if (stateAfter == LF_ToolCondition.STATE_WORN_OUT)
			return LF_ToolCondition.StateKey(after);
		if (stateAfter > LF_ToolCondition.StateOf(before))
			return LF_ToolCondition.StateKey(after);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! One use of a pure tool state: it loses the tier's amount. Returns the warning key, or "".
	static string Wear(LF_ToolCondition tool)
	{
		return WearScaled(tool, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	//! One use of a pure tool state that costs `scale` times the tier's amount (a trade perk, #4408: 1
	//! is the full rate). The condition never goes below 0. Returns the warning key, or "".
	static string WearScaled(LF_ToolCondition tool, float scale)
	{
		if (!tool)
			return "";

		float before = tool.m_fCondition;
		float left = before - LF_ToolCondition.WearPerUse(tool.m_iTier) * scale;
		if (left < 0)
			left = 0;

		tool.m_fCondition = left;
		return WarnKey(before, left);
	}

	//------------------------------------------------------------------------------------------------
	//! The trade a tool kind works for, the key of its perks: pickaxe mining, axe logging, rod fishing.
	static string TradeOfKind(string kind)
	{
		if (kind == LF_ToolBench.KIND_PICKAXE)
			return "MINER";
		if (kind == LF_ToolBench.KIND_AXE)
			return "LUMBERJACK";
		if (kind == LF_ToolBench.KIND_ROD)
			return "FISHER";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the tool of this kind that did the work takes one use, and the player is told when it
	//! matters. `toolEntity` is the tool when the caller knows it (the mining hit); otherwise the one in
	//! the hands, else the first one carried. A user with no tool of the kind wears nothing.
	static void OnUse(IEntity user, string kind, IEntity toolEntity = null)
	{
		if (!Replication.IsServer() || !user || kind.IsEmpty())
			return;

		LF_ToolConditionComponent tool = LF_ToolConditionComponent.Of(toolEntity);
		if (!tool || tool.GetKind() != kind)
			tool = LF_ToolConditionComponent.InUse(user, kind);

		if (!tool)
		{
			LF_Debug.Log(FEATURE, string.Format("no %1 worn: the user holds or carries none with a condition", kind));
			return;
		}

		string uid = LF_Utils.GetPlayerUID(user);
		string warn = tool.Wear(LF_PerkEffects.WearScale(uid, TradeOfKind(kind)));
		LF_Debug.Info(FEATURE, string.Format("%1 (tier %2) used, condition now %3", kind, tool.GetTier(), tool.GetCondition()));
		if (warn.IsEmpty())
			return;

		LF_Utils.NotifyEntity(user, LF_Terms.Text(warn, ToolName(tool, kind)), "", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! The worn item's own name in words ("Fine pickaxe", #4730), or the kind's word when it has none.
	static string ToolName(LF_ToolConditionComponent tool, string kind)
	{
		if (tool)
		{
			string name = LF_ShopManagerComponent.ResolveDisplayName(LF_Utils.GetPrefabName(tool.GetOwner()));
			if (!name.IsEmpty())
				return LF_Terms.Text(name);
		}

		return LF_Terms.Text(KindKey(kind));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a finished gather of this family wears the tool it needs, when it tracks one.
	static void OnGather(IEntity user, string family)
	{
		OnUse(user, KindOfFamily(family));
	}
}
