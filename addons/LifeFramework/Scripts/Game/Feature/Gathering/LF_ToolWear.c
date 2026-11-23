//------------------------------------------------------------------------------------------------
//! Gather wear (#4300): mining, felling and fishing each lower the player's tool by the tier's rate
//! (LF_ToolTierConfig). A worn-out tool still works at full pace (the hold is timed on the client,
//! which cannot read the server's store), so the chosen consequence is the warning: a notice when the
//! tool drops into a worse band, and again after every use once it is worn out. The conditions live in
//! the LF_ToolBench store; saving them is #4301.
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
	//! One use of `tool`: it loses the tier's amount. Returns the warning key, or "".
	static string Wear(LF_ToolCondition tool)
	{
		return WearScaled(tool, 1.0);
	}

	//------------------------------------------------------------------------------------------------
	//! One use of `tool` that costs `scale` times the tier's amount (a trade perk, #4408: 1 is the full
	//! rate). The condition never goes below 0. Returns the warning key, or "".
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
	//! Server: the player's tool of this kind takes one use, and the player is told when it matters.
	//! A user with no player identity (a test body) wears nothing.
	static void OnUse(IEntity user, string kind)
	{
		if (!Replication.IsServer() || !user || kind.IsEmpty())
			return;

		LF_ToolCondition tool = LF_ToolBench.ToolOf(LF_Utils.GetPlayerUID(user), kind);
		if (!tool)
		{
			LF_Debug.Log(FEATURE, string.Format("no tool worn for %1: the user has no player identity", kind));
			return;
		}

		string uid = LF_Utils.GetPlayerUID(user);
		string warn = WearScaled(tool, LF_PerkEffects.WearScale(uid, TradeOfKind(kind)));
		LF_Debug.Info(FEATURE, string.Format("%1 used, condition now %2", kind, tool.m_fCondition));
		if (warn.IsEmpty())
			return;

		LF_Utils.NotifyEntity(user, LF_Terms.Text(warn, LF_Terms.Text(KindKey(kind))), "", 4.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a finished gather of this family wears the tool it needs, when it tracks one.
	static void OnGather(IEntity user, string family)
	{
		OnUse(user, KindOfFamily(family));
	}
}
