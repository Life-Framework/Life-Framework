//------------------------------------------------------------------------------------------------
//! One action on the hardware bench (WK-W9, #4187): "Repair the pickaxe", "Sharpen the axe" or
//! "Re-string the rod", one per tool kind. It works on the tool of that kind the player holds (#4821).
//! The client sends no price; the server reads the fee from the bench prefab, the cash and the scrap
//! from the player, and says how it went in words.
class LF_ToolBenchAction : ScriptedUserAction
{
	[Attribute("0", UIWidgets.ComboBox, "Tool kind this action works on", "", ParamEnumArray.FromEnum(LF_EToolBenchKind))]
	protected LF_EToolBenchKind m_eKind;

	[Attribute("12", UIWidgets.EditBox, "Cash a repair costs", "0 100000 1")]
	protected int m_iFee;

	//------------------------------------------------------------------------------------------------
	static string KindId(LF_EToolBenchKind kind)
	{
		return LF_ToolConditionComponent.KindId(kind);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (m_eKind == LF_EToolBenchKind.AXE)
			outName = "#LF-ToolBench_SharpenAxe";
		else if (m_eKind == LF_EToolBenchKind.ROD)
			outName = "#LF-ToolBench_RestringRod";
		else
			outName = "#LF-ToolBench_RepairPickaxe";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The holder's client reads the held tool's replicated condition: no tool in hand, or one already
	//! at full, greys the action out with the reason in words.
	override bool CanBePerformedScript(IEntity user)
	{
		LF_ToolConditionComponent tool = LF_ToolConditionComponent.Held(user, KindId(m_eKind));
		if (!tool)
		{
			SetCannotPerformReason("#LF-ToolBench_NoTool");
			return false;
		}

		if (tool.GetCondition() >= LF_ToolCondition.FULL)
		{
			SetCannotPerformReason("#LF-ToolBench_Full");
			return false;
		}

		if (LF_MoneyUtils.GetCash(user) < m_iFee)
		{
			SetCannotPerformReason("#LF-ToolBench_NoMoney");
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		if (!pUserEntity)
		{
			LF_Debug.Error(LF_ToolBench.FEATURE, "bench action performed without a user");
			return;
		}

		LF_ToolConditionComponent tool = LF_ToolConditionComponent.Held(pUserEntity, KindId(m_eKind));
		LF_EToolBenchResult result = LF_EToolBenchResult.FAILED;
		ResourceName scrap = LF_ToolBench.ScrapPrefab();
		if (scrap.IsEmpty())
			LF_Debug.Error(LF_ToolBench.FEATURE, string.Format("no '%1' in the item catalogue, the bench cannot repair", LF_ToolBench.SCRAP_ID));
		else
			result = LF_ToolBench.Repair(pUserEntity, tool, scrap, m_iFee);

		int playerId = 0;
		if (GetGame().GetPlayerManager())
			playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(pUserEntity);
		if (playerId <= 0)
			return;

		LF_ENotificationType type = LF_ENotificationType.WARNING;
		if (result == LF_EToolBenchResult.DONE)
			type = LF_ENotificationType.SUCCESS;
		LF_NotificationManagerComponent.NotifyPlayer(playerId, "#LF-ToolBench_Title", LF_ToolBench.ResultKey(result), 5.0, type, LF_MoneyFormat.FormatMoney(m_iFee));
	}
}

//------------------------------------------------------------------------------------------------
enum LF_EToolBenchKind
{
	PICKAXE,
	AXE,
	ROD
}
