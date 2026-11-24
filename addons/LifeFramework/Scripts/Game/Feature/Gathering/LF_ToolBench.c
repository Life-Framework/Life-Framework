//------------------------------------------------------------------------------------------------
//! The outcome of one repair at the hardware bench (WK-W9, #4187).
enum LF_EToolBenchResult
{
	DONE,
	NO_TOOL,
	ALREADY_FULL,
	NO_MONEY,
	NO_SCRAP,
	FAILED
}

//------------------------------------------------------------------------------------------------
//! The hardware bench (WK-W9, #4187): a worn tool goes back to full condition for a cash fee and one
//! scrap part. The rules are pure (`Check`); `Repair` is the server step that takes the cash, then the
//! scrap, and gives the cash back when the scrap take comes up short, so a refusal never costs money.
//! The bench works on the tool the player holds (#4821): its condition is on the item
//! (LF_ToolConditionComponent), so a second pickaxe in the pack is not touched.
class LF_ToolBench
{
	static const string FEATURE = "ToolBench";
	static const int FEE = 12; // lf-fixed: the plan's price of a repair, "$12 + 1 scrap metal"
	static const int SCRAP_PER_REPAIR = 1; // lf-fixed: one scrap part per repair
	static const string SCRAP_ID = "scrap_metal";
	static const string REASON = "tool_repair";

	static const string KIND_PICKAXE = "pickaxe";
	static const string KIND_AXE = "axe";
	static const string KIND_ROD = "rod";

	//------------------------------------------------------------------------------------------------
	//! What a repair would do: refused with a reason, or DONE. Pure.
	static LF_EToolBenchResult Check(bool hasTool, float condition, int cash, int scrap, int fee)
	{
		if (!hasTool)
			return LF_EToolBenchResult.NO_TOOL;
		if (condition >= LF_ToolCondition.FULL)
			return LF_EToolBenchResult.ALREADY_FULL;
		if (cash < fee)
			return LF_EToolBenchResult.NO_MONEY;
		if (scrap < SCRAP_PER_REPAIR)
			return LF_EToolBenchResult.NO_SCRAP;
		return LF_EToolBenchResult.DONE;
	}

	//------------------------------------------------------------------------------------------------
	//! Localization key that says the outcome in words.
	static string ResultKey(LF_EToolBenchResult result)
	{
		if (result == LF_EToolBenchResult.DONE)
			return "#LF-ToolBench_Done";
		if (result == LF_EToolBenchResult.NO_TOOL)
			return "#LF-ToolBench_NoTool";
		if (result == LF_EToolBenchResult.ALREADY_FULL)
			return "#LF-ToolBench_Full";
		if (result == LF_EToolBenchResult.NO_MONEY)
			return "#LF-ToolBench_NoMoney";
		if (result == LF_EToolBenchResult.NO_SCRAP)
			return "#LF-ToolBench_NoScrap";
		return "#LF-ToolBench_Failed";
	}

	//------------------------------------------------------------------------------------------------
	//! The prefab of the scrap part, from the catalogue; empty when the server has none.
	static ResourceName ScrapPrefab()
	{
		LF_CatalogueItem item = LF_ItemCatalogue.Item(LF_ItemCatalogueConfig.Get(), SCRAP_ID);
		if (!item)
			return ResourceName.Empty;
		return item.m_Prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Server only. Repairs `tool` (the item the user holds) to full, taking `fee` cash and one
	//! `scrapPrefab` from the user. Nothing is taken on a refusal; a short scrap take returns the cash.
	static LF_EToolBenchResult Repair(IEntity user, LF_ToolConditionComponent tool, ResourceName scrapPrefab, int fee)
	{
		if (!Replication.IsServer())
		{
			LF_Debug.Warn(FEATURE, "repair asked off the server, ignored");
			return LF_EToolBenchResult.FAILED;
		}

		if (!user || scrapPrefab.IsEmpty() || fee < 0)
		{
			LF_Debug.Error(FEATURE, string.Format("repair with a bad input (user %1, scrap '%2', fee %3)", user != null, scrapPrefab, fee));
			return LF_EToolBenchResult.FAILED;
		}

		int cash = LF_MoneyUtils.GetCash(user);
		int scrap = LF_InventoryUtils.GetAmount(user, scrapPrefab);
		float condition = 0;
		if (tool)
			condition = tool.GetCondition();

		LF_EToolBenchResult check = Check(tool != null, condition, cash, scrap, fee);
		if (check != LF_EToolBenchResult.DONE)
		{
			LF_Debug.Info(FEATURE, string.Format("repair refused: %1 (cash %2, scrap %3)", typename.EnumToString(LF_EToolBenchResult, check), cash, scrap));
			return check;
		}

		int taken = 0;
		if (fee > 0)
		{
			taken = LF_MoneyUtils.RemoveCash(user, fee, REASON);
			if (taken != fee)
			{
				if (taken > 0)
					LF_MoneyUtils.AddCash(user, taken, REASON);
				LF_Debug.Warn(FEATURE, string.Format("repair undone: took %1 of %2 cash", taken, fee));
				return LF_EToolBenchResult.NO_MONEY;
			}
		}

		int removed = LF_InventoryUtils.RemoveAmount(user, scrapPrefab, SCRAP_PER_REPAIR);
		if (removed != SCRAP_PER_REPAIR)
		{
			if (removed > 0)
				LF_InventoryUtils.AddAmount(user, scrapPrefab, removed);
			if (taken > 0)
				LF_MoneyUtils.AddCash(user, taken, REASON);
			LF_Debug.Warn(FEATURE, string.Format("repair undone: took %1 of %2 scrap, cash returned", removed, SCRAP_PER_REPAIR));
			return LF_EToolBenchResult.NO_SCRAP;
		}

		tool.SetCondition(LF_ToolCondition.FULL);
		LF_Debug.Info(FEATURE, string.Format("%1 (tier %2) repaired from %3 to %4 for %5 and %6 scrap", tool.GetKind(), tool.GetTier(), condition, tool.GetCondition(), fee, SCRAP_PER_REPAIR));
		return LF_EToolBenchResult.DONE;
	}
}
